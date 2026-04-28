// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/Driver/EmitAST.h — public entry point for the
// `nslc -emit=ast` driver path (FR-022, FR-024).
//
// The driver shape mirrors `EmitTokens.h`: a single `emitAST()` function
// invoked by `tools/nslc/main.cpp`. Behavior pinned by
// `specs/005-m2-parser/contracts/nslc-emit-ast.contract.md` (steps 1–6;
// exit codes 0/1/2/3).

#ifndef NSL_DRIVER_EMITAST_H
#define NSL_DRIVER_EMITAST_H

#include "nsl/Driver/EmitTokens.h" // re-uses EmitTokensOptions for flag set

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

namespace nsl::driver {

/// Run `-emit=ast` over `input_path`. Loads the file, runs the M1
/// preprocessor, lexes the post-preprocess buffer, parses into a
/// `CompilationUnit`, and prints the AST in the canonical text-only
/// S-expression format (per `nslc-emit-ast.contract.md`).
///
/// The AST output is buffered in-memory until completion; on a
/// diagnostic-bearing run nothing is written to `os` (the contract's
/// "no partial output on error" rule).
///
/// Exit codes per the contract:
///   - 0: success.
///   - 1: at least one error-severity diagnostic at any pipeline stage
///        (preprocess / lex / parse).
///   - 3: input file could not be opened.
///
/// The flag set is identical to `EmitTokensOptions` (FR-023: M2 adds
/// only the `-emit=ast` flag itself; all other flags inherit from M1).
int emitAST(llvm::StringRef input_path, const EmitTokensOptions &opts,
            llvm::raw_ostream &os, llvm::raw_ostream &err);

} // namespace nsl::driver

#endif // NSL_DRIVER_EMITAST_H
