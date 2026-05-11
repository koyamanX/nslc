// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/LSP/NslTU.cpp — per-document state impl with real
// preprocess + lex + parse + sema pipeline (Phase 3 / US1, T069).
// Mirrors the M3 driver pipeline in `lib/Driver/EmitAST.cpp`
// step-for-step but operates on an in-memory buffer instead of a
// file path.

#include "NslTU.h"

#include "IncludeSearchPath.h"
#include "nsl/AST/CompilationUnit.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"
#include "nsl/Driver/Sema.h"
#include "nsl/Lex/Lexer.h"
#include "nsl/Parse/Parser.h"
#include "nsl/Preprocess/Preprocessor.h"
#include "nsl/Sema/Sema.h"
#include "nsl/Sema/SymbolTable.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/ErrorOr.h"

#include <utility>
#include <vector>

namespace nsl {
namespace lsp {

NslTU::NslTU() = default;
NslTU::~NslTU() = default;

namespace {

/// Scan the synthetic preprocessed buffer for `#line N "path"` lines
/// and register each on `synth_fid` via `SourceManager::addLineDirective`.
/// Mirrors `lib/Driver/EmitAST.cpp::replayLineDirectives` — without
/// it, `resolveVirtual` falls back to the synth-buffer's own path and
/// physical line number, breaking diagnostic localization (Principle
/// IV) and the FR-026 include-from-notes auto-attach for any
/// diagnostic in `#include`'d content.
void replayLineDirectivesOntoSynth(SourceManager &sm, FileID synth_fid) {
  llvm::StringRef const syn = sm.getBuffer(synth_fid);
  std::size_t off = 0;
  while (off < syn.size()) {
    std::size_t const line_begin = off;
    while (off < syn.size() && syn[off] != '\n') {
      ++off;
    }
    std::size_t const line_end_excl = off;
    if (off < syn.size()) {
      ++off; // consume newline
    }
    llvm::StringRef const line =
        syn.substr(line_begin, line_end_excl - line_begin);
    if (!line.starts_with("#line ")) {
      continue;
    }
    std::size_t i = 6;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
      ++i;
    }
    if (i >= line.size() || line[i] < '0' || line[i] > '9') {
      continue;
    }
    long long ln = 0;
    while (i < line.size() && line[i] >= '0' && line[i] <= '9') {
      ln = ln * 10 + (line[i] - '0');
      ++i;
    }
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
      ++i;
    }
    std::string vpath;
    if (i < line.size() && line[i] == '"') {
      ++i;
      std::size_t const fb = i;
      while (i < line.size() && line[i] != '"') {
        ++i;
      }
      if (i < line.size()) {
        vpath = line.substr(fb, i - fb).str();
      }
    }
    auto at_off = static_cast<uint32_t>(off);
    sm.addLineDirective(SourceLocation::make(synth_fid, at_off),
                        static_cast<uint32_t>(ln), llvm::StringRef(vpath));
  }
}

} // namespace

namespace {

void runPipeline(int version, std::string contents,
                 const IncludeSearchPath &includes, NslTU::State *out) {
  out->version = version;
  out->contents = std::move(contents);
  out->ast.reset();
  out->symbols.reset();
  out->diagnostics.clear();

  // 1. SourceManager + DiagnosticEngine.
  auto sm = std::make_shared<nsl::SourceManager>();
  nsl::DiagnosticEngine diag(*sm);

  // 2. Register the document buffer in the SourceManager.
  std::vector<char> bytes(out->contents.begin(), out->contents.end());
  nsl::FileID input_fid = sm->addBufferInMemory(
      std::string("file:///in-memory.nsl"), std::move(bytes));

  // 3. Build preprocess::IncludeSearchPath from the LSP-side
  //    paths (NSL_INCLUDE) plus the document's parent directory
  //    for quote-form resolution. Phase 3 keeps quote-form
  //    document-relative resolution implicit (FR-020b is a
  //    follow-up; quote-form lookups against an in-memory URI
  //    have nothing useful to resolve to in this Phase).
  preprocess::IncludeSearchPath search;
  for (const auto &p : includes.anglePaths())
    search.appendAnglePath(p);

  std::vector<std::pair<std::string, std::string>> predefined;

  preprocess::Preprocessor pp(*sm, diag, search, predefined);
  llvm::ErrorOr<std::string> pp_out = pp.run(input_fid);

  if (pp_out) {
    // 4. Register the preprocessed buffer + run the lexer + parser.
    std::vector<char> synth_bytes(pp_out->begin(), pp_out->end());
    nsl::FileID synth_fid = sm->addBufferInMemory(
        std::string("file:///in-memory.nsl-pp"), std::move(synth_bytes));

    // Replay `#line` directives from the synthetic buffer so
    // `resolveVirtual` resolves locations back to original file
    // coordinates — required for Principle IV diagnostic
    // localization and FR-026 include-from-notes auto-attach.
    replayLineDirectivesOntoSynth(*sm, synth_fid);

    Lexer lexer(*sm, synth_fid, diag);
    auto cu = parse::parseCompilationUnit(lexer, diag);

    if (cu) {
      // 5. Sema.
      sema::SemaResult sema_res = driver::runSema(*cu, diag);
      out->symbols = std::move(sema_res.symbols);
      // (TypeSystem currently moves with sema_res.types but
      // NslTU::State doesn't expose a types field at T3 — the
      // post-Sema printer / future LSP features (T4 hover) will
      // request it. Phase 3 doesn't need it.)
      out->ast = std::move(cu);
    }
  }

  // 6. Capture every diagnostic accumulated across the pipeline.
  for (const auto &d : diag.diagnostics())
    out->diagnostics.push_back(d);

  // 7. Stash the SourceManager alongside the state so the protocol
  //    layer can resolve diagnostic locations. State doesn't
  //    declare a sm_ field publicly per the contract; we hold a
  //    shared_ptr in a side-channel below.
  // (Adopt the contract field-set strictly: keeping sm_ as a
  // separate member.)
  out->source_manager = sm;
}

} // namespace

int NslTU::reparse(int version, std::string contents,
                   const IncludeSearchPath &includes) {
  std::lock_guard<std::mutex> guard(mtx_);
  runPipeline(version, std::move(contents), includes, &state_);
  return version;
}

int NslTU::latestVersion() const {
  std::lock_guard<std::mutex> guard(mtx_);
  return state_.version;
}

} // namespace lsp
} // namespace nsl
