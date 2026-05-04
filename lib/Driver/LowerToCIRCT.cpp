// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Driver/LowerToCIRCT.cpp — `Compilation::lowerToCIRCT` body
// (M6).
//
// **Specification anchors**:
//   - `specs/010-m6-circt-lowering/spec.md` FR-022, FR-024.
//   - `specs/010-m6-circt-lowering/data-model.md` §4 — driver
//     member-function shape.
//   - `specs/010-m6-circt-lowering/contracts/lower-api.contract.md`
//     §4 — semantics of `createNSLToCIRCTPass`.
//   - `specs/010-m6-circt-lowering/research.md` §10 — implementation
//     mirror of M5's `Compilation::runNSLPasses`.
//
// The body assembles a single-pass `mlir::PassManager` rooted at
// the supplied `mlir::ModuleOp`, registers `NSLToCIRCTPass`, and
// runs it. Diagnostics route through the shared `DiagnosticBridge`
// (Constitution Principle IV).

#include "../Lower/Pass/Common/DiagnosticBridge.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Support/LogicalResult.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Driver/Compilation.h"
#include "nsl/Lower/Lower.h"

namespace nsl::driver {

mlir::LogicalResult Compilation::lowerToCIRCT(mlir::ModuleOp module) {
  if (diag_.hasError()) {
    return mlir::failure();
  }

  // RAII bridge: every MLIR-internal diagnostic emitted during the
  // conversion pass (pattern-match failures, type-converter errors,
  // verifier diagnostics on the converted IR) routes through `diag_`
  // per Constitution Principle IV.
  nsl::lower::DiagnosticBridge bridge(diag_, mlir_ctx_);

  mlir::PassManager pm(&mlir_ctx_, mlir::ModuleOp::getOperationName());
  pm.addPass(nsl::lower::createNSLToCIRCTPass());

  return pm.run(module);
}

} // namespace nsl::driver
