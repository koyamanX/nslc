// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Dialect/NSL/IR/NSLOps.td — TableGen records for the 41
// `nsl.*` ops (M4, layer 7, post-Q6).
//
// **Specification anchors**:
//   - `specs/007-m4-mlir-dialect/spec.md` FR-009 / FR-010 — full op
//     set table (41 named ops + auto-generated terminators).
//   - `specs/007-m4-mlir-dialect/data-model.md` §2 — per-op trait set
//     and verifier-implementation style (TableGen-trait-only vs
//     hand-written body per Q2 Option B).
//   - `docs/design/nsl_compiler_design.md` §§7–10 — design-doc anchor
//     for every op record.
//
// Each op carries `hasVerifier = 1` if data-model §2's "Verifier
// style" column lists "hand-written" — Phase 4 (US2, T100–T118) fills
// the bodies. At Phase 3 (US1) the bodies are empty stubs (return
// success); the round-trip fixtures don't exercise them.

#ifndef NSL_OPS_TD
#define NSL_OPS_TD

include "mlir/IR/AttrTypeBase.td"
include "mlir/IR/BuiltinAttributes.td"
include "mlir/IR/CommonAttrConstraints.td"
include "mlir/IR/CommonTypeConstraints.td"
include "mlir/IR/EnumAttr.td"
include "mlir/IR/OpBase.td"
include "mlir/IR/SymbolInterfaces.td"

include "NSLDialect.td"
include "NSLTypes.td"

//===----------------------------------------------------------------------===//
// Type-constraint convenience defs
//===----------------------------------------------------------------------===//

def NSL_AnyBits : DialectType<NSL_Dialect,
    CPred<"::llvm::isa<::nsl::dialect::BitsType>($_self)">,
    "an `!nsl.bits<N>` type", "::mlir::Type">;

def NSL_AnyStruct : DialectType<NSL_Dialect,
    CPred<"::llvm::isa<::nsl::dialect::StructType>($_self)">,
    "an `!nsl.struct<@T>` type", "::mlir::Type">;

def NSL_AnyMem : DialectType<NSL_Dialect,
    CPred<"::llvm::isa<::nsl::dialect::MemType>($_self)">,
    "an `!nsl.mem<[D x T]>` type", "::mlir::Type">;

// `!nsl.bits<N>` OR `!nsl.struct<@T>` (the value-bearing types).
def NSL_BitsOrStruct : AnyTypeOf<[NSL_AnyBits, NSL_AnyStruct]>;

//===----------------------------------------------------------------------===//
// `nsl.incdec` kind enum (per spec FR-013 + Phase 3 fixture)
//===----------------------------------------------------------------------===//

def IncDecKindPreInc : I32EnumAttrCase<"PreInc", 0, "pre_inc">;
def IncDecKindPostInc : I32EnumAttrCase<"PostInc", 1, "post_inc">;
def IncDecKindPreDec : I32EnumAttrCase<"PreDec", 2, "pre_dec">;
def IncDecKindPostDec : I32EnumAttrCase<"PostDec", 3, "post_dec">;

def IncDecKindEnum : I32EnumAttr<"IncDecKind",
    "Pre/post increment/decrement variant for `nsl.incdec`",
    [IncDecKindPreInc, IncDecKindPostInc,
     IncDecKindPreDec, IncDecKindPostDec]> {
  let cppNamespace = "::nsl::dialect";
  // Suppress I32EnumAttr's auto-generated `IncDecKindAttr` wrapper
  // class (NSLOpsEnums.h.inc) so the dialect-style `EnumAttr<>` def
  // below can claim that name in NSLOpsAttrDefs.h.inc without
  // collision. This is the standard MLIR-modern pattern (see
  // mlir/include/mlir/IR/EnumAttr.td) for enums that want the
  // `#dialect<mnemonic value>` print form rather than the integer
  // form. The Phase 3 fixture `incdec_roundtrip.mlir` uses
  // `#nsl<incdec_kind pre_inc>`, which requires the dialect-attr
  // form.
  let genSpecializedAttr = 0;
}

def IncDecKindAttr : EnumAttr<NSL_Dialect, IncDecKindEnum, "incdec_kind">;

//===----------------------------------------------------------------------===//
// 2.1 Module-level ops (4): nsl.module, nsl.struct, nsl.submodule, nsl.connect
//===----------------------------------------------------------------------===//

def NSL_ModuleOp : NSL_Op<"module", [
    Symbol, SymbolTable, NoTerminator, SingleBlock,
    HasParent<"::mlir::ModuleOp">]> {
  let summary = "An NSL module — the unit of synthesizable hardware.";
  let description = [{
    `nsl.module @M { ... }` carries a single region whose body holds
    the module's storage (`nsl.reg`, `nsl.wire`, `nsl.mem`,
    `nsl.variable`), structural-typed children (`nsl.struct`,
    `nsl.submodule`), control terminals (`nsl.func_in` /
    `nsl.func_out` / `nsl.func_self`), procedures (`nsl.proc`,
    `nsl.func`), and atomic / system-task ops at module scope.
  }];
  let arguments = (ins SymbolNameAttr:$sym_name);
  let regions = (region SizedRegion<1>:$body);
  let assemblyFormat = "$sym_name attr-dict-with-keyword $body";
  let hasVerifier = 1;
}

def NSL_StructOp : NSL_Op<"struct", [
    Symbol, NoTerminator, SingleBlock,
    HasParent<"ModuleOp">]> {
  let summary = "A structural-type declaration (children are `nsl.field_decl` ops).";
  let description = [{
    `nsl.struct @S { nsl.field_decl "a" : !nsl.bits<4> ... }` declares
    a structural type. Per Q6 Option B, the struct-internal field
    declarations use `nsl.field_decl`; the access-marker form
    (`nsl.field`) is split off into the marker category.
  }];
  let arguments = (ins SymbolNameAttr:$sym_name);
  let regions = (region SizedRegion<1>:$body);
  let assemblyFormat = "$sym_name attr-dict-with-keyword $body";
  let hasVerifier = 1;
}

def NSL_SubmoduleOp : NSL_Op<"submodule", [
    Symbol, HasParent<"ModuleOp">]> {
  let summary = "Instantiate a sibling `nsl.module` as a child instance.";
  let description = [{
    `nsl.submodule @u_inner : @Inner` creates a structural instance
    `u_inner` of the sibling-module `@Inner`. Both operands are
    symbolic — the parser resolves the template via MLIR's
    `SymbolRefAttr` machinery.
  }];
  let arguments = (ins
    SymbolNameAttr:$sym_name,
    // NOTE: rename `$template` → `$templateRef`; `template` is a
    // C++ reserved keyword and TableGen emits per-attribute getter/
    // setter / property-storage members that fail to compile when
    // the attribute name collides with reserved keywords.
    FlatSymbolRefAttr:$templateRef);
  let assemblyFormat = "$sym_name `:` $templateRef attr-dict";
}

def NSL_ConnectOp : NSL_Op<"connect", [
    HasParent<"ModuleOp">, SameTypeOperands]> {
  let summary = "Wire one structural value to another (strict type equality).";
  let description = [{
    `nsl.connect %dst, %src : !nsl.bits<8>` wires the source value to
    the destination. Per Q3 Option A, both operands MUST share a
    pointer-equal `mlir::Type` — M5 lowering inserts an explicit
    `nsl.struct_cast` at any user-written struct↔bits cast site;
    the dialect verifier never tolerates implicit reinterpretation.
  }];
  let arguments = (ins NSL_BitsOrStruct:$dst, NSL_BitsOrStruct:$src);
  let assemblyFormat = "$dst `,` $src `:` type($dst) attr-dict";
  let hasVerifier = 1;
}

//===----------------------------------------------------------------------===//
// 2.2 Storage ops (4): nsl.reg, nsl.wire, nsl.variable, nsl.mem
//===----------------------------------------------------------------------===//

def NSL_RegOp : NSL_Op<"reg", [
    ParentOneOf<["ModuleOp", "ProcOp"]>]> {
  let summary = "An NSL register (clocked storage).";
  let description = [{
    `nsl.reg "name" : !nsl.bits<N> = init` (or `!nsl.struct<@T>`).
    The `init` attribute is the register's reset value; per FR-013
    the result type is one of `!nsl.bits<N>` or `!nsl.struct<@T>`.
  }];
  let arguments = (ins
    StrAttr:$name,
    OptionalAttr<I64Attr>:$init);
  let results = (outs NSL_BitsOrStruct:$result);
  let assemblyFormat = [{
    $name `:` type($result) (`=` $init^)? attr-dict
  }];
  let hasVerifier = 1;
}

def NSL_WireOp : NSL_Op<"wire", [HasParent<"ModuleOp">]> {
  let summary = "An NSL wire (combinational signal).";
  let description = [{
    `nsl.wire "name" : !nsl.bits<N>`. Per FR-013, wires never carry a
    `!nsl.struct<@T>` result type.
  }];
  let arguments = (ins StrAttr:$name);
  let results = (outs NSL_AnyBits:$result);
  let assemblyFormat = "$name `:` type($result) attr-dict";
  let hasVerifier = 1;
}

def NSL_VariableOp : NSL_Op<"variable", [
    ParentOneOf<["ModuleOp", "FuncOp"]>]> {
  let summary = "An NSL variable (local mutable scalar).";
  let description = [{
    `nsl.variable "name" : !nsl.bits<N>`. Per FR-013, variables carry
    `!nsl.bits<N>` only.
  }];
  let arguments = (ins StrAttr:$name);
  let results = (outs NSL_AnyBits:$result);
  let assemblyFormat = "$name `:` type($result) attr-dict";
  let hasVerifier = 1;
}

def NSL_MemOp : NSL_Op<"mem", [HasParent<"ModuleOp">]> {
  let summary = "An NSL memory.";
  let description = [{
    `nsl.mem "name" : !nsl.mem<[D x T]>`.
  }];
  let arguments = (ins StrAttr:$name);
  let results = (outs NSL_AnyMem:$result);
  let assemblyFormat = "$name `:` type($result) attr-dict";
  let hasVerifier = 1;
}

//===----------------------------------------------------------------------===//
// 2.3 Control-terminal ops (3): nsl.func_in, nsl.func_out, nsl.func_self
//===----------------------------------------------------------------------===//

// Phase 4 SYN finding (carry-over from Phase 3 fixture authoring): the
// Phase 3 fixtures spelled `nsl.func_in "do"(%a, %b) : !nsl.bits<8>`
// — i.e. a single `:` followed by what was meant to be the return
// type. That spelling is unparseable for MLIR's variadic-operand
// printer/parser without a hand-written `parse`/`print` pair, because
// variadic operand types must be supplied textually. Phase 4 picks
// MLIR's standard `functional-type($args, $result)` form
// (`(<arg_types>) -> <ret_type>` — matches `func.call`, `func.func`,
// etc.) and updates the three fixtures `func_in_roundtrip.mlir`,
// `func_out_roundtrip.mlir`, `func_self_roundtrip.mlir` accordingly.
// Reported in the Phase 4 hand-off as SYN-4.

def NSL_FuncInOp : NSL_Op<"func_in", [HasParent<"ModuleOp">]> {
  let summary = "Declare an NSL `func_in` control terminal (input port).";
  let description = [{
    `nsl.func_in "do"(%a, %b) : (!nsl.bits<8>, !nsl.bits<8>) -> !nsl.bits<8>`
    declares a control terminal taking the listed argument values;
    optional return type follows the `->`.
  }];
  let arguments = (ins
    StrAttr:$name,
    Variadic<NSL_BitsOrStruct>:$args);
  let results = (outs Optional<NSL_BitsOrStruct>:$result);
  let assemblyFormat = [{
    $name `(` $args `)` attr-dict `:` functional-type($args, results)
  }];
}

def NSL_FuncOutOp : NSL_Op<"func_out", [HasParent<"ModuleOp">]> {
  let summary = "Declare an NSL `func_out` control terminal (output port).";
  let description = [{
    `nsl.func_out "done"(%r) : (!nsl.bits<8>) -> ()` declares an
    output-port control terminal carrying zero or more arguments.
  }];
  let arguments = (ins
    StrAttr:$name,
    Variadic<NSL_BitsOrStruct>:$args);
  let assemblyFormat = [{
    $name `(` $args `)` attr-dict `:` functional-type($args, results)
  }];
}

def NSL_FuncSelfOp : NSL_Op<"func_self", [HasParent<"ModuleOp">]> {
  let summary = "Declare an NSL `func_self` control terminal.";
  let description = [{
    `nsl.func_self "fire"(%w) : (!nsl.bits<1>) -> ()` declares a
    self-firing control terminal carrying zero or more arguments.
  }];
  let arguments = (ins
    StrAttr:$name,
    Variadic<NSL_BitsOrStruct>:$args);
  let assemblyFormat = [{
    $name `(` $args `)` attr-dict `:` functional-type($args, results)
  }];
}

//===----------------------------------------------------------------------===//
// 2.4 Action-block ops (7): alt, any, if, parallel, seq, while, for
//===----------------------------------------------------------------------===//

def NSL_AltOp : NSL_Op<"alt", [NoTerminator, SingleBlock]> {
  let summary = "Priority-encoded alternation (children are `nsl.case`/`nsl.default`).";
  let regions = (region SizedRegion<1>:$body);
  let assemblyFormat = "attr-dict-with-keyword $body";
  let hasVerifier = 1;
}

def NSL_AnyOp : NSL_Op<"any", [NoTerminator, SingleBlock]> {
  let summary = "Parallel alternation (children are `nsl.case`/`nsl.default`).";
  let regions = (region SizedRegion<1>:$body);
  let assemblyFormat = "attr-dict-with-keyword $body";
  let hasVerifier = 1;
}

def NSL_IfOp : NSL_Op<"if", [NoTerminator, SingleBlock]> {
  let summary = "Two-region conditional (then / else).";
  let description = [{
    `nsl.if %c { ... } else { ... }`. Two regions; both MAY be empty
    per FR-013 (else region's emptiness is structurally legal but
    may be semantically degenerate — Sema's M3 domain).

    Carries `SingleBlock` (in addition to `NoTerminator`) so the parser
    auto-materializes an implicit `^bb0:` block when the source spells
    an empty region as `{}`. Without `SingleBlock`, MLIR parses `{}` as
    a zero-block region and the `SizedRegion<1>` constraint then rejects
    it.
  }];
  let arguments = (ins NSL_AnyBits:$cond);
  let regions = (region SizedRegion<1>:$thenRegion, SizedRegion<1>:$elseRegion);
  let assemblyFormat = [{
    $cond `:` type($cond) attr-dict-with-keyword $thenRegion `else` $elseRegion
  }];
}

def NSL_ParallelOp : NSL_Op<"parallel", [NoTerminator, SingleBlock]> {
  let summary = "One-region parallel-action block.";
  let regions = (region SizedRegion<1>:$body);
  let assemblyFormat = "attr-dict-with-keyword $body";
}

def NSL_SeqOp : NSL_Op<"seq", [
    NoTerminator, SingleBlock,
    HasParent<"FuncOp">]> {
  let summary = "One-region sequential-action block (parent = `nsl.func`).";
  let regions = (region SizedRegion<1>:$body);
  let assemblyFormat = "attr-dict-with-keyword $body";
}

def NSL_WhileOp : NSL_Op<"while", [NoTerminator, SingleBlock]> {
  let summary = "While-loop (transitive parent = `nsl.seq` per Q2 Option B).";
  let arguments = (ins NSL_AnyBits:$cond);
  let regions = (region SizedRegion<1>:$body);
  let assemblyFormat = "$cond `:` type($cond) attr-dict-with-keyword $body";
  let hasVerifier = 1;
}

def NSL_ForOp : NSL_Op<"for", [NoTerminator, SingleBlock]> {
  let summary = "C-style three-operand for-loop (transitive parent = `nsl.seq`).";
  let arguments = (ins
    NSL_BitsOrStruct:$init,
    NSL_AnyBits:$cond,
    NSL_BitsOrStruct:$step);
  let regions = (region SizedRegion<1>:$body);
  let assemblyFormat = [{
    $init `,` $cond `,` $step `:` type($init) `,` type($cond) `,` type($step) attr-dict-with-keyword $body
  }];
  let hasVerifier = 1;
}

//===----------------------------------------------------------------------===//
// 2.5 Action-helper ops (2): nsl.case, nsl.default
//===----------------------------------------------------------------------===//

def NSL_CaseOp : NSL_Op<"case", [
    NoTerminator, SingleBlock,
    ParentOneOf<["AltOp", "AnyOp"]>]> {
  let summary = "A case in `nsl.alt` / `nsl.any`.";
  let arguments = (ins NSL_AnyBits:$cond);
  let regions = (region SizedRegion<1>:$body);
  let assemblyFormat = "$cond `:` type($cond) attr-dict-with-keyword $body";
}

def NSL_DefaultOp : NSL_Op<"default", [
    NoTerminator, SingleBlock,
    ParentOneOf<["AltOp", "AnyOp"]>]> {
  let summary = "The default branch of `nsl.alt` / `nsl.any`.";
  let regions = (region SizedRegion<1>:$body);
  let assemblyFormat = "attr-dict-with-keyword $body";
}

//===----------------------------------------------------------------------===//
// 2.6 Atomic ops (7): transfer, clocked_transfer, incdec, call, finish,
//                     finish_method, invoke_method
//===----------------------------------------------------------------------===//

def NSL_TransferOp : NSL_Op<"transfer", [SameTypeOperands]> {
  let summary = "Wire-style transfer (`=`).";
  let arguments = (ins NSL_BitsOrStruct:$dst, NSL_BitsOrStruct:$src);
  let assemblyFormat = "$dst `,` $src `:` type($dst) attr-dict";
}

def NSL_ClockedTransferOp : NSL_Op<"clocked_transfer", [SameTypeOperands]> {
  let summary = "Reg-style clocked transfer (`:=`).";
  let arguments = (ins NSL_BitsOrStruct:$dst, NSL_BitsOrStruct:$src);
  let assemblyFormat = "$dst `,` $src `:` type($dst) attr-dict";
  let hasVerifier = 1;
}

def NSL_IncDecOp : NSL_Op<"incdec"> {
  let summary = "Pre/post increment-decrement on a reg-like value.";
  let arguments = (ins
    NSL_AnyBits:$dst,
    IncDecKindAttr:$kind);
  let assemblyFormat = "$dst `:` type($dst) attr-dict";
  let hasVerifier = 1;
}

def NSL_CallOp : NSL_Op<"call"> {
  let summary = "Call a sibling control-terminal / proc by symbol name.";
  let description = [{
    `nsl.call @target(%a, %b) : (!nsl.bits<8>, !nsl.bits<8>) -> ()`.
    Per Phase 4 SYN-4 (variadic operand types use the standard
    `functional-type` form for parseability).
  }];
  let arguments = (ins
    FlatSymbolRefAttr:$callee,
    Variadic<NSL_BitsOrStruct>:$args);
  let results = (outs Variadic<NSL_BitsOrStruct>:$results);
  let assemblyFormat = [{
    $callee `(` $args `)` attr-dict `:` functional-type($args, results)
  }];
  let hasVerifier = 1;
}

def NSL_FinishOp : NSL_Op<"finish"> {
  let summary = "Finish (transitive parent = `nsl.proc` per Q2 Option B).";
  let assemblyFormat = "attr-dict";
  let hasVerifier = 1;
}

def NSL_FinishMethodOp : NSL_Op<"finish_method"> {
  let summary = "Finish a peer module's `nsl.proc` by symbol ref.";
  let arguments = (ins FlatSymbolRefAttr:$callee);
  let assemblyFormat = "$callee attr-dict";
}

def NSL_InvokeMethodOp : NSL_Op<"invoke_method"> {
  let summary = "Invoke a peer module's `nsl.proc` by symbol ref.";
  let description = [{
    Per Phase 4 SYN-4: variadic operand types via `functional-type`.
  }];
  let arguments = (ins
    FlatSymbolRefAttr:$callee,
    Variadic<NSL_BitsOrStruct>:$args);
  let results = (outs Variadic<NSL_BitsOrStruct>:$results);
  let assemblyFormat = [{
    $callee `(` $args `)` attr-dict `:` functional-type($args, results)
  }];
}

//===----------------------------------------------------------------------===//
// 2.7 Procedure ops (4): nsl.proc, nsl.first_state, nsl.state, nsl.func
//===----------------------------------------------------------------------===//

def NSL_ProcOp : NSL_Op<"proc", [
    Symbol, SymbolTable, NoTerminator, SingleBlock,
    HasParent<"ModuleOp">]> {
  let summary = "An NSL proc (state-machine procedure).";
  let arguments = (ins SymbolNameAttr:$sym_name);
  let regions = (region SizedRegion<1>:$body);
  let assemblyFormat = "$sym_name attr-dict-with-keyword $body";
  let hasVerifier = 1;
}

def NSL_FirstStateOp : NSL_Op<"first_state", [HasParent<"ProcOp">]> {
  let summary = "Mark the first state of an `nsl.proc`.";
  let arguments = (ins FlatSymbolRefAttr:$target);
  let assemblyFormat = "$target attr-dict";
  let hasVerifier = 1;
}

def NSL_StateOp : NSL_Op<"state", [
    Symbol, NoTerminator, SingleBlock,
    HasParent<"ProcOp">]> {
  let summary = "An NSL `nsl.state` (one state of an `nsl.proc`).";
  let arguments = (ins SymbolNameAttr:$sym_name);
  let regions = (region SizedRegion<1>:$body);
  let assemblyFormat = "$sym_name attr-dict-with-keyword $body";
}

def NSL_FuncOp : NSL_Op<"func", [
    Symbol, NoTerminator, SingleBlock,
    HasParent<"ModuleOp">]> {
  let summary = "An NSL func (combinational procedure).";
  let description = [{
    Per Q5 Option A', `sym_name` is a `StringAttr` containing the
    literal dotted form: bare `nsl.func @reset` stores `"reset"`;
    dotted-form `nsl.func @ic.ready` stores `"ic.ready"` as a single
    StringAttr (matches `func.func @some.dotted.name` upstream
    precedent — MLIR symbol identifiers permit `.`).
  }];
  let arguments = (ins SymbolNameAttr:$sym_name);
  let regions = (region SizedRegion<1>:$body);
  let assemblyFormat = "$sym_name attr-dict-with-keyword $body";
}

//===----------------------------------------------------------------------===//
// 2.8 Procedure-helper op (1): nsl.goto
//===----------------------------------------------------------------------===//

def NSL_GotoOp : NSL_Op<"goto"> {
  let summary = "Goto sibling state / label (transitive-parent verifier).";
  let description = [{
    Per Q2 Option B, `nsl.goto`'s structural-parent rule is the
    transitive walk: parent = `nsl.state` (state form) OR `nsl.seq`
    (label form). The hand-written verifier walks `getParentOp()`
    upward to find one of those two ancestor kinds.
  }];
  let arguments = (ins FlatSymbolRefAttr:$target);
  let assemblyFormat = "$target attr-dict";
  let hasVerifier = 1;
}

//===----------------------------------------------------------------------===//
// 2.9 System-task ops (4): sim_display, sim_finish, sim_init, sim_delay
//===----------------------------------------------------------------------===//
//
// Phase 3 fixtures place `nsl.sim_display` / `nsl.sim_delay` either
// directly in `nsl.module` OR inside a sibling `nsl.sim_init` body —
// so those two ops accept either parent. `nsl.sim_init` /
// `nsl.sim_finish` are module-level only per the fixture corpus.

def NSL_SimDisplayOp : NSL_Op<"sim_display",
    [ParentOneOf<["ModuleOp", "SimInitOp"]>]> {
  let summary = "Sim-only display task (`$display`-equivalent).";
  let description = [{
    `nsl.sim_display "fmt", %a, %b : !nsl.bits<8>, !nsl.bits<8>` —
    a `printf`-style task. Per Phase 4 SYN-4, operand types are
    listed after the format string for parseability.
  }];
  let arguments = (ins
    StrAttr:$format,
    Variadic<NSL_BitsOrStruct>:$args);
  let assemblyFormat = [{
    $format (`,` $args^ `:` type($args))? attr-dict
  }];
}

def NSL_SimFinishOp : NSL_Op<"sim_finish", [HasParent<"ModuleOp">]> {
  let summary = "Sim-only finish task.";
  let arguments = (ins StrAttr:$reason);
  let assemblyFormat = "$reason attr-dict";
}

def NSL_SimInitOp : NSL_Op<"sim_init", [
    NoTerminator, SingleBlock,
    HasParent<"ModuleOp">]> {
  let summary = "Sim-only init block (one region; sim_delay/sim_display children).";
  let regions = (region SizedRegion<1>:$body);
  let assemblyFormat = "attr-dict-with-keyword $body";
}

def NSL_SimDelayOp : NSL_Op<"sim_delay",
    [ParentOneOf<["ModuleOp", "SimInitOp"]>]> {
  let summary = "Sim-only delay (integer-literal cycles).";
  let arguments = (ins I64Attr:$cycles);
  let assemblyFormat = "$cycles attr-dict";
}

//===----------------------------------------------------------------------===//
// 2.10 Marker ops (4 — post-Q6): fire_probe, struct_cast, field, field_decl
//===----------------------------------------------------------------------===//

def NSL_FireProbeOp : NSL_Op<"fire_probe"> {
  let summary = "1-bit tap on a sibling control-terminal (per S27).";
  let arguments = (ins FlatSymbolRefAttr:$target);
  let assemblyFormat = "$target attr-dict";
  let hasVerifier = 1;
}

def NSL_StructCastOp : NSL_Op<"struct_cast"> {
  let summary = "Bits → struct (or struct → bits) preserve cast.";
  let description = [{
    `nsl.struct_cast %raw : !nsl.bits<16> to !nsl.struct<@S>`. Per
    Q3 Option A, this is the explicit op that M5 lowering inserts at
    every user-written struct↔bits conversion site.
  }];
  let arguments = (ins NSL_BitsOrStruct:$source);
  let results = (outs NSL_BitsOrStruct:$result);
  let assemblyFormat = [{
    $source `:` type($source) `to` type($result) attr-dict
  }];
  let hasVerifier = 1;
}

def NSL_FieldOp : NSL_Op<"field"> {
  let summary = "Struct-field access marker (carries an integer field index).";
  let description = [{
    `nsl.field %v {index = K : i64} : !nsl.struct<@T> -> !nsl.bits<N>`.
    Per Q6 Option B, this op handles **only the access-marker role**;
    the struct-internal field-declaration role moved to
    `nsl.field_decl` (a sibling op).
  }];
  let arguments = (ins NSL_AnyStruct:$source, I64Attr:$index);
  let results = (outs NSL_BitsOrStruct:$result);
  let assemblyFormat = [{
    $source attr-dict `:` type($source) `->` type($result)
  }];
  let hasVerifier = 1;
}

def NSL_FieldDeclOp : NSL_Op<"field_decl", [HasParent<"StructOp">]> {
  let summary = "Struct-internal field declaration (per Q6 Option B).";
  let description = [{
    `nsl.field_decl "name" : !nsl.bits<N>` declares a field inside
    an `nsl.struct` body. Per Q6 Option B, the access-marker form
    (`nsl.field`) is split off into the marker category as a sibling
    op record. `name` is a `StringAttr` carrying the field's
    NSL-spec name; `fieldType` (a `TypeAttrOf<>`) is the field
    width-or-struct type. The op deliberately does NOT use the
    `Symbol` interface — Phase 3 fixtures spell the field name as a
    bare string-literal (`"a"`), not as a SymbolRef (`@a`).
  }];
  let arguments = (ins
    StrAttr:$name,
    TypeAttrOf<NSL_BitsOrStruct>:$fieldType);
  let assemblyFormat = "$name `:` $fieldType attr-dict";
}

//===----------------------------------------------------------------------===//
// 2.11 Expansion-only op (1): nsl.structural_generate
//===----------------------------------------------------------------------===//

def NSL_StructuralGenerateOp : NSL_Op<"structural_generate",
    [NoTerminator, SingleBlock]> {
  let summary = "M5-expanded `generate` block.";
  let description = [{
    Carries `lower` / `upper` / `step` integer attributes describing
    the expansion bounds; consumed by M5's `NSLExpandGeneratePass`.
  }];
  let arguments = (ins
    I64Attr:$lower,
    I64Attr:$upper,
    I64Attr:$step);
  let regions = (region SizedRegion<1>:$body);
  let assemblyFormat = "attr-dict-with-keyword $body";
  let hasVerifier = 1;
}

#endif // NSL_OPS_TD
