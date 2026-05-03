// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/NSLResolveParamsPass.cpp — slot 1 of the M5
// structural-expansion pipeline (FR-013).
//
// **At HEAD `7915cda` (M5 frozen 79-op dialect surface), this pass
// is a defensive no-op for pure-NSL inputs.** Rationale, recorded in
// detail because it surfaced during US2 implementation:
//
//   FR-013 mandates "replace every nsl.param_int / nsl.param_str
//   operand reference inside any op (including inside
//   nsl.structural_generate bound expressions) with the constant
//   value from the M3 Sema parameter map." At M5's frozen dialect
//   surface, however, NO `nsl::*` op carries a FlatSymbolRefAttr
//   slot pointing at a param symbol:
//
//     - nsl.submodule.templateRef  -> sibling-module symbol
//     - nsl.func_call.callee       -> nsl.func symbol
//     - nsl.fire_probe.target      -> control-terminal symbol
//     - nsl.structural_generate.   -> I64Attr (NOT a SymbolRef)
//       {lower,upper,step}
//
//   So the pass has nothing to substitute against on pure-NSL
//   inputs. Param eagerness is performed at the AST→MLIR visitor
//   stage instead — `visit(TopLevelParamDecl)` populates the
//   visitor's `paramTable_<StringRef, int64_t>`; `visit(Structural
//   Generate)` consults it when resolving bounds that are
//   IdentifierExpr referencing a `param_int`. See
//   `lib/Lower/ASTToMLIR.h:paramTable_` + the Commit 1 implementation.
//
// The pass body remains as a defensive walk for forward-compat:
// when a future M5/M6/M7 op grows a `FlatSymbolRefAttr` slot whose
// target may resolve to an `nsl.param_int`, this pass picks the
// substitution up automatically. That op does not exist yet.
//
// At M7 the param ops also serve as `nsl.submodule` instantiation
// arguments (Verilog `param_int` instance args); their final
// disposition (whether they survive past M7 or are erased there) is
// out of scope for M5.
//
// Anchors:
//   - `specs/008-m5-structural-passes/contracts/lower-api.contract.md`
//     §2.2 row 1
//   - `specs/008-m5-structural-passes/contracts/pass-pipeline.contract.md`
//     §2 row 1
//   - `specs/008-m5-structural-passes/spec.md` FR-013
//   - design §10 — Verilog `nsl.submodule` instantiation lowering at M7

#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/SymbolTable.h"
#include "mlir/Pass/Pass.h"
#include "nsl/Dialect/NSL/IR/NSLDialect.h"
#include "nsl/Lower/Lower.h"

#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"

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
    mlir::ModuleOp module = getOperation();

    // Step 1 — build the paramMap from every top-level
    // `nsl.param_int`. String params are catalogued separately
    // (their substitution lands at M7 / Verilog instantiation; at
    // M5 there is no expression-position consumer).
    //
    // Determinism (Constitution Principle V): the map is populated
    // by walking `module.getOps<...>()` in source order; lookup
    // is by `StringRef` (the `nsl.param_int` symbol name, which is
    // stable across builds because emission was source-driven).
    llvm::StringMap<int64_t> paramMap;
    for (auto p : module.getOps<nsl::dialect::ParamIntOp>()) {
      paramMap[p.getSymName()] = static_cast<int64_t>(p.getValue());
    }

    if (paramMap.empty()) {
      // No `nsl.param_int` ops — nothing to substitute. Common case
      // on pure-NSL inputs without `param_int` declarations.
      return;
    }

    // Step 2 — defensive walk for forward-compat. At M5's frozen
    // 79-op surface no operand-side `FlatSymbolRefAttr` references
    // a param symbol, so this loop performs no substitutions on
    // pure-NSL inputs. The walk is preserved so a future op carrying
    // such a slot is handled automatically without re-implementing
    // the pass.
    //
    // Substitution rule: for every op (excluding the param ops
    // themselves), inspect every named `Attribute`. If the attribute
    // is a `FlatSymbolRefAttr` whose value is in `paramMap`, replace
    // the carrying attribute with an `IntegerAttr` (i64) carrying
    // the resolved value. The op MUST tolerate the type swap on
    // its attribute slot — otherwise no current op's TableGen
    // declaration would have permitted the substitution in the
    // first place. This pass is intentionally lenient; mismatches
    // surface as MLIR verifier errors at the next pass boundary.
    auto *ctx = &getContext();
    module.walk([&](mlir::Operation *op) {
      if (mlir::isa<nsl::dialect::ParamIntOp>(op) ||
          mlir::isa<nsl::dialect::ParamStrOp>(op)) {
        return mlir::WalkResult::skip();
      }
      // Iterate the op's attribute dictionary by name; build a
      // working list of replacements (don't mutate during iteration).
      llvm::SmallVector<std::pair<mlir::StringAttr, mlir::Attribute>, 4>
          replacements;
      for (auto namedAttr : op->getAttrs()) {
        auto sym =
            mlir::dyn_cast<mlir::FlatSymbolRefAttr>(namedAttr.getValue());
        if (!sym) {
          continue;
        }
        auto it = paramMap.find(sym.getValue());
        if (it == paramMap.end()) {
          continue;
        }
        replacements.emplace_back(
            namedAttr.getName(),
            mlir::IntegerAttr::get(mlir::IntegerType::get(ctx, 64),
                                   it->second));
      }
      for (auto &kv : replacements) {
        op->setAttr(kv.first, kv.second);
      }
      return mlir::WalkResult::advance();
    });

    // Step 3 — leave param ops in place. Per the design note above,
    // M7 consumes them when generating Verilog `nsl.submodule`
    // instantiation arguments; erasing them at M5 would discard
    // information the later milestone needs. If a future amendment
    // decides to erase them post-resolution, this is the spot.
  }
};

} // namespace

std::unique_ptr<mlir::Pass> createNSLResolveParamsPass() {
  return std::make_unique<NSLResolveParamsPass>();
}

} // namespace nsl::lower
