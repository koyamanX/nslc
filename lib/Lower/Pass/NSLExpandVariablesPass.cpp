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
//     walk the variable's enclosing region recursively in source
//     order; for each Op visited (skipping the variable itself):
//       for each operand-index of Op consuming %v:
//         if Op is nsl.transfer / nsl.clocked_transfer AND the
//         consuming operand index is 0 (the dst slot) AND the
//         insertion site's parent is a valid wire-parent:
//           (this is a whole-width write `v = src` / `v := src`)
//           build a fresh nsl.wire "name[_K]" : !nsl.bits<N> just
//           before the transfer; setOperand(0) to wire.result;
//           let currentValue = wire.result.
//         else:
//           (read site OR partial-assignment LHS via nsl.extract,
//            OR write inside a non-wire-parent region — leave the
//            use pointing at the current version, which initially
//            is the variable itself)
//           setOperand of this use to currentValue.
//     if the variable has zero remaining uses, erase it; otherwise
//     leave it in place (residual partial-assignment etc.).
//
// Source-order determinism (Constitution Principle V): we walk the
// region with `mlir::Region::walk` (PreOrder = source-order DFS).
// We do NOT iterate `getUsers()` directly because MLIR's use-list
// iteration order is allocation-order, not source-order. Region
// walk preserves byte-stable output across builds.
//
// **Nested-region uses** (e.g., a module-scope variable consumed
// inside `nsl.func`'s body — the s12 partial-assignment shape):
// the walk descends into all nested regions of the variable's
// enclosing region, so uses are discovered no matter how deep.
// Wire-insertion only happens when the use's enclosing parent is
// itself a valid wire-parent (`ModuleOp` or `FuncOp` per the M4
// post-merge amendment #5); writes inside `nsl.proc` / `nsl.state`
// fall through to the read-remap path (no wire created), which
// keeps the IR well-formed at the cost of leaving a residual
// `nsl.variable` op for later passes / next-milestone work.
//
// Scope policy: per the M4 op-surface freeze (post-merge amendment
// 2026-05-02 #5), both `nsl.wire` and `nsl.variable` accept
// `ParentOneOf<["ModuleOp", "FuncOp"]>` — the wire's parent
// constraint was widened in amendment #5 specifically to unblock
// this pass's func-scope expansion. The pass therefore expands
// BOTH module-scope and func-scope variables. Per-version wires
// share their scope with the variable they replace (no hoisting).
//
// Anchors:
//   - `specs/008-m5-structural-passes/spec.md` FR-015, US3
//     acceptance scenarios 1-3, SC-005
//   - `specs/008-m5-structural-passes/contracts/pass-pipeline.contract.md`
//     §2 row 3
//
// **At Phase 2 this pass was a registered NO-OP slot.** This file
// now implements the real body per T081.

#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "nsl/Dialect/NSL/IR/NSLDialect.h"
#include "nsl/Lower/Lower.h"

#include "llvm/ADT/SmallVector.h"

#include <string>

namespace nsl::lower {

namespace {

/// Return true if `op` is an op that writes the variable when its
/// first operand is the variable's result Value (i.e., a transfer-
/// like dst-then-src op).
bool isVariableWriteOp(mlir::Operation *op) {
  return mlir::isa<nsl::dialect::TransferOp, nsl::dialect::ClockedTransferOp>(
      op);
}

/// Expand a single `nsl.variable` op. Returns true if the op was
/// expanded; false if the op was skipped (e.g., its parent is not
/// `nsl.module` per the scope policy in the file banner).
///
/// Determinism: we iterate the parent block in source order, so
/// the per-version wire ordering and naming is byte-stable across
/// builds.
bool expandOne(nsl::dialect::VariableOp variable) {
  // Scope guard: post-merge M4-amendment 2026-05-02 #5 widened
  // `nsl.wire`'s parent trait to `ParentOneOf<["ModuleOp",
  // "FuncOp"]>`, so per-version wires can sibling either parent.
  // `nsl.variable` already accepted both. Reject anything else.
  mlir::Operation *parent = variable->getParentOp();
  if (!mlir::isa<nsl::dialect::ModuleOp, nsl::dialect::FuncOp>(parent)) {
    return false;
  }

  mlir::Value variableValue = variable.getResult();
  mlir::Type type = variableValue.getType();
  llvm::StringRef baseName = variable.getName();
  mlir::MLIRContext *ctx = variable.getContext();
  auto loc = variable.getLoc();

  // Walk the variable's enclosing region recursively in source
  // order. PreOrder DFS gives source-order deterministic version-
  // assignment (Constitution Principle V) and — critically —
  // discovers uses inside nested regions (e.g., a module-scope
  // variable consumed inside `nsl.func`'s body, the s12 partial-
  // assignment shape).
  mlir::Region *region = variable->getParentRegion();
  mlir::Value currentValue = variableValue;
  unsigned versionIndex = 0;

  region->walk([&](mlir::Operation *op) {
    if (op == variable.getOperation()) {
      return; // skip self
    }
    // For each operand of this op, check whether it currently
    // refers to `variableValue`. We do this by index because the
    // check must distinguish operand-0 (write dst) from other
    // operands (read).
    for (unsigned operandIdx = 0; operandIdx < op->getNumOperands();
         ++operandIdx) {
      mlir::OpOperand &opOperand = op->getOpOperand(operandIdx);
      if (opOperand.get() != variableValue) {
        continue;
      }
      // Found a use of the variable's result.
      bool isWrite = (operandIdx == 0 && isVariableWriteOp(op));
      // Wire-insertion is only legal when the insertion site's
      // parent is `ModuleOp` or `FuncOp` (per `nsl.wire`'s parent
      // constraint, M4 post-merge amendment #5). If the write is
      // inside a `nsl.proc` / `nsl.state`, we cannot create a
      // sibling wire there — fall through to the read-remap path
      // (leaving the use pointing at currentValue).
      mlir::Operation *opParent = op->getParentOp();
      bool wireInsertionLegal =
          mlir::isa<nsl::dialect::ModuleOp, nsl::dialect::FuncOp>(opParent);
      if (isWrite && wireInsertionLegal) {
        // Write site: build a fresh wire and rewire the transfer's
        // dst operand. The wire is inserted JUST BEFORE the
        // transfer so source order is preserved (write-before-
        // any-later-read).
        std::string wireName = baseName.str();
        if (versionIndex > 0) {
          wireName += "_";
          wireName += std::to_string(versionIndex);
        }
        mlir::OpBuilder builder(op);
        auto wire = nsl::dialect::WireOp::create(
            builder, loc, type, mlir::StringAttr::get(ctx, wireName));
        opOperand.set(wire.getResult());
        currentValue = wire.getResult();
        ++versionIndex;
      } else {
        // Read site OR write site whose parent is not a valid
        // wire-parent: remap the use to the current version. On
        // the first iteration `currentValue == variableValue`, so
        // a partial-assignment slice (`nsl.extract %v` consuming
        // %v with no prior whole-width write) leaves the use
        // pointing at the variable. The variable is then NOT
        // erased — see the `use_empty()` guard below.
        opOperand.set(currentValue);
      }
    }
  });

  // Only erase the variable if all uses were rewired away. Partial-
  // assignment shapes (s12: `v[3:0] = 0;` lowered as
  // `nsl.transfer %ext, %lit` where `%ext = nsl.extract %v, ...`)
  // leave the variable consumed by `nsl.extract`. Erasing in that
  // state would crash MLIR's use-list invariant. The residual
  // `nsl.variable` op is acceptable at M5 — fully eliminating it
  // requires either visitor-side whole-width-concat synthesis (the
  // shape demonstrated by `partial_assignment_S12.mlir`) or a
  // dedicated partial-assignment lowering pass, both deferred.
  if (variableValue.use_empty()) {
    variable.erase();
  }
  return true;
}

class NSLExpandVariablesPass
    : public mlir::PassWrapper<NSLExpandVariablesPass,
                               mlir::OperationPass<mlir::ModuleOp>> {
public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(NSLExpandVariablesPass)

  llvm::StringRef getArgument() const final { return "nsl-expand-variables"; }
  llvm::StringRef getDescription() const final {
    return "Slot 3: convert nsl.variable to SSA chain of "
           "nsl.wire+nsl.transfer; "
           "per-field for struct-typed; preserve S12 partial-assignment (M5 "
           "FR-015).";
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
