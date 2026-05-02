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
include "mlir/Interfaces/InferTypeOpInterface.td"
include "mlir/Interfaces/SideEffectInterfaces.td"

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
    ParentOneOf<["::mlir::ModuleOp", "ModuleOp"]>]> {
  let summary = "A structural-type declaration (children are `nsl.field_decl` ops).";
  let description = [{
    `nsl.struct @S { nsl.field_decl "a" : !nsl.bits<4> ... }` declares
    a structural type. Per Q6 Option B, the struct-internal field
    declarations use `nsl.field_decl`; the access-marker form
    (`nsl.field`) is split off into the marker category.

    Post-merge M4-amendment 2026-05-02 (#3): the parent constraint is
    relaxed from `HasParent<"ModuleOp">` to
    `ParentOneOf<["::mlir::ModuleOp", "ModuleOp"]>` so a top-level
    `nsl.struct` (sibling of `nsl.module` under the builtin
    `mlir::ModuleOp`) is legal. NSL grammar places `struct S { ... }`
    at compilation-unit top level (sibling of `module B { ... }` per
    `lang.ebnf §1`); the prior immediate-parent restriction forced
    AST→nsl lowering to invent a synthetic enclosing `nsl.module` for
    every top-level struct. Symbol resolution of `!nsl.struct<@T>`
    references continues to use `mlir::SymbolTable::getNearestSymbolTable`,
    which walks to the parent `mlir::ModuleOp` (a SymbolTable) and
    finds the struct sibling there — no verifier-body amendment
    required. See `specs/007-m4-mlir-dialect/contracts/dialect-api.contract.md`
    §2 post-merge note #3 and `specs/008-m5-structural-passes/research.md`
    §17 for the three-way decision rationale.
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

    Post-merge M4-amendment 2026-05-02 (#4): an OPTIONAL `array_size`
    `I64Attr` carries the array size when the source spells
    `SUB[3] inst;` (NSL submodule-array form). When present, the op
    represents an array of `array_size` submodule instances; when
    absent, a singleton (the original M4 shape). M5's
    `NSLExplodeSubmodArrayPass` (slot 4 per design §9) consumes
    the array form per FR-016, replicating each entry as a sibling
    singleton submodule. The printer emits `@inst : @SUB[3]` for
    the array form and `@inst : @SUB` for the singleton form.
  }];
  let arguments = (ins
    SymbolNameAttr:$sym_name,
    // NOTE: rename `$template` → `$templateRef`; `template` is a
    // C++ reserved keyword and TableGen emits per-attribute getter/
    // setter / property-storage members that fail to compile when
    // the attribute name collides with reserved keywords.
    FlatSymbolRefAttr:$templateRef,
    OptionalAttr<I64Attr>:$array_size);
  let assemblyFormat = [{
    $sym_name `:` $templateRef (`[` $array_size^ `]`)? attr-dict
  }];
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
// 2.1bis Top-level parameter ops (2): nsl.param_int, nsl.param_str
//===----------------------------------------------------------------------===//
//
// Post-merge M4-amendment 2026-05-02 (#4) — bundled. M5 US2 + US3
// surfaced a structural gap: NSL grammar (lang.ebnf §3.1) places
// `param_int N = 8;` / `param_str WIDTH = "8";` at compilation-unit
// top level (sibling of `module B { ... }` / `struct S { ... }` per
// S16); the M4 dialect (post-merge surface 77) carried no top-level
// parameter ops, leaving M5's `NSLResolveParamsPass` (slot 1 per
// design §9 line 1151) with no IR target to consume. The user
// authorised four-way decision option (B) — "amend M4 to add the
// missing primitives" — over (A) emit `hw.constant` + custom-cast
// helpers (violates Principle III: M5 lowers to the `nsl` dialect),
// (C) defer the resolve-params pass to M6 (semantically opaque IR),
// (D) downgrade to a stub. The two ops are top-level placement
// (parent = `mlir::ModuleOp`) and Symbol-bearing so M5's resolve-
// params pass can look them up by name via `mlir::SymbolTable`.
// CIRCT-side conversion code does NOT land at M4 — design §10
// documents the M6 mapping per-op (Principle III firewall: M4
// dialect is the seam, NOT CIRCT). Cross-reference:
// `specs/008-m5-structural-passes/research.md` §18.

def NSL_ParamIntOp : NSL_Op<"param_int", [
    Symbol,
    HasParent<"::mlir::ModuleOp">]> {
  let summary = "An NSL integer parameter (Verilog/VHDL/SystemC submodule param_int).";
  let description = [{
    `nsl.param_int @sym_name = value` declares a top-level integer
    parameter used by Verilog/VHDL/SystemC submodules per S16. Value
    is an `I64Attr`; consumed at M5's `NSLResolveParamsPass` slot 1
    (per design §9), which resolves operand-side `FlatSymbolRefAttr`
    references to the `sym_name` and replaces them with `nsl.constant`
    SSA values. Top-level placement (sibling of `nsl.module`) per S16
    + grammar §3.1.
  }];
  let arguments = (ins SymbolNameAttr:$sym_name, I64Attr:$value);
  let assemblyFormat = "$sym_name `=` $value attr-dict";
}

def NSL_ParamStrOp : NSL_Op<"param_str", [
    Symbol,
    HasParent<"::mlir::ModuleOp">]> {
  let summary = "An NSL string parameter (Verilog/VHDL/SystemC submodule param_str).";
  let description = [{
    `nsl.param_str @sym_name = "value"` declares a top-level string
    parameter per S16. Symmetric to `nsl.param_int`; consumed by M5's
    `NSLResolveParamsPass`. Top-level placement (sibling of
    `nsl.module`) per S16 + grammar §3.1.
  }];
  let arguments = (ins SymbolNameAttr:$sym_name, StrAttr:$value);
  let assemblyFormat = "$sym_name `=` $value attr-dict";
}

//===----------------------------------------------------------------------===//
// 2.2 Storage ops (4): nsl.reg, nsl.wire, nsl.variable, nsl.mem
//===----------------------------------------------------------------------===//

def NSL_RegOp : NSL_Op<"reg", [
    ParentOneOf<["ModuleOp", "ProcOp", "StructuralGenerateOp"]>]> {
  let summary = "An NSL register (clocked storage).";
  let description = [{
    `nsl.reg "name" : !nsl.bits<N> = init` (or `!nsl.struct<@T>`).
    The `init` attribute is the register's reset value; per FR-013
    the result type is one of `!nsl.bits<N>` or `!nsl.struct<@T>`.

    Post-merge M4-amendment 2026-05-02 (#5): the parent constraint
    is widened from `ParentOneOf<["ModuleOp", "ProcOp"]>` to also
    admit `StructuralGenerateOp` so that `generate` blocks (per
    NSL grammar `lang.ebnf §8`) may declare per-iteration registers
    inside their bodies. M5's `NSLExpandGeneratePass` (slot 2)
    consumes the `StructuralGenerateOp` and replicates the body
    once per iteration; the resulting registers all reparent to the
    enclosing `ModuleOp` after expansion.
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

def NSL_WireOp : NSL_Op<"wire",
    [ParentOneOf<["ModuleOp", "FuncOp"]>]> {
  let summary = "An NSL wire (combinational signal).";
  let description = [{
    `nsl.wire "name" : !nsl.bits<N>`. Per FR-013, wires never carry a
    `!nsl.struct<@T>` result type.

    Post-merge M4-amendment 2026-05-02 (#5): the parent constraint
    is widened from `HasParent<"ModuleOp">` to
    `ParentOneOf<["ModuleOp", "FuncOp"]>` so that
    `NSLExpandVariablesPass` (slot 3) can replace func-scope
    `nsl.variable` ops with sibling `nsl.wire` ops without hoisting
    them out of the enclosing `nsl.func`. The pass remains scope-
    deterministic: per-version wires share their scope with the
    variable they replace.
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
    `nsl.variable "name" : !nsl.bits<N>` or `!nsl.struct<@T>`. Per
    FR-013 + post-merge M4-amendment 2026-05-02 (#5), variables
    carry either a bits-type or a struct-type result; struct-typed
    variables enable the FR-015 per-field SSA-split chain in
    `NSLExpandVariablesPass`.
  }];
  let arguments = (ins StrAttr:$name);
  let results = (outs NSL_BitsOrStruct:$result);
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
// 2.2bis Constant op (1): nsl.constant
//===----------------------------------------------------------------------===//
//
// Post-merge M4-amendment 2026-05-01 (per /speckit-clarify four-way
// decision, option (a)). M5 expression-lowering surfaced the gap: every
// `LiteralExpr` lowering needs an `mlir::Value` producer of `!nsl.bits<N>`
// to feed `nsl.transfer`'s `SameTypeOperands`-constrained `$src`, and
// `hw.constant` (i<N>) can't satisfy that constraint. This op closes the
// gap; widths > 64 bits are deferred to a future amendment.

def NSL_ConstantOp : NSL_Op<"constant", [Pure, ConstantLike]> {
  let summary = "An NSL bit-vector constant.";
  let description = [{
    `nsl.constant <value> : !nsl.bits<N>` materialises an integer literal
    as an `mlir::Value` of `!nsl.bits<N>`, suitable to feed operands
    carrying `NSL_BitsOrStruct` constraints (e.g., `nsl.transfer`'s
    `$src`). The `value` attribute is an `I64Attr`; widths > 64 bits are
    deferred to a future amendment. Verifier checks that `value` fits in
    the result type's width (`value < (1 << width)` for `width < 64`;
    any I64 value is admissible at exactly `width == 64`).
  }];
  let arguments = (ins I64Attr:$value);
  let results = (outs NSL_AnyBits:$result);
  let assemblyFormat = "$value `:` type($result) attr-dict";
  let hasVerifier = 1;
}

//===----------------------------------------------------------------------===//
// 2.2ter Expression-binary ops (Phase A cluster 1+3+4 of post-merge
//        M4-amendment 2026-05-02 — option (B) on FR-007 vs design §10).
//
// Expression-position AST nodes (`BinaryExpr`, `UnaryExpr`,
// `ConditionalExpr`, `SliceExpr`, `ConcatExpr`) per spec FR-007 must
// lower to `mlir::Value` at M5. Design §10's M6 mapping table lists the
// stock-CIRCT target for each op (`comb.<x>` / `hwarith.<x>`). M4's
// expression-op surface is the seam between FR-007's M5 obligation and
// design §10's M6 mapping; Phase A enumerates the structural shape per
// EBNF §11. CIRCT-side conversion code lands at M6 (Principle III
// firewall: M4 dialect is the seam, NOT CIRCT).
//
// This first cluster covers the trait-covered binary ops:
//   - arith: `nsl.add`, `nsl.sub`, `nsl.mul`           (EBNF §11 line 622, 624)
//   - bitwise: `nsl.and`, `nsl.or`, `nsl.xor`           (§11 lines 610–614)
//   - shift: `nsl.shl`, `nsl.shr`                       (§11 line 620)
//
// All eight share the same trait set: `Pure` (no side-effects),
// `SameOperandsAndResultType` (operand widths must match the result
// width, and per-bit element types are identical), and where applicable
// `Commutative`. The trait machinery covers every structural invariant;
// no hand-written verifier body is required.
//===----------------------------------------------------------------------===//

class NSL_BinaryArithOp<string mnemonic, list<Trait> extraTraits = []>
    : NSL_Op<mnemonic, !listconcat(
        [Pure, SameOperandsAndResultType], extraTraits)> {
  let arguments = (ins NSL_AnyBits:$lhs, NSL_AnyBits:$rhs);
  let results = (outs NSL_AnyBits:$result);
  let assemblyFormat = "$lhs `,` $rhs `:` type($result) attr-dict";
}

def NSL_AddOp : NSL_BinaryArithOp<"add", [Commutative]> {
  let summary = "Bit-vector addition (NSL `+`).";
  let description = [{
    `nsl.add %a, %b : !nsl.bits<N>` — unsigned addition modulo 2^N (per
    NSL's value-neutral grammar; signed semantics, when needed at M6,
    are surfaced via the `nsl.sign_extend` op chain). Lowers to
    `comb.add` at M6 per design §10.
  }];
}

def NSL_SubOp : NSL_BinaryArithOp<"sub"> {
  let summary = "Bit-vector subtraction (NSL `-` binary).";
  let description = [{
    `nsl.sub %a, %b : !nsl.bits<N>`. Lowers to `comb.sub` at M6.
  }];
}

def NSL_MulOp : NSL_BinaryArithOp<"mul", [Commutative]> {
  let summary = "Bit-vector multiplication (NSL `*`).";
  let description = [{
    `nsl.mul %a, %b : !nsl.bits<N>` — unsigned multiplication, low N
    bits of the 2N-bit product. Lowers to `comb.mul` at M6.
  }];
}

def NSL_AndOp : NSL_BinaryArithOp<"and", [Commutative]> {
  let summary = "Bitwise AND (NSL `&` binary form per N2).";
  let description = [{
    `nsl.and %a, %b : !nsl.bits<N>`. Disambiguated from the `&` reduction
    unary form (`nsl.reduce_and`) by parser-side N2 (already resolved at
    AST level). Lowers to `comb.and` at M6.
  }];
}

def NSL_OrOp : NSL_BinaryArithOp<"or", [Commutative]> {
  let summary = "Bitwise OR (NSL `|` binary form per N2).";
  let description = [{
    `nsl.or %a, %b : !nsl.bits<N>`. Lowers to `comb.or` at M6.
  }];
}

def NSL_XorOp : NSL_BinaryArithOp<"xor", [Commutative]> {
  let summary = "Bitwise XOR (NSL `^` binary form per N2).";
  let description = [{
    `nsl.xor %a, %b : !nsl.bits<N>`. Lowers to `comb.xor` at M6.
  }];
}

def NSL_ShlOp : NSL_BinaryArithOp<"shl"> {
  let summary = "Left shift (NSL `<<`).";
  let description = [{
    `nsl.shl %a, %b : !nsl.bits<N>` — `%a` shifted left by `%b` bits;
    high bits discarded, low bits zero-filled. Lowers to `comb.shl` at
    M6.
  }];
}

def NSL_ShrOp : NSL_BinaryArithOp<"shr"> {
  let summary = "Logical right shift (NSL `>>`).";
  let description = [{
    `nsl.shr %a, %b : !nsl.bits<N>` — `%a` shifted right by `%b` bits;
    low bits discarded, high bits zero-filled. NSL's grammar (§11 line
    620) lists only `<<` and `>>`; signed-arithmetic right shift is not
    in §11 and is deferred to a future amendment if surfaced. Lowers to
    `comb.shru` at M6 (the unsigned/logical variant in CIRCT `comb`).
  }];
}

//===----------------------------------------------------------------------===//
// 2.2quater Expression-comparison + logical ops (Phase A cluster 2 of
//           post-merge M4-amendment 2026-05-02 — option (B)).
//
// Result type is `!nsl.bits<1>` regardless of operand widths. Operands
// share types per `SameTypeOperands` (NOT `SameOperandsAndResultType`,
// since the result width 1 differs from operand width N). A hand-
// written verifier body asserts the result type is `!nsl.bits<1>`.
//
// Comparisons are unsigned (`comb.icmp ult/ule/ugt/uge`) — NSL's
// grammar (§11 line 618) is value-neutral and the spec carries no
// signed-comparison primitive at this layer; signed semantics would
// surface via `nsl.sign_extend` chains in a future amendment if
// required.
//
// Logical AND / OR (NSL `&&` / `||` per §11 lines 606–608) are listed
// here because they share the result-type-1 + same-operand-type shape.
// Per N2 they are syntactically distinct from bitwise `&` / `|` (which
// produce result-width-equal-to-operand-width); the AST layer
// disambiguates.
//===----------------------------------------------------------------------===//

class NSL_BinaryCmpOp<string mnemonic, list<Trait> extraTraits = []>
    : NSL_Op<mnemonic, !listconcat([Pure, SameTypeOperands], extraTraits)> {
  let arguments = (ins NSL_AnyBits:$lhs, NSL_AnyBits:$rhs);
  let results = (outs NSL_AnyBits:$result);
  let assemblyFormat = [{
    $lhs `,` $rhs `:` type($lhs) `->` type($result) attr-dict
  }];
  let hasVerifier = 1;
}

def NSL_EqOp : NSL_BinaryCmpOp<"eq", [Commutative]> {
  let summary = "Equality comparison (NSL `==`).";
  let description = [{
    `nsl.eq %a, %b : !nsl.bits<N> -> !nsl.bits<1>` — unsigned equality.
    Lowers to `comb.icmp eq` at M6.
  }];
}

def NSL_NeOp : NSL_BinaryCmpOp<"ne", [Commutative]> {
  let summary = "Inequality comparison (NSL `!=`).";
  let description = [{
    `nsl.ne %a, %b : !nsl.bits<N> -> !nsl.bits<1>`. Lowers to
    `comb.icmp ne` at M6.
  }];
}

def NSL_LtOp : NSL_BinaryCmpOp<"lt"> {
  let summary = "Unsigned less-than comparison (NSL `<`).";
  let description = [{
    `nsl.lt %a, %b : !nsl.bits<N> -> !nsl.bits<1>`. Lowers to
    `comb.icmp ult` at M6.
  }];
}

def NSL_LeOp : NSL_BinaryCmpOp<"le"> {
  let summary = "Unsigned less-than-or-equal comparison (NSL `<=`).";
  let description = [{
    `nsl.le %a, %b : !nsl.bits<N> -> !nsl.bits<1>`. Lowers to
    `comb.icmp ule` at M6.
  }];
}

def NSL_GtOp : NSL_BinaryCmpOp<"gt"> {
  let summary = "Unsigned greater-than comparison (NSL `>`).";
  let description = [{
    `nsl.gt %a, %b : !nsl.bits<N> -> !nsl.bits<1>`. Lowers to
    `comb.icmp ugt` at M6.
  }];
}

def NSL_GeOp : NSL_BinaryCmpOp<"ge"> {
  let summary = "Unsigned greater-than-or-equal comparison (NSL `>=`).";
  let description = [{
    `nsl.ge %a, %b : !nsl.bits<N> -> !nsl.bits<1>`. Lowers to
    `comb.icmp uge` at M6.
  }];
}

def NSL_LandOp : NSL_BinaryCmpOp<"land", [Commutative]> {
  let summary = "Logical AND (NSL `&&`).";
  let description = [{
    `nsl.land %a, %b : !nsl.bits<1> -> !nsl.bits<1>` — both operands are
    boolean (per NSL semantics, only `!nsl.bits<1>` is meaningful as a
    logical operand; M3 Sema enforces this at the source level). The
    op accepts any `!nsl.bits<N>` operand pair to match the parent
    grammar's value-neutrality, and the verifier asserts both are
    width-1 (delegating wider-operand rejection to Sema). Lowers to
    `comb.and` (on width-1 operands) at M6.
  }];
}

def NSL_LorOp : NSL_BinaryCmpOp<"lor", [Commutative]> {
  let summary = "Logical OR (NSL `||`).";
  let description = [{
    `nsl.lor %a, %b : !nsl.bits<1> -> !nsl.bits<1>`. Same shape as
    `nsl.land`. Lowers to `comb.or` (on width-1 operands) at M6.
  }];
}

//===----------------------------------------------------------------------===//
// 2.2quinque Expression-unary ops (Phase A cluster 5 of post-merge
//            M4-amendment 2026-05-02 — option (B)).
//
// Two shapes:
//   (a) Same-operand-and-result-type: bitwise NOT (`~`), unary minus (`-`).
//       Trait-covered by `Pure + SameOperandsAndResultType`; no
//       hand-written verifier.
//   (b) Width-1-result: logical NOT (`!`), reduction `& | ^`. Operand
//       width is N, result width is 1. Hand-written verifier asserts
//       result is `!nsl.bits<1>`; for `nsl.lnot` the operand must also
//       be width-1 (logical NOT's operand is boolean per N2).
//
// Negated reductions (`~&`, `~|`, `~^` from §11 line 635) decompose at
// M5 lowering into a reduction + `nsl.not`; not surfaced as
// dedicated ops at M4 to keep the surface minimal.
//
// Unary plus (§11 line 629) is a no-op identity — M5 lowering drops
// it entirely; no dialect op needed.
//===----------------------------------------------------------------------===//

class NSL_UnaryArithOp<string mnemonic>
    : NSL_Op<mnemonic, [Pure, SameOperandsAndResultType]> {
  let arguments = (ins NSL_AnyBits:$operand);
  let results = (outs NSL_AnyBits:$result);
  let assemblyFormat = "$operand `:` type($result) attr-dict";
}

def NSL_NotOp : NSL_UnaryArithOp<"not"> {
  let summary = "Bitwise NOT (NSL `~`).";
  let description = [{
    `nsl.not %a : !nsl.bits<N>` — bitwise complement (every bit flipped).
    Lowers to `comb.xor %a, all-ones` at M6 per design §10.
  }];
}

def NSL_NegOp : NSL_UnaryArithOp<"neg"> {
  let summary = "Two's-complement negation (NSL unary `-`).";
  let description = [{
    `nsl.neg %a : !nsl.bits<N>` — two's-complement negation; equivalent
    to `(0 - %a)` modulo 2^N. Lowers to `comb.sub 0, %a` at M6.
  }];
}

class NSL_UnaryReduceOp<string mnemonic>
    : NSL_Op<mnemonic, [Pure]> {
  let arguments = (ins NSL_AnyBits:$operand);
  let results = (outs NSL_AnyBits:$result);
  let assemblyFormat = [{
    $operand `:` type($operand) `->` type($result) attr-dict
  }];
  let hasVerifier = 1;
}

def NSL_LnotOp : NSL_UnaryReduceOp<"lnot"> {
  let summary = "Logical NOT (NSL `!`).";
  let description = [{
    `nsl.lnot %a : !nsl.bits<1> -> !nsl.bits<1>`. Operand and result are
    both width-1; the verifier rejects wider operands (those are NSL
    Sema's domain at M3 — but the dialect surfaces them here for
    symmetry with `nsl.land` / `nsl.lor`). Lowers to `comb.icmp eq %a, 0`
    at M6.
  }];
}

def NSL_ReduceAndOp : NSL_UnaryReduceOp<"reduce_and"> {
  let summary = "Reduction AND (NSL `&` prefix per §11 line 634).";
  let description = [{
    `nsl.reduce_and %a : !nsl.bits<N> -> !nsl.bits<1>` — true iff every
    bit of `%a` is 1. Lowers to `comb.icmp eq %a, all-ones` at M6
    (CIRCT has no dedicated reduction-AND op; the equality-with-all-ones
    is the canonical form).
  }];
}

def NSL_ReduceOrOp : NSL_UnaryReduceOp<"reduce_or"> {
  let summary = "Reduction OR (NSL `|` prefix per §11 line 634).";
  let description = [{
    `nsl.reduce_or %a : !nsl.bits<N> -> !nsl.bits<1>` — true iff any
    bit of `%a` is 1. Lowers to `comb.icmp ne %a, 0` at M6.
  }];
}

def NSL_ReduceXorOp : NSL_UnaryReduceOp<"reduce_xor"> {
  let summary = "Reduction XOR / parity (NSL `^` prefix per §11 line 634).";
  let description = [{
    `nsl.reduce_xor %a : !nsl.bits<N> -> !nsl.bits<1>` — XOR-fold of all
    bits of `%a` (parity). Lowers to `comb.parity` at M6.
  }];
}

//===----------------------------------------------------------------------===//
// 2.2sext Expression-extend ops (Phase A cluster 6 of post-merge
//         M4-amendment 2026-05-02 — option (B)).
//
// Width-changing extends per EBNF §11 lines 702–705:
//   - sign-extend `N#expr` / `N#(expr)`  → `nsl.sign_extend`
//   - zero-extend `N'(expr)`             → `nsl.zero_extend`
// Both: operand is `!nsl.bits<M>`, result is `!nsl.bits<N>` with N ≥ M.
// Hand-written verifier asserts result-width ≥ operand-width.
//===----------------------------------------------------------------------===//

class NSL_ExtendOp<string mnemonic> : NSL_Op<mnemonic, [Pure]> {
  let arguments = (ins NSL_AnyBits:$operand);
  let results = (outs NSL_AnyBits:$result);
  let assemblyFormat = [{
    $operand `:` type($operand) `to` type($result) attr-dict
  }];
  let hasVerifier = 1;
}

def NSL_SignExtendOp : NSL_ExtendOp<"sign_extend"> {
  let summary = "Sign-extend (NSL `N#expr` per §11 lines 702–703).";
  let description = [{
    `nsl.sign_extend %a : !nsl.bits<M> to !nsl.bits<N>` — sign-extend
    the M-bit operand to N bits (N ≥ M); the high (N-M) bits are filled
    with the operand's MSB. Lowers via CIRCT's standard sign-extend
    pattern at M6 (`comb.concat` of `comb.replicate` + operand, or the
    forthcoming `hwarith.cast` pattern when applicable).
  }];
}

def NSL_ZeroExtendOp : NSL_ExtendOp<"zero_extend"> {
  let summary = "Zero-extend (NSL `N'(expr)` per §11 line 705).";
  let description = [{
    `nsl.zero_extend %a : !nsl.bits<M> to !nsl.bits<N>` — zero-extend
    the M-bit operand to N bits (N ≥ M); the high (N-M) bits are zero.
    Lowers to `comb.concat` of an all-zeros constant + operand at M6.
  }];
}

//===----------------------------------------------------------------------===//
// 2.2mux Conditional-expression + concat ops (Phase A cluster 7a of
//        post-merge M4-amendment 2026-05-02 — option (B)).
//
// `nsl.mux` (NSL `if (c) e1 else e2` per EBNF §11 lines 601–604;
//   spec S14 mandates the else-branch in conditional expressions, so
//   both branches are required at M4-verify time).
//
// `nsl.concat` (NSL `{a, b, ...}` per §11 line 698; result width is
//   the sum of operand widths).
//===----------------------------------------------------------------------===//

def NSL_MuxOp : NSL_Op<"mux", [Pure]> {
  let summary = "Conditional-expression mux (NSL `if (c) a else b`).";
  let description = [{
    `nsl.mux %cond, %a, %b : !nsl.bits<N>` — selects `%a` if `%cond` is
    1, else `%b`. `%cond` is `!nsl.bits<1>`; `%a`, `%b`, and the result
    share type `!nsl.bits<N>` (or `!nsl.struct<@T>`). S14 mandates the
    else-branch in expressions; both `then` and `else` operands are
    required at M4. Lowers to `comb.mux` at M6.
  }];
  let arguments = (ins
    NSL_AnyBits:$cond,
    NSL_BitsOrStruct:$thenValue,
    NSL_BitsOrStruct:$elseValue);
  let results = (outs NSL_BitsOrStruct:$result);
  let assemblyFormat = [{
    $cond `,` $thenValue `,` $elseValue `:`
    type($cond) `,` type($thenValue) `,` type($elseValue) `->` type($result) attr-dict
  }];
  let hasVerifier = 1;
}

def NSL_ConcatOp : NSL_Op<"concat", [Pure]> {
  let summary = "Bit-vector concatenation (NSL `{a, b, ...}` per §11 line 698).";
  let description = [{
    `nsl.concat %a, %b, %c : (!nsl.bits<W1>, !nsl.bits<W2>, !nsl.bits<W3>) -> !nsl.bits<W>`
    where `W = W1 + W2 + W3`. Operands are listed in NSL source order
    (left-to-right MSB-first per S18 packing convention). At least one
    operand is required. Lowers to `comb.concat` (variadic) at M6.
  }];
  let arguments = (ins Variadic<NSL_AnyBits>:$operands);
  let results = (outs NSL_AnyBits:$result);
  let assemblyFormat = [{
    $operands `:` `(` type($operands) `)` `->` type($result) attr-dict
  }];
  let hasVerifier = 1;
}

//===----------------------------------------------------------------------===//
// 2.2slice Slice + repeat ops (Phase A cluster 7b of post-merge
//          M4-amendment 2026-05-02 — option (B)).
//
// `nsl.extract` (NSL `v[hi:lo]` slice + `v[i]` single-bit per EBNF
//   §11 lines 639–640; spec S15 mandates compile-time-constant
//   indices). Per-MLIR convention: `lowBit: I64Attr`; result width is
//   derived from the result type; verifier asserts
//   `lowBit + result_width <= operand_width`.
//
// `nsl.repeat` (NSL `N{a}` per EBNF §11 line 700; spec S15-style
//   compile-time `N`).
//===----------------------------------------------------------------------===//

def NSL_ExtractOp : NSL_Op<"extract", [Pure]> {
  let summary = "Bit-slice extraction (NSL `v[hi:lo]` / `v[i]`).";
  let description = [{
    `nsl.extract %v {lowBit = N : i64} : !nsl.bits<W> -> !nsl.bits<R>`
    extracts the R-bit slice `[N+R-1:N]` from the W-bit operand. Per S15
    the source NSL indices are compile-time constants; at the IR level
    `lowBit` is an `I64Attr` and the slice width is derived from the
    result type. Verifier asserts `lowBit + R <= W`. Lowers to
    `comb.extract` at M6 (CIRCT's `comb.extract` matches this shape:
    `lowBit` IntegerAttr + result-type-derived width).
  }];
  let arguments = (ins NSL_AnyBits:$operand, I64Attr:$lowBit);
  let results = (outs NSL_AnyBits:$result);
  let assemblyFormat = [{
    $operand `,` $lowBit `:` type($operand) `->` type($result) attr-dict
  }];
  let hasVerifier = 1;
}

def NSL_RepeatOp : NSL_Op<"repeat", [Pure]> {
  let summary = "Bit-vector repeat (NSL `N{a}` per §11 line 700).";
  let description = [{
    `nsl.repeat %a {count = N : i64} : !nsl.bits<W> -> !nsl.bits<R>` —
    the operand is repeated `N` times left-to-right; result width
    `R = N * W` (with `N >= 1`). Per S15 `N` is a compile-time
    constant; here it's an `I64Attr`. Lowers to `comb.replicate`
    (or an N-fold `comb.concat` chain) at M6.
  }];
  let arguments = (ins NSL_AnyBits:$operand, I64Attr:$count);
  let results = (outs NSL_AnyBits:$result);
  let assemblyFormat = [{
    $operand `,` $count `:` type($operand) `->` type($result) attr-dict
  }];
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
    ParentOneOf<["FuncOp", "ProcOp", "StateOp"]>]> {
  let summary = "One-region sequential-action block (parent = `nsl.func` / `nsl.proc` / `nsl.state` per amendment #6).";
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
  let summary = "For-loop (transitive parent = `nsl.seq`); 2- or 3-operand.";
  let description = [{
    Two surface forms per NSL grammar `lang.ebnf §8`:
      * **C-style 3-operand**: `nsl.for %init, %cond, %step { ... }`
        materialises the canonical `(init; cond; step)` triplet.
      * **Enum-form 2-operand** (post-merge M4-amendment 2026-05-02
        #5): `nsl.for %init, %end { ... }` materialises the NSL
        spec's `for (i = 0..N) { ... }` shape, where `step` is
        implicit (`i++`). The `step` operand is `Variadic`; zero
        operands is the enum form, exactly one operand is the
        C-style form. (Two or more is rejected by the verifier.)

    Either form requires a transitive `nsl.seq` ancestor (per Q2
    Option B, hand-written ancestor walk in `ForOp::verify()`).
    The body region is unchanged across forms.
  }];
  let arguments = (ins
    NSL_BitsOrStruct:$init,
    NSL_AnyBits:$cond,
    Variadic<NSL_BitsOrStruct>:$step);
  let regions = (region SizedRegion<1>:$body);
  let assemblyFormat = [{
    $init `,` $cond ( `,` $step^ )? `:` type($init) `,` type($cond) ( `,` type($step)^ )? attr-dict-with-keyword $body
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
// so those two ops accept either parent. `nsl.sim_finish` likewise
// accepts both parents per S29 (`_init { _finish(); }` is the
// canonical NSL simulation-termination idiom; post-merge amendment
// 2026-04-30 #7 widened the trait from `HasParent<"ModuleOp">` to
// `ParentOneOf<["ModuleOp", "SimInitOp"]>`). `nsl.sim_init` itself
// remains module-level only.

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

def NSL_SimFinishOp : NSL_Op<"sim_finish",
    [ParentOneOf<["ModuleOp", "SimInitOp"]>]> {
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

    Post-merge M4-amendment 2026-05-02 (#4): an OPTIONAL
    `loop_var` `StrAttr` carries the loop variable name (e.g., `"i"`
    from `generate(i = 0..N)`). M5's `NSLExpandGeneratePass` reads
    this to know which `%IDENT%` macro residue to substitute when
    materialising the per-iteration body. Empty-string default
    preserves backward compatibility with existing fixtures that
    omit the attribute.
  }];
  let arguments = (ins
    I64Attr:$lower,
    I64Attr:$upper,
    I64Attr:$step,
    OptionalAttr<StrAttr>:$loop_var);
  let regions = (region SizedRegion<1>:$body);
  let assemblyFormat = "attr-dict-with-keyword $body";
  let hasVerifier = 1;
}

#endif // NSL_OPS_TD
