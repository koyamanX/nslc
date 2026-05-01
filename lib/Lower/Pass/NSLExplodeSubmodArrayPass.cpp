// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/NSLExplodeSubmodArrayPass.cpp — slot 4 of the M5
// structural-expansion pipeline (FR-016).
//
// **At Phase 2 this pass is a registered NO-OP slot.** The real
// submod-array decomposition body lands at `tasks.md` T095 (US4
// phase) per `pass-pipeline.contract.md` §2 row 4.

#include "nsl/Lower/Lower.h"

#include "nsl/Dialect/NSL/IR/NSLDialect.h"

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

namespace nsl::lower {

namespace {

class NSLExplodeSubmodArrayPass
    : public mlir::PassWrapper<NSLExplodeSubmodArrayPass,
                               mlir::OperationPass<mlir::ModuleOp>> {
public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(NSLExplodeSubmodArrayPass)

  llvm::StringRef getArgument() const final {
    return "nsl-explode-submod-array";
  }
  llvm::StringRef getDescription() const final {
    return "Slot 4: replace array-form nsl.submodule (SUB[3]) with N independent "
           "ops + rewrite cross-IR port references (M5 FR-016).";
  }

  void runOnOperation() final {
    // Phase 2 NO-OP slot. Real body lands at T095 (US4 phase).
  }
};

} // namespace

std::unique_ptr<mlir::Pass> createNSLExplodeSubmodArrayPass() {
  return std::make_unique<NSLExplodeSubmodArrayPass>();
}

} // namespace nsl::lower
