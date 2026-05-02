<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: `nsl-dialect` Public API Surface

**Branch**: `007-m4-mlir-dialect` | **Date**: 2026-04-30
**Plan**: [../plan.md](../plan.md)

This contract documents the **public-header surface** of the
`nsl-dialect` library — the API that downstream layers (M5
`nsl-lower`, M6 CIRCT lowering, M7 `nsl-driver`, plus
the `nsl-opt` developer/test binary) consume. Once M4 lands, this
surface is **frozen** for the remainder of the project; any change
is treated as an M4 amendment requiring a same-PR design-doc
update (per Principle VII).

## 1. Public include path

The library exposes exactly **one** umbrella public header:

```
#include "nsl/Dialect/NSL/IR/NSLDialect.h"
```

No consumer code may include the TableGen-generated per-op headers
(`NSLOps.h.inc`, `NSLDialect.h.inc`, `NSLTypes.h.inc`) directly;
those live under `lib/Dialect/NSL/IR/` (private) and the umbrella
re-exports them. This is the single-public-header rule of
Constitution Principle II applied to `nsl-dialect` (NOT an
exception; `nsl-dialect` is not named in Principle II §3's two-
header carve-out for `nsl-ast` and `nsl-sema`).

## 2. Symbols re-exported by `NSLDialect.h`

| Symbol | C++ kind | Source |
|---|---|---|
| `nsl::dialect::NSLDialect` | class | TableGen `def NSL_Dialect : Dialect` |
| `nsl::dialect::ModuleOp` | class | TableGen `def NSL_ModuleOp` |
| `nsl::dialect::StructOp` | class | (TableGen) |
| `nsl::dialect::SubmoduleOp` | class | (TableGen) |
| `nsl::dialect::ConnectOp` | class | (TableGen) |
| `nsl::dialect::RegOp` | class | (TableGen) |
| `nsl::dialect::WireOp` | class | (TableGen) |
| `nsl::dialect::VariableOp` | class | (TableGen) |
| `nsl::dialect::MemOp` | class | (TableGen) |
| `nsl::dialect::ParamIntOp` | class | (TableGen, post-merge amendment 2026-05-02 #4) |
| `nsl::dialect::ParamStrOp` | class | (TableGen, post-merge amendment 2026-05-02 #4) |
| `nsl::dialect::ConstantOp` | class | (TableGen, post-merge amendment 2026-05-01) |
| `nsl::dialect::AddOp` | class | (TableGen, post-merge amendment 2026-05-02 cluster 1) |
| `nsl::dialect::SubOp` | class | (TableGen, post-merge amendment 2026-05-02 cluster 1) |
| `nsl::dialect::MulOp` | class | (TableGen, post-merge amendment 2026-05-02 cluster 1) |
| `nsl::dialect::AndOp` | class | (TableGen, post-merge amendment 2026-05-02 cluster 3) |
| `nsl::dialect::OrOp` | class | (TableGen, post-merge amendment 2026-05-02 cluster 3) |
| `nsl::dialect::XorOp` | class | (TableGen, post-merge amendment 2026-05-02 cluster 3) |
| `nsl::dialect::ShlOp` | class | (TableGen, post-merge amendment 2026-05-02 cluster 4) |
| `nsl::dialect::ShrOp` | class | (TableGen, post-merge amendment 2026-05-02 cluster 4) |
| `nsl::dialect::EqOp` | class | (TableGen, post-merge amendment 2026-05-02 cluster 2) |
| `nsl::dialect::NeOp` | class | (TableGen, post-merge amendment 2026-05-02 cluster 2) |
| `nsl::dialect::LtOp` | class | (TableGen, post-merge amendment 2026-05-02 cluster 2) |
| `nsl::dialect::LeOp` | class | (TableGen, post-merge amendment 2026-05-02 cluster 2) |
| `nsl::dialect::GtOp` | class | (TableGen, post-merge amendment 2026-05-02 cluster 2) |
| `nsl::dialect::GeOp` | class | (TableGen, post-merge amendment 2026-05-02 cluster 2) |
| `nsl::dialect::LandOp` | class | (TableGen, post-merge amendment 2026-05-02 cluster 2) |
| `nsl::dialect::LorOp` | class | (TableGen, post-merge amendment 2026-05-02 cluster 2) |
| `nsl::dialect::NotOp` | class | (TableGen, post-merge amendment 2026-05-02 cluster 5) |
| `nsl::dialect::NegOp` | class | (TableGen, post-merge amendment 2026-05-02 cluster 5) |
| `nsl::dialect::LnotOp` | class | (TableGen, post-merge amendment 2026-05-02 cluster 5) |
| `nsl::dialect::ReduceAndOp` | class | (TableGen, post-merge amendment 2026-05-02 cluster 5) |
| `nsl::dialect::ReduceOrOp` | class | (TableGen, post-merge amendment 2026-05-02 cluster 5) |
| `nsl::dialect::ReduceXorOp` | class | (TableGen, post-merge amendment 2026-05-02 cluster 5) |
| `nsl::dialect::SignExtendOp` | class | (TableGen, post-merge amendment 2026-05-02 cluster 6) |
| `nsl::dialect::ZeroExtendOp` | class | (TableGen, post-merge amendment 2026-05-02 cluster 6) |
| `nsl::dialect::MuxOp` | class | (TableGen, post-merge amendment 2026-05-02 cluster 7a) |
| `nsl::dialect::ConcatOp` | class | (TableGen, post-merge amendment 2026-05-02 cluster 7a) |
| `nsl::dialect::ExtractOp` | class | (TableGen, post-merge amendment 2026-05-02 cluster 7b) |
| `nsl::dialect::RepeatOp` | class | (TableGen, post-merge amendment 2026-05-02 cluster 7b) |
| `nsl::dialect::FuncInOp` | class | (TableGen) |
| `nsl::dialect::FuncOutOp` | class | (TableGen) |
| `nsl::dialect::FuncSelfOp` | class | (TableGen) |
| `nsl::dialect::AltOp` | class | (TableGen) |
| `nsl::dialect::AnyOp` | class | (TableGen) |
| `nsl::dialect::IfOp` | class | (TableGen) |
| `nsl::dialect::ParallelOp` | class | (TableGen) |
| `nsl::dialect::SeqOp` | class | (TableGen) |
| `nsl::dialect::WhileOp` | class | (TableGen) |
| `nsl::dialect::ForOp` | class | (TableGen) |
| `nsl::dialect::CaseOp` | class | (TableGen) |
| `nsl::dialect::DefaultOp` | class | (TableGen) |
| `nsl::dialect::TransferOp` | class | (TableGen) |
| `nsl::dialect::ClockedTransferOp` | class | (TableGen) |
| `nsl::dialect::IncDecOp` | class | (TableGen) |
| `nsl::dialect::CallOp` | class | (TableGen) |
| `nsl::dialect::FinishOp` | class | (TableGen) |
| `nsl::dialect::FinishMethodOp` | class | (TableGen) |
| `nsl::dialect::InvokeMethodOp` | class | (TableGen) |
| `nsl::dialect::ProcOp` | class | (TableGen) |
| `nsl::dialect::FirstStateOp` | class | (TableGen) |
| `nsl::dialect::StateOp` | class | (TableGen) |
| `nsl::dialect::FuncOp` | class | (TableGen) |
| `nsl::dialect::GotoOp` | class | (TableGen) |
| `nsl::dialect::SimDisplayOp` | class | (TableGen) |
| `nsl::dialect::SimFinishOp` | class | (TableGen) |
| `nsl::dialect::SimInitOp` | class | (TableGen) |
| `nsl::dialect::SimDelayOp` | class | (TableGen) |
| `nsl::dialect::FireProbeOp` | class | (TableGen) |
| `nsl::dialect::StructCastOp` | class | (TableGen) |
| `nsl::dialect::FieldOp` | class | (TableGen) |
| `nsl::dialect::FieldDeclOp` | class | (TableGen, post-Q6) |
| `nsl::dialect::StructuralGenerateOp` | class | (TableGen) |
| `nsl::dialect::ModuleTerminatorOp` | class | (TableGen, auto from `SingleBlockImplicitTerminator`) |
| `nsl::dialect::ProcTerminatorOp` | class | (TableGen, auto) |
| `nsl::dialect::BitsType` | class | TableGen `def NSL_BitsType` |
| `nsl::dialect::StructType` | class | TableGen `def NSL_StructType` |
| `nsl::dialect::MemType` | class | TableGen `def NSL_MemType` |
| `nsl::dialect::registerNSLDialect` | function | hand-written |

That's 72 op classes + 2 auto-generated terminators + 3 type
classes + the dialect class + the registration function = **79
public types/functions** (post-Q6: `nsl.field_decl` added; post-merge
amendment 2026-05-01: `nsl.constant` added; post-merge amendment
2026-05-02 (Phase A): the 28-op expression surface added; post-merge
amendment 2026-05-02 (#4): `nsl.param_int` + `nsl.param_str` added;
post-merge amendment 2026-05-02 (#5): no new ops (five trait /
type-constraint relaxations only — see notes below);
post-merge amendment 2026-04-30 (#6): no new ops (single trait
relaxation on `nsl.seq` — see notes below).

> **Post-merge amendment 2026-05-01 (#1).** `nsl.constant` (a Pure +
> ConstantLike value-producer of `!nsl.bits<N>`) was added after M4
> merged because M5 expression-lowering surfaced a gap: every
> `LiteralExpr` lowering needs an `mlir::Value` of `!nsl.bits<N>` to
> feed `nsl.transfer`'s `SameTypeOperands`-constrained `$src`, and
> `hw.constant` (`iN`) cannot satisfy that constraint. The user
> authorised the four-way decision option (a) — "amend M4 to add the
> missing primitive" — over the alternatives (b) inline `hw.constant`
> with a custom-cast helper, (c) widen `nsl.transfer` to admit
> cross-dialect operands, (d) defer the M5 expression-lowering pivot.
> This grows the freeze surface from 48 → 49. Round-trip fixture is
> `test/Dialect/storage/constant_roundtrip.mlir`; verifier-reject
> fixture is `test/Dialect/storage/constant_invalid_overflow.mlir`.
> SC-012's "next op" baseline updates from "42nd op" to "43rd op".

> **Post-merge amendment 2026-05-02 (#2).** The 28-op expression
> surface (`nsl.add`, `nsl.sub`, `nsl.mul`, `nsl.and`, `nsl.or`,
> `nsl.xor`, `nsl.shl`, `nsl.shr`, `nsl.eq`, `nsl.ne`, `nsl.lt`,
> `nsl.le`, `nsl.gt`, `nsl.ge`, `nsl.land`, `nsl.lor`, `nsl.not`,
> `nsl.neg`, `nsl.lnot`, `nsl.reduce_and`, `nsl.reduce_or`,
> `nsl.reduce_xor`, `nsl.sign_extend`, `nsl.zero_extend`, `nsl.mux`,
> `nsl.concat`, `nsl.extract`, `nsl.repeat`) was added after M4
> merged because M5 expression-lowering surfaced a structural gap
> between FR-007 (M5 spec; mandates `BinaryExpr` / `UnaryExpr` /
> `ConditionalExpr` / `SliceExpr` / `ConcatExpr` lower to
> `mlir::Value`) and design §10 (whose mapping table assumes the
> per-op CIRCT target lands at M6). The user authorised the four-way
> decision option (B) — "amend M4 to add the expression-op surface"
> — over (A) implement FR-007 by emitting CIRCT `comb.*` directly
> from M5 (violates Principle III: M5 must lower to the `nsl`
> dialect, not skip ahead to CIRCT), (C) defer FR-007 / cross-
> dialect sleight-of-hand (semantically opaque IR), (D) downgrade
> FR-007 to a stub (postpones the M5→M6 cut and breaks Principle
> VIII test sequencing). This grows the freeze surface from 49 →
> 77. Round-trip + invalid fixtures live under `test/Dialect/expr/`
> (28 round-trip fixtures + 16 invalid fixtures for the ops with
> non-trait-covered invariants — the eight Pure +
> SameOperandsAndResultType ops in clusters 1+3+4 are trait-covered
> so they have no `_invalid_*.mlir` files). SC-012's "next op"
> baseline updates from "43rd op" to "71st op". CIRCT-side conversion
> code does NOT land at M4 — design §10 documents the M6 mapping
> per-op (Principle III: M4 dialect is the seam, NOT CIRCT).

> **Post-merge amendment 2026-05-02 (#3).** The `nsl.struct` parent
> trait was relaxed from `HasParent<"ModuleOp">` to
> `ParentOneOf<["::mlir::ModuleOp", "ModuleOp"]>` after M4 merged
> because M5 lowering of `StructCastExpr` / `FieldAccessExpr` (T043 +
> T044) surfaced a structural gap: NSL grammar places
> `struct S { ... }` at compilation-unit top level (sibling of
> `module B { ... }` per `lang.ebnf §1`), but the prior immediate-
> parent restriction forced the AST→nsl seam to invent a synthetic
> enclosing `nsl.module` for every top-level struct. The user
> authorised the three-way decision option (ii) — "relax the parent
> trait so top-level placement under `mlir::ModuleOp` is legal" —
> over (i) keep the strict parent + synthesise a wrapping
> `nsl.module` for every struct (clutters the IR; violates the
> "structurally faithful" lowering principle established by Q1
> Option A) and (iii) move structs into a per-module child slot
> (contradicts NSL grammar; would force every cross-module struct
> reference to use a multi-segment SymbolRef). This is **purely
> additive** (existing module-scoped struct fixtures continue to
> verify and round-trip); no new ops are introduced, so the freeze
> surface stays at **77**. The per-op invariant row for `nsl.struct`
> updates from `parent = ModuleOp` to
> `parent ∈ {::mlir::ModuleOp, ModuleOp}` (data-model §2.1).
> `StructOp::verify()` is unchanged: its field-cycle check uses
> `mlir::SymbolTable::getNearestSymbolTable(*this)`, which already
> resolves correctly when the struct is a sibling of `nsl.module`
> under the builtin `mlir::ModuleOp` (which itself implements
> `SymbolTable`). Sibling consumers (`StructCastOp::verify`,
> `FieldOp::verify`) likewise use `getNearestSymbolTable` and
> require no amendment. Round-trip fixture is
> `test/Dialect/Types/struct_toplevel_roundtrip.mlir`; existing
> module-scoped round-trip + invalid fixtures
> (`test/Dialect/Types/struct_roundtrip.mlir`,
> `test/Dialect/module-level/struct_roundtrip.mlir`,
> `test/Dialect/module-level/struct_invalid_no_symname.mlir`,
> `test/Dialect/module-level/struct_invalid_circular_field.mlir`)
> remain valid and continue to PASS. SC-012's "next op" baseline is
> unchanged ("71st op"). Cross-reference:
> `specs/008-m5-structural-passes/research.md` §17 documents the
> M5-side reasoning.

> **Post-merge amendment 2026-05-02 (#4).** Bundled three changes
> that unblock M5 US2 + US3, all surfaced during the implementation
> of the structural-expansion passes (`NSLResolveParamsPass`,
> `NSLExpandGeneratePass`, `NSLExplodeSubmodArrayPass` per design §9
> lines 1151–1154):
>
> 1. **Two new top-level parameter ops**: `nsl.param_int` (`I64Attr:
>    $value`) and `nsl.param_str` (`StrAttr:$value`), both
>    Symbol-bearing with parent = `mlir::ModuleOp` (top-level
>    placement, sibling of `nsl.module` per S16 + grammar §3.1).
>    M5's `NSLResolveParamsPass` slot 1 references them by
>    `FlatSymbolRefAttr` lookup via `mlir::SymbolTable`. Without
>    these ops, the resolve-params pass had nothing to consume.
> 2. **Field addition to `nsl.structural_generate`**: an
>    `OptionalAttr<StrAttr>:$loop_var` carrying the loop variable
>    name (e.g., `"i"` from `generate(i = 0..N)`). M5's
>    `NSLExpandGeneratePass` reads this to know which `%IDENT%`
>    macro residue to substitute when materialising per-iteration
>    body copies. Optional-with-empty-default preserves backward
>    compatibility with existing fixtures.
> 3. **Field addition to `nsl.submodule`**: an
>    `OptionalAttr<I64Attr>:$array_size` representing the array
>    size when the source spells `SUB[3] inst;` (NSL submodule-
>    array form per FR-016). The printer emits `@inst : @SUB[3]`
>    when present and `@inst : @SUB` when absent (the original M4
>    singleton form). M5's `NSLExplodeSubmodArrayPass` slot 4
>    consumes the array form by replicating each entry as a
>    sibling singleton submodule.
>
> The user authorised four-way decision option (B) — "amend M4 to
> add the missing primitives + field surface" — over (A) emit
> CIRCT-side helpers directly from M5 (violates Principle III: M5
> lowers to the `nsl` dialect, not skip ahead to CIRCT), (C) defer
> the structural-expansion passes to M6 (semantically opaque IR),
> (D) downgrade to stub-only ops (postpones the M5→M6 cut and
> breaks Principle VIII test sequencing). This grows the freeze
> surface from 77 → 79 (the two new ops only — the field
> additions on `nsl.structural_generate` and `nsl.submodule` do
> NOT add new op classes). Round-trip fixtures live under
> `test/Dialect/storage/param_int_roundtrip.mlir`,
> `test/Dialect/storage/param_str_roundtrip.mlir`,
> `test/Dialect/storage/submodule_array_roundtrip.mlir`, and
> `test/Dialect/expansion-only/structural_generate_loopvar_roundtrip.mlir`.
> The pre-existing fixtures
> (`test/Dialect/module-level/submodule_roundtrip.mlir`,
> `test/Dialect/module-level/submodule_invalid_wrong_parent.mlir`,
> `test/Dialect/expansion-only/structural_generate_roundtrip.mlir`,
> `test/Dialect/expansion-only/structural_generate_invalid_bad_loop_attrs.mlir`)
> remain valid and continue to PASS — the optional-attr defaults
> (empty StrAttr / absent I64Attr) preserve their printed form
> exactly. SC-012's "next op" baseline updates from "71st op" to
> "73rd op". CIRCT-side conversion code does NOT land at M4 —
> design §10 documents the M6 mapping per-op (Principle III
> firewall: M4 dialect is the seam, NOT CIRCT). Cross-reference:
> `specs/008-m5-structural-passes/research.md` §18 documents the
> M5-side reasoning.

> **Post-merge amendment 2026-05-02 (#5).** Bundled five
> trait / type-constraint relaxations on existing op records,
> all surfaced as deferred items in the offload-2026-04-30 close
> (M5 US2 + US3 fixtures `T035`, `T045`, `T066`, `T067`, `T076`,
> `T077`, `T079`). All five are minimal "accept more" relaxations
> on existing op records — **no new op classes** introduced; the
> freeze surface stays at **79**.
>
> 1. **`nsl.for` enum-form (2-operand).** `step` operand changes
>    from `NSL_BitsOrStruct:$step` (mandatory) to
>    `Variadic<NSL_BitsOrStruct>:$step` (0 or 1). Enables the NSL
>    grammar `for (i = 0..N) { ... }` enum form (`lang.ebnf §8`).
>    `ForOp::verify()` rejects ≥ 2 step operands. Existing
>    3-operand fixtures round-trip unchanged.
> 2. **`nsl.fire_probe` target-set extension.** `FireProbeOp::
>    verify()` adds two additional sibling-lookup walks:
>    Symbol-bearing `nsl.proc` at module scope (proc_name) AND
>    Symbol-bearing `nsl.state` inside an enclosing `nsl.proc`
>    (state_name). Per S27 (constructive), proc_name and
>    state_name identifiers are 1-bit values too. The original
>    `nsl.func_in`/`nsl.func_out`/`nsl.func_self` walk is
>    preserved as walk #1 (StrAttr-keyed).
> 3. **`nsl.reg` parent relax.** Trait changes from
>    `ParentOneOf<["ModuleOp", "ProcOp"]>` to
>    `ParentOneOf<["ModuleOp", "ProcOp", "StructuralGenerateOp"]>`
>    so that `generate` blocks (per `lang.ebnf §8`) may declare
>    per-iteration registers in their bodies. M5's
>    `NSLExpandGeneratePass` (slot 2) replicates the body once per
>    iteration; the resulting registers reparent to the enclosing
>    `nsl.module` after expansion.
> 4. **`nsl.variable` result-type relax.** Result type changes
>    from `NSL_AnyBits` to `NSL_BitsOrStruct`, enabling the
>    FR-015 per-field SSA-split chain in `NSLExpandVariablesPass`
>    for struct-typed variables. `VariableOp::verify()` updates
>    accordingly (now mirrors `RegOp::verify()`).
> 5. **`nsl.wire` parent relax.** Trait changes from
>    `HasParent<"ModuleOp">` to `ParentOneOf<["ModuleOp",
>    "FuncOp"]>` so that `NSLExpandVariablesPass` (slot 3) may
>    replace func-scope `nsl.variable` ops with sibling
>    `nsl.wire` ops without hoisting.
>
> The user authorised four-way decision option (B) — "amend M4
> with the bundled five relaxations" — over (A) emit CIRCT-side
> helpers directly from M5 (violates Principle III), (C) defer
> the deferred items to M6 (semantically opaque IR; breaks the
> M5→M6 cut), (D) downgrade to stub-only ops (postpones the cut
> and breaks Principle VIII test sequencing). Cost: **zero new op
> records**, five existing-op edits, plus 6 new round-trip
> fixtures (`test/Dialect/action-block/for_2operand_roundtrip.mlir`,
> `test/Dialect/marker/fire_probe_proc_target_roundtrip.mlir`,
> `test/Dialect/marker/fire_probe_state_target_roundtrip.mlir`,
> `test/Dialect/expansion-only/structural_generate_with_reg_body_roundtrip.mlir`,
> `test/Dialect/storage/variable_struct_typed_roundtrip.mlir`,
> `test/Dialect/storage/wire_in_func_roundtrip.mlir`). The
> pre-existing fixtures (`test/Dialect/action-block/for_roundtrip.mlir`,
> `test/Dialect/marker/fire_probe_roundtrip.mlir`,
> `test/Dialect/storage/{reg,wire,variable}_roundtrip.mlir`,
> `test/Dialect/expansion-only/structural_generate_roundtrip.mlir`)
> remain valid and continue to PASS — the relaxations are
> strictly "accept more". The existing invalid fixtures
> (`test/Dialect/storage/{reg,wire,variable}_invalid_*.mlir`,
> `test/Dialect/marker/fire_probe_invalid_bad_target.mlir`,
> `test/Dialect/action-block/for_invalid_*.mlir`) likewise
> continue to fail-as-expected (the relaxations don't change
> what's REJECTED — `wire` still rejects `!nsl.struct<@T>`,
> `fire_probe` still rejects unresolved targets, `for` still
> requires a `nsl.seq` ancestor). SC-012's "next op" baseline is
> **unchanged** ("73rd op"); since no new op records were added
> the count stays at 79. CIRCT-side conversion code does NOT land
> at M4 — design §10 documents the M6 mapping per-op. Cross-
> reference: `specs/008-m5-structural-passes/research.md` §20
> documents the M5-side reasoning.

> **Post-merge amendment 2026-04-30 (#6).** Single trait
> relaxation on `nsl.seq`. Trait changes from
> `HasParent<"FuncOp">` to
> `ParentOneOf<["FuncOp", "ProcOp", "StateOp"]>`. Surfaced during
> the M5 close-out as the root cause behind 5 XFAIL'd M3-corpus
> fixtures (`test/Lower/m3_corpus/s{07,08,09,19,25}/pass.test`):
> `visit(SeqBlock)` (`lib/Lower/ASTToMLIR.cpp:565`) IS fully
> implemented, but for these fixtures the `seq` lands inside an
> `nsl.proc` body (s07/s08/s09) or inside an `nsl.state` body
> (s19/s25), and the prior strict `HasParent<"FuncOp">` rejected
> both shapes. NSL spec **S7** (`lang.ebnf §8 line 850`)
> explicitly permits `seq` inside a *function or procedure* body;
> the prior trait was therefore stricter than the grammar. The
> user authorised four-way decision option (B) — "amend M4 with
> the single trait relaxation" — over (A) emit CIRCT-side
> helpers directly from M5 (violates Principle III), (C) defer
> the 5 fixtures to M6 (semantically opaque IR; breaks the M5→M6
> cut), (D) downgrade `nsl.seq` to a stub-only op (postpones the
> cut and breaks Principle VIII test sequencing). **Cost: zero
> new op records, zero new verifier code** — single TableGen
> trait swap. Plus 1 new round-trip fixture
> (`test/Dialect/action-block/seq_in_proc_roundtrip.mlir`)
> covering both the proc-direct and via-state placements. The
> pre-existing fixture
> (`test/Dialect/action-block/seq_roundtrip.mlir`, immediate-
> parent `nsl.func` form) remains valid and continues to PASS —
> the relaxation is strictly "accept more". The pre-existing
> invalid fixture
> (`test/Dialect/action-block/seq_invalid_wrong_parent.mlir`,
> placing `nsl.seq` directly under `nsl.module`) likewise
> continues to fail-as-expected (`nsl.module` is still not in
> the parent set). Same precedent as amendments #3 and #5: a
> "strictly accept more" trait relaxation, no semantic change to
> what gets rejected. Freeze surface stays at **79** (no new op
> records); SC-012's "next op" baseline is **unchanged** ("73rd
> op"). CIRCT-side conversion code does NOT land at M4 —
> design §10 documents the M6 mapping per-op (Principle III
> firewall: M4 dialect is the seam, NOT CIRCT). Cross-reference:
> `specs/008-m5-structural-passes/research.md` §21 documents the
> M5-side reasoning (5 M3-corpus fixtures unblocked).

## 3. Registration entry-point contract

Signature:

```cpp
namespace nsl::dialect {
void registerNSLDialect(mlir::DialectRegistry &registry);
}
```

Behavior:

- Registers the `NSLDialect` class on the given registry.
- **Idempotent**: calling twice on the same `registry` is a no-op
  (verified by `test_unit/dialect_register_test/`).
- Thread-safe at the call site (MLIR's `DialectRegistry::insert` is
  documented thread-safe).
- Returns `void`; failures (out of memory, etc.) propagate as MLIR
  exceptions, which is the upstream convention.

## 4. Op class API surface (per op)

Each TableGen-generated op class has the standard MLIR `Op<>`
interface:

```cpp
class ModuleOp : public mlir::Op<ModuleOp, /* traits */ ...> {
public:
  static llvm::StringRef getOperationName() { return "nsl.module"; }
  static void build(mlir::OpBuilder&, mlir::OperationState&, ...);
  llvm::StringRef getSymName();         // from Symbol trait
  llvm::StringRef setSymName(llvm::StringRef);
  mlir::Region &getBody();
  mlir::Block &getBodyBlock();
  // ... per-trait accessors
  static mlir::ParseResult parse(mlir::OpAsmParser&, mlir::OperationState&);
  void print(mlir::OpAsmPrinter&);
  mlir::LogicalResult verify();
};
```

The exact accessor set is determined by the op's traits + arguments
+ regions in the `.td` record. Consumers (M5 lowering) use these
accessors via TableGen's standard generated API; consumers do NOT
dynamically construct ops by raw `Operation *` manipulation.

## 5. Type class API surface (per type)

Each type class has the standard MLIR `Type<>` interface:

```cpp
class BitsType : public mlir::Type::TypeBase<BitsType, mlir::Type, BitsTypeStorage> {
public:
  static llvm::StringRef getMnemonic() { return "bits"; }
  static BitsType get(mlir::MLIRContext *ctx, unsigned width);
  unsigned getWidth() const;
};
```

`get` is the standard interner factory; pointer equality on
`BitsType` instances implies value equality (per FR-008 + research
§5).

## 6. Stability surface

Once M4 is merged, the following are FROZEN:

1. **Op-class set**: 70 ops + 2 auto-generated terminators (post-merge
   amendment 2026-05-01: `nsl.constant` is the 42nd; post-merge
   amendment 2026-05-02 Phase A: the 28-op expression surface adds the
   43rd–70th). Adding a 71st is an M4 amendment + design §7 update
   (per SC-012). Removing or renaming an op is a MAJOR version change.
2. **Op-class trait set per op**: per the data-model.md table.
   Adding a trait is a minor amendment; removing one (especially
   `Symbol` or `SymbolTable`) is MAJOR (consumers rely on the
   trait machinery).
3. **Type set**: 3 types. Same amendment rules.
4. **Op TableGen mnemonic** (`nsl.module`, `nsl.proc`, etc.): part
   of the textual IR contract; renaming is MAJOR (every fixture
   under `test/Dialect/` plus M5 lowering needs to be rewritten).
5. **`registerNSLDialect` signature**: stable.

## 7. Consumer responsibilities

- M5 `nsl-lower` SHOULD include `NSLDialect.h` once via the
  umbrella header; SHOULD NOT include any `.h.inc` directly.
- M5 `nsl-lower` SHOULD use the TableGen-generated op-builder API
  (`ModuleOp::build(...)`) rather than raw `mlir::OpBuilder::create<>()` 
  with hand-constructed `OperationState`.
- M5/M6 lowering passes SHOULD NOT mutate the dialect's TableGen
  records or hand-written verifier bodies; if a structural
  invariant turns out to be wrong, it's an M4 amendment, not an M5
  workaround.
- Tooling-track consumers (T2/T3/T6) at their respective milestones
  SHOULD continue to consume `libNSLFrontend.a` (Sema's domain) for
  symbol/type queries; they MAY consume the dialect for
  `-emit=mlir`-output inspection but MUST NOT take dialect-level
  cross-references as authoritative for source-level analyses
  (Sema is authoritative for that).
