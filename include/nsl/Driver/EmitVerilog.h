// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/Driver/EmitVerilog.h — public entry point for the
// `nslc -emit=verilog` driver path (M7; FR-001, FR-006).
//
// The driver shape mirrors `EmitHW.h`: a single `emitVerilog()` free
// function invoked by `tools/nslc/main.cpp`. Behaviour pinned by
// `specs/011-m7-driver-e2e/contracts/driver-emit-verilog.contract.md`
// §1–§7 (CLI flag, `-o` dispatch table, exit codes, determinism,
// diagnostic routing, build/link, test surface).

#ifndef NSL_DRIVER_EMITVERILOG_H
#define NSL_DRIVER_EMITVERILOG_H

#include "nsl/Driver/EmitTokens.h" // re-uses EmitTokensOptions for flag set

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

namespace nsl::driver {

/// Run `-emit=verilog` over `input_path`. Loads the file, runs M1
/// preprocess + M1 lex + M2 parse + M3 Sema + M5 AST → `nsl`
/// dialect lowering + M5 structural-expansion pipeline + M6
/// `nsl` → CIRCT conversion + M7 stock-CIRCT post-processing
/// pipeline (`createConvertFSMToSVPass` → `createLowerSeqToSVPass`
/// → `createPrepareForEmissionPass`) + `circt::exportVerilog` /
/// `circt::exportSplitVerilog` per the `-o` argument dispatch table.
///
/// **Output dispatch (Q1 → B; pinned by
/// `driver-emit-verilog.contract.md` §1)**:
///   - `output_path` empty OR `"-"` → single-file Verilog to `os`
///     (typically stdout).
///   - `output_path` ends in `'/'` OR names an existing directory →
///     split-file via `circt::exportSplitVerilog(module, path)`;
///     directory is created via `llvm::sys::fs::create_directories`
///     if missing.
///   - else → single-file via `circt::exportVerilog(module, ofs)` to
///     `output_path`.
///
/// **No partial output on error** — Verilog bytes are buffered (in a
/// `std::string` for single-file mode, or staged via CIRCT's own
/// split-file helper for directory mode) and only written to the
/// final sink on full pipeline success.
///
/// Exit codes per `driver-emit-verilog.contract.md` §2:
///   - 0: success.
///   - 1: at least one error-severity diagnostic at any pipeline
///        stage.
///   - 3: input file could not be opened.
///   - 4: output sink (split-file directory) could not be created.
///
/// The flag set is identical to `EmitTokensOptions` plus the
/// optional `output_path` argument (typically populated from `-o
/// <path>` parsed in `tools/nslc/main.cpp`).
int emitVerilog(llvm::StringRef input_path, llvm::StringRef output_path,
                const EmitTokensOptions &opts, llvm::raw_ostream &os,
                llvm::raw_ostream &err);

} // namespace nsl::driver

#endif // NSL_DRIVER_EMITVERILOG_H
