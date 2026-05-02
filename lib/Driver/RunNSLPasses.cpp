// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Driver/RunNSLPasses.cpp — `Compilation::runNSLPasses` body
// fill (M5; replaces the M4 stub).
//
// **Specification anchors**:
//   - `specs/008-m5-structural-passes/spec.md` FR-021, FR-012
//     (pipeline ordering frozen).
//   - `specs/008-m5-structural-passes/data-model.md` §2 (six-slot
//     pipeline) + §4 (driver wiring).
//   - `specs/008-m5-structural-passes/contracts/pass-pipeline.contract.md`
//     §1 (pipeline structure freeze).
//
// At M5 the body assembles an `mlir::PassManager` rooted at the
// supplied `mlir::ModuleOp`, registers the six structural-expansion
// passes in the FR-012 frozen order, and runs them. Diagnostics
// route through the shared `DiagnosticBridge` (FR-019).

#include "nsl/Driver/Compilation.h"

#include "../Lower/Pass/Common/DiagnosticBridge.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Lower/Lower.h"

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Support/LogicalResult.h"

namespace nsl::driver {

mlir::LogicalResult Compilation::runNSLPasses(mlir::ModuleOp module) {
  if (diag_.hasError()) {
    return mlir::failure();
  }

  // RAII bridge: every MLIR-internal diagnostic emitted during pass
  // execution (verifier failures, op-emitError calls inside pass
  // bodies) routes through `diag_` per FR-019.
  nsl::lower::DiagnosticBridge bridge(diag_, mlir_ctx_);

  mlir::PassManager pm(&mlir_ctx_, mlir::ModuleOp::getOperationName());

  // Pipeline order frozen by `pass-pipeline.contract.md` §1 (FR-012).
  // Reordering changes semantics; reordering = contract amendment.
  pm.addPass(nsl::lower::createNSLResolveParamsPass());      // slot 1
  pm.addPass(nsl::lower::createNSLExpandGeneratePass());     // slot 2
  pm.addPass(nsl::lower::createNSLExpandVariablesPass());    // slot 3
  pm.addPass(nsl::lower::createNSLExplodeSubmodArrayPass()); // slot 4
  pm.addPass(nsl::lower::createNSLInlineInternalFuncPass()); // slot 5 (no-op at M5)
  pm.addPass(nsl::lower::createNSLCheckSemanticsPass());     // slot 6

  return pm.run(module);
}

} // namespace nsl::driver
