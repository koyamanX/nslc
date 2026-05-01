<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Feature Specification: M4 — `nsl` MLIR Dialect (`nsl-dialect`: TableGen ops + types + verifiers + `nsl-opt` round-trip)

**Feature Branch**: `007-m4-mlir-dialect`
**Created**: 2026-04-30
**Status**: Draft
**Input**: User description: "M4"

> **Scope interpretation.** "M4" maps to the **M4** row of
> [`README.md`](../../README.md) §Roadmap, which delivers the next
> compiler-track library: `nsl-dialect` (7) — the **MLIR dialect**
> defining every `nsl.*` operation and `!nsl.*` type that subsequent
> milestones (M5 lowering, M6 CIRCT, M7 end-to-end) consume. The same
> row defines the milestone's test gate ("**`nsl-opt foo.mlir → verify
> → print → diff foo.expected.mlir` for every op listed in
> `nsl_compiler_design.md` §7**, all green") and its constitutional
> anchors (VI dialect tests; III stock CIRCT below the dialect — no
> hand-rolled passes). The NSL-feature → milestone roll-up in
> [`CLAUDE.md`](../../CLAUDE.md) §1 confirms which language-spec rows
> land here: every grammar row's "Lower to dialect" column carries an
> M4 entry naming its dialect op (e.g., `declare → M4 (nsl::DeclareOp)`
> shorthand for "declare contents become ports/attributes on
> `nsl::ModuleOp`"; `module → M4 (nsl::ModuleOp)`; `func / proc /
> state defns → M4 (FuncInOp, FuncOutOp, ProcOp, StateOp)`; `par /
> alt / any → M4 (AltOp, AnyOp)`; `seq / if / for / while → M4 (SeqOp)
> + M5 generate unroll`; transfers + control calls + finish + system
> tasks → `M4 (TransferOp)` + the `sim_*` family).
>
> **What lands as a deliverable.** A new static library `nsl-dialect`
> (layer 7 per
> [`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md)
> §3 lines 132–148), built via the `add_nsl_library` macro from M0,
> with a single public umbrella header
> `include/nsl/Dialect/NSL/IR/NSLDialect.h` (per Constitution
> Principle II's single-public-header rule — `nsl-dialect` is NOT an
> exception; the umbrella re-exports the TableGen-generated per-op
> headers). The library depends only on `nsl-basic` and the upstream
> MLIR `IR` / `Support` headers (NOT on `nsl-ast` or `nsl-sema`: the
> dialect is independent of the AST). A TableGen + ODS `.td` file
> defines every op listed in
> [`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md)
> §7 (lines 882–931) plus the marker / structural-expansion ops
> introduced in §§8–10 (`nsl.fire_probe`, `nsl.struct_cast`,
> `nsl.field`, `nsl.goto`, `nsl.case`, `nsl.default`,
> `nsl.structural_generate`, plus implicit terminator ops). Three
> dialect types (`!nsl.bits<N>`, `!nsl.struct<@StructName>`,
> `!nsl.mem<[D x T]>`) round-trip via the default printer/parser. Each
> op carries a `hasVerifier = 1` hook with a structural-invariant
> verifier (parent-op kind, region count + kind, attribute
> presence/type, operand/result type relations); the dialect
> registration handler is wired into MLIR's `mlir::DialectRegistry`.
> An operational `nsl-opt` developer/test binary (per Constitution
> Principle II's developer-tool clause; the M0-stubbed target) loads
> the dialect and round-trips `.mlir` text. A test corpus under
> `test/Dialect/` covering every op + every type with at minimum a
> round-trip pass fixture (`<op>_roundtrip.mlir`) and at least one
> verifier-reject fixture (`<op>_invalid.mlir`) where structural
> invariants exist to assert.
>
> **What does NOT land at M4.** AST → `nsl` dialect lowering and any
> `nsl.*` → `nsl.*` rewrite pass (M5) — including
> `NSLResolveParamsPass`, `NSLExpandGeneratePass`,
> `NSLExpandVariablesPass`, `NSLExplodeSubmodArrayPass`,
> `NSLInlineInternalFuncPass`, `NSLCheckSemanticsPass` per design §9.
> No `nsl.*` → CIRCT conversion (M6). No new `-emit=*` flag in
> `nslc` — `-emit=mlir` lands at **M5** per
> [`README.md`](../../README.md) §Usage status block. End-to-end
> Verilog (M7), formal (M8), tagged release (M9) are all
> forward-looking. The M3 driver surface (`nslc -emit=tokens` /
> `-emit=ast`) is **unchanged** at M4 — M4 adds a developer/test
> binary (`nsl-opt`), not a user-facing driver flag. The
> `Compilation` class itself is **created at M4** (the M3 driver
> used free functions per `lib/Driver/EmitTokens.cpp` / `EmitAST.cpp`
> / `Sema.cpp` — design §11's class definition was target-state,
> not extant code). The new class skeleton ships with a constructor
> that calls `loadDialect<NSLDialect>()` plus stub `lowerToNSL` /
> `runNSLPasses` member functions; their **signatures are frozen
> at M4** but their **bodies land at M5**.

## Clarifications

### Session 2026-04-30

- Q: M4 op verifier strictness — what does each `nsl.*` op's `hasVerifier = 1` hook check (structural invariants only; structural + cheap post-Sema; or full `Sn` re-check)? → A: **Option A — structural invariants only.** The verifier asserts parent-op kind via `HasParent<...>`, region count + kind, attribute presence/type, and trait-declared operand-result type relations (`SameOperandsElementType`, `SameOperandsShape`, `SingleBlockImplicitTerminator`, etc.). Re-checking of `S1`–`S29` semantic constraints is OUT of scope at M4 — those are Sema's M3 domain. Rationale: preserves the architectural seam between Sema (the single semantic checker) and the dialect (the IR), avoids drift between two parallel checkers, matches MLIR upstream conventions, and aligns with Constitution Principle III ("stock CIRCT below the dialect" → clean separation of concerns). Expected fail-fixture cardinality: ~50 invalid cases (≈ 41 ops × ~1.22 invariants each, post-Q6).
- Q: "Parent" relation semantics in FR-013 — for rows marked "parent (transitively) = X", does the verifier check immediate parent only (TableGen `HasParent`), any ancestor (custom walk), or a widened immediate-parent set? → A: **Option B — any-ancestor walk via custom verifier.** Rows in FR-013 marked "parent (transitively) = X" are checked by a hand-written `verify()` body that walks `op->getParentOp()` upward until it finds an ancestor of kind X or hits the top of the op tree. Affects ~5 ops (`nsl.while`, `nsl.for`, `nsl.finish`, `nsl.goto` label-form, plus any future transitive-parent op). Standard MLIR `HasParent<...>` TableGen trait covers the immediate-parent rows (the bulk of FR-013); custom ancestor-walk covers the ~5 transitive rows. Rationale: NSL grammar permits intervening control-flow ops (`nsl.parallel`, `nsl.alt`, `nsl.any`, `nsl.if`) between an enclosing `nsl.seq` and a `nsl.while` body — strict immediate-parent (Option A) would force the M5 AST→MLIR lowering to rewrite block shapes or insert wrapper ops; Option B keeps M5's lowering structurally faithful to the AST and pushes the validation cost to ~5 small custom verifiers. Option C (widened immediate-parent set) is laxer than NSL grammar and would silently accept `nsl.alt { nsl.while }` without any enclosing `nsl.seq` (a `S8` violation that the dialect should still surface even though Sema also catches it at M3).
- Q: `nsl.connect` operand type-match semantics — strict `mlir::Type` equality, bidirectional `!nsl.bits<N>` ↔ `!nsl.struct<@T>` width-equality, or CIRCT-`hw`-style element-and-width compatibility? → A: **Option A — strict `mlir::Type` equality.** `ConnectOp::verify()` rejects any pair of operands whose `mlir::Type`s are not pointer-equal (types are interned in `MLIRContext` per FR-008, so pointer equality implies type equality). The M5 AST→MLIR lowering is responsible for inserting `nsl.struct_cast` (per FR-010) at any user-written `struct ↔ bits` conversion site; the dialect verifier never tolerates implicit reinterpretation. Rationale: aligns with Constitution Principle III's clean architectural seam (the dialect is shape-only IR; semantic type-compatibility lives in Sema + M5 lowering); matches CIRCT's `hw`/`comb`/`seq` dialect conventions where verifiers check exact-type equality; M5 lowering burden is small (~3 lines per cast site, and the `nsl.struct_cast` op is already in the M4 op set for exactly this purpose). Option B (bidirectional bits↔struct) was rejected because it would permit silent reinterpretation across the seam and obscure user-intent in the printed IR. Option C (CIRCT-`hw`-style) was rejected because NSL has no narrowing/widening implicit conversions in source; the strict-equality form is honest about that.
- Q: `nsl.func` scoped-name attribute encoding — `mlir::SymbolRefAttr` with multi-segment, `mlir::StringAttr` containing the literal dotted form, or a custom `NSL_ScopedNameAttr`? → A: **Option A — `mlir::SymbolRefAttr` with multi-segment.** _**SUPERSEDED by Q5 — see below; this option contradicted MLIR's `SymbolOpInterface` contract (sym_name must be `StringAttr`), surfaced by `/speckit-analyze` Pass 3 finding F14.**_
- Q: `nsl.func` `sym_name` encoding — re-clarification of Q4 per F14 (`SymbolOpInterface` requires `sym_name: StringAttr`, not `SymbolRefAttr`). Choose: A' (StringAttr literal dotted form), A'' (StringAttr local-name + optional `submodule_qualifier: FlatSymbolRefAttr`), or A''' (nested-region symbol scoping)? → A: **Option A' — `sym_name: StringAttr` containing the literal dotted form.** Bare-form `nsl.func @reset` stores `sym_name = "reset"`; dotted-form `nsl.func @ic.ready` stores `sym_name = "ic.ready"` as a single `StringAttr`. MLIR symbol identifiers permit `.` per upstream lexical rules (matches `func.func @some.dotted.name` precedent). Cross-op references (`nsl.call @ic.ready`) use `FlatSymbolRefAttr` with literal-string match against `sym_name`. Rationale: simplest encoding compatible with `SymbolOpInterface`; matches MLIR upstream convention verbatim per Constitution Principle II/III; no new public-attribute surface (the `dialect-api.contract.md` §2 freeze surface stays at 47 public types — note Q6 below subsequently grows it to 48 with the addition of `nsl.field_decl`). Option A'' (`submodule_qualifier`) was rejected because it adds a new attribute to the API surface for no tangible benefit — `nsl.call @ic.ready` resolves the same way under both encodings. Option A''' (nested-region scoping) was rejected as overengineered for M4 — would force the M5 lowering to construct nested symbol-table regions for every dotted-form func.
- Q: `nsl.field` op overloading — Phase 3 fixture-author (`nsl-test-author` agent) surfaced two distinct roles: (1) struct-decl form (`nsl.field "name" : !nsl.bits<N>` inside `nsl.struct`), (2) access-marker form (`nsl.field %v {index = K} : !nsl.struct<@T> -> !nsl.bits<N>`). Spec FR-010 + design §8 line 1061 named only the access-marker form. How should the dialect express both? → A: **Option B — two-op split.** Add a new dialect op `nsl.field_decl` for the struct-internal field-declaration role; keep `nsl.field` for the access-marker role only. FR-010 grows from 40 to 41 named ops; FR-013 gains an invariant row for `nsl.field_decl` (parent = `nsl.struct`; `sym_name` StringAttr present); SC-012's "next op" baseline updates from "41st op" to "42nd op"; `dialect-api.contract.md` §2 freeze surface grows from 47 to 48 public types/functions. Phase 3's `Types/struct_roundtrip.mlir` will need its in-struct-body `nsl.field` uses renamed to `nsl.field_decl`; a new `test/Dialect/marker/field_decl_roundtrip.mlir` fixture covers the new op per FR-017. Rationale: cleanest disambiguation; one op per role; matches MLIR's "small single-purpose ops" principle. Option A (attribute-encoded fields) was rejected because it loses per-field SourceRange granularity that NSL source's `struct S { a[4]; b[12]; }` line-by-line layout naturally supplies. Option C (custom assemblyFormat with branching) was rejected because the TableGen complexity outweighs the cost of a second op record. Option D (type-level encoding) was rejected because it forces hand-written type printer/parser (losing `useDefaultTypePrinterParser`) AND requires substantial Phase 3 fixture rewrite.

### Session 2026-05-01 (post-merge amendment)

- Q: M5 expression-lowering surfaced a missing primitive — every `LiteralExpr` lowering needs an `mlir::Value` of `!nsl.bits<N>` to feed `nsl.transfer`'s `SameTypeOperands`-constrained `$src`, and `hw.constant` produces `iN` (cross-dialect type). Choose between (a) amend M4 to add `nsl.constant` (Pure + ConstantLike, `I64Attr:$value`, `NSL_AnyBits:$result`); (b) inline a `hw.constant` + `unrealized_conversion_cast` helper at every literal site; (c) relax `nsl.transfer`'s `SameTypeOperands` constraint to admit cross-dialect operands; (d) defer the M5 expression-lowering pivot until M6 (where `hw` is the natural producer dialect). → A: **Option (a) — amend M4.** Add `nsl.constant <value> : !nsl.bits<N>` as a Pure + ConstantLike op with an `I64Attr:$value` and a hand-written verifier asserting `value` fits in the result-type width. Rationale: (b) introduces a permanent dialect-bridge wart at every literal site and clouds the IR; (c) erodes the dialect's strict-type invariant (Q3 Option A) which the architectural seam relies on; (d) blocks all of M5's expression-lowering work until a much later milestone, defeating the purpose of having an `nsl` IR layer at all. Cost: one new op record (TableGen + verifier ~30 LOC + two fixtures); freeze surface grows 48 → 49 (per `contracts/dialect-api.contract.md` §2 post-merge note); SC-012's "next op" baseline updates from "42nd op" to "43rd op"; Phase 3 fixture corpus grows by `test/Dialect/storage/constant_roundtrip.mlir` + `test/Dialect/storage/constant_invalid_overflow.mlir`. Widths > 64 bits are deferred to a future amendment (the `value` attr is `I64Attr` per data-model §2 conventions). The op carries no `Symbol` trait — it's a value-producer, not a named declaration. Cross-reference: `specs/008-m5-structural-passes/research.md` §15 documents the M5-side reasoning; `specs/008-m5-structural-passes/spec.md` Assumptions bullet for "M4-frozen op set" updates the count from 48 to 49.

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Round-trip every `nsl.*` op via `nsl-opt` (Priority: P1)

A contributor authors a hand-written `.mlir` fixture exercising one of
the dialect's ops (or a small group of related ops), runs
`nsl-opt fixture.mlir`, and observes that the output is byte-identical
to a committed `expected.mlir` (or that piping through `nsl-opt` a
second time produces a stable form). Round-trip implies three
post-conditions for every op in the dialect: (1) the textual form
parses to an `mlir::Operation*` of the correct kind, (2) the operation
verifies (`hasVerifier = 1` returns success on well-formed input),
(3) the operation re-prints to the same canonical text (modulo MLIR's
default whitespace-normalization rules). The same property holds for
each of the three dialect types (`!nsl.bits<N>`, `!nsl.struct<@T>`,
`!nsl.mem<[D x T]>`).

**Why this priority**: This **is** the M4 acceptance gate — the
README's M4 row literally specifies "`nsl-opt foo.mlir → verify →
print → diff foo.expected.mlir` for every op listed in
`nsl_compiler_design.md` §7, all green." Without this, M5 has no
target IR to lower into and no IR-printer-stable test surface to
write FileCheck assertions against; M6 has no source dialect to
convert from; M7 cannot run end-to-end. Constitution Principle VI
names "**Dialect tests** use `nsl-opt` for round-trip verification of
`.mlir`" as the layer's canonical test driver. P1 because every
downstream milestone consumes the dialect's textual form.

**Independent Test**: Build the project (the `nsl-dialect` library
and the `nsl-opt` binary). For every op in the per-op table in
FR-010, ship `test/Dialect/<op_category>/<op>_roundtrip.mlir`
exercising the op's well-formed shape; assert via lit + FileCheck
that `nsl-opt %s | nsl-opt -` produces output matching the same
file's `// CHECK:` lines. For each of the three dialect types, ship
`test/Dialect/Types/<type>_roundtrip.mlir` doing the same on a
trivial op carrying that type. The test gate is mechanical:
every op in the dialect's `.td` file MUST have a matching
`<op>_roundtrip.mlir` (CI guard verifies the file's existence by
enumerating the registered op classes). Does not depend on US2
(verifier rejection) or US3 (driver invariant) — this is the
well-formed-input round-trip path.

**Acceptance Scenarios**:

1. **Given** a fixture `nsl.module @M { ... }` containing a single
   empty module declaration, **When** `nsl-opt module_roundtrip.mlir`
   runs, **Then** stdout contains `nsl.module @M`, the run exits zero
   with no diagnostics, AND `nsl-opt %s | nsl-opt -` is a fixed
   point (second pass produces byte-identical output).
2. **Given** a fixture exercising `nsl.reg "q" : !nsl.bits<8> = 0`
   inside an `nsl.module`, **When** `nsl-opt` parses-verifies-prints,
   **Then** the printed form preserves the init attribute, the
   `!nsl.bits<8>` type, AND the symbol-name attribute "q".
3. **Given** a fixture exercising `nsl.proc @P { nsl.first_state @s0
   nsl.state @s0 { nsl.goto @s1 } nsl.state @s1 { ... } }` (procedure
   with two states and a transition), **When** `nsl-opt` runs,
   **Then** the round-trip preserves: (a) the proc's `Symbol` +
   `SymbolTable` traits' implicit terminator, (b) the `nsl.first_state`
   attribute-like marker as a child op of the proc region, (c) the
   `nsl.state` ops as siblings under the proc region, (d) the
   `nsl.goto @s1` op's symbol reference.
4. **Given** a fixture exercising `nsl.alt { nsl.case %c { ... }
   nsl.case %d { ... } nsl.default { ... } }`, **When** `nsl-opt`
   runs, **Then** the round-trip preserves the case/default child
   ordering AND the priority-ordering implicit in `nsl.alt` (vs.
   `nsl.any`).
5. **Given** a fixture exercising every action-block op (`nsl.seq`,
   `nsl.parallel`, `nsl.if`, `nsl.for`, `nsl.while`) nested inside
   `nsl.func` / `nsl.proc` regions appropriately, **When** `nsl-opt`
   runs, **Then** every op verifies clean and round-trips.
6. **Given** a fixture exercising `nsl.transfer` (`=` lowered) and
   `nsl.clocked_transfer` (`:=` lowered) on a wire and a reg
   respectively, **When** `nsl-opt` runs, **Then** both ops verify
   clean and the `SameOperandsElementType` constraint is observable
   (mismatched-type fixture is rejected, but that is US2's domain).
7. **Given** a fixture exercising the simulation-only ops
   (`nsl.sim_display`, `nsl.sim_finish`, `nsl.sim_init`,
   `nsl.sim_delay`), **When** `nsl-opt` runs, **Then** all four
   round-trip.
8. **Given** a fixture exercising the marker / lowering-helper ops
   introduced in design §§8–10 (`nsl.fire_probe`, `nsl.struct_cast`,
   `nsl.field`, `nsl.case`, `nsl.default`, `nsl.goto`,
   `nsl.structural_generate`), **When** `nsl-opt` runs, **Then** all
   round-trip and verify (their semantic consumers — M5 expansion
   passes, M6 CIRCT lowering — are forward-looking; M4 only asserts
   shape).
9. **Given** any successful pass through `nsl-opt`, **When** the
   diagnostic stream is queried, **Then** it is empty (zero
   verifier diagnostics, zero MLIR-built-in diagnostics).

---

### User Story 2 — Verifier rejects malformed `nsl.*` ops with source-locating diagnostics (Priority: P1)

A contributor authoring a hand-written `.mlir` fixture that violates
one of an op's structural invariants (e.g., an `nsl.seq` placed
directly under `nsl.module` rather than inside an `nsl.func`; an
`nsl.first_state` placed outside any `nsl.proc`; an `nsl.module`
without its `SymbolNameAttr`; an `nsl.transfer` whose source and
destination element types disagree) sees a single-line diagnostic of
the form `<path>:<line>:<col>: error: 'nsl.<op>': <message>` (the
standard MLIR `op->emitOpError(...)` format) AND `nsl-opt` exits
non-zero. The diagnostic's `mlir::Location` resolves to the offending
op's text-position in the input `.mlir` (since hand-written fixtures
have no upstream NSL source, the location is the `.mlir` file
itself). When the same op is consumed downstream by an AST-built
MLIR (M5 onward), the location resolves to the originating NSL
`SourceRange` per Constitution Principle IV.

**Why this priority**: A round-trip test that doesn't exercise the
verifier is vacuous — the test gate's "verify" step (per the README
M4 row) requires the verifier to reject malformed input as readily
as it accepts well-formed input. Without verifier rejection, the M5
AST→MLIR lowering can ship invalid IR undetected; the M6 conversion
patterns assume well-formed input from the dialect's verifier; LSP
/ IDE tooling that walks the dialect's IR (a future T-track
opportunity once M5 lands `-emit=mlir`) needs verifier-clean IR for
its analyses. Shares P1 with US1 because the M4 test gate requires
both well-formed-acceptance AND malformed-rejection. Per
Clarifications session 2026-04-30 Q1 (Option A), the verifier's
scope is **structural invariants only** — Sema-equivalent re-checks
of `S1`–`S29` are NOT in scope (those run at M3, before lowering;
duplicating them in the dialect violates the architectural seam of
Principle III).

**Independent Test**: For every op with at least one structural
invariant, ship a fixture
`test/Dialect/<category>/<op>_invalid_<reason>.mlir` containing an
ill-formed instance, asserted via lit's `// expected-error{{...}}`
syntax (MLIR's standard mechanism: every diagnostic on or near the
annotated line MUST match the substring; missing-or-mismatched
diagnostic fails the test). The fixture exercises ONE invariant
violation per file, with the substring matching the op-name + the
short invariant name (e.g.,
`expected-error{{'nsl.seq' op expects parent op 'nsl.func'}}`). Does
not depend on US1 (round-trip), since invalid fixtures intentionally
fail before round-trip; depends on the verifier hooks themselves
being implemented per FR-011.

**Acceptance Scenarios** (one per invariant family — exhaustive list
in FR-013):

1. **Given** a fixture with `nsl.module @M { nsl.seq { ... } }`
   (an `nsl.seq` directly under `nsl.module`, not inside an
   `nsl.func`), **When** `nsl-opt` runs, **Then** the verifier emits
   `error: 'nsl.seq' op expects parent op 'nsl.func'`, the
   diagnostic locates to the `nsl.seq` line, AND the run exits
   non-zero.
2. **Given** a fixture with `nsl.first_state @s0` placed at top
   level (not inside an `nsl.proc`), **When** `nsl-opt` runs,
   **Then** the verifier emits
   `error: 'nsl.first_state' op expects parent op 'nsl.proc'`.
3. **Given** a fixture with `nsl.state @s0 { ... }` placed inside
   `nsl.module` (not inside `nsl.proc`), **When** `nsl-opt` runs,
   **Then** the verifier emits
   `error: 'nsl.state' op expects parent op 'nsl.proc'`.
4. **Given** a fixture with `nsl.transfer %a, %b` where `%a` has
   type `!nsl.bits<8>` and `%b` has type `!nsl.bits<16>` (operand-
   width mismatch, the `SameOperandsElementType` trait's domain),
   **When** `nsl-opt` runs, **Then** the verifier emits an
   element-type-mismatch diagnostic at the transfer's location.
5. **Given** a fixture with an `nsl.module` op missing its
   `SymbolNameAttr` (`nsl.module { ... }` instead of
   `nsl.module @M { ... }`), **When** `nsl-opt` runs, **Then** the
   parser-or-verifier rejects the op with a
   `missing 'sym_name' attribute` or equivalent MLIR-built-in
   diagnostic (the `Symbol` trait machinery enforces this).
6. **Given** a fixture with `nsl.alt { }` containing zero `nsl.case`
   children (the empty-alt structural shape, distinct from the
   `S13` semantic question of priority semantics), **When**
   `nsl-opt` runs, **Then** the verifier emits an
   "alt requires at least one case or default" diagnostic.
7. **Given** any successful round-trip pass through `nsl-opt`,
   **When** the verifier was invoked, **Then** zero
   `nsl-opt` exit-code regressions occur on the well-formed
   fixtures (US1 must remain green).

---

### User Story 3 — Driver and `-emit=*` surface from M0–M3 unchanged (Priority: P2)

A contributor running `nslc --version`, `nslc -emit=tokens foo.nsl`,
or `nslc -emit=ast foo.nsl` after M4 lands sees byte-identical
output to a pre-M4 invocation on the same input under the same
flags. M4 introduces the `nsl-dialect` library and the `nsl-opt`
binary, but adds **no new `-emit=*` flag** to `nslc` (per the M4
scope: `-emit=mlir` lands at M5). The driver binary loads the
dialect into its `mlir::MLIRContext` (per design §11 line 1145:
`mlirCtx_.loadDialect<nsl::dialect::NSLDialect>()`) so that M5's
`Compilation::lowerToNSL()` body has a registered dialect to build
ops in, but the driver exposes no observable behavior change at
M4 — the dialect is dormant from `nslc`'s perspective until M5.

**Why this priority**: This is a regression-guard story, not a new
feature, so P2 (lower than US1/US2). It exists because the dialect
build inserts a non-trivial dependency edge (`nsl-driver` now links
against `nsl-dialect` and the upstream MLIR `IR` library) and any
careless wiring could change `nslc`'s startup behavior, error
output, or determinism. Constitution Principle V (deterministic
pipeline) and Principle II (layered structure with downward-flowing
deps) both apply: the driver's output MUST stay byte-stable, and
the new dependency edge MUST flow correctly (driver → dialect, never
the reverse).

**Independent Test**: Run `nslc --version`, `nslc -emit=tokens` on
the M1 corpus, and `nslc -emit=ast` on the M2/M3 corpus before and
after merging M4. Diff stdout/stderr. Both MUST be byte-identical.
The CI's existing M1/M2/M3 lit fixture set is the test corpus; no
M4-specific fixtures needed beyond a build-and-run smoke. CI guards
the link-time dependency direction (per FR-005) by failing fast if
`nsl-dialect` ever appears as a build-time dep of `nsl-ast`,
`nsl-sema`, `nsl-parse`, `nsl-lex`, `nsl-preprocess`, or
`nsl-basic`.

**Acceptance Scenarios**:

1. **Given** the same input as a pre-M4 `nslc -emit=tokens foo.nsl`
   invocation, **When** the post-M4 binary runs on the same flags,
   **Then** stdout, stderr, AND exit code are byte-identical.
2. **Given** the same input as a pre-M4 `nslc -emit=ast foo.nsl`
   invocation, **When** the post-M4 binary runs, **Then** stdout,
   stderr, AND exit code are byte-identical (Sema's resolved-type
   and decl-loc enrichments per M3's FR-020 are unchanged at M4).
3. **Given** any input file that triggered an M3-era diagnostic,
   **When** the post-M4 `nslc` runs on the same flags, **Then** the
   diagnostic message text and exit code match — M4 introduces no
   new diagnostic source on the parser/sema path.
4. **Given** a post-M4 `nslc --help` invocation, **When** the help
   text is printed, **Then** the `-emit=*` choices listed are
   exactly `tokens` and `ast` (NOT `mlir` — per the M4 scope).
   Any future amendment that exposes `-emit=mlir` without the M5
   AST→MLIR lowering body is rejected at code-review time as a
   spec/design coupling violation (Principle VII).

---

### Edge Cases

- An empty `.mlir` file. `nsl-opt` MUST parse it as an empty
  `builtin.module`, verify clean (no `nsl.*` ops to verify), and
  exit zero.
- A `.mlir` file containing only a comment. Same behavior — the
  parser ignores the comment; the verifier has nothing to do.
- A `.mlir` file containing only `builtin.module` ops with no
  `nsl.*` content (i.e., the dialect is loaded but unused).
  `nsl-opt` MUST round-trip this as a no-op.
- A nested `nsl.module` inside another `nsl.module` (forbidden by
  `HasParent<"::mlir::ModuleOp">` on `nsl::ModuleOp`). The
  verifier MUST emit the standard parent-op-mismatch diagnostic.
- An `nsl.alt` with exactly one `nsl.case` and no `nsl.default`.
  Per Q1 Option A (structural-only), the verifier accepts this —
  the `S13` "alt-priority-with-only-one-case" lint observation is
  **NOT** an M4 verifier concern (it's a future `S002` / `S003`
  lint at T6 per [`CLAUDE.md`](../../CLAUDE.md) §2.2).
- An `nsl.proc` with no `nsl.first_state` and no `nsl.state`
  children (an empty proc). Per Q1 Option A, the verifier accepts
  this — the `S28` "first_state must exist if any state exists"
  check is Sema's M3 domain, not the M4 verifier's.
- An `nsl.struct` with zero fields. The verifier accepts this; the
  type `!nsl.struct<@EmptyStruct>` carries `totalWidth = 0`. Per
  M3's edge-case handling (zero-member struct; Sema accepts), this
  is consistent.
- A `!nsl.bits<0>` (zero-width bit-vector type). Per design §7
  line 981 ("These lower bijectively to CIRCT's `i<N>`"), CIRCT's
  `i0` is a degenerate but valid type; `!nsl.bits<0>` MUST
  round-trip without verifier rejection (downstream CIRCT lowering
  at M6 will treat it as the zero-width identity).
- A `!nsl.mem<[0 x !nsl.bits<8>]>` (zero-depth memory type). Same
  reasoning — degenerate but parseable; downstream concerns are
  M5's `NSLExpandVariablesPass` / M6's `seq.firmem` lowering.
- An `nsl.connect %sub.port, %sig` where `%sub` does not resolve
  to an `nsl.submodule` instance. Symbol resolution is handled by
  MLIR's standard `SymbolRefAttr` machinery (the parser rejects
  unresolved symbol refs); the M4 verifier does NOT additionally
  check that the symbol's kind is `nsl.submodule` (per Q1
  Option A).
- An `nsl.structural_generate` op with malformed loop-bound
  attributes (e.g., a non-integer attribute where an integer was
  required). The op's TableGen attribute-type-check MUST reject;
  the verifier additionally MUST NOT crash on partially-parsed
  forms (a hard MLIR convention).
- A `.mlir` file mixing `nsl.*` ops with `hw.*` / `comb.*` /
  `seq.*` / `fsm.*` ops. The dialect registration in `nsl-opt`
  MUST register the CIRCT dialects too (since they appear in the
  `Compilation` ctor's `loadDialect` list at design §11 lines
  1146–1150) so that mixed-dialect fixtures can be hand-authored
  for forward-compatibility tests; pure round-trip on `nsl.*`-only
  input MUST behave identically whether the CIRCT dialects are
  registered or not.
- A `nsl-opt` invocation with no input file (stdin mode). MLIR
  convention: read from stdin; the M4 binary MUST follow this
  convention (the same flag layout as upstream `mlir-opt`).

## Requirements *(mandatory)*

### Functional Requirements

**Library deliverables (M4 layer per [`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md) §3, lines 132–148):**

- **FR-001**: The build MUST produce the static library `nsl-dialect`
  at the layer-7 position, exposing a single public umbrella header
  `include/nsl/Dialect/NSL/IR/NSLDialect.h`. The library depends on
  `nsl-basic` (for `SourceManager` / `SourceRange` /
  `DiagnosticEngine`-bridging types) AND on the upstream MLIR `IR`
  + `Support` libraries (and any `Pass`-related headers needed for
  `mlir::Dialect` registration). It MUST NOT depend on `nsl-ast`,
  `nsl-sema`, `nsl-parse`, `nsl-lex`, or `nsl-preprocess` (per
  Constitution Principle II's downward-flowing-deps rule and the
  dialect's role as an AST-independent IR).
- **FR-002**: The library MUST be declared via the `add_nsl_library`
  macro from M0; its dependencies MUST be expressed exclusively via
  that macro's `DEPENDS` argument plus the standard MLIR-library
  glob (`MLIR_LIBS` or equivalent). The TableGen integration MUST
  use the standard CMake helpers (`add_mlir_dialect`,
  `add_mlir_doc`, `mlir_tablegen`) so that adding a new op is an
  in-band `.td` edit + regeneration, not a hand-written
  `IR/NSLOps.cpp.inc`.
- **FR-003**: `nsl-dialect` MUST NOT introduce any cyclic dependency
  on `nsl-ast` or `nsl-sema`. The dialect IS NOT the AST; it is a
  separate IR. CI MUST guard this with a static dependency-graph
  assertion (re-using M3's FR-003 mechanism).
- **FR-004**: The `Compilation` class described in design §11 is
  **created at M4** (the M3 driver uses free functions per
  `lib/Driver/EmitTokens.cpp` / `EmitAST.cpp` / `Sema.cpp`
  precedent — `Compilation` did not exist as a class pre-M4;
  design §11 was target-state, not extant code). M4 introduces
  the minimal class skeleton in `include/nsl/Driver/Compilation.h`
  + `lib/Driver/Compilation.cpp` carrying (a) a `DiagnosticEngine&`
  and an `mlir::MLIRContext` member, (b) a constructor that calls
  `mlirCtx_.loadDialect<nsl::dialect::NSLDialect>()` (per design
  §11 line 1145), and (c) declarations of the `lowerToNSL` and
  `runNSLPasses` member functions. Their **bodies** are M5
  deliverables; at M4 they MUST exist as trivial stubs that emit
  a diagnostic ("MLIR lowering not yet implemented; see M5") if
  ever invoked. The `nslc` driver MUST NOT expose `-emit=mlir` at
  M4 (per FR-024) — invoking the stub from `nslc` is unreachable
  via the public CLI. M5 will extend `Compilation` with the full
  per-stage pipeline (`preprocess()` → `parse()` → `sema()` →
  `lowerToNSL()` → `runNSLPasses()` → `lowerToCIRCT()` →
  `runCIRCTPasses()` → `emit()` per design §11 lines 1156–1166)
  AND with the CIRCT-dialect-load lines per design §11 lines
  1146–1150 (those are loaded by `nsl-opt` only at M4, since the
  driver never reaches a CIRCT-emitting stage at M4).
- **FR-005**: The CI dependency-graph guard MUST prevent any of
  `nsl-basic`, `nsl-preprocess`, `nsl-lex`, `nsl-parse`, `nsl-ast`,
  `nsl-sema` from gaining a dependency edge into `nsl-dialect`.
  Adding such an edge is a Constitution Principle II violation
  (lower layers do not depend on higher layers). The guard MUST run
  in CI's static-checks stage (per Principle IX stage 2).

**Dialect registration & types (per [`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md) §7, lines 940–981):**

- **FR-006**: The `nsl-dialect` library MUST register a single MLIR
  dialect named `"nsl"` with `cppNamespace = "::nsl::dialect"` (per
  design §7 line 945) and `useDefaultTypePrinterParser = 1`. The
  dialect's registration entry-point function (e.g.,
  `nsl::dialect::registerNSLDialect(mlir::DialectRegistry&)`) MUST
  be exported in the umbrella public header so consumers (driver,
  `nsl-opt`) call it once at startup.
- **FR-007**: The dialect MUST define three MLIR types per design §7
  line 981, each with a default printer/parser symmetric round-trip:
  - `!nsl.bits<N>` — N-wide bit-vector, integer parameter `N : i64`
    (the bit width). Lowers bijectively to CIRCT's `i<N>` at M6.
  - `!nsl.struct<@StructName>` — structural type with an
    `mlir::SymbolRefAttr` to a sibling `nsl.struct` definition.
    Lowers to CIRCT's `hw.struct<...>` at M6.
  - `!nsl.mem<[D x T]>` — memory type with `D : i64` depth and
    `T : Type` element. Lowers to CIRCT's `hw.array<D x T>` at M6.
- **FR-008**: Each of the three dialect types MUST round-trip
  byte-identically through MLIR's default printer/parser when
  exercised in a round-trip fixture (per FR-018). The
  `useDefaultTypePrinterParser = 1` declaration covers this when
  combined with TableGen's standard type-class generation.

**Operation set (per [`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md) §7 lines 882–931, plus marker / lowering-helper ops introduced in §§8–10):**

- **FR-009**: The dialect MUST define ALL ops listed in design §7's
  "Operation summary" block (lines 884–930) — the ops are formally
  enumerated below in FR-010 — plus the marker / lowering-helper
  ops referenced in §§8–10 (`nsl.fire_probe`, `nsl.struct_cast`,
  `nsl.field`, `nsl.case`, `nsl.default`, `nsl.goto`,
  `nsl.structural_generate`, plus auto-generated terminator ops
  for region-bearing parents). **No op may be omitted from the M4
  deliverable** — per the README test gate ("Every `nsl::*` op
  verifies"). Conversely, no op may be added beyond what design
  §§7–10 names without a same-PR design-doc update (Principle VII).

- **FR-010**: The full op table — the dialect's M4 surface — is:

  | Category | Op (`nsl.*`) | TableGen class | Traits / parents | Spec / design anchor |
  |---|---|---|---|---|
  | Module-level | `nsl.module` | `NSL_ModuleOp` | `Symbol`, `SymbolTable`, `SingleBlockImplicitTerminator<"ModuleTerminatorOp">` | design §7 line 949 |
  | Module-level | `nsl.struct` | `NSL_StructOp` | `Symbol`, container of `nsl.field` declarations | design §7 line 887 |
  | Module-level | `nsl.submodule` | `NSL_SubmoduleOp` | `Symbol`, parent = `nsl::ModuleOp` | design §7 line 888 |
  | Module-level | `nsl.connect` | `NSL_ConnectOp` | parent = `nsl::ModuleOp`; structural wiring | design §7 line 889 |
  | Storage | `nsl.reg` | `NSL_RegOp` | parent = `nsl::ModuleOp`; init attribute | design §7 line 892 |
  | Storage | `nsl.wire` | `NSL_WireOp` | parent = `nsl::ModuleOp` | design §7 line 893 |
  | Storage | `nsl.variable` | `NSL_VariableOp` | parent = `nsl::ModuleOp` | design §7 line 894 |
  | Storage | `nsl.mem` | `NSL_MemOp` | parent = `nsl::ModuleOp` | design §7 line 895 |
  | Control terminal | `nsl.func_in` | `NSL_FuncInOp` | parent = `nsl::ModuleOp`; arg + ret attrs | design §7 line 898 |
  | Control terminal | `nsl.func_out` | `NSL_FuncOutOp` | parent = `nsl::ModuleOp` | design §7 line 899 |
  | Control terminal | `nsl.func_self` | `NSL_FuncSelfOp` | parent = `nsl::ModuleOp` | design §7 line 900 |
  | Action block | `nsl.alt` | `NSL_AltOp` | priority-encoded; children = `nsl.case` / `nsl.default` | design §7 line 903 |
  | Action block | `nsl.any` | `NSL_AnyOp` | parallel; children = `nsl.case` / `nsl.default` | design §7 line 904 |
  | Action block | `nsl.if` | `NSL_IfOp` | two-region (then / else) | design §7 line 905 |
  | Action block | `nsl.parallel` | `NSL_ParallelOp` | one-region | design §7 line 906 |
  | Action block | `nsl.seq` | `NSL_SeqOp` | one-region; parent = `nsl::FuncOp` | design §7 line 907 |
  | Action block | `nsl.while` | `NSL_WhileOp` | parent = `nsl::SeqOp` | design §7 line 908 |
  | Action block | `nsl.for` | `NSL_ForOp` | parent = `nsl::SeqOp`; two shape variants | design §7 line 909 |
  | Action helper | `nsl.case` | `NSL_CaseOp` | parent = `nsl::AltOp` or `nsl::AnyOp` | design §7 lines 903–904 |
  | Action helper | `nsl.default` | `NSL_DefaultOp` | parent = `nsl::AltOp` or `nsl::AnyOp` | design §7 lines 903–904 |
  | Atomic | `nsl.transfer` | `NSL_TransferOp` | `SameOperandsElementType`, `SameOperandsShape`; wire-style `=` | design §7 line 912 |
  | Atomic | `nsl.clocked_transfer` | `NSL_ClockedTransferOp` | reg-style `:=` | design §7 line 913 |
  | Atomic | `nsl.incdec` | `NSL_IncDecOp` | reg-style ++/-- with kind enum | design §7 line 914 |
  | Atomic | `nsl.call` | `NSL_CallOp` | symbol ref to `func_in` / `func_out` / `func_self` / `proc_name` | design §7 line 915 |
  | Atomic | `nsl.finish` | `NSL_FinishOp` | parent = `nsl::ProcOp` (per design §10 line 1104) | design §7 line 916 |
  | Atomic | `nsl.finish_method` | `NSL_FinishMethodOp` | symbol ref to `nsl::ProcOp`; called from peer module | design §7 line 917 |
  | Atomic | `nsl.invoke_method` | `NSL_InvokeMethodOp` | symbol ref to `nsl::ProcOp` | design §7 line 918 |
  | Procedure | `nsl.proc` | `NSL_ProcOp` | `Symbol`, `SymbolTable`, parent = `nsl::ModuleOp`, `SingleBlockImplicitTerminator<"ProcTerminatorOp">` | design §7 lines 921, 958 |
  | Procedure | `nsl.first_state` | `NSL_FirstStateOp` | parent = `nsl::ProcOp`; symbol ref child | design §7 line 922 |
  | Procedure | `nsl.state` | `NSL_StateOp` | `Symbol`, parent = `nsl::ProcOp`; one region | design §7 line 923 |
  | Procedure | `nsl.func` | `NSL_FuncOp` | `Symbol`, parent = `nsl::ModuleOp`; **`sym_name` is `StringAttr` containing the literal dotted form** (per Q5 Option A'; bare-form stores `"reset"`, dotted-form per `N7` stores `"ic.ready"`; cross-op refs use `FlatSymbolRefAttr` literal-match) | design §7 line 924 |
  | Procedure helper | `nsl.goto` | `NSL_GotoOp` | symbol ref; parent = `nsl::SeqOp` (label form) or `nsl::StateOp` (state form) | design §7 line 923; design §10 lines 1101–1102 |
  | System task | `nsl.sim_display` | `NSL_SimDisplayOp` | sim-only; format-string + var-args | design §7 line 927 |
  | System task | `nsl.sim_finish` | `NSL_SimFinishOp` | sim-only | design §7 line 928 |
  | System task | `nsl.sim_init` | `NSL_SimInitOp` | sim-only; one-region | design §7 line 929 |
  | System task | `nsl.sim_delay` | `NSL_SimDelayOp` | sim-only; integer-literal cycles attr | design §7 line 929 |
  | Marker | `nsl.fire_probe` | `NSL_FireProbeOp` | `S27` 1-bit-tap marker; symbol ref to control-terminal | design §8 line 1062 |
  | Marker | `nsl.struct_cast` | `NSL_StructCastOp` | bits → struct preserve | design §8 line 1061 |
  | Marker | `nsl.field` | `NSL_FieldOp` | struct field access (expression position) | design §8 line 1061 |
  | Marker | `nsl.field_decl` | `NSL_FieldDeclOp` | struct-internal field declaration; parent = `nsl::StructOp`; `sym_name` StringAttr + width type | per Q6 Option B (Session 2026-04-30) |
  | Expansion-only | `nsl.structural_generate` | `NSL_StructuralGenerateOp` | one-region; consumed by M5's `NSLExpandGeneratePass` | design §9 line 1073 |
  | Auto-terminators | `nsl.module_terminator`, `nsl.proc_terminator`, ... | (auto) | per-parent implicit-terminator ops | design §7 lines 950, 959 |

  *Total: 41 named ops + auto-generated terminators. Adding a 42nd
  is a routine PR provided design §7 (or §8/§9/§10) is updated in
  the same change per Principle VII; M4's scaffolding is
  layer-extensible (per SC-009).*

  *Note on op ordering: the categories above mirror design §7's
  "Operation summary" block layout (module-level → storage →
  control-terminal → action-block → atomic → procedure → system-
  task → marker → expansion-only). The TableGen `.td` file MAY
  preserve this ordering or sort alphabetically — either is
  acceptable per FR-002.*

**Verifier hooks (per Clarifications session 2026-04-30 Q1 → Option A):**

- **FR-011**: Every op in the FR-010 table MUST carry
  `hasVerifier = 1` in its TableGen record AND a custom
  `LogicalResult ::verify();` implementation in
  `lib/Dialect/NSL/IR/NSLOps.cpp` (or per-category
  `NSL<Category>Ops.cpp` if FR-002 splits the file). The verifier's
  scope is **structural invariants only** per Q1 Option A — parent-
  op kind, region count + kind, attribute presence/type, and
  operand-result type relations. Re-checking of `S1`–`S29` semantic
  constraints is OUT of scope at M4 — those are Sema's M3 domain.
  Per Q2 Option B, parent-op-kind invariants split by row in
  FR-013 into two implementation styles: (a) rows specifying
  `parent = X` (immediate parent) MUST use the standard MLIR
  `HasParent<X>` TableGen trait, with no hand-written body for
  that check; (b) rows specifying `parent (transitively) = X`
  (any-ancestor) MUST use a hand-written `verify()` body that
  walks `op->getParentOp()` upward until it finds an ancestor of
  kind X or hits the top of the op tree. Trait-declared
  operand-result type relations (`SameOperandsElementType`,
  `SameOperandsShape`, `SingleBlockImplicitTerminator`, etc.) are
  encoded purely in TableGen.
- **FR-012**: For every op whose structural invariant set is non-
  empty, the verifier MUST emit a diagnostic of the form
  `error: 'nsl.<op>': <invariant violated>` via
  `op->emitOpError(...)`. The diagnostic's `mlir::Location`
  resolves to the offending op's `loc()` attribute (which carries
  the AST node's `SourceRange` post-M5; in M4 hand-written
  fixtures, it's the `.mlir` text-position). Diagnostic message
  text is **NOT** frozen by fail-case fixture text-asserts (unlike
  Sema's M3 `Sn` diagnostics) — fixture asserts use FileCheck's
  `// expected-error{{<substring>}}` syntax matching only the
  op-name + the invariant name, tolerating MLIR upstream wording
  drift.
- **FR-013**: The structural-invariant set per op is enumerated
  below. **Adding to this set requires a same-PR design-doc
  update** (Principle VII). Each invariant in this table is paired
  with a `<op>_invalid_<reason>.mlir` fixture per FR-019. Rows
  specifying `parent = X` use the TableGen `HasParent<X>` trait
  (immediate parent); rows specifying `parent (transitively) = X`
  use a hand-written ancestor-walk verifier (per Q2 Option B; see
  FR-011). The table:

  | Op | Structural invariants checked at M4 |
  |---|---|
  | `nsl.module` | parent = `builtin.module`; `sym_name` attribute present |
  | `nsl.struct` | parent = `builtin.module`; `sym_name` attribute present; field list non-circular |
  | `nsl.submodule` | parent = `nsl.module`; symbol ref resolves at the `Symbol` trait level |
  | `nsl.connect` | parent = `nsl.module`; **strict `mlir::Type` equality on operands** (per Q3 Option A; the dialect's interner per FR-008 makes this a pointer-equality check; M5 inserts `nsl.struct_cast` for any user-written struct↔bits conversion) |
  | `nsl.reg` | parent = `nsl.module` or `nsl.proc`; result type is `!nsl.bits<N>` or `!nsl.struct<@T>` |
  | `nsl.wire` | parent = `nsl.module`; result type is `!nsl.bits<N>` |
  | `nsl.variable` | parent = `nsl.module` or `nsl.func`; result type is `!nsl.bits<N>` |
  | `nsl.mem` | parent = `nsl.module`; result type is `!nsl.mem<[D x T]>` |
  | `nsl.func_in`, `nsl.func_out`, `nsl.func_self` | parent = `nsl.module` |
  | `nsl.alt`, `nsl.any` | one region; **≥ 1 child total** (one-or-more `nsl.case`, optionally followed by one `nsl.default`; OR a lone `nsl.default`); case-ordering is preserved (priority for `nsl.alt`, semantic-don't-care for `nsl.any` — but the verifier itself does NOT distinguish) |
  | `nsl.if` | two regions (then, else); else region MAY be empty |
  | `nsl.parallel` | one region |
  | `nsl.seq` | parent = `nsl.func`; one region |
  | `nsl.while` | parent (transitively) = `nsl.seq`; one region |
  | `nsl.for` | parent (transitively) = `nsl.seq`; one region; loop-bound attributes shape (the two C-style vs. enum forms) |
  | `nsl.case`, `nsl.default` | parent = `nsl.alt` or `nsl.any` |
  | `nsl.transfer` | `SameOperandsElementType`, `SameOperandsShape` |
  | `nsl.clocked_transfer` | first operand kind = reg-like (`nsl.reg` result or `nsl.field`-of-reg-struct); type-match on operands |
  | `nsl.incdec` | first operand kind = reg-like; kind enum attribute valid |
  | `nsl.call` | symbol ref present; arg count matches the resolved control-terminal op's arg count (resolved via SymbolTable; failure to resolve is a parser/Symbol-trait error, not a verifier error) |
  | `nsl.finish` | parent (transitively) = `nsl.proc` |
  | `nsl.finish_method`, `nsl.invoke_method` | symbol ref present |
  | `nsl.proc` | parent = `nsl.module`; `sym_name` attribute present; at most one `nsl.first_state` child |
  | `nsl.first_state` | parent = `nsl.proc`; symbol ref present; resolves to a sibling `nsl.state` |
  | `nsl.state` | parent = `nsl.proc`; `sym_name` attribute present |
  | `nsl.func` | parent = `nsl.module`; `sym_name` attribute present |
  | `nsl.goto` | parent (transitively) = `nsl.seq` (label form, target = sibling label op) or = `nsl.state` (state-name form, target = sibling `nsl.state`) |
  | `nsl.sim_*` | parent = `nsl.module` (system-task placement) |
  | `nsl.fire_probe` | symbol ref to a sibling `nsl.func_in` / `nsl.func_out` / `nsl.func_self` |
  | `nsl.struct_cast`, `nsl.field` | type-match on operand and result; `nsl.field` carries an integer attribute for the field index |
  | `nsl.field_decl` | parent = `nsl.struct`; `sym_name` StringAttr present; result type is `!nsl.bits<N>` (or `!nsl.struct<@T>` for nested struct fields per future amendment) |
  | `nsl.structural_generate` | one region; loop-bound attributes shape |

  *Cardinality (per Q1 Option A — structural-only): ~41 ops
  × ~1.25 invariants each ≈ 50 distinct invalid-fixture cases.
  The exact count is mechanical: every row above whose
  "invariants" cell names ≥ 1 invariant ships ≥ 1
  `<op>_invalid_<reason>.mlir` fixture per FR-019.*

**`nsl-opt` developer/test binary (per Constitution Principle II's developer-tool clause; the M0-stubbed target):**

- **FR-014**: The build MUST produce an `nsl-opt` binary at
  `tools/nsl-opt/main.cpp`, linked against `nsl-dialect`,
  `MLIRIR`, `MLIRSupport`, `MLIROptLib` (the upstream
  `MlirOptMain` driver), AND the CIRCT dialects loaded by
  `Compilation` per design §11 lines 1146–1150 (`hw`, `comb`,
  `seq`, `fsm`, `sv`). The binary MUST register the `nsl` dialect
  via the registration entry point exported by the `nsl-dialect`
  umbrella header (FR-006).
- **FR-015**: `nsl-opt` MUST support stdin input (no positional
  argument → read from stdin), stdout output, and the standard
  `mlir-opt` flag layout (per the upstream `MlirOptMain` driver:
  `--allow-unregistered-dialect`, `--verify-diagnostics`,
  `-o <output>`, etc.). At M4, `nsl-opt` MUST register **zero
  passes** (passes are M5+; the `--<pass-name>` flag set is empty
  beyond MLIR's built-ins).
- **FR-016**: `nsl-opt` MUST be classified as a developer/test tool
  per Constitution Principle II §4 — it is **NOT** a user-facing
  T-track deliverable. Its install rule MUST be optional (build
  tree only by default; `cmake --install` does not place it in a
  release tarball). Documentation references to `nsl-opt` MUST
  cite it as a developer/test binary (per design §13 line 1262
  comment "mlir-opt for the nsl dialect"), parallel to LLVM's
  `mlir-opt` and CIRCT's `circt-opt`.

**Test gates (per Constitution Principles VI and VIII; lit + FileCheck per Principle VI's "Dialect tests use `nsl-opt` for round-trip verification of `.mlir`"):**

- **FR-017**: Every op in the FR-010 table MUST have a corresponding
  `test/Dialect/<category>/<op>_roundtrip.mlir` fixture exercising
  its well-formed shape, with `// CHECK:` lines asserting the
  printed form contains the op-name and the structurally-significant
  attributes / types / operands. The fixture's lit run-line is
  `// RUN: nsl-opt %s | FileCheck %s` (single-pass round-trip)
  AND `// RUN: nsl-opt %s | nsl-opt - | FileCheck %s`
  (two-pass round-trip; asserts the second pass is a fixed point).
- **FR-018**: For every dialect type (`!nsl.bits<N>`,
  `!nsl.struct<@T>`, `!nsl.mem<[D x T]>`), `test/Dialect/Types/`
  MUST contain a round-trip fixture exercising the type on a
  representative op (e.g., `!nsl.bits<8>` on an `nsl.wire`,
  `!nsl.struct<@S>` on an `nsl.reg`, `!nsl.mem<[256 x !nsl.bits<8>]>`
  on an `nsl.mem`).
- **FR-019**: For every structural invariant in the FR-013 table
  (every cell with ≥ 1 invariant), `test/Dialect/<category>/` MUST
  contain at least one `<op>_invalid_<reason>.mlir` fixture
  triggering that invariant, asserted via lit's
  `// expected-error{{<substring>}}` syntax. The substring matches
  the op-name + the invariant-shape (e.g.,
  `expects parent op 'nsl.func'`), tolerating MLIR upstream
  wording drift.
- **FR-020**: Every fixture under `test/Dialect/` MUST be authored
  before its driving implementation (Principle VIII TDD); the test
  commit MUST be observed failing prior to the implementation
  commit being accepted. The TDD evidence path (failing-CI link
  in the PR description) is the standard mechanism (per M3's
  FR-028).
- **FR-021**: A CI guard MUST verify that for every op enumerated
  in FR-010 there exists a `<op>_roundtrip.mlir` fixture, AND for
  every cell in FR-013 with ≥ 1 invariant there exists at least
  one `<op>_invalid_*.mlir` fixture. A missing fixture is a build
  failure (parallel to M3's FR-023 mechanical guard).

**Driver invariant (per US3):**

- **FR-022**: `nslc -emit=tokens` and `nslc -emit=ast` outputs MUST
  be byte-identical pre- vs. post-M4 on every fixture in the M1
  / M2 / M3 corpus. The `nslc --version` output MAY change to
  reflect the MLIR / CIRCT version pins per design §13 line 1221
  (a routine version-string update), but no flag-set or behavior
  changes are permitted at M4.
- **FR-023**: The `nslc` driver's `-emit=*` choice list at M4 is
  exactly `{tokens, ast}`. The CompileOptions::EmitKind enum (per
  design §11 lines 1128–1130) MAY contain `NSLMLIR`, `CIRCT`, `HW`,
  `Verilog` for source-stable forward-compatibility, but the CLI
  parser MUST reject any `-emit=*` value beyond `{tokens, ast}` at
  M4. Adding `-emit=mlir` is the M5 deliverable.
- **FR-024**: `nslc --help` output MUST list the available
  `-emit=*` choices as exactly `tokens` or `ast`. A help-text
  change advertising `mlir`/`hw`/`verilog` before its lowering
  body lands is a Principle VII spec/design coupling violation
  and rejected at code-review time.

**Determinism (Constitution Principle V):**

- **FR-025**: `nsl-opt` output on the same input under the same
  flags MUST be byte-identical across (a) two consecutive runs,
  (b) Debug and Release build types, (c) gcc and clang compilers.
  This re-states M3's FR-029 for the M4 dialect surface.
- **FR-026**: All collection types whose iteration order is part
  of any printed-IR output (e.g., the `nsl.module`'s child-op
  ordering, the `nsl.alt`'s case ordering, attribute-dictionary
  ordering on every op) MUST be deterministic — no
  `unordered_map` / `DenseMap` iteration in serialization paths.
  MLIR's standard `Operation::getRegions()` / `Block::getOps()`
  iteration order is insertion-ordered and satisfies this; the M4
  verifier code MUST NOT introduce an unordered iteration in any
  diagnostic-producing path.
- **FR-027**: Symbol identity in printed IR (the `@<name>`
  references on every op carrying a `SymbolRefAttr`) MUST resolve
  by name, not by `Operation*` pointer. This re-states M3's FR-031
  for the dialect surface — pointer-derived ordering is forbidden.

**SPDX hygiene (per Constitution Build/Code/Licensing):**

- **FR-028**: 100% of files newly added under `lib/Dialect/`,
  `include/nsl/Dialect/`, `tools/nsl-opt/`, AND `test/Dialect/`
  MUST carry the `Apache-2.0 WITH LLVM-exception` SPDX header
  (M0 FR-010 hygiene re-stated for the M4 file set). TableGen
  `.td` files MUST carry the header as a `//` line-comment per
  `scripts/check_spdx.py`'s `.td` recipe.

### Key Entities

- **`NSLDialect`**: the MLIR dialect class (TableGen-generated
  bases + hand-written initialization in
  `lib/Dialect/NSL/IR/NSLDialect.cpp`); the entry point for
  `mlir::DialectRegistry::insert<NSLDialect>()`. Public type
  re-exported from `nsl-dialect`'s umbrella header.
- **`NSL_*Op`** (per FR-010): one TableGen op class per dialect
  op, generating a C++ class (`nsl::dialect::ModuleOp`,
  `nsl::dialect::ProcOp`, etc.) inheriting from `mlir::Op<...>`
  with the appropriate trait set. Defined entirely in
  `lib/Dialect/NSL/IR/NSLOps.td` (or per-category split per
  FR-002).
- **`!nsl.bits<N>`, `!nsl.struct<@T>`, `!nsl.mem<[D x T]>`**: the
  three dialect types (per FR-007), each generated from a
  TableGen `Type_Class` record; default-printed via
  `useDefaultTypePrinterParser = 1`.
- **`nsl-opt`**: the developer/test binary (per FR-014–FR-016);
  ships with the build but is **not** a user-facing T-track
  deliverable.
- **`mlir::Location`** (re-used from upstream MLIR): every
  `nsl.*` op's location attribute carries the source position;
  for hand-written `.mlir` fixtures this is the `.mlir` text
  position, for AST-built MLIR (M5+) this is the originating NSL
  `SourceRange` encoded as `FileLineColLoc` (per design §12 line
  1211 and Principle IV).
- **`mlir::DialectRegistry`** (re-used from upstream MLIR): both
  `nsl-opt` and the `nslc` driver call
  `nsl::dialect::registerNSLDialect(registry)` at startup; the
  registry hands the dialect to the `MLIRContext`.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: For every op enumerated in FR-010 (41 named ops +
  auto-generated terminators), `test/Dialect/<category>/
  <op>_roundtrip.mlir` exists, is authored before its
  implementation per Principle VIII TDD, AND passes lit + FileCheck
  on a green CI run. 0 of the 41 ops are without a round-trip
  fixture; 100% op coverage.
- **SC-002**: For every cell in the FR-013 invariant table with
  ≥ 1 structural invariant, `test/Dialect/<category>/
  <op>_invalid_<reason>.mlir` exists with `// expected-error{{...}}`
  asserting the substring "<op-name>" + the invariant-shape; ≥ 50
  distinct invalid-fixture cases total. 100% invariant coverage.
- **SC-003**: For each of the three dialect types
  (`!nsl.bits<N>`, `!nsl.struct<@T>`, `!nsl.mem<[D x T]>`), a
  round-trip fixture exists under `test/Dialect/Types/` and
  passes (per FR-018).
- **SC-004**: `nsl-opt %s | nsl-opt -` is a fixed point on every
  round-trip fixture: the second-pass output is byte-identical to
  the first-pass output (per FR-017's two-pass run-line).
- **SC-005**: A diagnostic emitted by an `nsl.*` op verifier
  matches the regex `^[^:]+:\d+:\d+: error: 'nsl\.[a-z_]+' op .+$`
  (the standard `op->emitOpError(...)` form), with the diagnostic's
  `mlir::Location` resolving to a precise `file:line:col` per
  Principle IV. 0% deviation tolerated.
- **SC-006**: Two consecutive `nsl-opt` invocations on the same
  `.mlir` input under the same flag list produce byte-identical
  stdout — across both supported build types (Debug and Release)
  and both supported compilers (gcc and clang) (Principle V;
  FR-025; matches M0/M1/M2/M3's reproducibility gates).
- **SC-007**: `nslc -emit=tokens` and `nslc -emit=ast` output a
  byte-identical stdout/stderr pair pre- vs. post-M4 across the
  M1+M2+M3 fixture corpus; the `--version` string MAY change to
  reflect the new MLIR/CIRCT pins, but no other CLI behavior
  changes (FR-022).
- **SC-008**: A reviewer opening a red CI run from a dialect
  regression can identify the failing op + the failing invariant
  within 10 seconds, by reading the failing FileCheck or
  `expected-error` substring alone, without inspecting the
  TableGen or verifier source code (re-statement of M1 SC-008 /
  M2 SC-005 / M3 SC-006 for M4 fixtures).
- **SC-009**: After M4 lands, the M5 AST→`nsl` lowering work
  begins against a frozen dialect surface — all 41 ops + 3 types
  + the registration entry-point are stable, allowing
  `Compilation::lowerToNSL` (declaration shipped at M4 per FR-004,
  body landing at M5) to be implemented without additional dialect
  changes. This is the "M4 unlocks M5" property: a dialect change
  during M5 implementation is treated as an M4 amendment, not an
  in-band M5 PR.
- **SC-010**: 100% of files newly added under `lib/Dialect/`,
  `include/nsl/Dialect/`, `tools/nsl-opt/`, and `test/Dialect/`
  carry the `Apache-2.0 WITH LLVM-exception` SPDX header (M0
  FR-010 hygiene re-stated for the M4 file set; FR-028).
- **SC-011**: The `nsl-dialect` library's only build-time
  dependencies are `nsl-basic` and the upstream MLIR `IR` /
  `Support` libraries (the layered structure of Principle II). A
  CI guard MUST verify this — no link-time edge from
  `nsl-dialect` to `nsl-ast`, `nsl-sema`, `nsl-parse`, `nsl-lex`,
  `nsl-preprocess`, `nsl-lower`, or `nsl-driver`.
- **SC-012**: Adding a 42nd op to the dialect (a hypothetical
  language change in a later release) requires editing exactly
  the `.td` file, one new round-trip fixture, one or more
  invalid fixtures (one per structural invariant), one new row in
  FR-010's table (in the M4-spec amendment) AND the corresponding
  row in design §7 (per Principle VII coupling) — no edit to the
  M4 scaffolding itself (Principle II layer extensibility,
  applied to the dialect).

## Assumptions

- **Scope is the M4 row of `README.md` §Roadmap, the
  `nsl-dialect` library, the dialect's `.td` op + type set, the
  per-op verifier hook (structural invariants only per Q1 Option
  A), the `nsl-opt` developer/test binary, and the
  `test/Dialect/` fixture corpus.** No AST → MLIR lowering
  (M5). No structural-expansion passes (M5). No CIRCT lowering
  (M6). No `-emit=mlir` flag in `nslc` (M5). No new lint rules —
  `W001`–`H009` are tooling-track per
  [`CLAUDE.md`](../../CLAUDE.md) §2.2 and ride T6/T7. No
  audited-project regression (`P-VEN`, `P-VCD` gate M7). No
  formal (M8). No release (M9).
- **Per Clarifications session 2026-04-30 Q1 → Option A, the M4
  verifier's scope is structural invariants only.** Re-checking
  of `S1`–`S29` semantic constraints in the dialect is OUT of
  scope; those run at Sema (M3, before lowering). The
  architectural seam of Constitution Principle III ("stock CIRCT
  below the dialect") implies a clean separation: Sema is the
  authoritative semantic checker and the dialect is the IR.
  Hand-written `.mlir` test fixtures bypassing Sema receive
  structural-invariant diagnostics from the verifier (e.g.,
  "operation 'nsl.seq' not allowed inside 'nsl.module' — must be
  inside 'nsl.func'"); semantic ill-formedness in such fixtures
  is **not** caught by the M4 verifier and the test author is
  responsible for authoring well-formed-modulo-structural input.

- **Per Clarifications session 2026-04-30 Q2 → Option B, parent-
  op invariants split into two implementation styles by row in
  FR-013.** Immediate-parent rows use the standard MLIR
  `HasParent<X>` TableGen trait (declarative, no hand-written
  verifier body for that check). Any-ancestor rows — marked
  "parent (transitively) = X" in FR-013, covering ~5 ops
  (`nsl.while`, `nsl.for`, `nsl.finish`, `nsl.goto` label-form,
  plus any subsequent transitive-parent op added to the dialect)
  — use a hand-written `verify()` body that walks
  `op->getParentOp()` upward to find the named ancestor. The
  any-ancestor strategy preserves NSL's grammatical permission
  for intervening control-flow ops between, e.g., an enclosing
  `nsl.seq` and a contained `nsl.while`, so the M5 AST→MLIR
  lowering can stay structurally faithful to the AST without
  inserting wrapper ops. Diagnostic shape on rejection: the
  ancestor-walk verifier emits
  `error: 'nsl.<op>': must be enclosed by 'nsl.<ancestor>'` (per
  FR-012), with the standard `op->emitOpError(...)` form; fixture
  asserts use FileCheck's substring match per FR-019.

- **The `nsl-dialect` library exposes a single public umbrella
  header** (`include/nsl/Dialect/NSL/IR/NSLDialect.h`) per
  Constitution Principle II's "single public header" rule. The
  TableGen-generated per-op headers (`NSLOps.h.inc`,
  `NSLOpsDialect.h.inc`, etc.) are private build artifacts under
  the `nsl-dialect` library's internal include path; consumers
  reach them only via the umbrella. `nsl-dialect` is **NOT** an
  exception to the single-public-header rule (unlike `nsl-ast`
  and `nsl-sema`, which carry constitutional carve-outs per
  Principle II).
- **`nsl-opt` is a developer/test tool per Constitution Principle
  II §4, parallel to LLVM's `mlir-opt` and CIRCT's `circt-opt`.**
  It is NOT a user-facing T-track deliverable. Its install rule is
  optional; release tarballs do not bundle it. T-track bins
  (`nsl-lsp`, `nsl-fmt`, `nsl-lint`) link against
  `libNSLFrontend.a` (Sema's domain) and do NOT consume the
  dialect at all in their M4 form.
- **The `Compilation` class is created at M4** (the M3 driver
  used free functions; design §11's class was target-state, not
  extant code). The class skeleton ships at M4 with a constructor
  that calls `mlirCtx_.loadDialect<nsl::dialect::NSLDialect>()`
  plus stub `lowerToNSL` / `runNSLPasses` member functions whose
  signatures are frozen but whose bodies emit a
  "MLIR lowering not yet implemented; see M5" diagnostic. M5 will
  extend the class with the full per-stage pipeline (per design §11
  lines 1156–1166) AND with the CIRCT-dialect-load lines per design
  §11 lines 1146–1150. `nslc -emit=mlir` is **not** wired at M4
  (per FR-023/FR-024); the M5 spec opens that surface.
- **Diagnostic message text from the M4 verifier is NOT frozen by
  fail-case fixture text-asserts** (unlike Sema's `Sn`
  diagnostics, frozen at M3 per Principle VIII's
  diagnostic-string clause). The verifier's diagnostic format
  follows MLIR's standard `op->emitOpError(...)` shape; fixture
  asserts use FileCheck's `// expected-error{{<substring>}}`
  syntax matching only the op-name and the invariant name,
  tolerating MLIR upstream wording drift. This is a deliberate
  carve-out from Principle VIII's `Sn`/`Nn`/`Pn` clause because
  dialect invariants are NOT semantic constraints in NSL's spec —
  they are MLIR-layer structural rules whose canonical form is
  set by upstream MLIR conventions, and re-stating them with
  frozen text would defeat the convention.
- **Dialect ops referenced in design §§8–10 but not enumerated in
  §7's "Operation summary" block** (`nsl.fire_probe`,
  `nsl.struct_cast`, `nsl.field`, `nsl.case`, `nsl.default`,
  `nsl.goto`, `nsl.structural_generate`) **are part of the M4
  deliverable** per FR-009 — the design doc's §§7–10 are
  collectively the dialect's spec, with §7 listing the bulk and
  §§8–10 introducing markers and lowering-helpers in the context
  where they're consumed. Per Principle VII, design §7 SHOULD be
  amended to consolidate the full op list in a future PR; this is
  noted in the M4 spec's followup-plan and is NOT a blocker for
  M4.
- **MLIR + CIRCT version pins** are inherited from M0's
  `cmake/deps.lock` (per design §13 line 1221: "LLVM + MLIR
  CIRCT-pinned commit"). M4 does NOT bump pins; if a pin bump is
  needed (e.g., for a TableGen feature added upstream), it lands
  as a separate PR before the M4 implementation lands. The dev
  container `ghcr.io/koyamanx/nsl-nslc:dev` (per
  [`README.md`](../../README.md) §Building) ships the pinned
  LLVM/MLIR/CIRCT pre-staged; M4 is built and tested inside this
  container.
- **Audited-project ingestion (`P-VEN`) and golden VCDs (`P-VCD`)
  are out of scope** — they gate M7. CI's end-to-end and formal
  stages remain in the "wired-but-empty" state established at M0.
- **Tooling track** (T1–T12) implementation is out of scope. M4's
  job is to publish the dialect surface for M5–M7 consumption.
  T-track binaries do not consume the dialect at M4 (they consume
  `libNSLFrontend.a` only — Sema's domain).
- **Reference host, build matrix, CI infrastructure, SPDX hygiene,
  `add_nsl_library`, `DiagnosticEngine`, `SourceManager`, `Lexer`,
  `Preprocessor`, `Parser`, AST node hierarchy, SymbolTable,
  TypeSystem** are M0/M1/M2/M3 deliverables and are taken as
  given. This spec inherits and does not re-justify them.
