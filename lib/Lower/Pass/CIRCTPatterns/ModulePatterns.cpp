// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/CIRCTPatterns/ModulePatterns.cpp — M6 module-skeleton
// patterns (Phase 4 US2 implementation; T038 + T042 per
// `specs/010-m6-circt-lowering/tasks.md`).
//
// **Design §10 rows covered**: nsl.module → hw.HWModuleOp (port list
// derived from paired nsl.declare per circt-lowering.contract.md §3).
// nsl.declare is consumed during ModuleOp lowering. The dual-placement
// `nsl.input_port`/`nsl.output_port`/`nsl.inout_port` ops in the
// module-body half are also rewritten by THIS pattern (block-arg
// substitution / output wiring) — `PortPatterns.cpp` stays empty
// at Phase 4 (declare-body port-info ops are erased transitively
// when their parent DeclareOp is erased).
//
// Also handled inline at Phase 4 (necessary for the simple-shape
// fixtures `q = 0` and `q = a`):
//   - `nsl::SubmoduleOp` (singleton form, T039 / `submodule_singleton.mlir`)
//   - `nsl::ConstantOp` → `hw::ConstantOp` (used by output_only fixture)
//   - `nsl::TransferOp` whose %dst is an output port → output-wiring
//     contribution
//   - `nsl::ParamIntOp` / `nsl::ParamStrOp` (T040 / T041b consumed by
//     submodule instantiation as `hw.instance` parameter wires)
//
// Phase-5 refactor of the unrecognized-op handling (US3): the
// per-op fail-fast `else` arm (which previously emitted a
// hardcoded "M6 Phase 4 has no conversion pattern" error and
// returned failure) is replaced with a MOVE — unrecognized ops are
// relocated into the new `hw::HWModuleOp`'s body. Phase 5's
// FSM pre-pass and Phase 6's leaf-op patterns then operate on
// them; if no pattern can legalize an op, `applyFullConversion`
// fails with the standard "failed to legalize operation 'nsl.<X>'"
// diagnostic (FR-028; T029 fixture asserts this path).
//
// **Implementation strategy**: a custom `runModuleStructuralLowering`
// pass-driver function (called by `NSLToCIRCTPass::runOnOperation`
// BEFORE `applyFullConversion`) walks every `nsl::ModuleOp` in the
// outer `mlir::ModuleOp` and rewrites it manually using a stock
// `mlir::OpBuilder`. This bypasses MLIR's DialectConversion worklist
// for the structural rewrite (which would interact poorly with the
// dual-placement port-info-op design — the framework would attempt
// to legalize the in-module port-info ops independently before the
// ModuleOp pattern got a chance to consume them). Per Constitution
// Principle III: zero hand-rolled CIRCT-equivalent passes — the
// output goes to real `circt::hw::HWModuleOp` / `hw::OutputOp` /
// `hw::ConstantOp` / `hw::InstanceOp` ops; we drive the *creation*
// of those ops manually but the ops themselves are stock CIRCT.

#include "../CIRCTTypeConverter.h"
#include "../NSLToCIRCTPass.h"

#include "circt/Dialect/HW/HWAttributes.h"
#include "circt/Dialect/HW/HWOps.h"
#include "circt/Dialect/HW/HWTypes.h"
#include "circt/Dialect/HW/PortImplementation.h"

#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Transforms/DialectConversion.h"

#include "nsl/Dialect/NSL/IR/NSLDialect.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"

namespace nsl::lower {

namespace {

/// Find the `nsl::DeclareOp` whose `pair_name` matches `moduleSymName`
/// among the direct children of the enclosing `mlir::ModuleOp`. Walks
/// in source order (deterministic per Principle V); returns the first
/// hit or `nullptr` if none. `nullptr` is the legal port-less-module
/// path (matches T035 `empty_module.nsl`).
nsl::dialect::DeclareOp findPairedDeclare(nsl::dialect::ModuleOp moduleOp) {
  auto parent =
      llvm::dyn_cast_or_null<mlir::ModuleOp>(moduleOp->getParentOp());
  if (!parent) {
    return nullptr;
  }
  llvm::StringRef name = moduleOp.getSymName();
  for (auto declareOp : parent.getOps<nsl::dialect::DeclareOp>()) {
    if (declareOp.getPairName() == name) {
      return declareOp;
    }
  }
  return nullptr;
}

/// Convert a `!nsl.bits<W>` to `iW`.
mlir::IntegerType bitsToInteger(mlir::Type bitsType) {
  auto bits = mlir::cast<nsl::dialect::BitsType>(bitsType);
  return mlir::IntegerType::get(bitsType.getContext(), bits.getWidth());
}

/// Build the `hw::ModulePortInfo` from the paired declare body.
///
/// Clock + reset port appending follows the S20 / M4-amendment-#10
/// branching from `circt-lowering.contract.md` §3 rule 6 + rule 7 +
/// `firreg-convention.contract.md` §1 + §2:
///
///   * No paired declare (declareOp == nullptr): no clk/rst_n appended
///     (the port-less-module shape per T035).
///   * Paired declare WITHOUT `interface_clock` / `interface_reset`:
///     append implicit `clk` + `rst_n` (Q2 → C convention).
///   * Paired declare WITH `interface_clock` + `interface_reset`:
///     append the user's named clock + reset ports verbatim. This is
///     the explicit-`interface`-path port-list piece; reg-on-explicit-
///     interface lowering (`seq::CompRegOp` instead of FirRegOp) is
///     Phase 6 territory — Phase 4 only plumbs the port-naming half.
void buildPortInfo(nsl::dialect::DeclareOp declareOp,
                   mlir::Location implicitLoc,
                   llvm::SmallVectorImpl<circt::hw::PortInfo> &ports) {
  mlir::MLIRContext *ctx = implicitLoc.getContext();
  llvm::SmallVector<circt::hw::PortInfo, 4> inputs;
  llvm::SmallVector<circt::hw::PortInfo, 4> outputs;
  if (declareOp) {
    for (auto &op : declareOp.getBody().front()) {
      if (auto in = llvm::dyn_cast<nsl::dialect::InputPortOp>(op)) {
        circt::hw::PortInfo p;
        p.name = mlir::StringAttr::get(ctx, in.getName());
        p.type = bitsToInteger(in.getResult().getType());
        p.dir = circt::hw::ModulePort::Direction::Input;
        p.loc = in.getLoc();
        inputs.push_back(p);
      } else if (auto inout =
                     llvm::dyn_cast<nsl::dialect::InoutPortOp>(op)) {
        circt::hw::PortInfo p;
        p.name = mlir::StringAttr::get(ctx, inout.getName());
        p.type = bitsToInteger(inout.getResult().getType());
        p.dir = circt::hw::ModulePort::Direction::InOut;
        p.loc = inout.getLoc();
        inputs.push_back(p);
      } else if (auto out =
                     llvm::dyn_cast<nsl::dialect::OutputPortOp>(op)) {
        circt::hw::PortInfo p;
        p.name = mlir::StringAttr::get(ctx, out.getName());
        p.type = bitsToInteger(out.getResult().getType());
        p.dir = circt::hw::ModulePort::Direction::Output;
        p.loc = out.getLoc();
        outputs.push_back(p);
      }
    }
  }

  if (declareOp) {
    std::optional<llvm::StringRef> ifaceClk = declareOp.getInterfaceClock();
    std::optional<llvm::StringRef> ifaceRst = declareOp.getInterfaceReset();
    bool hasInterface = ifaceClk.has_value() && ifaceRst.has_value();
    auto i1 = mlir::IntegerType::get(ctx, 1);

    if (hasInterface) {
      // M4-amendment-#10: explicit S20 `interface` modifier → emit
      // user-named clock + reset ports (verbatim, including any `_n`
      // polarity suffix the user wrote). The implicit `clk` / `rst_n`
      // pair is NOT auto-added on this path.
      circt::hw::PortInfo clkPort;
      clkPort.name = mlir::StringAttr::get(ctx, *ifaceClk);
      clkPort.type = i1;
      clkPort.dir = circt::hw::ModulePort::Direction::Input;
      clkPort.loc = declareOp.getLoc();
      inputs.push_back(clkPort);

      circt::hw::PortInfo rstPort;
      rstPort.name = mlir::StringAttr::get(ctx, *ifaceRst);
      rstPort.type = i1;
      rstPort.dir = circt::hw::ModulePort::Direction::Input;
      rstPort.loc = declareOp.getLoc();
      inputs.push_back(rstPort);
    } else {
      // No S20 modifier → implicit `clk` / `rst_n` (Q2 → C).
      circt::hw::PortInfo clkPort;
      clkPort.name = mlir::StringAttr::get(ctx, "clk");
      clkPort.type = i1;
      clkPort.dir = circt::hw::ModulePort::Direction::Input;
      clkPort.loc = implicitLoc;
      inputs.push_back(clkPort);

      circt::hw::PortInfo rstnPort;
      rstnPort.name = mlir::StringAttr::get(ctx, "rst_n");
      rstnPort.type = i1;
      rstnPort.dir = circt::hw::ModulePort::Direction::Input;
      rstnPort.loc = implicitLoc;
      inputs.push_back(rstnPort);
    }
  }

  unsigned argIdx = 0;
  for (auto &p : inputs) {
    p.argNum = argIdx++;
  }
  unsigned resIdx = 0;
  for (auto &p : outputs) {
    p.argNum = resIdx++;
  }
  ports.append(inputs.begin(), inputs.end());
  ports.append(outputs.begin(), outputs.end());
}

/// Collect every top-level `nsl::ParamIntOp` / `nsl::ParamStrOp` into
/// a `ParamDeclArrayAttr`. Per design line 1278, the contract for
/// Phase 4 is "every consuming `hw.instance` carries every top-level
/// param". A future M4 amendment surfacing per-instance param
/// assignments will refine this; the simplification is acceptable
/// for the Phase-4 fixture set (T037, T041) which exercise a single
/// submodule + a single param.
mlir::ArrayAttr collectInstanceParameters(mlir::ModuleOp parentModule,
                                          mlir::OpBuilder &builder) {
  mlir::MLIRContext *ctx = parentModule.getContext();
  llvm::SmallVector<mlir::Attribute, 4> params;
  for (auto &op : parentModule.getBody()->getOperations()) {
    if (auto pi = llvm::dyn_cast<nsl::dialect::ParamIntOp>(op)) {
      auto i32 = mlir::IntegerType::get(ctx, 32);
      auto valueAttr = mlir::IntegerAttr::get(
          i32, static_cast<int32_t>(pi.getValue()));
      params.push_back(circt::hw::ParamDeclAttr::get(
          mlir::StringAttr::get(ctx, pi.getSymName()), valueAttr));
    } else if (auto ps = llvm::dyn_cast<nsl::dialect::ParamStrOp>(op)) {
      // String-typed instance parameter. CIRCT's ParamDeclAttr accepts
      // a TypedAttr value; we use a `none`-typed StringAttr to encode
      // the string payload (matches the design line 1256 convention
      // and parses round-trip through circt-opt).
      auto noneType = mlir::NoneType::get(ctx);
      auto valueAttr = mlir::StringAttr::get(ctx, ps.getValue());
      params.push_back(circt::hw::ParamDeclAttr::get(
          mlir::StringAttr::get(ctx, ps.getSymName()), noneType, valueAttr));
    }
  }
  (void)builder;
  return mlir::ArrayAttr::get(ctx, params);
}

/// Rewrite a single `nsl::ModuleOp` into an `hw::HWModuleOp` with the
/// derived port list. Returns failure on any structural mismatch
/// (forward reference, unsupported body op shape) — the caller signals
/// pass failure.
mlir::LogicalResult lowerOneModule(nsl::dialect::ModuleOp moduleOp,
                                   mlir::OpBuilder &builder) {
  mlir::Location loc = moduleOp.getLoc();
  mlir::MLIRContext *ctx = builder.getContext();

  // Step 1: locate paired declare; build port list. The clk/rst_n
  // policy is encapsulated inside `buildPortInfo` per M4-amendment-#10
  // (no paired declare → no clk/rst_n; paired declare without
  // `interface_*` attrs → implicit `clk`/`rst_n`; paired declare WITH
  // attrs → user-named clock + reset).
  nsl::dialect::DeclareOp declareOp = findPairedDeclare(moduleOp);
  llvm::SmallVector<circt::hw::PortInfo, 8> portInfos;
  buildPortInfo(declareOp, loc, portInfos);
  circt::hw::ModulePortInfo ports(portInfos);

  // Step 2: create hw::HWModuleOp before the nsl.module.
  // Per Phase-4 simplification (design line 1278 — every consuming
  // hw.instance carries every top-level param), declare all top-
  // level params on every hw.module too, so that the verifier
  // accepts the resulting hw.instance op (which validates that its
  // parameter list matches the target hw.module's declared params).
  // A future M4 amendment surfacing per-instance param assignments
  // will refine this.
  auto parentModuleOpForParams =
      llvm::cast<mlir::ModuleOp>(moduleOp->getParentOp());
  mlir::ArrayAttr declaredParams =
      collectInstanceParameters(parentModuleOpForParams, builder);
  builder.setInsertionPoint(moduleOp);
  auto hwModuleOp = circt::hw::HWModuleOp::create(
      builder, loc, moduleOp.getSymNameAttr(), ports,
      /*parameters=*/declaredParams);

  // Step 3: build name → block-arg map for input/inout ports.
  llvm::SmallVector<std::pair<llvm::StringRef, mlir::Value>, 8>
      inputBindings;
  {
    mlir::Block *entry = hwModuleOp.getBodyBlock();
    unsigned argIdx = 0;
    for (auto &p : ports) {
      if (p.dir == circt::hw::ModulePort::Direction::Input ||
          p.dir == circt::hw::ModulePort::Direction::InOut) {
        inputBindings.emplace_back(p.name.getValue(),
                                   entry->getArgument(argIdx));
        ++argIdx;
      }
    }
  }
  auto lookupInputArg = [&](llvm::StringRef name) -> mlir::Value {
    for (auto &kv : inputBindings) {
      if (kv.first == name) {
        return kv.second;
      }
    }
    return {};
  };

  // Step 4: walk module body, materialise CIRCT ops, collect output
  // assignments. We process source-order so SSA defs precede uses.
  llvm::SmallVector<std::pair<mlir::Value, llvm::StringRef>, 4>
      outputPortValues;
  llvm::SmallVector<std::pair<llvm::StringRef, mlir::Value>, 4>
      outputAssignments;

  builder.setInsertionPointToStart(hwModuleOp.getBodyBlock());

  llvm::SmallDenseMap<mlir::Value, mlir::Value, 16> valueMap;
  auto getOutputPortName = [&](mlir::Value v) -> llvm::StringRef {
    for (auto &kv : outputPortValues) {
      if (kv.first == v) {
        return kv.second;
      }
    }
    return {};
  };

  // Snapshot ops first (we'll erase the entire ModuleOp later, but
  // walking via a snapshot is robust against in-loop mutation).
  llvm::SmallVector<mlir::Operation *, 16> bodyOps;
  for (auto &op : moduleOp.getBody().front()) {
    bodyOps.push_back(&op);
  }

  // Resolve top-level param attrs once for any submodule we encounter.
  auto parentModuleOp =
      llvm::cast<mlir::ModuleOp>(moduleOp->getParentOp());
  mlir::ArrayAttr instanceParams =
      collectInstanceParameters(parentModuleOp, builder);

  for (mlir::Operation *op : bodyOps) {
    if (auto in = llvm::dyn_cast<nsl::dialect::InputPortOp>(op)) {
      mlir::Value blockArg = lookupInputArg(in.getName());
      if (!blockArg) {
        moduleOp.emitError() << "in-module nsl.input_port \""
                             << in.getName()
                             << "\" not in paired declare's port list";
        return mlir::failure();
      }
      valueMap[in.getResult()] = blockArg;
    } else if (auto out =
                   llvm::dyn_cast<nsl::dialect::OutputPortOp>(op)) {
      outputPortValues.emplace_back(out.getResult(), out.getName());
    } else if (auto inout =
                   llvm::dyn_cast<nsl::dialect::InoutPortOp>(op)) {
      mlir::Value blockArg = lookupInputArg(inout.getName());
      if (!blockArg) {
        moduleOp.emitError() << "in-module nsl.inout_port \""
                             << inout.getName()
                             << "\" not in paired declare's port list";
        return mlir::failure();
      }
      valueMap[inout.getResult()] = blockArg;
    } else if (auto cst =
                   llvm::dyn_cast<nsl::dialect::ConstantOp>(op)) {
      auto resultType = bitsToInteger(cst.getResult().getType());
      auto valueAttr =
          mlir::IntegerAttr::get(resultType, cst.getValue());
      auto hwCst =
          circt::hw::ConstantOp::create(builder, cst.getLoc(), valueAttr);
      valueMap[cst.getResult()] = hwCst.getResult();
    } else if (auto sub =
                   llvm::dyn_cast<nsl::dialect::SubmoduleOp>(op)) {
      // Look up the paired hw.module via symbol lookup at the parent
      // mlir::ModuleOp level. The conversion order (sequential walk
      // of nsl::ModuleOp ops in source order) guarantees the target
      // hw.module exists if it was declared earlier; if the target
      // was declared later we must defer creation. Phase 4 fixtures
      // declare submodule targets BEFORE their consumers (see
      // submodule_singleton.mlir's source-order ordering); a later
      // amendment can add a two-pass conversion if downstream
      // fixtures need forward references.
      auto target =
          parentModuleOp.lookupSymbol<circt::hw::HWModuleOp>(
              sub.getTemplateRef());
      if (!target) {
        sub.emitError()
            << "submodule target @" << sub.getTemplateRef()
            << " not yet lowered to hw.module (forward reference is "
            << "Phase-4-out-of-scope; declare submodule targets "
            << "before their consumers)";
        return mlir::failure();
      }
      // Build hw.instance with no operand wiring (Phase 4 doesn't
      // surface per-instance port connections — those would come
      // from a wider visit(SubmoduleDecl) lowering at M5+; future
      // amendment T-TBD). Pass the collected top-level params.
      // The builder taking `Operation*` + `ArrayRef<Value>` infers
      // the instance's argNames/resultNames from the referenced
      // module's port list.
      llvm::SmallVector<mlir::Value, 0> emptyInputs;
      auto inst = circt::hw::InstanceOp::create(
          builder, sub.getLoc(), target.getOperation(),
          sub.getSymNameAttr(),
          llvm::ArrayRef<mlir::Value>(emptyInputs), instanceParams);
      (void)inst;
    } else if (auto xfer =
                   llvm::dyn_cast<nsl::dialect::TransferOp>(op)) {
      llvm::StringRef outName = getOutputPortName(xfer.getDst());
      if (outName.empty()) {
        // Not an output-port write (LHS is wire/reg/mem). Phase 6's
        // StatePatterns will handle this. The transfer references
        // a non-port stub (e.g., a `nsl.reg`'s result); since the
        // reg is being moved to the new hw.module body too (via the
        // catch-all `else` arm below, in source order), the SSA
        // chain stays consistent. The terminator's body-position
        // ordering preserves source-order moves. Fall through to
        // the move arm.
      } else {
        mlir::Value srcMapped = valueMap.lookup(xfer.getSrc());
        if (srcMapped) {
          outputAssignments.emplace_back(outName, srcMapped);
          continue;
        }
        // Output-port write whose source is not yet materialised
        // (e.g., `q = r;` where `r` is `nsl.reg`). The naive Phase-4
        // forward-flow assumption breaks; we report fail-fast with
        // a stable diagnostic per FR-028 (T029 fixture asserts).
        xfer.emitError()
            << "M6 cannot lower output-port write whose source ('"
            << xfer.getSrc() << "') is not a Phase-4-materialised "
            << "value (combinational forward-flow only at Phase 4; "
            << "non-combinational sources land at Phase 6)";
        return mlir::failure();
      }
      // Move arm — the transfer is left for Phase 6.
      auto *terminator = hwModuleOp.getBodyBlock()->getTerminator();
      op->moveBefore(terminator);
    } else {
      // Unrecognized leaf op at Phase 4. Per the Phase-5 refactor
      // (US3), instead of fail-fasting here, MOVE the op into the
      // new `hw::HWModuleOp` body so that `applyFullConversion`
      // (which runs AFTER this structural pre-pass per
      // `NSLToCIRCTPass::runOnOperation`) sees it. Two cases:
      //
      //   (a) A registered pattern exists (e.g., Phase 5's
      //       FSM patterns for `nsl::ProcOp` / `nsl::StateOp`
      //       / `nsl::GotoOp` / `nsl::FinishOp` / proc-target
      //       `nsl::CallOp`) — the op gets rewritten cleanly.
      //
      //   (b) No pattern is registered (Phase-6 territory ops like
      //       `nsl::RegOp` / `nsl::ClockedTransferOp` / `nsl::WireOp`
      //       / `nsl::MemOp` / arith / bit-op / sim) — `nsl` is an
      //       illegal dialect via the conversion target, so
      //       `applyFullConversion` fail-fasts with the standard
      //       "failed to legalize operation 'nsl.<X>'" diagnostic
      //       (FR-028). The T029 fixture asserts this path.
      //
      // The move uses MLIR's standard list-splice via `Operation`'s
      // `moveBefore`. We move the op to before the (existing
      // empty) `hw::OutputOp` terminator created by the
      // `HWModuleOp` constructor, so SSA-dominance ordering (defs
      // before uses) is preserved relative to the just-built
      // `hw::ConstantOp` / block-arg map entries above.
      auto *terminator = hwModuleOp.getBodyBlock()->getTerminator();
      op->moveBefore(terminator);
    }
  }

  // Step 5: build hw::OutputOp with collected output values.
  llvm::SmallVector<mlir::Value, 4> outputOperands;
  for (auto &p : ports.getOutputs()) {
    mlir::Value v;
    for (auto &kv : outputAssignments) {
      if (kv.first == p.name.getValue()) {
        v = kv.second;
        break;
      }
    }
    if (!v) {
      // Undriven output → zero-driver. Matches the design intent
      // that outputs always get driven so the hw.module verifier
      // accepts the result.
      auto zero = mlir::IntegerAttr::get(p.type, 0);
      auto cst = circt::hw::ConstantOp::create(builder, loc, zero);
      v = cst.getResult();
    }
    outputOperands.push_back(v);
  }

  // The HWModuleOp ctor inserts an empty hw.output terminator; replace
  // it with one carrying our operands.
  auto *terminator = hwModuleOp.getBodyBlock()->getTerminator();
  builder.setInsertionPoint(terminator);
  circt::hw::OutputOp::create(builder, loc, outputOperands);
  terminator->erase();

  // Step 6: erase the paired declare and the original nsl.module.
  if (declareOp) {
    declareOp.erase();
  }
  moduleOp.erase();

  (void)ctx;
  return mlir::success();
}

} // namespace

mlir::LogicalResult
lowerNSLModulesToHWModules(mlir::ModuleOp parentModule) {
  mlir::OpBuilder builder(parentModule);

  // Snapshot the nsl::ModuleOp set first; we'll mutate the parent
  // body during the walk.
  llvm::SmallVector<nsl::dialect::ModuleOp, 4> nslModules;
  for (auto m : parentModule.getOps<nsl::dialect::ModuleOp>()) {
    nslModules.push_back(m);
  }
  // Process in source order — SubmoduleOp lookup relies on the
  // referenced hw.module already existing (Phase 4 limitation; see
  // SubmoduleOp arm of lowerOneModule).
  for (auto m : nslModules) {
    if (mlir::failed(lowerOneModule(m, builder))) {
      return mlir::failure();
    }
  }

  // Erase any leftover top-level nsl.param_int / nsl.param_str ops.
  // Their values were consumed at instance construction; the dialect
  // conversion target marks them illegal, so we erase eagerly here
  // rather than relying on a separate pattern.
  llvm::SmallVector<mlir::Operation *, 4> stragglers;
  for (auto &op : parentModule.getBody()->getOperations()) {
    if (llvm::isa<nsl::dialect::ParamIntOp>(op) ||
        llvm::isa<nsl::dialect::ParamStrOp>(op)) {
      stragglers.push_back(&op);
    }
  }
  for (auto *op : stragglers) {
    op->erase();
  }

  return mlir::success();
}

void populateModulePatterns(mlir::RewritePatternSet & /*patterns*/,
                            CIRCTTypeConverter & /*type_converter*/) {
  // Phase 4: structural rewrite is performed by
  // `lowerNSLModulesToHWModules` (a manual pre-pass invoked from
  // `NSLToCIRCTPass::runOnOperation` BEFORE applyFullConversion).
  // No DialectConversion patterns are registered here. Future phases
  // adding leaf-op patterns against the new `hw::HWModuleOp` body
  // can register them here. The coverage_guard.cmake regex looks
  // for `OpConversionPattern<` tokens in this file; the structural
  // rewrite owns its own coverage via the `test/Lower/circt/module/`
  // fixture set, and the guard's bijection holds because once a leaf
  // pattern lands here in a future phase, the matching fixture is
  // co-authored under the same directory.
}

} // namespace nsl::lower
