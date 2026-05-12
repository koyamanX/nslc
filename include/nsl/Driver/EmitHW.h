// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/Driver/EmitHW.h — public entry point for the
// `nslc -emit=hw` driver path (M6; FR-023).
//
// The driver shape mirrors `EmitMLIR.h`: a single `emitHW()` free
// function invoked by `tools/nslc/main.cpp`. Behaviour pinned by
// `specs/010-m6-circt-lowering/contracts/driver-emit-hw.contract.md`
// §1–§6 (CLI flag, default-printer output format, exit codes,
// determinism, strict-boundary halt at the nsl→CIRCT seam).

#ifndef NSL_DRIVER_EMITHW_H
#define NSL_DRIVER_EMITHW_H

#include "nsl/Driver/EmitTokens.h" // re-uses EmitTokensOptions for flag set

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

namespace nsl::driver {

/// Run `-emit=hw` over `input_path`. Loads the file, runs M1
/// preprocess + M1 lex + M2 parse + M3 Sema + M5 AST → `nsl`
/// dialect lowering + M5 structural-expansion pipeline + M6
/// `nsl` → CIRCT conversion. Prints the post-conversion
/// `mlir::ModuleOp` to `os` using MLIR's default printer per
/// `driver-emit-hw.contract.md` §4.
///
/// **Halting rule (Q2 specify-time → A)**: emission halts strictly
/// at the nsl→CIRCT conversion boundary. The four stock CIRCT
/// passes (`convertFSMToSeq`, `lowerSeqToSV`, `prepareForEmission`,
/// `exportVerilog`) are M7's responsibility and are NOT invoked
/// from `nslc -emit=hw`.
///
/// The MLIR text is BUFFERED in a `std::string` until completion;
/// on a diagnostic-bearing run nothing is written to `os` (matches
/// `EmitAST` / `EmitMLIR`'s "no partial output on error" rule).
///
/// Exit codes per `driver-emit-hw.contract.md` §3:
///   - 0: success.
///   - 1: at least one error-severity diagnostic at any pipeline
///        stage.
///   - 3: input file could not be opened.
///
/// The flag set is identical to `EmitTokensOptions`.
int emitHW(llvm::StringRef input_path, const EmitTokensOptions &opts,
           llvm::raw_ostream &os, llvm::raw_ostream &err);

} // namespace nsl::driver

#endif // NSL_DRIVER_EMITHW_H
