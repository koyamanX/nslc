// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/NSLExpandGeneratePass.cpp — slot 2 of the M5
// structural-expansion pipeline (FR-014).
//
// **At Phase 2 this pass is a registered NO-OP slot.** The real
// generate-unroll body lands at `tasks.md` T070 (US2 phase) per
// `pass-pipeline.contract.md` §2 row 2.

#include "nsl/Lower/Lower.h"

#include "nsl/Dialect/NSL/IR/NSLDialect.h"

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

namespace nsl::lower {

namespace {

class NSLExpandGeneratePass
    : public mlir::PassWrapper<NSLExpandGeneratePass,
                               mlir::OperationPass<mlir::ModuleOp>> {
public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(NSLExpandGeneratePass)

  llvm::StringRef getArgument() const final { return "nsl-expand-generate"; }
  llvm::StringRef getDescription() const final {
    return "Slot 2: unroll nsl.structural_generate into N copies of body; "
           "substitute %IDENT% loop-var references (M5 FR-014).";
  }

  void runOnOperation() final {
    // Phase 2 NO-OP slot. Real body lands at T070 (US2 phase).
  }
};

} // namespace

std::unique_ptr<mlir::Pass> createNSLExpandGeneratePass() {
  return std::make_unique<NSLExpandGeneratePass>();
}

} // namespace nsl::lower
