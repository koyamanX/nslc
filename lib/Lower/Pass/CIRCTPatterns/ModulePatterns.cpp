// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/CIRCTPatterns/ModulePatterns.cpp — M6 module-skeleton
// + leaf-op lowering driver (Phase 4 US2 + Phase 6 US4 implementation).
//
// **Design §10 rows covered (Phase 4)**: nsl.module → hw.HWModuleOp,
// nsl.declare → consumed, nsl.{input,output,inout}_port → port +
// block-arg / output wiring, nsl.submodule → hw.instance,
// nsl.{param_int,param_str} → hw.instance parameters,
// nsl.constant → hw.constant, nsl.transfer (output-port LHS) →
// hw.output operand wiring.
//
// **Design §10 rows covered (Phase 6 — newly added)**:
//   * Arith family: nsl.{add,sub,mul,eq,ne,lt,le,gt,ge} →
//     comb.{add,sub,mul,icmp <pred>}.
//   * Bit-op family: nsl.{and,or,xor,shl,shr,land,lor,not,neg,lnot,
//     reduce_and,reduce_or,reduce_xor,sign_extend,zero_extend,mux,
//     concat,extract,repeat} → comb.* equivalents.
//   * State family: nsl.{reg,wire,mem} → seq.firreg / seq.compreg /
//     hw.wire / seq.firmem; nsl.{transfer,clocked_transfer} → wire
//     drivers / firreg data inputs.
//   * Control family: nsl.{alt,any,if,case,default,call (func_in
//     variant)} → nested comb.mux chains, parallel comb.or-of-mux
//     envelopes, mux-on-data conditional reg updates per Q3 → A.
//   * Sim family: nsl.{sim_display,sim_finish,sim_init,sim_delay} +
//     S29 _init block → sv.{fwrite,finish,initial,verbatim} inside
//     a single per-module sv.ifdef "SIMULATION" block (research §9).
//
// **Implementation strategy** (extension of the Phase 4/5 inline-
// structural-pre-pass precedent): all conversion is performed as a
// manual structural walk inside `lowerOneModule`. The
// `populate*Patterns` functions in the family files stay empty (no
// DialectConversion patterns registered); the `applyFullConversion`
// step that runs in `NSLToCIRCTPass::runOnOperation` finds zero
// reachable `nsl::*` ops in the IR after the pre-pass completes and
// becomes a no-op. The coverage_guard.cmake bijection-checker
// requires fixtures under `test/Lower/circt/<dir>/` for every family
// file with at least one `OpConversionPattern<` token; with no such
// tokens (empty populators), bijection trivially holds — the per-
// family fixtures live under `test/Lower/circt/<arith,state,control,
// sim>/` regardless and the per-family files (ArithPatterns.cpp etc.)
// remain as documentation surfaces with helper-function homes.
//
// Rationale (consistent with Phase 4/5 commentary): inline lowering
// from a structural-pre-pass driver avoids interleaving issues that
// arise when the standard DialectConversion worklist tries to
// legalize hierarchical `nsl::*` op trees (`nsl.proc > nsl.state`,
// `nsl.if > nsl.clocked_transfer > nsl.add > nsl.input_port`)
// independently. The pre-pass walks each `nsl::ModuleOp` body in
// source order, materialising CIRCT ops in dominance-correct
// positions and tracking SSA equivalences via a `valueMap`. Per
// Constitution Principle III: zero hand-rolled CIRCT-equivalent
// passes — every output op is stock CIRCT (`hw::*`, `comb::*`,
// `seq::*`, `sv::*`); we drive their *creation* manually.

#include "../CIRCTTypeConverter.h"
#include "../NSLToCIRCTPass.h"

#include "circt/Dialect/Comb/CombOps.h"
#include "circt/Dialect/HW/HWAttributes.h"
#include "circt/Dialect/HW/HWOps.h"
#include "circt/Dialect/HW/HWTypes.h"
#include "circt/Dialect/HW/PortImplementation.h"
#include "circt/Dialect/SV/SVOps.h"
#include "circt/Dialect/Seq/SeqOps.h"
#include "circt/Dialect/Seq/SeqTypes.h"

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
/// among the direct children of the enclosing `mlir::ModuleOp`.
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
      auto noneType = mlir::NoneType::get(ctx);
      auto valueAttr = mlir::StringAttr::get(ctx, ps.getValue());
      params.push_back(circt::hw::ParamDeclAttr::get(
          mlir::StringAttr::get(ctx, ps.getSymName()), noneType, valueAttr));
    }
  }
  (void)builder;
  return mlir::ArrayAttr::get(ctx, params);
}

//===----------------------------------------------------------------------===//
// Phase 6: Per-module lowering context (track SSA equivalences,
// implicit clk/rst_n block-args, output-port assignments, register
// post-processing, sim-ifdef materialisation).
//===----------------------------------------------------------------------===//

struct ModuleLoweringCtx {
  /// SSA-mapping: nsl::* result → CIRCT result (post-conversion).
  llvm::SmallDenseMap<mlir::Value, mlir::Value, 32> valueMap;

  /// Ops scheduled for erasure after the body walk completes.
  /// We accumulate here instead of erasing eagerly because the walk
  /// may not yet have lowered consumers that still hold the SSA Value
  /// the op produces. We erase in reverse order so users get cleared
  /// before defs.
  llvm::SmallVector<mlir::Operation *, 32> pendingErase;

  /// Pending output-port assignment list (transfer-to-output writes
  /// resolved post-conversion). Map: output-port name → driving CIRCT
  /// SSA value.
  llvm::SmallVector<std::pair<llvm::StringRef, mlir::Value>, 4>
      outputAssignments;

  /// Map: nsl.output_port result Value → port name.
  llvm::SmallVector<std::pair<mlir::Value, llvm::StringRef>, 4>
      outputPortValues;

  /// Map: register name (i.e. nsl.reg's StrAttr name) → final
  /// (data-input SSA value, location, init-value, has-interface,
  /// reset-port-name). Populated as we encounter `clocked_transfer`
  /// + `if`-over-reg ops; consumed at end of body walk to materialise
  /// the seq.firreg / seq.compreg.
  struct RegInfo {
    mlir::Value defaultPrev; // SSA self-reference (the firreg's own result)
    mlir::Value pendingNext; // running mux chain (most-recent assignment)
    std::optional<mlir::Location> loc;
    std::optional<int64_t> initValue;
    mlir::IntegerType intType;
    bool hasInterface = false;
    bool isMaterialised = false;
    circt::seq::FirRegOp firRegOp; // populated lazily on first write
    circt::seq::CompRegOp compRegOp;
  };
  llvm::SmallVector<std::pair<llvm::StringRef, RegInfo>, 4> regs;

  /// Wire records: (name, original-nsl-result, location). Each wire
  /// is materialised as an `hw::WireOp` ONLY when its driver is
  /// known (via the first nsl.transfer with this wire as LHS), so
  /// SSA dominance is preserved.
  struct WireInfo {
    llvm::StringRef name;
    mlir::Value origResult;
    std::optional<mlir::Location> loc;
    circt::hw::WireOp hwWireOp; // populated lazily
  };
  llvm::SmallVector<WireInfo, 4> wires;

  /// hw.module clock + reset block-arg references (set up during port
  /// processing). Empty if the module has no paired declare.
  mlir::Value clkArg;
  mlir::Value rstnArg;
  bool hasInterface = false;
  mlir::Value clockSeq;       // clk after seq.to_clock conversion (lazy)
  mlir::Value asyncResetCond; // computed reset-fires bit (lazy)

  /// Lazy sim-ifdef block.
  circt::sv::IfDefOp simIfDef;

  /// Track the hw.module being built (for output-op rewrite at end).
  circt::hw::HWModuleOp hwModuleOp;
};

/// Look up SSA-mapped value (returns the original if not in map —
/// e.g., constants already created via hw.constant get registered;
/// uses through an unmapped value are presumed ill-formed).
mlir::Value lookupValue(ModuleLoweringCtx &ctx, mlir::Value v) {
  auto it = ctx.valueMap.find(v);
  if (it != ctx.valueMap.end()) {
    return it->second;
  }
  return v;
}

/// Replace all uses of `from` with `to` ONLY where the consumer's
/// operand-type matches `to.getType()`. This avoids verifier-failures
/// while we incrementally lower nsl ops to CIRCT ops: a still-alive
/// nsl op consuming `from` (`!nsl.bits<N>`) will not accept an `iN`
/// replacement until itself lowered. We rely on subsequent
/// lower-helpers calling `lookupValue` instead. So we DON'T do a
/// blanket replaceAllUsesWith here — we just record the mapping.
void recordValueMapping(ModuleLoweringCtx &ctx, mlir::Value from,
                        mlir::Value to) {
  ctx.valueMap[from] = to;
}

/// Get-or-create a `seq::ToClockOp` from the i1 clk block-arg.
mlir::Value getOrBuildClockSeq(ModuleLoweringCtx &ctx,
                               mlir::OpBuilder &builder,
                               mlir::Location loc) {
  if (ctx.clockSeq) {
    return ctx.clockSeq;
  }
  if (!ctx.clkArg) {
    return {};
  }
  mlir::OpBuilder::InsertionGuard g(builder);
  builder.setInsertionPointToStart(ctx.hwModuleOp.getBodyBlock());
  ctx.clockSeq =
      circt::seq::ToClockOp::create(builder, loc, ctx.clkArg).getResult();
  return ctx.clockSeq;
}

/// Get-or-create the async-active-low reset-fires bit
/// (`comb.icmp eq %rst_n, 0`).
mlir::Value getOrBuildResetCond(ModuleLoweringCtx &ctx,
                                mlir::OpBuilder &builder,
                                mlir::Location loc) {
  if (ctx.asyncResetCond) {
    return ctx.asyncResetCond;
  }
  if (!ctx.rstnArg) {
    return {};
  }
  mlir::OpBuilder::InsertionGuard g(builder);
  builder.setInsertionPointToStart(ctx.hwModuleOp.getBodyBlock());
  // Skip past ToClockOp if just inserted.
  if (ctx.clockSeq) {
    builder.setInsertionPointAfterValue(ctx.clockSeq);
  }
  auto i1 = builder.getI1Type();
  auto zeroAttr = mlir::IntegerAttr::get(i1, 0);
  auto zero = circt::hw::ConstantOp::create(builder, loc, zeroAttr);
  ctx.asyncResetCond = circt::comb::ICmpOp::create(
                            builder, loc,
                            circt::comb::ICmpPredicate::eq, ctx.rstnArg,
                            zero, /*twoState=*/false)
                          .getResult();
  return ctx.asyncResetCond;
}

/// Get-or-create the SIMULATION macro decl at the outer mlir.module
/// level (idempotent across multiple hw.modules in the same compile).
void ensureSimulationMacroDecl(circt::hw::HWModuleOp hwModuleOp,
                                mlir::OpBuilder &builder) {
  auto outer = llvm::dyn_cast_or_null<mlir::ModuleOp>(
      hwModuleOp->getParentOp());
  if (!outer) {
    return;
  }
  // Symbol-table lookup (the MacroDeclOp uses `sym_name`).
  if (auto *existing = mlir::SymbolTable::lookupSymbolIn(
          outer.getOperation(), "SIMULATION")) {
    (void)existing;
    return;
  }
  mlir::OpBuilder::InsertionGuard g(builder);
  builder.setInsertionPointToStart(outer.getBody());
  circt::sv::MacroDeclOp::create(builder, hwModuleOp.getLoc(),
                                   llvm::StringRef("SIMULATION"));
}

/// Get-or-create the per-module SIMULATION ifdef. The ifdef sits at
/// the END of the hw.module body (after synthesizable ops); insertion
/// is idempotent — repeated calls return the same op + body region.
circt::sv::IfDefOp getOrBuildSimIfDef(ModuleLoweringCtx &ctx,
                                       mlir::OpBuilder &builder,
                                       mlir::Location loc) {
  if (ctx.simIfDef) {
    return ctx.simIfDef;
  }
  ensureSimulationMacroDecl(ctx.hwModuleOp, builder);
  mlir::OpBuilder::InsertionGuard g(builder);
  // Insert before the (existing) hw.output terminator.
  auto *terminator = ctx.hwModuleOp.getBodyBlock()->getTerminator();
  builder.setInsertionPoint(terminator);
  ctx.simIfDef = circt::sv::IfDefOp::create(builder, loc,
                                             llvm::StringRef("SIMULATION"));
  return ctx.simIfDef;
}

/// Comb predicate for nsl ICmp ops (currently unsigned only — the
/// dialect verifier already documents this; signedness for slt/sle/
/// sgt/sge would surface via a sign_extend chain).
circt::comb::ICmpPredicate icmpPredicateFor(mlir::Operation *op) {
  if (llvm::isa<nsl::dialect::EqOp>(op)) {
    return circt::comb::ICmpPredicate::eq;
  }
  if (llvm::isa<nsl::dialect::NeOp>(op)) {
    return circt::comb::ICmpPredicate::ne;
  }
  if (llvm::isa<nsl::dialect::LtOp>(op)) {
    return circt::comb::ICmpPredicate::ult;
  }
  if (llvm::isa<nsl::dialect::LeOp>(op)) {
    return circt::comb::ICmpPredicate::ule;
  }
  if (llvm::isa<nsl::dialect::GtOp>(op)) {
    return circt::comb::ICmpPredicate::ugt;
  }
  if (llvm::isa<nsl::dialect::GeOp>(op)) {
    return circt::comb::ICmpPredicate::uge;
  }
  llvm_unreachable("unhandled icmp op kind");
}

//===----------------------------------------------------------------------===//
// Forward declarations.
//===----------------------------------------------------------------------===//
mlir::LogicalResult lowerOpTree(mlir::Operation *op, ModuleLoweringCtx &ctx,
                                mlir::OpBuilder &builder);

mlir::LogicalResult lowerBodyOps(mlir::Block &block, ModuleLoweringCtx &ctx,
                                 mlir::OpBuilder &builder);

//===----------------------------------------------------------------------===//
// Phase 6: Arith + Bit-op + extend / extract / repeat / mux / concat
// lowering. Each helper materialises one or more CIRCT ops, registers
// the result(s) in `ctx.valueMap`, and erases the source nsl op.
//===----------------------------------------------------------------------===//

mlir::LogicalResult lowerArithOp(mlir::Operation *op, ModuleLoweringCtx &ctx,
                                 mlir::OpBuilder &builder) {
  mlir::Location loc = op->getLoc();
  // Caller sets insertion point (lowerControlOp anchors it outside
  // any soon-to-be-erased nsl region).

  // Binary commutative arith: AddOp, MulOp, AndOp, OrOp, XorOp.
  if (auto add = llvm::dyn_cast<nsl::dialect::AddOp>(op)) {
    auto lhs = lookupValue(ctx, add.getLhs());
    auto rhs = lookupValue(ctx, add.getRhs());
    auto res = circt::comb::AddOp::create(builder, loc, lhs, rhs).getResult();
    ctx.valueMap[add.getResult()] = res;
    ctx.pendingErase.push_back(op);
    return mlir::success();
  }
  if (auto mul = llvm::dyn_cast<nsl::dialect::MulOp>(op)) {
    auto lhs = lookupValue(ctx, mul.getLhs());
    auto rhs = lookupValue(ctx, mul.getRhs());
    auto res = circt::comb::MulOp::create(builder, loc, lhs, rhs).getResult();
    ctx.valueMap[mul.getResult()] = res;
    ctx.pendingErase.push_back(op);
    return mlir::success();
  }
  if (auto andOp = llvm::dyn_cast<nsl::dialect::AndOp>(op)) {
    auto lhs = lookupValue(ctx, andOp.getLhs());
    auto rhs = lookupValue(ctx, andOp.getRhs());
    auto res = circt::comb::AndOp::create(builder, loc, lhs, rhs).getResult();
    ctx.valueMap[andOp.getResult()] = res;
    ctx.pendingErase.push_back(op);
    return mlir::success();
  }
  if (auto orOp = llvm::dyn_cast<nsl::dialect::OrOp>(op)) {
    auto lhs = lookupValue(ctx, orOp.getLhs());
    auto rhs = lookupValue(ctx, orOp.getRhs());
    auto res = circt::comb::OrOp::create(builder, loc, lhs, rhs).getResult();
    ctx.valueMap[orOp.getResult()] = res;
    ctx.pendingErase.push_back(op);
    return mlir::success();
  }
  if (auto xorOp = llvm::dyn_cast<nsl::dialect::XorOp>(op)) {
    auto lhs = lookupValue(ctx, xorOp.getLhs());
    auto rhs = lookupValue(ctx, xorOp.getRhs());
    auto res = circt::comb::XorOp::create(builder, loc, lhs, rhs).getResult();
    ctx.valueMap[xorOp.getResult()] = res;
    ctx.pendingErase.push_back(op);
    return mlir::success();
  }

  // Binary non-commutative: SubOp, ShlOp, ShrOp.
  if (auto sub = llvm::dyn_cast<nsl::dialect::SubOp>(op)) {
    auto lhs = lookupValue(ctx, sub.getLhs());
    auto rhs = lookupValue(ctx, sub.getRhs());
    auto res = circt::comb::SubOp::create(builder, loc, lhs, rhs).getResult();
    ctx.valueMap[sub.getResult()] = res;
    ctx.pendingErase.push_back(op);
    return mlir::success();
  }
  if (auto shl = llvm::dyn_cast<nsl::dialect::ShlOp>(op)) {
    auto lhs = lookupValue(ctx, shl.getLhs());
    auto rhs = lookupValue(ctx, shl.getRhs());
    auto res = circt::comb::ShlOp::create(builder, loc, lhs, rhs).getResult();
    ctx.valueMap[shl.getResult()] = res;
    ctx.pendingErase.push_back(op);
    return mlir::success();
  }
  if (auto shr = llvm::dyn_cast<nsl::dialect::ShrOp>(op)) {
    auto lhs = lookupValue(ctx, shr.getLhs());
    auto rhs = lookupValue(ctx, shr.getRhs());
    auto res = circt::comb::ShrUOp::create(builder, loc, lhs, rhs).getResult();
    ctx.valueMap[shr.getResult()] = res;
    ctx.pendingErase.push_back(op);
    return mlir::success();
  }

  // Comparisons → comb.icmp (unsigned predicates per dialect docs).
  if (llvm::isa<nsl::dialect::EqOp, nsl::dialect::NeOp, nsl::dialect::LtOp,
                nsl::dialect::LeOp, nsl::dialect::GtOp, nsl::dialect::GeOp>(
          op)) {
    auto lhs = lookupValue(ctx, op->getOperand(0));
    auto rhs = lookupValue(ctx, op->getOperand(1));
    auto pred = icmpPredicateFor(op);
    auto res = circt::comb::ICmpOp::create(builder, loc, pred, lhs, rhs,
                                            /*twoState=*/false)
                  .getResult();
    ctx.valueMap[op->getResult(0)] = res;
    ctx.pendingErase.push_back(op);
    return mlir::success();
  }

  // Logical AND / OR (operands width-1) → comb.and / or on i1.
  if (auto land = llvm::dyn_cast<nsl::dialect::LandOp>(op)) {
    auto lhs = lookupValue(ctx, land.getLhs());
    auto rhs = lookupValue(ctx, land.getRhs());
    auto res = circt::comb::AndOp::create(builder, loc, lhs, rhs).getResult();
    ctx.valueMap[land.getResult()] = res;
    ctx.pendingErase.push_back(op);
    return mlir::success();
  }
  if (auto lor = llvm::dyn_cast<nsl::dialect::LorOp>(op)) {
    auto lhs = lookupValue(ctx, lor.getLhs());
    auto rhs = lookupValue(ctx, lor.getRhs());
    auto res = circt::comb::OrOp::create(builder, loc, lhs, rhs).getResult();
    ctx.valueMap[lor.getResult()] = res;
    ctx.pendingErase.push_back(op);
    return mlir::success();
  }
  return mlir::failure();
}

mlir::LogicalResult lowerBitOp(mlir::Operation *op, ModuleLoweringCtx &ctx,
                               mlir::OpBuilder &builder) {
  mlir::Location loc = op->getLoc();
  // Caller sets insertion point.

  // Bitwise NOT → comb.xor %a, all-ones.
  if (auto notOp = llvm::dyn_cast<nsl::dialect::NotOp>(op)) {
    auto a = lookupValue(ctx, notOp.getOperand());
    auto intType = mlir::cast<mlir::IntegerType>(a.getType());
    auto allOnesAttr = mlir::IntegerAttr::get(
        intType, llvm::APInt::getAllOnes(intType.getWidth()));
    auto allOnes = circt::hw::ConstantOp::create(builder, loc, allOnesAttr);
    auto res = circt::comb::XorOp::create(builder, loc, a, allOnes.getResult())
                  .getResult();
    ctx.valueMap[notOp.getResult()] = res;
    ctx.pendingErase.push_back(op);
    return mlir::success();
  }
  // Two's-complement negation → comb.sub 0, %a.
  if (auto neg = llvm::dyn_cast<nsl::dialect::NegOp>(op)) {
    auto a = lookupValue(ctx, neg.getOperand());
    auto intType = mlir::cast<mlir::IntegerType>(a.getType());
    auto zeroAttr = mlir::IntegerAttr::get(intType, 0);
    auto zero = circt::hw::ConstantOp::create(builder, loc, zeroAttr);
    auto res =
        circt::comb::SubOp::create(builder, loc, zero.getResult(), a)
            .getResult();
    ctx.valueMap[neg.getResult()] = res;
    ctx.pendingErase.push_back(op);
    return mlir::success();
  }
  // Logical NOT → comb.icmp eq %a, 0.
  if (auto lnot = llvm::dyn_cast<nsl::dialect::LnotOp>(op)) {
    auto a = lookupValue(ctx, lnot.getOperand());
    auto intType = mlir::cast<mlir::IntegerType>(a.getType());
    auto zeroAttr = mlir::IntegerAttr::get(intType, 0);
    auto zero = circt::hw::ConstantOp::create(builder, loc, zeroAttr);
    auto res = circt::comb::ICmpOp::create(
                    builder, loc, circt::comb::ICmpPredicate::eq, a,
                    zero.getResult(), /*twoState=*/false)
                  .getResult();
    ctx.valueMap[lnot.getResult()] = res;
    ctx.pendingErase.push_back(op);
    return mlir::success();
  }
  // Reduction AND → comb.icmp eq %a, all-ones.
  if (auto rand = llvm::dyn_cast<nsl::dialect::ReduceAndOp>(op)) {
    auto a = lookupValue(ctx, rand.getOperand());
    auto intType = mlir::cast<mlir::IntegerType>(a.getType());
    auto allOnesAttr = mlir::IntegerAttr::get(
        intType, llvm::APInt::getAllOnes(intType.getWidth()));
    auto allOnes = circt::hw::ConstantOp::create(builder, loc, allOnesAttr);
    auto res = circt::comb::ICmpOp::create(
                    builder, loc, circt::comb::ICmpPredicate::eq, a,
                    allOnes.getResult(), /*twoState=*/false)
                  .getResult();
    ctx.valueMap[rand.getResult()] = res;
    ctx.pendingErase.push_back(op);
    return mlir::success();
  }
  // Reduction OR → comb.icmp ne %a, 0.
  if (auto ror = llvm::dyn_cast<nsl::dialect::ReduceOrOp>(op)) {
    auto a = lookupValue(ctx, ror.getOperand());
    auto intType = mlir::cast<mlir::IntegerType>(a.getType());
    auto zeroAttr = mlir::IntegerAttr::get(intType, 0);
    auto zero = circt::hw::ConstantOp::create(builder, loc, zeroAttr);
    auto res = circt::comb::ICmpOp::create(
                    builder, loc, circt::comb::ICmpPredicate::ne, a,
                    zero.getResult(), /*twoState=*/false)
                  .getResult();
    ctx.valueMap[ror.getResult()] = res;
    ctx.pendingErase.push_back(op);
    return mlir::success();
  }
  // Reduction XOR → comb.parity.
  if (auto rxor = llvm::dyn_cast<nsl::dialect::ReduceXorOp>(op)) {
    auto a = lookupValue(ctx, rxor.getOperand());
    auto res =
        circt::comb::ParityOp::create(builder, loc, a, /*twoState=*/false)
            .getResult();
    ctx.valueMap[rxor.getResult()] = res;
    ctx.pendingErase.push_back(op);
    return mlir::success();
  }
  // Sign-extend → comb.concat (replicate MSB, operand).
  if (auto se = llvm::dyn_cast<nsl::dialect::SignExtendOp>(op)) {
    auto a = lookupValue(ctx, se.getOperand());
    auto srcType = mlir::cast<mlir::IntegerType>(a.getType());
    auto dstWidth = mlir::cast<nsl::dialect::BitsType>(
                          se.getResult().getType())
                          .getWidth();
    if (dstWidth == srcType.getWidth()) {
      // No-op extension.
      ctx.valueMap[se.getResult()] = a;
      ctx.pendingErase.push_back(op);
      return mlir::success();
    }
    // Extract MSB.
    auto i1 = builder.getI1Type();
    auto msb = circt::comb::ExtractOp::create(builder, loc, i1, a,
                                                srcType.getWidth() - 1)
                  .getResult();
    // Replicate MSB by (dstWidth - srcWidth) times.
    unsigned padBits = dstWidth - srcType.getWidth();
    auto padType = mlir::IntegerType::get(builder.getContext(), padBits);
    auto rep =
        circt::comb::ReplicateOp::create(builder, loc, padType, msb)
            .getResult();
    auto dstType = mlir::IntegerType::get(builder.getContext(), dstWidth);
    auto cat = circt::comb::ConcatOp::create(builder, loc, dstType,
                                               mlir::ValueRange{rep, a})
                  .getResult();
    ctx.valueMap[se.getResult()] = cat;
    ctx.pendingErase.push_back(op);
    return mlir::success();
  }
  // Zero-extend → comb.concat (zeros, operand).
  if (auto ze = llvm::dyn_cast<nsl::dialect::ZeroExtendOp>(op)) {
    auto a = lookupValue(ctx, ze.getOperand());
    auto srcType = mlir::cast<mlir::IntegerType>(a.getType());
    auto dstWidth = mlir::cast<nsl::dialect::BitsType>(
                          ze.getResult().getType())
                          .getWidth();
    if (dstWidth == srcType.getWidth()) {
      ctx.valueMap[ze.getResult()] = a;
      ctx.pendingErase.push_back(op);
      return mlir::success();
    }
    unsigned padBits = dstWidth - srcType.getWidth();
    auto padType = mlir::IntegerType::get(builder.getContext(), padBits);
    auto zeroAttr = mlir::IntegerAttr::get(padType, 0);
    auto zeros = circt::hw::ConstantOp::create(builder, loc, zeroAttr);
    auto dstType = mlir::IntegerType::get(builder.getContext(), dstWidth);
    auto cat =
        circt::comb::ConcatOp::create(builder, loc, dstType,
                                        mlir::ValueRange{zeros.getResult(), a})
            .getResult();
    ctx.valueMap[ze.getResult()] = cat;
    ctx.pendingErase.push_back(op);
    return mlir::success();
  }
  // Mux (3-input expression) → comb.mux.
  if (auto mux = llvm::dyn_cast<nsl::dialect::MuxOp>(op)) {
    auto cond = lookupValue(ctx, mux.getCond());
    auto thenV = lookupValue(ctx, mux.getThenValue());
    auto elseV = lookupValue(ctx, mux.getElseValue());
    auto res = circt::comb::MuxOp::create(builder, loc, cond, thenV, elseV,
                                            /*twoState=*/false)
                  .getResult();
    ctx.valueMap[mux.getResult()] = res;
    ctx.pendingErase.push_back(op);
    return mlir::success();
  }
  // Concat (variadic) → comb.concat.
  if (auto concat = llvm::dyn_cast<nsl::dialect::ConcatOp>(op)) {
    llvm::SmallVector<mlir::Value, 4> mapped;
    for (auto v : concat.getOperands()) {
      mapped.push_back(lookupValue(ctx, v));
    }
    auto dstWidth = mlir::cast<nsl::dialect::BitsType>(
                          concat.getResult().getType())
                          .getWidth();
    auto dstType = mlir::IntegerType::get(builder.getContext(), dstWidth);
    auto res =
        circt::comb::ConcatOp::create(builder, loc, dstType, mapped)
            .getResult();
    ctx.valueMap[concat.getResult()] = res;
    ctx.pendingErase.push_back(op);
    return mlir::success();
  }
  // Extract → comb.extract.
  if (auto ext = llvm::dyn_cast<nsl::dialect::ExtractOp>(op)) {
    auto a = lookupValue(ctx, ext.getOperand());
    auto resWidth = mlir::cast<nsl::dialect::BitsType>(
                          ext.getResult().getType())
                          .getWidth();
    auto resType = mlir::IntegerType::get(builder.getContext(), resWidth);
    auto res = circt::comb::ExtractOp::create(
                    builder, loc, resType, a,
                    static_cast<int32_t>(ext.getLowBit()))
                  .getResult();
    ctx.valueMap[ext.getResult()] = res;
    ctx.pendingErase.push_back(op);
    return mlir::success();
  }
  // Repeat → comb.replicate.
  if (auto rep = llvm::dyn_cast<nsl::dialect::RepeatOp>(op)) {
    auto a = lookupValue(ctx, rep.getOperand());
    auto count = static_cast<int32_t>(rep.getCount());
    auto srcType = mlir::cast<mlir::IntegerType>(a.getType());
    auto resType = mlir::IntegerType::get(builder.getContext(),
                                            srcType.getWidth() * count);
    auto res =
        circt::comb::ReplicateOp::create(builder, loc, resType, a)
            .getResult();
    ctx.valueMap[rep.getResult()] = res;
    ctx.pendingErase.push_back(op);
    return mlir::success();
  }
  return mlir::failure();
}

//===----------------------------------------------------------------------===//
// State family: nsl.reg → seq.firreg / seq.compreg lazy materialise.
// nsl.wire → tracked for hw.wire emission on transfer.
// nsl.mem → seq.firmem.
//===----------------------------------------------------------------------===//

ModuleLoweringCtx::RegInfo *findRegInfo(ModuleLoweringCtx &ctx,
                                         llvm::StringRef name) {
  for (auto &kv : ctx.regs) {
    if (kv.first == name) {
      return &kv.second;
    }
  }
  return nullptr;
}

mlir::LogicalResult lowerRegOp(nsl::dialect::RegOp regOp,
                               ModuleLoweringCtx &ctx,
                               mlir::OpBuilder &builder) {
  auto bits = mlir::cast<nsl::dialect::BitsType>(regOp.getResult().getType());
  auto intType = mlir::IntegerType::get(builder.getContext(), bits.getWidth());

  ModuleLoweringCtx::RegInfo info;
  info.intType = intType;
  info.loc = regOp.getLoc();
  info.hasInterface = ctx.hasInterface;
  if (auto initAttr = regOp.getInit()) {
    info.initValue = *initAttr;
  } else {
    info.initValue = 0;
  }
  ctx.regs.emplace_back(regOp.getName(), info);

  // Materialise the firreg / compreg eagerly with placeholder data
  // input that we'll update via setOperand once we know the final
  // mux chain. Circt's seq.firreg requires a `next` operand at
  // construction time so we use a temporary placeholder value (the
  // reg's own result value, i.e., a self-loop). After
  // post-processing we'll setOperand to the actual mux chain.
  mlir::OpBuilder::InsertionGuard g(builder);
  // Keep the firreg near the top of the body but after clk/rst
  // computations (getOrBuildClockSeq inserts at start; we go after
  // those).
  builder.setInsertionPointToStart(ctx.hwModuleOp.getBodyBlock());
  // Skip past existing ToClockOp / ICmpOp the helpers may have
  // inserted.
  auto loc = regOp.getLoc();

  // Build seq.firreg path (no-interface case — Q2 → C async-active-low).
  if (!ctx.hasInterface) {
    auto clk = getOrBuildClockSeq(ctx, builder, loc);
    auto rstFires = getOrBuildResetCond(ctx, builder, loc);
    if (!clk || !rstFires) {
      return regOp.emitError(
          "no-interface module requires implicit clk + rst_n ports for "
          "nsl.reg lowering, but they were not constructed");
    }
    // Build reset value (preset).
    builder.setInsertionPointAfterValue(rstFires);
    auto rstValAttr =
        mlir::IntegerAttr::get(intType, info.initValue.value_or(0));
    auto rstVal = circt::hw::ConstantOp::create(builder, loc, rstValAttr)
                      .getResult();
    // Initial data input: the reset value (pre-loop placeholder).
    // We will swap to the real data SSA via setOperand at end.
    auto nameStrAttr = builder.getStringAttr(regOp.getName());
    auto firReg = circt::seq::FirRegOp::create(
        builder, loc, /*next=*/rstVal, /*clk=*/clk, /*name=*/nameStrAttr,
        /*reset=*/rstFires, /*resetValue=*/rstVal,
        /*innerSym=*/circt::hw::InnerSymAttr{},
        /*isAsync=*/true);
    auto &back = ctx.regs.back();
    back.second.firRegOp = firReg;
    back.second.defaultPrev = firReg.getResult();
    back.second.pendingNext = rstVal; // start with reset value (the
                                       // unconditional default-driver)
    ctx.valueMap[regOp.getResult()] = firReg.getResult();
  } else {
    // Explicit-`interface` path → seq.compreg with user-named clock/
    // reset operands.
    auto clkV = ctx.clkArg;
    auto rstV = ctx.rstnArg;
    // Convert clk to ClockType.
    auto clkSeq = circt::seq::ToClockOp::create(builder, loc, clkV)
                       .getResult();
    auto rstValAttr =
        mlir::IntegerAttr::get(intType, info.initValue.value_or(0));
    auto rstVal = circt::hw::ConstantOp::create(builder, loc, rstValAttr)
                      .getResult();
    // CompRegOp builder takes (input, clk, reset, rstValue, name).
    auto compReg = circt::seq::CompRegOp::create(
        builder, loc, /*input=*/rstVal, /*clk=*/clkSeq,
        /*reset=*/rstV, /*rstValue=*/rstVal,
        /*name=*/circt::StringAttrOrRef(regOp.getName()));
    auto &back = ctx.regs.back();
    back.second.compRegOp = compReg;
    back.second.defaultPrev = compReg.getResult();
    back.second.pendingNext = rstVal;
    ctx.valueMap[regOp.getResult()] = compReg.getResult();
  }
  ctx.pendingErase.push_back(regOp);
  return mlir::success();
}

mlir::LogicalResult lowerWireOp(nsl::dialect::WireOp wireOp,
                                ModuleLoweringCtx &ctx,
                                mlir::OpBuilder &builder) {
  // Defer hw.wire materialisation until the driving nsl.transfer is
  // lowered. The wire's name + width are tracked here; the actual
  // hw.wire op is created during lowerTransferOp once we know the
  // driver SSA value.
  ModuleLoweringCtx::WireInfo wi;
  wi.name = wireOp.getName();
  wi.origResult = wireOp.getResult();
  wi.loc = wireOp.getLoc();
  ctx.wires.push_back(wi);
  ctx.pendingErase.push_back(wireOp);
  return mlir::success();
}

mlir::LogicalResult lowerMemOp(nsl::dialect::MemOp memOp,
                               ModuleLoweringCtx &ctx,
                               mlir::OpBuilder &builder) {
  // nsl.mem → seq.firmem with default rd/wr latency = 0/1, ruw=undef,
  // wuw=portOrder.
  auto memType = mlir::cast<nsl::dialect::MemType>(memOp.getResult().getType());
  auto elemBits = mlir::cast<nsl::dialect::BitsType>(memType.getElementType());
  uint64_t depth = memType.getDepth();
  uint32_t width = elemBits.getWidth();

  auto firMemType = circt::seq::FirMemType::get(
      builder.getContext(), depth, width,
      /*maskWidth=*/std::nullopt);

  mlir::OpBuilder::InsertionGuard g(builder);
  builder.setInsertionPoint(memOp);
  auto firMem = circt::seq::FirMemOp::create(
      builder, memOp.getLoc(), firMemType,
      /*readLatency=*/0u, /*writeLatency=*/1u,
      /*ruw=*/circt::seq::RUW::Undefined,
      /*wuw=*/circt::seq::WUW::PortOrder,
      /*name=*/mlir::StringAttr::get(builder.getContext(), memOp.getName()),
      /*innerSym=*/circt::hw::InnerSymAttr{},
      /*init=*/circt::seq::FirMemInitAttr{},
      /*prefix=*/mlir::StringAttr{},
      /*outputFile=*/mlir::Attribute{});
  ctx.valueMap[memOp.getResult()] = firMem.getResult();
  ctx.pendingErase.push_back(memOp);
  return mlir::success();
}

mlir::LogicalResult lowerTransferOp(nsl::dialect::TransferOp xfer,
                                    ModuleLoweringCtx &ctx,
                                    mlir::OpBuilder &builder,
                                    mlir::Value condGate);

mlir::LogicalResult
lowerClockedTransferOp(nsl::dialect::ClockedTransferOp xfer,
                        ModuleLoweringCtx &ctx, mlir::OpBuilder &builder,
                        mlir::Value condGate);

/// Get the output-port name for a SSA value `v` if it's an
/// nsl.output_port result; empty StringRef otherwise.
llvm::StringRef getOutputPortName(ModuleLoweringCtx &ctx, mlir::Value v) {
  for (auto &kv : ctx.outputPortValues) {
    if (kv.first == v) {
      return kv.second;
    }
  }
  return {};
}

mlir::LogicalResult lowerTransferOp(nsl::dialect::TransferOp xfer,
                                    ModuleLoweringCtx &ctx,
                                    mlir::OpBuilder &builder,
                                    mlir::Value condGate) {
  // Check if the LHS is an output port.
  llvm::StringRef outName = getOutputPortName(ctx, xfer.getDst());
  auto srcMapped = lookupValue(ctx, xfer.getSrc());
  if (!srcMapped) {
    return xfer.emitError("transfer source not yet materialised");
  }
  if (!outName.empty()) {
    // Output-port write. Track the assignment.
    if (condGate) {
      // Conditional output-port write — wrap previous assignment
      // (or zero) in a comb.mux. Find the previous assignment for
      // this port (if any).
      mlir::Value prev;
      for (auto &kv : ctx.outputAssignments) {
        if (kv.first == outName) {
          prev = kv.second;
          break;
        }
      }
      if (!prev) {
        // Default to zero of the appropriate width.
        auto t = mlir::cast<mlir::IntegerType>(srcMapped.getType());
        auto zAttr = mlir::IntegerAttr::get(t, 0);
        prev =
            circt::hw::ConstantOp::create(builder, xfer.getLoc(), zAttr)
                .getResult();
      }
      auto muxed = circt::comb::MuxOp::create(builder, xfer.getLoc(),
                                                condGate, srcMapped, prev,
                                                /*twoState=*/false)
                       .getResult();
      // Update or append.
      bool updated = false;
      for (auto &kv : ctx.outputAssignments) {
        if (kv.first == outName) {
          kv.second = muxed;
          updated = true;
          break;
        }
      }
      if (!updated) {
        ctx.outputAssignments.emplace_back(outName, muxed);
      }
    } else {
      bool updated = false;
      for (auto &kv : ctx.outputAssignments) {
        if (kv.first == outName) {
          kv.second = srcMapped;
          updated = true;
          break;
        }
      }
      if (!updated) {
        ctx.outputAssignments.emplace_back(outName, srcMapped);
      }
    }
    ctx.pendingErase.push_back(xfer);
    return mlir::success();
  }
  // LHS is a wire — find or create its hw.wire (lazy materialise).
  ModuleLoweringCtx::WireInfo *wInfo = nullptr;
  for (auto &w : ctx.wires) {
    if (w.origResult == xfer.getDst()) {
      wInfo = &w;
      break;
    }
  }
  if (wInfo) {
    if (!wInfo->hwWireOp) {
      mlir::Value driver = srcMapped;
      if (condGate) {
        auto t = mlir::cast<mlir::IntegerType>(srcMapped.getType());
        auto zAttr = mlir::IntegerAttr::get(t, 0);
        auto z = circt::hw::ConstantOp::create(builder, xfer.getLoc(),
                                                 zAttr)
                     .getResult();
        driver = circt::comb::MuxOp::create(builder, xfer.getLoc(),
                                              condGate, srcMapped, z,
                                              /*twoState=*/false)
                     .getResult();
      }
      auto nameAttr = builder.getStringAttr(wInfo->name);
      wInfo->hwWireOp = circt::hw::WireOp::create(
          builder, xfer.getLoc(), driver, /*name=*/nameAttr,
          /*innerSym=*/circt::hw::InnerSymAttr{});
      ctx.valueMap[wInfo->origResult] = wInfo->hwWireOp.getResult();
    } else {
      auto prev = wInfo->hwWireOp.getInput();
      mlir::Value muxed;
      if (condGate) {
        muxed = circt::comb::MuxOp::create(builder, xfer.getLoc(),
                                              condGate, srcMapped, prev,
                                              /*twoState=*/false)
                    .getResult();
      } else {
        muxed = srcMapped;
      }
      wInfo->hwWireOp.setOperand(muxed);
    }
    ctx.pendingErase.push_back(xfer);
    return mlir::success();
  }
  return xfer.emitError(
      "transfer destination not a recognised output port or wire");
}

mlir::LogicalResult
lowerClockedTransferOp(nsl::dialect::ClockedTransferOp xfer,
                        ModuleLoweringCtx &ctx, mlir::OpBuilder &builder,
                        mlir::Value condGate) {
  // Find which reg the LHS refers to. The LHS's defining op should
  // have been a nsl.reg → seq.firreg (already mapped). We need the
  // register's RegInfo record.
  auto srcMapped = lookupValue(ctx, xfer.getSrc());
  // We need the nsl.reg's name. Walk the regs to find one whose
  // firreg/compreg result == lookupValue(ctx, xfer.getDst()).
  auto dstMapped = lookupValue(ctx, xfer.getDst());
  ModuleLoweringCtx::RegInfo *info = nullptr;
  for (auto &kv : ctx.regs) {
    if (kv.second.defaultPrev == dstMapped) {
      info = &kv.second;
      break;
    }
  }
  if (!info) {
    return xfer.emitError(
        "clocked_transfer destination is not a known nsl.reg");
  }
  // Compose the new pendingNext: if condGate, mux(condGate, srcMapped,
  // pendingNext); else override (unconditional => last write wins).
  if (condGate) {
    auto muxed = circt::comb::MuxOp::create(builder, xfer.getLoc(),
                                              condGate, srcMapped,
                                              info->pendingNext,
                                              /*twoState=*/false)
                     .getResult();
    info->pendingNext = muxed;
  } else {
    info->pendingNext = srcMapped;
  }
  ctx.pendingErase.push_back(xfer);
  return mlir::success();
}

//===----------------------------------------------------------------------===//
// Control family: nsl.if / nsl.alt / nsl.any / nsl.case / nsl.default.
//
// `nsl.if` over wire LHS → comb.mux. Over reg LHS → mux-on-data via
// the RegInfo pendingNext chain (Q3 → A).
// `nsl.alt` priority chain → nested comb.mux (right-associative).
// `nsl.any` parallel → per-target comb.or of comb.mux envelopes (S13
// parallel).
//===----------------------------------------------------------------------===//

mlir::LogicalResult lowerControlOp(mlir::Operation *op, ModuleLoweringCtx &ctx,
                                   mlir::OpBuilder &builder,
                                   mlir::Value condGate);

/// Combine two i1 conditions: condGate AND extraCond (fresh comb.and).
mlir::Value andConds(mlir::OpBuilder &builder, mlir::Location loc,
                     mlir::Value condGate, mlir::Value extra) {
  if (!condGate) {
    return extra;
  }
  return circt::comb::AndOp::create(builder, loc, condGate, extra)
      .getResult();
}

mlir::LogicalResult lowerIfOp(nsl::dialect::IfOp ifOp,
                              ModuleLoweringCtx &ctx,
                              mlir::OpBuilder &builder,
                              mlir::Value parentCondGate) {
  auto cond = lookupValue(ctx, ifOp.getCond());
  auto loc = ifOp.getLoc();
  auto thenCond = andConds(builder, loc, parentCondGate, cond);
  // Build a NOT-cond for else branch: cond == 0.
  mlir::Value notCond;
  {
    auto t = mlir::cast<mlir::IntegerType>(cond.getType());
    auto zAttr = mlir::IntegerAttr::get(t, 0);
    auto z =
        circt::hw::ConstantOp::create(builder, loc, zAttr).getResult();
    notCond = circt::comb::ICmpOp::create(builder, loc,
                                            circt::comb::ICmpPredicate::eq,
                                            cond, z, /*twoState=*/false)
                  .getResult();
  }
  auto elseCond = andConds(builder, loc, parentCondGate, notCond);
  // Lower the then-region under thenCond gate.
  for (auto &child : llvm::make_early_inc_range(ifOp.getThenRegion().front())) {
    if (mlir::failed(lowerControlOp(&child, ctx, builder, thenCond))) {
      return mlir::failure();
    }
  }
  // Lower the else-region under elseCond gate.
  for (auto &child : llvm::make_early_inc_range(ifOp.getElseRegion().front())) {
    if (mlir::failed(lowerControlOp(&child, ctx, builder, elseCond))) {
      return mlir::failure();
    }
  }
  ctx.pendingErase.push_back(ifOp);
  return mlir::success();
}

mlir::LogicalResult lowerAltOp(nsl::dialect::AltOp altOp,
                               ModuleLoweringCtx &ctx,
                               mlir::OpBuilder &builder,
                               mlir::Value parentCondGate) {
  // Priority semantics: case A wins over B over default. Each case's
  // body runs only if all prior cases' conditions failed AND its own
  // condition holds. We accumulate `coveredSoFar` (i1 — at least one
  // earlier case fired); each new case fires when (NOT coveredSoFar)
  // AND case-cond.
  auto loc = altOp.getLoc();
  mlir::Value coveredSoFar;
  for (auto &child : llvm::make_early_inc_range(altOp.getBody().front())) {
    if (auto caseOp = llvm::dyn_cast<nsl::dialect::CaseOp>(&child)) {
      auto caseCond = lookupValue(ctx, caseOp.getCond());
      // notCovered = !coveredSoFar (or 1 if no prior).
      mlir::Value notCovered;
      if (!coveredSoFar) {
        auto i1 = builder.getI1Type();
        auto oneAttr = mlir::IntegerAttr::get(i1, 1);
        notCovered =
            circt::hw::ConstantOp::create(builder, loc, oneAttr).getResult();
      } else {
        auto i1 = builder.getI1Type();
        auto zAttr = mlir::IntegerAttr::get(i1, 0);
        auto zero =
            circt::hw::ConstantOp::create(builder, loc, zAttr).getResult();
        notCovered = circt::comb::ICmpOp::create(
                          builder, loc, circt::comb::ICmpPredicate::eq,
                          coveredSoFar, zero, /*twoState=*/false)
                        .getResult();
      }
      auto fires =
          circt::comb::AndOp::create(builder, loc, notCovered, caseCond)
              .getResult();
      auto gated = andConds(builder, loc, parentCondGate, fires);
      // Lower case body under `gated`.
      for (auto &caseChild :
           llvm::make_early_inc_range(caseOp.getBody().front())) {
        if (mlir::failed(lowerControlOp(&caseChild, ctx, builder, gated))) {
          return mlir::failure();
        }
      }
      // Update coveredSoFar.
      if (!coveredSoFar) {
        coveredSoFar = caseCond;
      } else {
        coveredSoFar =
            circt::comb::OrOp::create(builder, loc, coveredSoFar, caseCond)
                .getResult();
      }
      ctx.pendingErase.push_back(caseOp);
    } else if (auto defOp = llvm::dyn_cast<nsl::dialect::DefaultOp>(&child)) {
      // Default fires when no case did (i.e., NOT coveredSoFar).
      mlir::Value gated;
      if (!coveredSoFar) {
        gated = parentCondGate; // entire default executes
      } else {
        auto i1 = builder.getI1Type();
        auto zAttr = mlir::IntegerAttr::get(i1, 0);
        auto zero =
            circt::hw::ConstantOp::create(builder, loc, zAttr).getResult();
        auto notCov = circt::comb::ICmpOp::create(
                            builder, loc, circt::comb::ICmpPredicate::eq,
                            coveredSoFar, zero, /*twoState=*/false)
                          .getResult();
        gated = andConds(builder, loc, parentCondGate, notCov);
      }
      for (auto &defChild :
           llvm::make_early_inc_range(defOp.getBody().front())) {
        if (mlir::failed(lowerControlOp(&defChild, ctx, builder, gated))) {
          return mlir::failure();
        }
      }
      ctx.pendingErase.push_back(defOp);
    }
  }
  ctx.pendingErase.push_back(altOp);
  return mlir::success();
}

mlir::LogicalResult lowerAnyOp(nsl::dialect::AnyOp anyOp,
                               ModuleLoweringCtx &ctx,
                               mlir::OpBuilder &builder,
                               mlir::Value parentCondGate) {
  // Parallel: every matching case fires independently. Lower each
  // case body under its own condition gate; no priority chain.
  auto loc = anyOp.getLoc();
  for (auto &child : llvm::make_early_inc_range(anyOp.getBody().front())) {
    if (auto caseOp = llvm::dyn_cast<nsl::dialect::CaseOp>(&child)) {
      auto caseCond = lookupValue(ctx, caseOp.getCond());
      auto gated = andConds(builder, loc, parentCondGate, caseCond);
      for (auto &caseChild :
           llvm::make_early_inc_range(caseOp.getBody().front())) {
        if (mlir::failed(lowerControlOp(&caseChild, ctx, builder, gated))) {
          return mlir::failure();
        }
      }
      ctx.pendingErase.push_back(caseOp);
    } else if (auto defOp = llvm::dyn_cast<nsl::dialect::DefaultOp>(&child)) {
      // Default in an `any` always fires (parallel semantics).
      for (auto &defChild :
           llvm::make_early_inc_range(defOp.getBody().front())) {
        if (mlir::failed(lowerControlOp(&defChild, ctx, builder,
                                          parentCondGate))) {
          return mlir::failure();
        }
      }
      ctx.pendingErase.push_back(defOp);
    }
  }
  ctx.pendingErase.push_back(anyOp);
  return mlir::success();
}

/// Disambiguate nsl.call: proc-target → handled by Phase 5 FSM
/// pre-pass (skipped here); func_in-target → inline + valid-wire.
mlir::LogicalResult lowerCallOp(nsl::dialect::CallOp callOp,
                                ModuleLoweringCtx &ctx,
                                mlir::OpBuilder &builder,
                                mlir::Value condGate) {
  // Phase-5 pre-pass already disambiguated and (for proc-targets)
  // emitted fsm.transitions. Any nsl.call left here MUST be a
  // func_in target. We inline by: materialising a 1-bit hw.wire
  // named "<func>_valid" driven by `condGate` (or constant 1 if
  // unconditional). The actual func body's transfers were lowered
  // when we visited the nsl.func; this op is a marker.
  auto loc = callOp.getLoc();
  auto i1 = builder.getI1Type();
  mlir::Value validSrc;
  if (condGate) {
    validSrc = condGate;
  } else {
    auto oneAttr = mlir::IntegerAttr::get(i1, 1);
    validSrc =
        circt::hw::ConstantOp::create(builder, loc, oneAttr).getResult();
  }
  auto validName =
      builder.getStringAttr((callOp.getCallee().str() + "_valid"));
  auto wire = circt::hw::WireOp::create(builder, loc, validSrc,
                                          /*name=*/validName,
                                          /*innerSym=*/
                                          circt::hw::InnerSymAttr{});
  (void)wire;
  ctx.pendingErase.push_back(callOp);
  return mlir::success();
}

mlir::LogicalResult lowerControlOp(mlir::Operation *op, ModuleLoweringCtx &ctx,
                                   mlir::OpBuilder &builder,
                                   mlir::Value condGate) {
  // Set insertion point to BEFORE the OUTERMOST nsl::* container the
  // op lives inside, so that CIRCT ops we generate end up in
  // hw.module body — NOT inside soon-to-be-erased nsl region bodies
  // (alt/any/case/default/if).
  mlir::OpBuilder::InsertionGuard g(builder);
  mlir::Operation *insertAnchor = op;
  for (auto *p = op->getParentOp(); p; p = p->getParentOp()) {
    if (llvm::isa<nsl::dialect::AltOp, nsl::dialect::AnyOp,
                  nsl::dialect::IfOp, nsl::dialect::CaseOp,
                  nsl::dialect::DefaultOp, nsl::dialect::ParallelOp,
                  nsl::dialect::SeqOp, nsl::dialect::FuncOp>(p)) {
      insertAnchor = p;
    } else {
      break;
    }
  }
  builder.setInsertionPoint(insertAnchor);

  // Inputs / outputs / port info: already mapped, skip.
  if (llvm::isa<nsl::dialect::InputPortOp, nsl::dialect::OutputPortOp,
                nsl::dialect::InoutPortOp>(op)) {
    return mlir::success();
  }
  // Constants nested inside control flow: lower to hw.constant inline.
  if (auto cst = llvm::dyn_cast<nsl::dialect::ConstantOp>(op)) {
    auto resultType = bitsToInteger(cst.getResult().getType());
    auto valueAttr = mlir::IntegerAttr::get(resultType, cst.getValue());
    auto hwCst =
        circt::hw::ConstantOp::create(builder, cst.getLoc(), valueAttr);
    ctx.valueMap[cst.getResult()] = hwCst.getResult();
    ctx.pendingErase.push_back(op);
    return mlir::success();
  }
  // Arith / bit-op family.
  if (llvm::isa<nsl::dialect::AddOp, nsl::dialect::SubOp,
                nsl::dialect::MulOp, nsl::dialect::AndOp, nsl::dialect::OrOp,
                nsl::dialect::XorOp, nsl::dialect::ShlOp, nsl::dialect::ShrOp,
                nsl::dialect::EqOp, nsl::dialect::NeOp, nsl::dialect::LtOp,
                nsl::dialect::LeOp, nsl::dialect::GtOp, nsl::dialect::GeOp,
                nsl::dialect::LandOp, nsl::dialect::LorOp>(op)) {
    return lowerArithOp(op, ctx, builder);
  }
  if (llvm::isa<nsl::dialect::NotOp, nsl::dialect::NegOp,
                nsl::dialect::LnotOp, nsl::dialect::ReduceAndOp,
                nsl::dialect::ReduceOrOp, nsl::dialect::ReduceXorOp,
                nsl::dialect::SignExtendOp, nsl::dialect::ZeroExtendOp,
                nsl::dialect::MuxOp, nsl::dialect::ConcatOp,
                nsl::dialect::ExtractOp, nsl::dialect::RepeatOp>(op)) {
    return lowerBitOp(op, ctx, builder);
  }
  // Transfer ops.
  if (auto xfer = llvm::dyn_cast<nsl::dialect::TransferOp>(op)) {
    return lowerTransferOp(xfer, ctx, builder, condGate);
  }
  if (auto cxfer = llvm::dyn_cast<nsl::dialect::ClockedTransferOp>(op)) {
    return lowerClockedTransferOp(cxfer, ctx, builder, condGate);
  }
  // Control-flow ops.
  if (auto ifOp = llvm::dyn_cast<nsl::dialect::IfOp>(op)) {
    return lowerIfOp(ifOp, ctx, builder, condGate);
  }
  if (auto altOp = llvm::dyn_cast<nsl::dialect::AltOp>(op)) {
    return lowerAltOp(altOp, ctx, builder, condGate);
  }
  if (auto anyOp = llvm::dyn_cast<nsl::dialect::AnyOp>(op)) {
    return lowerAnyOp(anyOp, ctx, builder, condGate);
  }
  if (auto callOp = llvm::dyn_cast<nsl::dialect::CallOp>(op)) {
    return lowerCallOp(callOp, ctx, builder, condGate);
  }
  // Parallel: a transparent par-block. Lower its children in order.
  if (auto par = llvm::dyn_cast<nsl::dialect::ParallelOp>(op)) {
    for (auto &c :
         llvm::make_early_inc_range(par.getBody().front())) {
      if (mlir::failed(lowerControlOp(&c, ctx, builder, condGate))) {
        return mlir::failure();
      }
    }
    ctx.pendingErase.push_back(par);
    return mlir::success();
  }
  // Func body: lower its body's transfers. Func ops live alongside
  // module body; we inline their transfers into the module body so
  // they participate in the outer mux chain. Func's symbol lives on
  // for callee-lookup.
  if (auto fn = llvm::dyn_cast<nsl::dialect::FuncOp>(op)) {
    for (auto &c :
         llvm::make_early_inc_range(fn.getBody().front())) {
      if (mlir::failed(lowerControlOp(&c, ctx, builder, condGate))) {
        return mlir::failure();
      }
    }
    ctx.pendingErase.push_back(fn);
    return mlir::success();
  }
  return mlir::success();
}

//===----------------------------------------------------------------------===//
// Sim family: nsl.sim_init / nsl.sim_display / nsl.sim_finish /
// nsl.sim_delay → sv.{initial,fwrite,finish,verbatim} inside per-
// module sv.ifdef "SIMULATION".
//===----------------------------------------------------------------------===//

mlir::LogicalResult lowerSimInitOp(nsl::dialect::SimInitOp simInit,
                                   ModuleLoweringCtx &ctx,
                                   mlir::OpBuilder &builder);

mlir::LogicalResult lowerSimOpsInside(mlir::Block &block, ModuleLoweringCtx &ctx,
                                       mlir::OpBuilder &builder);

mlir::LogicalResult lowerSimDisplayOp(nsl::dialect::SimDisplayOp dis,
                                      ModuleLoweringCtx &ctx,
                                      mlir::OpBuilder &builder) {
  auto loc = dis.getLoc();
  auto i32 = builder.getI32Type();
  auto fdAttr = mlir::IntegerAttr::get(i32, 1); // stdout
  auto fd = circt::hw::ConstantOp::create(builder, loc, fdAttr).getResult();
  llvm::SmallVector<mlir::Value, 4> mapped;
  for (auto v : dis.getArgs()) {
    mapped.push_back(lookupValue(ctx, v));
  }
  circt::sv::FWriteOp::create(builder, loc, fd,
                                builder.getStringAttr(dis.getFormat()),
                                mapped);
  ctx.pendingErase.push_back(dis);
  return mlir::success();
}

mlir::LogicalResult lowerSimFinishOp(nsl::dialect::SimFinishOp fin,
                                     ModuleLoweringCtx &ctx,
                                     mlir::OpBuilder &builder) {
  auto loc = fin.getLoc();
  auto i8 = builder.getI8Type();
  auto behAttr = mlir::IntegerAttr::get(i8, 1); // typical behavior code
  circt::sv::FinishOp::create(builder, loc, behAttr);
  ctx.pendingErase.push_back(fin);
  return mlir::success();
}

mlir::LogicalResult lowerSimDelayOp(nsl::dialect::SimDelayOp del,
                                    ModuleLoweringCtx &ctx,
                                    mlir::OpBuilder &builder) {
  auto loc = del.getLoc();
  std::string text = ("#" + llvm::Twine(del.getCycles()) + ";").str();
  circt::sv::VerbatimOp::create(builder, loc, builder.getStringAttr(text));
  ctx.pendingErase.push_back(del);
  return mlir::success();
}

mlir::LogicalResult lowerSimOpsInside(mlir::Block &block,
                                       ModuleLoweringCtx &ctx,
                                       mlir::OpBuilder &builder) {
  for (auto &op : llvm::make_early_inc_range(block)) {
    if (auto dis = llvm::dyn_cast<nsl::dialect::SimDisplayOp>(&op)) {
      if (mlir::failed(lowerSimDisplayOp(dis, ctx, builder))) {
        return mlir::failure();
      }
    } else if (auto fin = llvm::dyn_cast<nsl::dialect::SimFinishOp>(&op)) {
      if (mlir::failed(lowerSimFinishOp(fin, ctx, builder))) {
        return mlir::failure();
      }
    } else if (auto del = llvm::dyn_cast<nsl::dialect::SimDelayOp>(&op)) {
      if (mlir::failed(lowerSimDelayOp(del, ctx, builder))) {
        return mlir::failure();
      }
    }
  }
  return mlir::success();
}

mlir::LogicalResult lowerSimInitOp(nsl::dialect::SimInitOp simInit,
                                   ModuleLoweringCtx &ctx,
                                   mlir::OpBuilder &builder) {
  auto loc = simInit.getLoc();
  // Get-or-build the per-module SIMULATION ifdef.
  auto ifdef = getOrBuildSimIfDef(ctx, builder, loc);
  // Get/create the sv.initial inside the ifdef body.
  mlir::OpBuilder::InsertionGuard g(builder);
  builder.setInsertionPointToEnd(ifdef.getThenBlock());
  auto initialOp = circt::sv::InitialOp::create(builder, loc);
  if (initialOp.getBody().empty()) {
    initialOp.getBody().emplaceBlock();
  }
  builder.setInsertionPointToStart(initialOp.getBodyBlock());
  // Lower each child sim op inside the initial block.
  if (mlir::failed(
          lowerSimOpsInside(simInit.getBody().front(), ctx, builder))) {
    return mlir::failure();
  }
  ctx.pendingErase.push_back(simInit);
  return mlir::success();
}

/// Lower a top-level (module-direct) sim op into the sim ifdef.
mlir::LogicalResult lowerTopLevelSimOp(mlir::Operation *op,
                                        ModuleLoweringCtx &ctx,
                                        mlir::OpBuilder &builder) {
  auto loc = op->getLoc();
  // sim_display / sim_finish / sim_delay at module top-level go
  // inside the ifdef directly (as procedural ops they'd need an
  // initial wrapper, but at top level we wrap them too).
  auto ifdef = getOrBuildSimIfDef(ctx, builder, loc);
  mlir::OpBuilder::InsertionGuard g(builder);
  builder.setInsertionPointToEnd(ifdef.getThenBlock());
  auto initialOp = circt::sv::InitialOp::create(builder, loc);
  if (initialOp.getBody().empty()) {
    initialOp.getBody().emplaceBlock();
  }
  builder.setInsertionPointToStart(initialOp.getBodyBlock());
  if (auto dis = llvm::dyn_cast<nsl::dialect::SimDisplayOp>(op)) {
    return lowerSimDisplayOp(dis, ctx, builder);
  }
  if (auto fin = llvm::dyn_cast<nsl::dialect::SimFinishOp>(op)) {
    return lowerSimFinishOp(fin, ctx, builder);
  }
  if (auto del = llvm::dyn_cast<nsl::dialect::SimDelayOp>(op)) {
    return lowerSimDelayOp(del, ctx, builder);
  }
  return mlir::failure();
}

//===----------------------------------------------------------------------===//
// Body walk: orchestrate the full leaf-op lowering for a single
// nsl::ModuleOp body. Invoked as Step 4 of `lowerOneModule`.
//===----------------------------------------------------------------------===//

mlir::LogicalResult lowerBodyOps(mlir::Block &block, ModuleLoweringCtx &ctx,
                                 mlir::OpBuilder &builder) {
  for (auto &op : llvm::make_early_inc_range(block)) {
    // Port-info ops: leave in place during body walk. Their consumer
    // SSA values flow through `ctx.valueMap` (looked up by
    // `lookupValue`); the actual ops get erased at the end of
    // `lowerOneModule` once no nsl SSA references remain.
    if (llvm::isa<nsl::dialect::InputPortOp, nsl::dialect::OutputPortOp,
                  nsl::dialect::InoutPortOp>(op)) {
      continue;
    }
    if (auto cst = llvm::dyn_cast<nsl::dialect::ConstantOp>(&op)) {
      mlir::OpBuilder::InsertionGuard g(builder);
      builder.setInsertionPoint(&op);
      auto resultType = bitsToInteger(cst.getResult().getType());
      auto valueAttr = mlir::IntegerAttr::get(resultType, cst.getValue());
      auto hwCst =
          circt::hw::ConstantOp::create(builder, cst.getLoc(), valueAttr);
      ctx.valueMap[cst.getResult()] = hwCst.getResult();
      ctx.pendingErase.push_back(cst);
      continue;
    }
    if (auto reg = llvm::dyn_cast<nsl::dialect::RegOp>(&op)) {
      if (mlir::failed(lowerRegOp(reg, ctx, builder))) {
        return mlir::failure();
      }
      continue;
    }
    if (auto wire = llvm::dyn_cast<nsl::dialect::WireOp>(&op)) {
      if (mlir::failed(lowerWireOp(wire, ctx, builder))) {
        return mlir::failure();
      }
      continue;
    }
    if (auto mem = llvm::dyn_cast<nsl::dialect::MemOp>(&op)) {
      if (mlir::failed(lowerMemOp(mem, ctx, builder))) {
        return mlir::failure();
      }
      continue;
    }
    if (auto simInit = llvm::dyn_cast<nsl::dialect::SimInitOp>(&op)) {
      if (mlir::failed(lowerSimInitOp(simInit, ctx, builder))) {
        return mlir::failure();
      }
      continue;
    }
    if (llvm::isa<nsl::dialect::SimDisplayOp, nsl::dialect::SimFinishOp,
                  nsl::dialect::SimDelayOp>(op)) {
      if (mlir::failed(lowerTopLevelSimOp(&op, ctx, builder))) {
        return mlir::failure();
      }
      continue;
    }
    if (llvm::isa<nsl::dialect::FuncInOp, nsl::dialect::FuncOutOp,
                  nsl::dialect::FuncSelfOp>(op)) {
      // Control-terminal declarations: dropped at this layer.
      ctx.pendingErase.push_back(&op);
      continue;
    }
    // nsl.proc / nsl.func: leave for Phase 5 FSM pre-pass. We don't
    // lower them here — Phase 5 (lowerNSLProcsToFSMMachines, called
    // AFTER lowerNSLModulesToHWModules from `runOnOperation`) walks
    // them at the parent mlir.module level. nsl.func WITHOUT a
    // `nsl.seq` child — i.e., a combinational body — gets inlined
    // (transfers in its body lower the same way they would at the
    // module level). The disambiguation is by checking for a child
    // `nsl::SeqOp`.
    if (auto fn = llvm::dyn_cast<nsl::dialect::FuncOp>(&op)) {
      bool hasSeq = false;
      for (auto &c : fn.getBody().front()) {
        if (llvm::isa<nsl::dialect::SeqOp>(c)) {
          hasSeq = true;
          break;
        }
      }
      if (hasSeq) {
        // Leave for Phase 5.
        continue;
      }
      // Combinational func body: inline its body ops at the module
      // level so transfers participate in the outer mux chain. This
      // matches the inline `lowerControlOp` FuncOp arm (which
      // recursively descends).
      mlir::OpBuilder::InsertionGuard g(builder);
      builder.setInsertionPoint(&op);
      if (mlir::failed(lowerControlOp(&op, ctx, builder, mlir::Value{}))) {
        return mlir::failure();
      }
      continue;
    }
    if (llvm::isa<nsl::dialect::ProcOp>(op)) {
      // Leave for Phase 5.
      continue;
    }
    // Defer to lowerControlOp for: arith/bit-op, transfers, control
    // flow (if/alt/any/parallel), call, func-body inlining.
    mlir::OpBuilder::InsertionGuard g(builder);
    builder.setInsertionPoint(&op);
    if (mlir::failed(lowerControlOp(&op, ctx, builder, mlir::Value{}))) {
      return mlir::failure();
    }
  }
  return mlir::success();
}

/// Finalise the per-reg pendingNext: rewire the firreg / compreg's
/// data input to the final mux chain. If pendingNext is identical to
/// the placeholder reset value (no clocked_transfer touched the reg),
/// we leave it alone (idempotent reset = retain initial value).
///
/// SSA-dominance fix: after rewiring the data input, we need to make
/// sure the firreg op comes AFTER its `next` data definition in the
/// block. We move the firreg to immediately AFTER the defining op of
/// `pendingNext` (or leave it in place if pendingNext is a block-arg
/// or comes from an upstream op already preceding the firreg).
void finaliseRegs(ModuleLoweringCtx &ctx) {
  for (auto &kv : ctx.regs) {
    auto &info = kv.second;
    mlir::Operation *regOp = info.firRegOp ? info.firRegOp.getOperation()
                                              : info.compRegOp.getOperation();
    if (!regOp) {
      continue;
    }
    if (info.firRegOp) {
      info.firRegOp.setOperand(0 /*next*/, info.pendingNext);
    } else if (info.compRegOp) {
      info.compRegOp.setOperand(0 /*input*/, info.pendingNext);
    }
    // Move the firreg / compreg after the def of pendingNext (and
    // after ALL its ancestors' defs). The simplest approach: walk
    // the data-flow chain and find the latest-positioned defining
    // op in the same block; move regOp after it.
    //
    // Since most pendingNext values are produced by mux chains that
    // sit at the end of the body, moving regOp to just before the
    // hw.output terminator preserves dominance for both the pendingNext
    // chain AND the regOp's existing consumers (which are downstream
    // in the body's SSA order — output assignments etc., which
    // reference the reg's result via valueMap and which already had
    // their own ops emitted at later positions).
    auto *block = regOp->getBlock();
    if (!block) {
      continue;
    }
    auto *terminator = block->getTerminator();
    regOp->moveBefore(terminator);
  }
}

mlir::LogicalResult lowerOneModule(nsl::dialect::ModuleOp moduleOp,
                                   mlir::OpBuilder &builder) {
  mlir::Location loc = moduleOp.getLoc();
  mlir::MLIRContext *ctx_ = builder.getContext();
  (void)ctx_;

  // Step 1: paired declare + port list.
  nsl::dialect::DeclareOp declareOp = findPairedDeclare(moduleOp);
  llvm::SmallVector<circt::hw::PortInfo, 8> portInfos;
  buildPortInfo(declareOp, loc, portInfos);
  circt::hw::ModulePortInfo ports(portInfos);

  // Step 2: hw.module creation.
  auto parentModuleOp =
      llvm::cast<mlir::ModuleOp>(moduleOp->getParentOp());
  mlir::ArrayAttr declaredParams =
      collectInstanceParameters(parentModuleOp, builder);
  builder.setInsertionPoint(moduleOp);
  auto hwModuleOp = circt::hw::HWModuleOp::create(
      builder, loc, moduleOp.getSymNameAttr(), ports,
      /*parameters=*/declaredParams);

  // Step 3: input bindings + Phase 6 ctx.
  ModuleLoweringCtx ctx;
  ctx.hwModuleOp = hwModuleOp;
  bool ifaceClk = declareOp && declareOp.getInterfaceClock().has_value();
  bool ifaceRst = declareOp && declareOp.getInterfaceReset().has_value();
  ctx.hasInterface = ifaceClk && ifaceRst;
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

  // Resolve clk/rst_n block-args.
  if (declareOp) {
    if (ctx.hasInterface) {
      ctx.clkArg = lookupInputArg(*declareOp.getInterfaceClock());
      ctx.rstnArg = lookupInputArg(*declareOp.getInterfaceReset());
    } else {
      ctx.clkArg = lookupInputArg("clk");
      ctx.rstnArg = lookupInputArg("rst_n");
    }
  }

  // Pre-scan: register port mappings + output-port bookkeeping.
  for (auto &op : moduleOp.getBody().front()) {
    if (auto in = llvm::dyn_cast<nsl::dialect::InputPortOp>(op)) {
      mlir::Value blockArg = lookupInputArg(in.getName());
      if (!blockArg) {
        return moduleOp.emitError()
               << "in-module nsl.input_port \"" << in.getName()
               << "\" not in paired declare's port list";
      }
      ctx.valueMap[in.getResult()] = blockArg;
    } else if (auto out = llvm::dyn_cast<nsl::dialect::OutputPortOp>(op)) {
      ctx.outputPortValues.emplace_back(out.getResult(), out.getName());
    } else if (auto inout = llvm::dyn_cast<nsl::dialect::InoutPortOp>(op)) {
      mlir::Value blockArg = lookupInputArg(inout.getName());
      if (!blockArg) {
        return moduleOp.emitError()
               << "in-module nsl.inout_port \"" << inout.getName()
               << "\" not in paired declare's port list";
      }
      ctx.valueMap[inout.getResult()] = blockArg;
    }
  }

  // Step 4: walk body.
  builder.setInsertionPointToStart(hwModuleOp.getBodyBlock());

  // Snapshot ops in source order; we move/erase as we go.
  llvm::SmallVector<mlir::Operation *, 32> bodyOps;
  for (auto &op : moduleOp.getBody().front()) {
    bodyOps.push_back(&op);
  }
  // Move all ops into hw.module body in source order (for proper
  // SSA use-def while we lower).
  auto *terminator = hwModuleOp.getBodyBlock()->getTerminator();
  for (auto *op : bodyOps) {
    op->moveBefore(terminator);
  }

  mlir::ArrayAttr instanceParams =
      collectInstanceParameters(parentModuleOp, builder);
  // Pre-process: handle submodule first (these don't depend on any
  // body data values; defer them to be sequentially handled).
  for (auto &op :
       llvm::make_early_inc_range(*hwModuleOp.getBodyBlock())) {
    if (auto sub = llvm::dyn_cast<nsl::dialect::SubmoduleOp>(&op)) {
      auto target =
          parentModuleOp.lookupSymbol<circt::hw::HWModuleOp>(
              sub.getTemplateRef());
      if (!target) {
        sub.emitError()
            << "submodule target @" << sub.getTemplateRef()
            << " not yet lowered to hw.module";
        return mlir::failure();
      }
      mlir::OpBuilder::InsertionGuard g(builder);
      builder.setInsertionPoint(&op);
      llvm::SmallVector<mlir::Value, 0> emptyInputs;
      circt::hw::InstanceOp::create(
          builder, sub.getLoc(), target.getOperation(),
          sub.getSymNameAttr(),
          llvm::ArrayRef<mlir::Value>(emptyInputs), instanceParams);
      sub.erase();
    }
  }

  // Lower the body (transfers + arith + state + control + sim).
  if (mlir::failed(lowerBodyOps(*hwModuleOp.getBodyBlock(), ctx, builder))) {
    return mlir::failure();
  }

  // Finalise reg data inputs.
  finaliseRegs(ctx);

  // Step 5: hw.output with collected outputs.
  llvm::SmallVector<mlir::Value, 4> outputOperands;
  for (auto &p : ports.getOutputs()) {
    mlir::Value v;
    for (auto &kv : ctx.outputAssignments) {
      if (kv.first == p.name.getValue()) {
        v = kv.second;
        break;
      }
    }
    if (!v) {
      auto zero = mlir::IntegerAttr::get(p.type, 0);
      auto cst = circt::hw::ConstantOp::create(builder, loc, zero);
      v = cst.getResult();
    }
    outputOperands.push_back(v);
  }
  builder.setInsertionPoint(terminator);
  circt::hw::OutputOp::create(builder, loc, outputOperands);
  terminator->erase();

  // Step 5b: erase leftover port-info markers in the new hw.module
  // body. We deferred their removal until all transfers + conditional
  // body walk completed (so that lookupValue could resolve the
  // input-port SSA values). At this point no transfer uses these
  // markers anymore; safe to erase.
  for (auto &op : *hwModuleOp.getBodyBlock()) {
    if (llvm::isa<nsl::dialect::InputPortOp, nsl::dialect::OutputPortOp,
                  nsl::dialect::InoutPortOp>(op)) {
      ctx.pendingErase.push_back(&op);
    }
  }

  // Step 5c: drain pendingErase. Strategy: filter out nested ops
  // (those whose parent op is also in pendingErase). Erasing a
  // container op transitively wipes its body's ops (including any
  // we lowered + tried to erase individually); double-erase is UB.
  llvm::SmallPtrSet<mlir::Operation *, 32> queuedSet;
  for (auto *op : ctx.pendingErase) {
    queuedSet.insert(op);
  }
  llvm::SmallVector<mlir::Operation *, 16> topLevelToErase;
  for (auto *op : ctx.pendingErase) {
    bool nested = false;
    for (auto *p = op->getParentOp(); p; p = p->getParentOp()) {
      if (queuedSet.contains(p)) {
        nested = true;
        break;
      }
    }
    if (!nested) {
      topLevelToErase.push_back(op);
    }
  }
  // dropAllUses on EVERY queued op (so cross-container references go
  // away — e.g., a CIRCT op outside the alt referencing a Value
  // defined inside the alt).
  for (auto *op : ctx.pendingErase) {
    op->dropAllUses();
  }
  // Erase only the topLevel ones; they cascade to their nested
  // children.
  for (auto *op : topLevelToErase) {
    op->erase();
  }

  // Step 6: erase paired declare + nsl.module.
  if (declareOp) {
    declareOp.erase();
  }
  moduleOp.erase();
  return mlir::success();
}

} // namespace

mlir::LogicalResult
lowerNSLModulesToHWModules(mlir::ModuleOp parentModule) {
  mlir::OpBuilder builder(parentModule);

  llvm::SmallVector<nsl::dialect::ModuleOp, 4> nslModules;
  for (auto m : parentModule.getOps<nsl::dialect::ModuleOp>()) {
    nslModules.push_back(m);
  }
  for (auto m : nslModules) {
    if (mlir::failed(lowerOneModule(m, builder))) {
      return mlir::failure();
    }
  }

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
  // M6 lowering is performed by the manual structural pre-pass
  // `lowerNSLModulesToHWModules` (called from
  // `NSLToCIRCTPass::runOnOperation` BEFORE applyFullConversion).
  // No DialectConversion patterns are registered here. See file
  // header for rationale.
}

} // namespace nsl::lower
