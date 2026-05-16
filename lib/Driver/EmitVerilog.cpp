// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Driver/EmitVerilog.cpp — `nslc -emit=verilog` driver glue (M7).
//
// Pipeline (mirrors EmitHW.cpp, extended with the M7 stock-CIRCT
// post-processing + ExportVerilog emission per
// driver-emit-verilog.contract.md §2):
//
//   load(input) → Preprocessor::run → Lexer::next → parseCompilationUnit
//   → runSema → Compilation::lowerToNSL → Compilation::runNSLPasses
//   → Compilation::lowerToCIRCT → Compilation::runCIRCTPasses  (M7 NEW)
//   → circt::exportVerilog / circt::exportSplitVerilog          (M7 NEW)
//
// Implementation note: this file is a near-duplicate of EmitHW.cpp
// extended with two additional stages (`runCIRCTPasses`, then the
// ExportVerilog dispatch). The shared preprocess/lex/parse/sema/
// lowering glue is duplicated rather than abstracted because the
// M2-onward driver-glue convention is per-stage-free-function and
// duplication keeps each fixture surface bounded to one TU.
//
// **Output dispatch (Q1 → B; driver-emit-verilog.contract.md §1)**:
// the post-runCIRCTPasses module is sent to one of three sinks
// based on `output_path` argument shape + filesystem state:
//   • empty or "-"               → exportVerilog → `os` (stdout)
//   • ends in "/" OR is-directory → exportSplitVerilog → directory
//   • else                       → exportVerilog → file at path
//
// **Design-doc deviation**: data-model.md §3 names a `Compilation::emit`
// member function. In practice the M2/M5/M6 driver-glue pattern keeps
// emission in the per-stage free function (matching emitTokens /
// emitAST / emitMLIR / emitHW); a Compilation::emit member function
// would awkwardly need an ostream parameter that doesn't match the
// design-doc signature. The dispatch lives here in emitVerilog;
// data-model.md will be updated post-implementation to reflect this.

#include "nsl/Driver/EmitVerilog.h"

#include "circt/Conversion/ExportVerilog.h"
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
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace nsl::driver {

namespace {

/// Split a `-D NAME=value` argument (mirrors
/// EmitHW/EmitMLIR/EmitAST/EmitTokens).
std::pair<std::string, std::string> splitMacroDef(llvm::StringRef arg) {
  std::size_t const eq = arg.find('=');
  if (eq == llvm::StringRef::npos) {
    return {arg.str(), "1"};
  }
  return {arg.substr(0, eq).str(), arg.substr(eq + 1).str()};
}

/// Replay `#line` directives that survived the preprocessor seam
/// (mirrors EmitHW.cpp's replayer; same logic).
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

/// Classify the output sink based on `output_path` argument shape +
/// filesystem state. Returns one of three kinds; see
/// driver-emit-verilog.contract.md §1 dispatch table.
enum class OutputSink { Stdout, SingleFile, SplitDirectory };

OutputSink classifyOutputSink(llvm::StringRef output_path) {
  if (output_path.empty() || output_path == "-") {
    return OutputSink::Stdout;
  }
  if (output_path.ends_with("/")) {
    return OutputSink::SplitDirectory;
  }
  // No trailing slash — check if the path exists and is a directory.
  // `is_directory` returns false for a non-existent path; both
  // "doesn't exist" and "exists as file" fall through to SingleFile.
  if (llvm::sys::fs::is_directory(output_path)) {
    return OutputSink::SplitDirectory;
  }
  return OutputSink::SingleFile;
}

} // namespace

int emitVerilog(llvm::StringRef input_path, llvm::StringRef output_path,
                const EmitTokensOptions &opts, llvm::raw_ostream &os,
                llvm::raw_ostream &err) {
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

  // ---------- M7 stock-CIRCT post-processing pipeline ----------
  if (mlir::failed(comp.runCIRCTPasses(*module)) || diag.hasError()) {
    diag.renderAll(err, opts.diagnostic_json ? DiagnosticEngine::Format::JSON
                                             : DiagnosticEngine::Format::Text);
    return 1;
  }

  // ---------- M7 ExportVerilog (single-file OR split-file) ----------
  // Dispatch per driver-emit-verilog.contract.md §1.
  OutputSink const sink = classifyOutputSink(output_path);

  if (sink == OutputSink::Stdout) {
    // Buffer single-file Verilog into a string so the
    // "no partial output on error" rule holds. Only on success do
    // we flush to `os`.
    std::string buf;
    {
      llvm::raw_string_ostream rs(buf);
      if (mlir::failed(circt::exportVerilog(*module, rs))) {
        diag.renderAll(err, opts.diagnostic_json
                                ? DiagnosticEngine::Format::JSON
                                : DiagnosticEngine::Format::Text);
        return 1;
      }
    }
    if (diag.hasError()) {
      diag.renderAll(err, opts.diagnostic_json
                              ? DiagnosticEngine::Format::JSON
                              : DiagnosticEngine::Format::Text);
      return 1;
    }
    os << buf;
  } else if (sink == OutputSink::SingleFile) {
    // Single-file to disk. Open with truncate; binary mode (LLVM
    // raw_fd_ostream defaults are appropriate for SystemVerilog
    // text).
    std::error_code ec;
    llvm::raw_fd_ostream ofs(output_path, ec, llvm::sys::fs::OF_None);
    if (ec) {
      err << "could not open output file " << output_path << ": "
          << ec.message() << "\n";
      return 4;
    }
    std::string buf;
    {
      llvm::raw_string_ostream rs(buf);
      if (mlir::failed(circt::exportVerilog(*module, rs))) {
        diag.renderAll(err, opts.diagnostic_json
                                ? DiagnosticEngine::Format::JSON
                                : DiagnosticEngine::Format::Text);
        return 1;
      }
    }
    if (diag.hasError()) {
      diag.renderAll(err, opts.diagnostic_json
                              ? DiagnosticEngine::Format::JSON
                              : DiagnosticEngine::Format::Text);
      return 1;
    }
    ofs << buf;
    ofs.close();
  } else {
    // SplitDirectory: create the directory if missing, then call
    // circt::exportSplitVerilog which writes one .v per hw.module
    // into the directory.
    if (std::error_code const ec =
            llvm::sys::fs::create_directories(output_path)) {
      err << "could not create output directory " << output_path << ": "
          << ec.message() << "\n";
      return 4;
    }
    if (mlir::failed(circt::exportSplitVerilog(*module, output_path))) {
      diag.renderAll(err, opts.diagnostic_json
                              ? DiagnosticEngine::Format::JSON
                              : DiagnosticEngine::Format::Text);
      return 1;
    }
    if (diag.hasError()) {
      diag.renderAll(err, opts.diagnostic_json
                              ? DiagnosticEngine::Format::JSON
                              : DiagnosticEngine::Format::Text);
      return 1;
    }
  }

  if (diag.numWarnings() > 0) {
    diag.renderAll(err, opts.diagnostic_json ? DiagnosticEngine::Format::JSON
                                             : DiagnosticEngine::Format::Text);
  }
  return 0;
}

} // namespace nsl::driver
