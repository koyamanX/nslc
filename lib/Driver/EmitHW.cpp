// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Driver/EmitHW.cpp — `nslc -emit=hw` driver glue (M6).
//
// Pipeline (mirrors EmitMLIR.cpp, extended with the M6
// nsl→CIRCT conversion stage per
// driver-emit-hw.contract.md §2):
//
//   load(input)  ──►  Preprocessor::run  ──►  addBufferInMemory
//        │                                          │
//        │                                          ▼
//        │                                       Lexer::next
//        │                                          │
//        │                                          ▼
//        │                                  parseCompilationUnit
//        │                                          │
//        │                                          ▼
//        │                                    runSema
//        │                                          │
//        │                                          ▼
//        ▼                                Compilation::lowerToNSL
//   SourceManager (post-#line virtual coords)         │
//                                                     ▼
//                                          Compilation::runNSLPasses
//                                                     │
//                                                     ▼
//                                          Compilation::lowerToCIRCT  ← M6
//                                                     │
//                                                     ▼
//                                          ModuleOp::print (default flags)
//
// Implementation note: this file is a near-duplicate of EmitMLIR.cpp
// extended with the M6 `lowerToCIRCT` stage. The shared preprocess /
// lex / parse / sema / lowering glue is duplicated rather than
// abstracted into a helper because (a) it matches the M5 EmitMLIR
// pattern exactly — diverging would create test-fixture surface for
// no benefit at M6, and (b) future post-M7 changes to either
// emit-stage may legitimately diverge (e.g., M7 `-emit=verilog` adds
// `circt::exportVerilog` invocation; M6 `-emit=hw` halts strictly
// per Q2 specify-time → A).

#include "nsl/Driver/EmitHW.h"

#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/OperationSupport.h"
#include "mlir/Support/LogicalResult.h"
#include "nsl/AST/CompilationUnit.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"
#include "nsl/Driver/Compilation.h"
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

/// Split a `-D NAME=value` argument (mirrors EmitMLIR/EmitAST/EmitTokens).
std::pair<std::string, std::string> splitMacroDef(llvm::StringRef arg) {
  std::size_t const eq = arg.find('=');
  if (eq == llvm::StringRef::npos) {
    return {arg.str(), "1"};
  }
  return {arg.substr(0, eq).str(), arg.substr(eq + 1).str()};
}

/// Replay `#line` directives that survived the preprocessor seam
/// (mirrors EmitMLIR.cpp's replayer; same logic).
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
      ++off;
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

int emitHW(llvm::StringRef input_path, const EmitTokensOptions &opts,
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

  // ---------- Preprocess ----------
  preprocess::IncludeSearchPath search;
  for (const auto &dir : opts.include_paths) {
    search.appendQuotePath(dir);
  }
  search.populateAngleFromEnv();

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

  // ---------- Lex (after replay of #line directives) ----------
  std::string synth_path = input_path.str();
  std::vector<char> synth_bytes(pp_out->begin(), pp_out->end());
  FileID const synth_fid =
      sm.addBufferInMemory(std::move(synth_path), std::move(synth_bytes));
  replayLineDirectives(sm, synth_fid);

  Lexer lexer(sm, synth_fid, diag);

  // ---------- Parse + Sema ----------
  auto cu = parse::parseCompilationUnit(lexer, diag);
  sema::SemaResult sema_result;
  if (cu) {
    sema_result = driver::runSema(*cu, diag);
  }

  if (diag.hasError() || !cu || sema_result.hasErrors) {
    diag.renderAll(err, opts.diagnostic_json ? DiagnosticEngine::Format::JSON
                                             : DiagnosticEngine::Format::Text);
    return 1;
  }

  // ---------- M5 AST → nsl + structural-expansion pipeline ----------
  Compilation comp(diag);
  auto module = comp.lowerToNSL(*cu, sema_result);
  if (!module || diag.hasError()) {
    diag.renderAll(err, opts.diagnostic_json ? DiagnosticEngine::Format::JSON
                                             : DiagnosticEngine::Format::Text);
    return 1;
  }
  if (mlir::failed(comp.runNSLPasses(*module)) || diag.hasError()) {
    diag.renderAll(err, opts.diagnostic_json ? DiagnosticEngine::Format::JSON
                                             : DiagnosticEngine::Format::Text);
    return 1;
  }

  // ---------- M6 nsl → CIRCT conversion ----------
  if (mlir::failed(comp.lowerToCIRCT(*module)) || diag.hasError()) {
    diag.renderAll(err, opts.diagnostic_json ? DiagnosticEngine::Format::JSON
                                             : DiagnosticEngine::Format::Text);
    return 1;
  }

  // ---------- Print (default printer per driver-emit-hw.contract.md §4) ----------
  // Buffer the CIRCT-dialect MLIR text BEFORE checking for errors —
  // same "no partial output on error" rule as EmitMLIR.
  std::string buf;
  {
    llvm::raw_string_ostream rs(buf);
    module->print(rs); // default mlir::OpPrintingFlags()
    rs.flush();
  }

  if (diag.hasError()) {
    diag.renderAll(err, opts.diagnostic_json ? DiagnosticEngine::Format::JSON
                                             : DiagnosticEngine::Format::Text);
    return 1;
  }

  os << buf;
  // Match nsl-opt's text writer + the EmitMLIR convention: trailing
  // newline so `nslc -emit=hw foo.nsl | nsl-opt -` round-trip is a
  // fixed point.
  os << "\n";
  if (diag.numWarnings() > 0) {
    diag.renderAll(err, opts.diagnostic_json ? DiagnosticEngine::Format::JSON
                                             : DiagnosticEngine::Format::Text);
  }
  return 0;
}

} // namespace nsl::driver
