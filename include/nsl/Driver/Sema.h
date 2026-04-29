// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/Driver/Sema.h — driver-side wrapper around
// `nsl::sema::Sema::run` for `nslc`'s `-emit=*` pipelines.
//
// `lib/Driver/Sema.cpp` provides the thin free-function entry point
// that constructs a `Sema(diag)` and forwards `run(unit)`. Higher-
// level driver glue (`lib/Driver/EmitAST.cpp`) calls this after a
// successful parse and before printing the AST so the post-Sema
// printer enrichments are observable end-to-end (per FR-019).

#ifndef NSL_DRIVER_SEMA_H
#define NSL_DRIVER_SEMA_H

#include "nsl/Sema/Sema.h"

namespace nsl {
class DiagnosticEngine;
} // namespace nsl

namespace nsl::ast {
class CompilationUnit;
} // namespace nsl::ast

namespace nsl::driver {

/// Run Sema over `unit`, routing every diagnostic through `diag`.
/// Returns the `SemaResult` whose `symbols` / `types` move-own the
/// produced state for downstream stages (`-emit=ast` post-Sema
/// printer; `-emit=mlir` at M5+).
///
/// This is a thin wrapper (~30 lines) — the orchestration is
/// `Sema::run()`'s responsibility. Existing for API symmetry with
/// `emitAST(...)` and `emitTokens(...)` so the M3+ driver code can
/// invoke Sema without manually constructing a `Sema` instance.
sema::SemaResult runSema(ast::CompilationUnit &unit, DiagnosticEngine &diag);

} // namespace nsl::driver

#endif // NSL_DRIVER_SEMA_H
