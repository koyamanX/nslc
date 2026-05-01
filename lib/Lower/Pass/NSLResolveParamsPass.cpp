// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/NSLResolveParamsPass.cpp — slot 1 of the M5
// structural-expansion pipeline (FR-013).
//
// **At Phase 2 this pass is a registered NO-OP slot.** The real
// param-resolution body lands at `tasks.md` T069 (US2 phase). The
// no-op slot exists so the full pipeline registers cleanly with
// MLIR's pass-registry from Phase 2 onward.
//
// Anchors:
//   - `specs/008-m5-structural-passes/contracts/lower-api.contract.md`
//     §2.2 row 1
//   - `specs/008-m5-structural-passes/contracts/pass-pipeline.contract.md`
//     §2 row 1

#include "nsl/Lower/Lower.h"

#include "nsl/Dialect/NSL/IR/NSLDialect.h"

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

namespace nsl::lower {

namespace {

class NSLResolveParamsPass
    : public mlir::PassWrapper<NSLResolveParamsPass,
                               mlir::OperationPass<mlir::ModuleOp>> {
public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(NSLResolveParamsPass)

  llvm::StringRef getArgument() const final { return "nsl-resolve-params"; }
  llvm::StringRef getDescription() const final {
    return "Slot 1: substitute nsl.param_int / nsl.param_str references with "
           "constants from the M3 Sema parameter map (M5 FR-013).";
  }

  void runOnOperation() final {
    // Phase 2 NO-OP slot. Real body lands at T069 (US2 phase).
  }
};

} // namespace

std::unique_ptr<mlir::Pass> createNSLResolveParamsPass() {
  return std::make_unique<NSLResolveParamsPass>();
}

} // namespace nsl::lower
