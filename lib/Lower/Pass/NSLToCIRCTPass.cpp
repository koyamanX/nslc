// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/NSLToCIRCTPass.cpp — M6 nsl→CIRCT conversion pass
// driver.
//
// **At Phase 2 the pass is a SCAFFOLD**: the 9 `populate*Patterns`
// helpers from `lib/Lower/Pass/CIRCTPatterns/*.cpp` are called but
// each is currently empty. The conversion target marks every
// `nsl::*` op illegal and the 5 CIRCT dialects (`hw`, `comb`, `seq`,
// `fsm`, `sv`) legal. Running this pass over input with any
// reachable `nsl::*` op fails with `applyFullConversion` reporting
// "no legalization for op 'nsl.<name>'" — the observed-failing
// baseline that Phase 4–6's pattern bodies turn green.
//
// Per Constitution Principle III: zero hand-rolled CIRCT-equivalent
// passes. The output goes to real `circt::*` ops via stock MLIR
// `DialectConversion`. Per research.md §1.

#include "NSLToCIRCTPass.h"
#include "CIRCTTypeConverter.h"

#include "circt/Dialect/Comb/CombDialect.h"
#include "circt/Dialect/FSM/FSMDialect.h"
#include "circt/Dialect/HW/HWDialect.h"
#include "circt/Dialect/SV/SVDialect.h"
#include "circt/Dialect/Seq/SeqDialect.h"

#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/DialectRegistry.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassRegistry.h"
#include "mlir/Transforms/DialectConversion.h"

#include "nsl/Dialect/NSL/IR/NSLDialect.h"
#include "nsl/Lower/Lower.h"

#include <memory>

namespace nsl::lower {

namespace {

class NSLToCIRCTPass
    : public mlir::PassWrapper<NSLToCIRCTPass,
                               mlir::OperationPass<mlir::ModuleOp>> {
public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(NSLToCIRCTPass)

  llvm::StringRef getArgument() const final { return "nsl-to-circt"; }

  llvm::StringRef getDescription() const final {
    // Frozen text per
    // specs/010-m6-circt-lowering/contracts/lower-api.contract.md §4.
    return "Lower nsl::* dialect ops to CIRCT (hw/comb/seq/fsm/sv)";
  }

  void getDependentDialects(mlir::DialectRegistry &registry) const final {
    // The conversion produces ops in all five CIRCT dialects. Loading
    // them into the pass-manager registry up-front lets the
    // OperationPass framework verify dialect availability before
    // pattern application begins.
    registry.insert<circt::hw::HWDialect>();
    registry.insert<circt::comb::CombDialect>();
    registry.insert<circt::seq::SeqDialect>();
    registry.insert<circt::fsm::FSMDialect>();
    registry.insert<circt::sv::SVDialect>();
  }

  void runOnOperation() final {
    mlir::ModuleOp module = getOperation();
    mlir::MLIRContext &ctx = getContext();

    // ---------- Phase 4 (US2) structural pre-pass ----------
    // Every `nsl::ModuleOp` is rewritten into a `hw::HWModuleOp`
    // with port list derived from the paired `nsl::DeclareOp`.
    // The dual-placement in-module port-info ops are consumed
    // during this walk too — see ModulePatterns.cpp's commentary
    // for the full rewrite recipe. After this pre-pass, the IR
    // contains zero `nsl::ModuleOp` / `nsl::DeclareOp` ops; any
    // remaining `nsl::*` ops belong to leaf-op families (Phase 5+).
    //
    // Phase-5 refactor: unrecognized leaf ops inside a
    // `nsl::ModuleOp` body are MOVED into the new `hw::HWModuleOp`
    // body (instead of fail-fasting); subsequent pre-passes (Phase
    // 5 FSM, Phase 6 leaf-ops) and `applyFullConversion` then handle
    // them.
    if (mlir::failed(lowerNSLModulesToHWModules(module))) {
      signalPassFailure();
      return;
    }

    // ---------- Phase 5 (US3) FSM pre-pass ----------
    // Every `nsl::ProcOp` (with its `nsl::StateOp` children) is
    // rewritten into a top-level `fsm::MachineOp` sibling of the
    // enclosing `hw::HWModuleOp`. Every `nsl::FuncOp` containing
    // a `nsl::SeqOp` is rewritten the same way with auto-generated
    // `seq_N` states. After this pre-pass, the IR contains zero
    // `nsl::ProcOp` / `nsl::StateOp` / `nsl::FirstStateOp` /
    // `nsl::GotoOp` / `nsl::FinishOp` / `nsl::FinishMethodOp` /
    // `nsl::CallOp`-to-proc / `nsl::SeqOp`-inside-FuncOp ops; the
    // remaining `nsl::*` ops belong to Phase-6 leaf-op families.
    if (mlir::failed(lowerNSLProcsToFSMMachines(module))) {
      signalPassFailure();
      return;
    }

    // ---------- ConversionTarget ----------
    // Mark `nsl` dialect illegal: every `nsl::*` op MUST be converted
    // away (FR-004). Mark the five CIRCT dialects legal — outputs
    // belong to one of them.
    mlir::ConversionTarget target(ctx);
    target.addIllegalDialect<nsl::dialect::NSLDialect>();
    target.addLegalDialect<circt::hw::HWDialect, circt::comb::CombDialect,
                           circt::seq::SeqDialect, circt::fsm::FSMDialect,
                           circt::sv::SVDialect>();
    // Built-in `mlir::ModuleOp` (the parent op the pass runs on)
    // stays legal — it's the IR container, not converted away.
    target.addLegalOp<mlir::ModuleOp>();

    // ---------- TypeConverter + patterns ----------
    CIRCTTypeConverter type_converter(ctx);
    mlir::RewritePatternSet patterns(&ctx);

    // Per research.md §11: alphabetic registration order for
    // determinism. At Phase 4 most populators are scaffolds (empty
    // body); the structural ModuleOp/DeclareOp/Port/Submodule/Param
    // rewrites happen in `lowerNSLModulesToHWModules` above, NOT
    // through these patterns. Phase 5+ fills in FSM / arith / bit /
    // state / control / sim leaf-op patterns.
    populateArithPatterns(patterns, type_converter);
    populateBitOpPatterns(patterns, type_converter);
    populateControlPatterns(patterns, type_converter);
    populateFSMPatterns(patterns, type_converter);
    populateModulePatterns(patterns, type_converter);
    populateParamPatterns(patterns, type_converter);
    populatePortPatterns(patterns, type_converter);
    populateSimPatterns(patterns, type_converter);
    populateStatePatterns(patterns, type_converter);

    // ---------- Run conversion ----------
    if (mlir::failed(mlir::applyFullConversion(module, target,
                                               std::move(patterns)))) {
      signalPassFailure();
    }
  }
};

} // namespace

std::unique_ptr<mlir::Pass> createNSLToCIRCTPass() {
  return std::make_unique<NSLToCIRCTPass>();
}

void registerNSLToCIRCTPass() {
  mlir::registerPass([]() { return createNSLToCIRCTPass(); });
}

} // namespace nsl::lower
