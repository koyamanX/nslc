// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Driver/LowerToNSL.cpp — `Compilation::lowerToNSL` body fill
// (M5; replaces the M4 stub).
//
// **Specification anchors**:
//   - `specs/008-m5-structural-passes/spec.md` FR-020.
//   - `specs/008-m5-structural-passes/data-model.md` §4.
//   - `specs/008-m5-structural-passes/contracts/lower-api.contract.md`
//     §2.1.
//
// At M5 the body installs a `DiagnosticBridge` (FR-019) for the
// duration of the lowering call, gates on `SemaResult::hasErrors()`
// per FR-020, then delegates to `nsl::lower::astToMLIR(...)`.
//
// **Pre-condition** (caller's responsibility): the `nslc` driver
// dispatcher only routes here when `EmitKind` reaches the
// post-`-emit=ast` arms. Stage-by-stage `hasErrors()` gates already
// handled by the dispatcher; this body re-checks `sr.hasErrors()`
// defensively per the M5 lowering contract.

#include "nsl/Driver/Compilation.h"

#include "../Lower/Pass/Common/DiagnosticBridge.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Lower/Lower.h"

namespace nsl::driver {

mlir::OwningOpRef<mlir::ModuleOp> Compilation::lowerToNSL(
    ast::CompilationUnit &unit, sema::SemaResult &sema_result) {
  if (diag_.hasError()) {
    // Defensive gate per FR-020. Caller's stage-dispatcher should
    // have already short-circuited here, but the lowering body MUST
    // NOT proceed past Sema-error input.
    return {};
  }

  // RAII bridge: every MLIR-internal diagnostic emitted during the
  // visitor's walk routes through `diag_` per FR-019. Released when
  // this function returns.
  nsl::lower::DiagnosticBridge bridge(diag_, mlir_ctx_);

  return nsl::lower::astToMLIR(mlir_ctx_, unit, sema_result);
}

} // namespace nsl::driver
