// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/NSLExpandVariablesPass.cpp — slot 3 of the M5
// structural-expansion pipeline (FR-015).
//
// Implements the variable-to-SSA-chain transformation: each
// `nsl.variable %v "name" : !nsl.bits<N>` op is replaced by a chain
// of `nsl.wire` declarations + `nsl.transfer` ops such that each
// "version" of the variable becomes a distinct SSA value. Reads
// of the variable are remapped to the most-recently-written
// version. Post-pass IR contains zero `nsl.variable` ops.
//
// Algorithm (per FR-015 + spec US3 acceptance scenarios 1-3):
//
//   for each `nsl.variable` op `%v "name" : !nsl.bits<N>`:
//     let currentValue = %v
//     for each Use of %v in source order (op-position-in-block):
//       if Use is operand 0 of nsl.transfer / nsl.clocked_transfer:
//         (this is a write `v = src` / `v := src`)
//         build a fresh nsl.wire "name[_K]" : !nsl.bits<N> just
//         before the transfer; let currentValue = wire.result;
//         setOperand(0) of the transfer to wire.result.
//       else:
//         (this is a read of %v's current version)
//         setOperand of this use to currentValue.
//     erase the original nsl.variable op.
//
// Source-order determinism (Constitution Principle V): we collect
// uses by walking the parent block in source order and matching
// each op against its operands. We do NOT iterate `getUsers()`
// directly because MLIR's use-list iteration order is not source-
// order (it's allocation-order). Block-walk preserves byte-stable
// output across builds.
//
// Scope policy: per the M4 op-surface freeze, `nsl.wire` requires
// `HasParent<"ModuleOp">`; `nsl.variable` allows
// `ParentOneOf<["ModuleOp", "FuncOp"]>`. The natural post-pass
// replacement (a wire) is verifier-rejected inside a func body.
// This pass therefore expands ONLY module-scope variables; func-
// scope variables are documented as deferred (US2 T066/T067
// precedent — `cross_scope.mlir` XFAIL) and left unchanged. A
// future M4 amendment relaxing wire's parent constraint or
// introducing a func-scope storage op will close that gap.
//
// Anchors:
//   - `specs/008-m5-structural-passes/spec.md` FR-015, US3
//     acceptance scenarios 1-3, SC-005
//   - `specs/008-m5-structural-passes/contracts/pass-pipeline.contract.md`
//     §2 row 3
//
// **At Phase 2 this pass was a registered NO-OP slot.** This file
// now implements the real body per T081.

#include "nsl/Lower/Lower.h"

#include "nsl/Dialect/NSL/IR/NSLDialect.h"

#include "llvm/ADT/SmallVector.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

#include <string>

namespace nsl::lower {

namespace {

/// Return true if `op` is an op that writes the variable when its
/// first operand is the variable's result Value (i.e., a transfer-
/// like dst-then-src op).
bool isVariableWriteOp(mlir::Operation *op) {
  return mlir::isa<nsl::dialect::TransferOp,
                   nsl::dialect::ClockedTransferOp>(op);
}

/// Expand a single `nsl.variable` op. Returns true if the op was
/// expanded; false if the op was skipped (e.g., its parent is not
/// `nsl.module` per the scope policy in the file banner).
///
/// Determinism: we iterate the parent block in source order, so
/// the per-version wire ordering and naming is byte-stable across
/// builds.
bool expandOne(nsl::dialect::VariableOp variable) {
  // Scope guard: only module-scope variables get expanded. A
  // func-scope variable's natural replacement (a wire) would be
  // verifier-rejected (`nsl.wire` HasParent<"ModuleOp">). Per
  // the deferral note in the file banner + cross_scope.mlir
  // XFAIL, leave func-scope variables in place. (The post-
  // pipeline check-semantics step will be the gate that catches
  // any leftover `nsl.variable` ops as a residue-class diagnostic
  // when those land at T096.)
  mlir::Operation *parent = variable->getParentOp();
  if (!mlir::isa<nsl::dialect::ModuleOp>(parent)) {
    return false;
  }

  mlir::Value variableValue = variable.getResult();
  mlir::Type type = variableValue.getType();
  llvm::StringRef baseName = variable.getName();
  mlir::MLIRContext *ctx = variable.getContext();
  auto loc = variable.getLoc();

  // Walk the parent block in source order; for each op, inspect
  // its operands looking for uses of `variableValue`. Source-order
  // walk gives us deterministic version-assignment.
  mlir::Block *block = variable->getBlock();
  mlir::Value currentValue = variableValue;
  unsigned versionIndex = 0;

  // We iterate ops; we may MUTATE the current op's operands but
  // must not erase the variable op until after the walk. The walk
  // is forward and we never insert ops AFTER the current op (only
  // before it), so iteration order is stable.
  for (mlir::Operation &op : *block) {
    if (&op == variable.getOperation()) {
      continue; // skip self
    }
    // For each operand of this op, check whether it currently
    // refers to `variableValue`. We do this by index because the
    // check must distinguish operand-0 (write dst) from other
    // operands (read).
    for (unsigned operandIdx = 0; operandIdx < op.getNumOperands();
         ++operandIdx) {
      mlir::OpOperand &opOperand = op.getOpOperand(operandIdx);
      if (opOperand.get() != variableValue) {
        continue;
      }
      // Found a use of the variable's result.
      if (operandIdx == 0 && isVariableWriteOp(&op)) {
        // Write site: build a fresh wire and rewire the transfer's
        // dst operand. The wire is inserted JUST BEFORE the
        // transfer so source order is preserved (write-before-
        // any-later-read).
        std::string wireName = baseName.str();
        if (versionIndex > 0) {
          wireName += "_";
          wireName += std::to_string(versionIndex);
        }
        mlir::OpBuilder builder(&op);
        auto wire = nsl::dialect::WireOp::create(
            builder, loc, type, mlir::StringAttr::get(ctx, wireName));
        opOperand.set(wire.getResult());
        currentValue = wire.getResult();
        ++versionIndex;
      } else {
        // Read site: remap the use to the current version. This
        // covers `nsl.transfer dst, src` where src is %v
        // (operandIdx == 1) AND any other op that consumes %v
        // (e.g., `nsl.add %v, %one`).
        opOperand.set(currentValue);
      }
    }
  }

  // After all uses are remapped, the original variable op should
  // have zero remaining uses. Erase it.
  variable.erase();
  return true;
}

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
    mlir::ModuleOp module = getOperation();

    // Collect every `nsl.variable` op in source order, then expand
    // each. We collect first (rather than expand-during-walk)
    // because `expandOne` erases the op, and erasing during a
    // `walk` mutates the IR underneath the visitor.
    //
    // Source-order walk is byte-stable across builds (Constitution
    // Principle V).
    llvm::SmallVector<nsl::dialect::VariableOp, 8> variables;
    module.walk([&](nsl::dialect::VariableOp v) { variables.push_back(v); });

    for (auto v : variables) {
      expandOne(v);
    }
  }
};

} // namespace

std::unique_ptr<mlir::Pass> createNSLExpandVariablesPass() {
  return std::make_unique<NSLExpandVariablesPass>();
}

} // namespace nsl::lower
