// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/Driver/EmitMLIR.h — public entry point for the
// `nslc -emit=mlir` driver path (M5; FR-022).
//
// The driver shape mirrors `EmitTokens.h` / `EmitAST.h`: a single
// `emitMLIR()` free function invoked by `tools/nslc/main.cpp`.
// Behaviour pinned by
// `specs/008-m5-structural-passes/contracts/driver-emit-mlir.contract.md`
// §1–§4 (CLI flag, default-printer output format, exit codes,
// determinism).

#ifndef NSL_DRIVER_EMITMLIR_H
#define NSL_DRIVER_EMITMLIR_H

#include "nsl/Driver/EmitTokens.h" // re-uses EmitTokensOptions for flag set

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

namespace nsl::driver {

/// Run `-emit=mlir` over `input_path`. Loads the file, runs M1
/// preprocess + M1 lex + M2 parse + M3 Sema, then M5 AST → `nsl`
/// dialect lowering + structural-expansion pass pipeline (six
/// passes per FR-012). Prints the post-pipeline `mlir::ModuleOp`
/// to `os` using MLIR's default printer (Q2 → Option A; Phase 2
/// pipeline runs as no-ops, so the printed IR is the post-AST→nsl
/// shape).
///
/// The MLIR text is BUFFERED in a `std::string` until completion;
/// on a diagnostic-bearing run nothing is written to `os` (matches
/// `EmitAST`'s "no partial output on error" rule).
///
/// Exit codes per `driver-emit-mlir.contract.md` §4:
///   - 0: success.
///   - 1: at least one error-severity diagnostic at any pipeline
///        stage (preprocess / lex / parse / Sema / lowering / pass).
///   - 3: input file could not be opened.
///
/// The flag set is identical to `EmitTokensOptions` (M5 adds only
/// the `-emit=mlir` stage; all other flags inherit from M1/M2).
int emitMLIR(llvm::StringRef input_path, const EmitTokensOptions &opts,
             llvm::raw_ostream &os, llvm::raw_ostream &err);

} // namespace nsl::driver

#endif // NSL_DRIVER_EMITMLIR_H
