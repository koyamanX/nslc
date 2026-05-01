// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/NSLExpandVariablesPass.cpp — slot 3 of the M5
// structural-expansion pipeline (FR-015).
//
// **At Phase 2 this pass is a registered NO-OP slot.** The real
// variable-to-SSA-chain body lands at `tasks.md` T081 (US3 phase)
// per `pass-pipeline.contract.md` §2 row 3.

#include "nsl/Lower/Lower.h"

#include "nsl/Dialect/NSL/IR/NSLDialect.h"

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

namespace nsl::lower {

namespace {

class NSLExpandVariablesPass
    : public mlir::PassWrapper<NSLExpandVariablesPass,
                               mlir::OperationPass<mlir::ModuleOp>> {
public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(NSLExpandVariablesPass)

  llvm::StringRef getArgument() const final { return "nsl-expand-variables"; }
  llvm::StringRef getDescription() const final {
    return "Slot 3: convert nsl.variable to SSA chain of nsl.wire+nsl.transfer; "
           "per-field for struct-typed; preserve S12 partial-assignment (M5 FR-015).";
  }

  void runOnOperation() final {
    // Phase 2 NO-OP slot. Real body lands at T081 (US3 phase).
  }
};

} // namespace

std::unique_ptr<mlir::Pass> createNSLExpandVariablesPass() {
  return std::make_unique<NSLExpandVariablesPass>();
}

} // namespace nsl::lower
