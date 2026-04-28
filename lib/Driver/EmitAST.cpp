// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Driver/EmitAST.cpp — `nslc -emit=ast` driver glue.
//
// Pipeline (per `nslc-emit-ast.contract.md` §"Behavior"):
//
//   load(input)  ──►  Preprocessor::run  ──►  addBufferInMemory
//        │                                          │
//        │                                          ▼
//        │                                       Lexer::next
//        │                                          │
//        │                                          ▼
//        │                                  parseCompilationUnit
//        │                                          │
//        ▼                                          ▼
//   SourceManager (post-#line virtual coords)   AST printer
//
// The AST text is BUFFERED in a `std::string` via `raw_string_ostream`
// so partial output can never reach `os` on a diagnostic-bearing run
// (FR-022 "no partial output on error").

#include "nsl/Driver/EmitAST.h"

#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/Printer.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"
#include "nsl/Driver/Sema.h"
#include "nsl/Lex/Lexer.h"
#include "nsl/Parse/Parser.h"
#include "nsl/Preprocess/Preprocessor.h"
#include "nsl/Sema/Sema.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/raw_ostream.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace nsl::driver {

namespace {

/// Split a `-D NAME=value` argument into `(name, value)`. A bare
/// `-D NAME` (no `=`) maps to `(NAME, "1")`.
std::pair<std::string, std::string> splitMacroDef(llvm::StringRef arg) {
  std::size_t const eq = arg.find('=');
  if (eq == llvm::StringRef::npos) {
    return {arg.str(), "1"};
  }
  return {arg.substr(0, eq).str(), arg.substr(eq + 1).str()};
}

/// Replay `#line` directives that survived the preprocessor seam onto
/// `synth_fid`. Mirrors `EmitTokens.cpp`'s replayer — same logic,
/// different SourceManager. The lexer needs the directives on the
/// SYNTHETIC FileID it scans, not on the original input FileID.
void replayLineDirectives(SourceManager &sm, FileID synth_fid) {
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
    std::size_t i = 6; // past "#line "
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

int emitAST(llvm::StringRef input_path, const EmitTokensOptions &opts,
            llvm::raw_ostream &os, llvm::raw_ostream &err) {
  SourceManager sm;
  DiagnosticEngine diag(sm);

  llvm::ErrorOr<FileID> fid_or = sm.loadFile(input_path);
  if (!fid_or) {
    err << "could not open " << input_path << ": "
        << fid_or.getError().message() << "\n";
    return 3;
  }
  FileID const input_fid = *fid_or;

  // Construct the include-search path. Quote-form from `-I`; angle-form
  // from NSL_INCLUDE.
  preprocess::IncludeSearchPath search;
  for (const auto &dir : opts.include_paths) {
    search.appendQuotePath(dir);
  }
  search.populateAngleFromEnv();

  // Predefine macros from `-D NAME=value`.
  std::vector<std::pair<std::string, std::string>> predefined;
  predefined.reserve(opts.predefined_macros.size());
  for (const auto &arg : opts.predefined_macros) {
    predefined.push_back(splitMacroDef(arg));
  }

  preprocess::Preprocessor pp(sm, diag, search, predefined);
  llvm::ErrorOr<std::string> pp_out = pp.run(input_fid);

  if (diag.hasError() || !pp_out) {
    diag.renderAll(err, opts.diagnostic_json ? DiagnosticEngine::Format::JSON
                                             : DiagnosticEngine::Format::Text);
    return 1;
  }

  // Register the preprocessed buffer as a synthetic in-memory buffer.
  std::string synth_path = input_path.str();
  std::vector<char> synth_bytes(pp_out->begin(), pp_out->end());
  FileID const synth_fid =
      sm.addBufferInMemory(std::move(synth_path), std::move(synth_bytes));

  // Replay surviving `#line` directives onto the synthetic FileID so
  // virtual-location resolution works for parser-emitted SourceRanges.
  replayLineDirectives(sm, synth_fid);

  Lexer lexer(sm, synth_fid, diag);

  // Drive the parser. On failure parseCompilationUnit returns nullptr
  // and the diagnostic is already in the engine.
  auto cu = parse::parseCompilationUnit(lexer, diag);

  // M3 Phase 2 (T017, FR-019): run Sema after parse and before AST
  // printing so the post-Sema printer (Phase 3 T031–T033) can emit
  // the resolved-type / decl-loc enrichments. On Sema failure, exit
  // non-zero with no AST output on stdout (parallel to the
  // parse-failure path below). At Phase 2 the `runSema` body is a
  // no-op stub — `result.hasErrors` is false on every well-parsed
  // input — so this call is observable but inert.
  sema::SemaResult sema_result;
  if (cu) {
    sema_result = driver::runSema(*cu, diag);
  }

  // Buffer the AST text BEFORE checking for errors — we want to
  // generate the bytes in a deterministic memory order, then either
  // commit (on no-errors) or discard (on error). The buffering
  // implements FR-022's "no partial output on error".
  std::string buf;
  if (cu) {
    llvm::raw_string_ostream rs(buf);
    ast::print(*cu, sm, rs);
    rs.flush();
  }

  if (diag.hasError() || !cu || sema_result.hasErrors) {
    diag.renderAll(err, opts.diagnostic_json ? DiagnosticEngine::Format::JSON
                                             : DiagnosticEngine::Format::Text);
    return 1;
  }

  // Success: commit AST to stdout, then render any non-error diagnostics
  // (warnings / notes) to stderr.
  os << buf;
  if (diag.numWarnings() > 0) {
    diag.renderAll(err, opts.diagnostic_json ? DiagnosticEngine::Format::JSON
                                             : DiagnosticEngine::Format::Text);
  }
  return 0;
}

} // namespace nsl::driver
