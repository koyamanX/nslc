// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/NSLExplodeSubmodArrayPass.cpp — slot 4 of the M5
// structural-expansion pipeline (FR-016).
//
// Replaces every array-form `nsl.submodule` (i.e., one carrying an
// `array_size` attribute, source spelling `SUB[N] inst;`) with N
// independent singleton-form `nsl.submodule` ops named
// `<orig-name>_<index>` (e.g., `@inst_0`, `@inst_1`, `@inst_2`).
// The original array-form op is erased.
//
// **Naming scheme**: `<orig-name>_<index>` (NOT `<orig-name>[<index>]`).
// MLIR symbol names cannot contain unescaped `[`/`]`; the in-printer
// notation `@inst[3]` decorates the OPTIONAL `array_size` attribute,
// it is not part of the canonical symbol name. After explosion the
// `array_size` attribute is absent on each replica (singleton form).
//
// **Cross-IR port references**: at M5's frozen 79-op surface, NO
// `nsl::*` op carries an operand-side reference to a specific
// submodule-port slot. (M6 will surface such ops when lowering
// `nsl.submodule` to `hw.instance`.) The pass is structurally sound
// for that future amendment — a defensive walk hook is documented
// inline below; for now the pass simply performs naming-replication.
//
// **Zero-element arrays**: `array_size = 0` produces zero replicas;
// the original op is simply erased. (Whether the source-language
// permits `SUB[0]` is a Sema question deferred to M3+; the dialect
// verifier currently permits any non-negative I64.)
//
// **Singleton pass-through**: a `nsl.submodule` op WITHOUT
// `array_size` is left unchanged. The pipeline post-condition
// "zero array-form `nsl.submodule`" follows.
//
// Anchors:
//   - `specs/008-m5-structural-passes/spec.md` FR-016, US4
//   - `specs/008-m5-structural-passes/contracts/pass-pipeline.contract.md`
//     §2 row 4
//   - `lib/Dialect/NSL/IR/NSLOps.td` `NSL_SubmoduleOp`
//     (post-merge M4-amendment 2026-05-02 #4 added `array_size`)

#include "nsl/Lower/Lower.h"

#include "nsl/Dialect/NSL/IR/NSLDialect.h"

#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

#include <string>

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
    mlir::ModuleOp module = getOperation();

    // Step 1 — collect every array-form `nsl.submodule` (carrying
    // an `array_size` attribute). We snapshot first then mutate so
    // the in-flight walk is not invalidated by ops we erase. Walk
    // is in source order (Constitution Principle V — determinism).
    llvm::SmallVector<nsl::dialect::SubmoduleOp, 8> arrayForms;
    module.walk([&](nsl::dialect::SubmoduleOp sub) {
      if (sub.getArraySize().has_value()) {
        arrayForms.push_back(sub);
      }
    });

    // Step 2 — for each array-form op, build N singleton replicas
    // named `<orig-name>_<index>`, then erase the original.
    for (auto sub : arrayForms) {
      auto sizeOpt = sub.getArraySize();
      // Defensive: sizeOpt presence was the predicate above.
      int64_t size = sizeOpt.has_value() ? *sizeOpt : 0;
      if (size < 0) {
        sub.emitOpError() << "array_size is negative — refusing to expand";
        signalPassFailure();
        continue;
      }

      llvm::StringRef baseName = sub.getSymName();
      mlir::FlatSymbolRefAttr templateRef = sub.getTemplateRefAttr();
      mlir::Location loc = sub.getLoc();

      // OpBuilder set to insert just before the array-form op so the
      // replicas appear in source order at the parent block.
      mlir::OpBuilder builder(sub);

      for (int64_t k = 0; k < size; ++k) {
        llvm::SmallString<64> name;
        name.append(baseName.begin(), baseName.end());
        name.push_back('_');
        // Decimal-encode k via std::to_string — deterministic, no
        // locale dependency.
        auto kStr = std::to_string(k);
        name.append(kStr.begin(), kStr.end());

        // Build a singleton-form replica: same templateRef, no
        // array_size attribute. SymbolNameAttr is taken from the
        // namespace context.
        auto nameAttr = mlir::StringAttr::get(&getContext(), name);
        // SubmoduleOp::build takes (sym_name, templateRef, array_size?)
        // per TableGen — we pass an empty IntegerAttr for the
        // optional array_size.
        nsl::dialect::SubmoduleOp::create(
            builder, loc, nameAttr, templateRef,
            /*array_size=*/mlir::IntegerAttr{});
      }

      // **Cross-IR port-reference rewrite hook**: when M6 introduces
      // an op that carries a reference to a specific submodule-port
      // slot (e.g., `nsl.submod_port @inst[2].out`), this pass MUST
      // walk those references and rewrite `@inst[k]` →
      // `@inst_<k>`. At M5's frozen 79-op surface no such op exists,
      // so this hook is intentionally empty — the architectural seam
      // is preserved.

      // Erase the original array-form op.
      sub.erase();
    }
  }
};

} // namespace

std::unique_ptr<mlir::Pass> createNSLExplodeSubmodArrayPass() {
  return std::make_unique<NSLExplodeSubmodArrayPass>();
}

} // namespace nsl::lower
