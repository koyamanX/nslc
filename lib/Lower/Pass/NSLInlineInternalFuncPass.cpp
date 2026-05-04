// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/NSLInlineInternalFuncPass.cpp — slot 5 of the M5
// structural-expansion pipeline (FR-017).
//
// **At M5 this pass STAYS a registered no-op slot** per
// Clarifications Q3 → Option B. The slot reserves the pipeline ABI
// (pass-name + signature + position) so a future PR can fill in
// functional `func_self` inlining without amending the M5 spec.
//
// Anchors:
//   - `specs/008-m5-structural-passes/research.md` §3.
//   - `specs/008-m5-structural-passes/contracts/pass-pipeline.contract.md`
//     §2 row 5.

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "nsl/Dialect/NSL/IR/NSLDialect.h"
#include "nsl/Lower/Lower.h"

namespace nsl::lower {

namespace {

class NSLInlineInternalFuncPass
    : public mlir::PassWrapper<NSLInlineInternalFuncPass,
                               mlir::OperationPass<mlir::ModuleOp>> {
public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(NSLInlineInternalFuncPass)

  llvm::StringRef getArgument() const final {
    return "nsl-inline-internal-func";
  }
  llvm::StringRef getDescription() const final {
    return "Slot 5: optional perf pass — inline single-call-site func_self. "
           "Registered no-op slot at M5 (Q3 → Option B); reserves pipeline "
           "ABI.";
  }

  void runOnOperation() final {
    // Permanent no-op at M5 per Q3 → Option B. A future PR may
    // implement functional inlining without amending the M5 spec.
  }
};

} // namespace

std::unique_ptr<mlir::Pass> createNSLInlineInternalFuncPass() {
  return std::make_unique<NSLInlineInternalFuncPass>();
}

} // namespace nsl::lower
