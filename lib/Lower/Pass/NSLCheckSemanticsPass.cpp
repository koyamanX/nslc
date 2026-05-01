// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/NSLCheckSemanticsPass.cpp — slot 6 of the M5
// structural-expansion pipeline (FR-018).
//
// **At Phase 2 this pass is a registered NO-OP slot.** The real
// residue-detection + sensitive-`Sn` re-check body lands at
// `tasks.md` T096 + T097 (US4 phase). The detection regex is frozen
// by `contracts/residue-detection.contract.md` §2.

#include "nsl/Lower/Lower.h"

#include "nsl/Dialect/NSL/IR/NSLDialect.h"

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

namespace nsl::lower {

namespace {

class NSLCheckSemanticsPass
    : public mlir::PassWrapper<NSLCheckSemanticsPass,
                               mlir::OperationPass<mlir::ModuleOp>> {
public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(NSLCheckSemanticsPass)

  llvm::StringRef getArgument() const final { return "nsl-check-semantics"; }
  llvm::StringRef getDescription() const final {
    return "Slot 6: regex-detect %IDENT% residue across nsl::* StringAttr "
           "values + re-check the six post-expansion-sensitive Sn "
           "(S6/S10/S15/S16/S20/S25) (M5 FR-018).";
  }

  void runOnOperation() final {
    // Phase 2 NO-OP slot. Residue-detection body + sensitive-Sn
    // re-check land at T096 + T097 (US4 phase).
  }
};

} // namespace

std::unique_ptr<mlir::Pass> createNSLCheckSemanticsPass() {
  return std::make_unique<NSLCheckSemanticsPass>();
}

} // namespace nsl::lower
