<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Feature Specification: M3 — Sema (`nsl-sema`: SymbolTable + TypeSystem + S1–S29)

**Feature Branch**: `006-m3-sema`
**Created**: 2026-04-28
**Status**: Draft
**Input**: User description: "M3"

> **Scope interpretation.** "M3" maps to the **M3** row of
> [`README.md`](../../README.md) §Roadmap, which delivers the next
> compiler-track library: `nsl-sema` (6) — the **SymbolTable + TypeSystem
> + per-`Sn` constraint checking** layer. The same row defines the
> milestone's test gate ("**One pass-case + one fail-case test per
> `S1`–`S29` (with diagnostic-string assertion per Principle VIII rule
> for `Sn` constraints)**, all green") and its constitutional anchors
> (VI sema tests NON-NEGOTIABLE; VIII test-first; I spec authority).
> The NSL-feature → milestone roll-up in
> [`CLAUDE.md`](../../CLAUDE.md) §1 confirms which language-spec rows
> land here ("Semantic constraints `S1`–`S29` → **M3 — one pass-case +
> one fail-case test each per Principle VI**"). The same roll-up's
> column "Lex / parse / sema" carries M3 entries on every grammar row
> that has a Sema-time interpretation (struct types `S18`, top-level
> params `S16`, `declare` modifier `S20`, internal-structure `S2`/`S6`/
> `S11`, defns `S6`/`S11`/`S21`/`S22`/`S26`/`S28`, statements `S7`–
> `S10` and `S13`, atomic actions `S3`/`S12`/`S21`, system tasks `S17`/
> `S29`, expressions `S14`/`S15`).
>
> **What lands as a deliverable.** A new static library `nsl-sema`
> (layer 6 per
> [`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md)
> §3 lines 132–148), with public headers `nsl/Sema/Sema.h`,
> `nsl/Sema/SymbolTable.h`, `nsl/Sema/TypeSystem.h`, depending only on
> `nsl-ast` (and transitively on `nsl-basic`). A `Compilation::sema()`
> stage in `nsl-driver` that runs after parse and before any later
> `-emit=*` lowering. A test corpus under `test/sema/` covering every
> `Sn` constraint with both a pass fixture and a fail fixture, the
> latter asserting the canonical diagnostic message text. Inside the
> AST, every `Expr` node's `inferredType()` slot reserved at M2 is
> filled with a `TypeRef` (post-width-inference) by Sema's run.
>
> **What does NOT land at M3.** The MLIR `nsl::*` dialect (M4),
> structural-expansion passes (M5), CIRCT lowering (M6), end-to-end
> `-emit=verilog` and the audited-project regression (M7), formal
> integration (M8), and tagged release (M9) are all forward-looking.
> No new lint rules — the lint *framework* itself is T6 and consumes
> M3's symbol table; lint rules `W001`–`H009` are tooling-track per
> [`CLAUDE.md`](../../CLAUDE.md) §2.2 and are NOT in M3 scope.

## Clarifications

### Session 2026-04-28

- Q: Sema fail-case shape for the constructive `Sn` (`S13`, `S18`,
  `S19`, `S23`, `S24`, `S27`) — those rows describe what Sema
  *constructs*, not what it *rejects*, so the standard "violate the
  rule → diagnostic" fail-case is asymmetric. Three options: (A)
  invent per-`Sn` rejection shapes (some contrived); (B) pair `pass.nsl`
  with a unit-test introspection assertion, and ship `fail.nsl` as the
  same input with the introspection-expected-value flipped; (C) drop
  the fail-case for those five `Sn` and amend Principle VI to carve
  them out. → A: **Option B (paired pass + introspection).** The
  diagnostic-string clause of Principle VIII is by construction
  inapplicable to constraints that produce no diagnostic, so
  substituting a structural-introspection assertion (e.g.,
  `StructTypeSymbol::fields[0]` MSB-first for `S18`, `MemSymbol::depth
  - len(init)` zero-fill count for `S24`, `Sema::classify(IdentifierExpr)
  → 1-bit-tap` for `S27`) honors Principle VI's "one pass + one fail
  per `Sn`" mandate literally without bending VIII or requiring a
  Constitution amendment. The "fail" fixture is the same `.nsl` input
  paired with a flipped expected-introspection value, so the test
  fails iff Sema diverges from the spec's constructive rule. This
  matches how Clang/CIRCT test constructive Sema rules in practice.
- Q: `-emit=ast` format-bump strategy at M3 to reflect Sema-resolved
  types — does the driver (A) re-cut the existing `-emit=ast`
  golden in place so the AST printer detects post-Sema input and
  prints `inferredType()` + resolved `Symbol*` `declLoc` inline,
  (B) add a new `-emit=ast-resolved` (or `-emit=sema`) flag for the
  post-Sema view and keep `-emit=ast` parser-only, or (C) skip
  printing resolution at M3 entirely (slot filled internally,
  observable only via diagnostics and via M5+ `-emit=mlir`)?
  → A: **Option A (re-cut in place).** This matches the M2 spec's
  pre-stated plan (M2 Clarifications session 2026-04-27 Q2: "M3
  adds Sema-resolved types, M5 adds expansion residue, etc.") and
  clang's `-ast-dump` evolution pattern (one flag, format drifts as
  the compiler matures, with the per-stage golden re-cut documented
  in the same patch as the format-bumping change). Keeps the CLI
  surface minimal; honors audited-corpus debuggability (a
  contributor sees resolution data via `nslc -emit=ast foo.nsl`
  without learning a new flag). The M2 parser-only `-emit=ast`
  golden is re-cut in the M3 patch as part of the deliverable.
- Q: Multi-error recovery granularity in Sema — does Sema (A) run
  each `Sn` check over the full AST as an independent pass with no
  suppression (~29 passes worst case; risks cascading errors via
  unresolved-name `Unresolved` types), (B) bail per-construct (one
  error on a declaration suppresses all later checks on that same
  declaration), or (C) run a hybrid: one top-down resolution pass
  (single error per unresolved name, no cascade per FR-017), then
  a per-`Sn` Option-A pass set for the rule checks themselves with
  resolution failures gating `Sn` checks on the unresolved subtree
  only? → A: **Option C (hybrid).** Matches the design doc's "Width
  inference is a single top-down pass" line
  ([`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md)
  line 856) for inference, preserves per-`Sn` independence for the
  rule checks, and is closest to clangd's actual style. Pure-A would
  emit cascading width-mismatch errors fed by an unresolved symbol
  (already forbidden by FR-017's "exactly one diagnostic" rule);
  pure-B would suppress sibling-declaration checks unnecessarily,
  defeating the `publishDiagnostics`-per-save UX that T3 needs.

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Resolve every name and infer every width across an NSL compilation unit (Priority: P1)

A contributor parses an NSL compilation unit (the M2 `CompilationUnit`
output) through Sema and receives back the same AST with three
post-conditions guaranteed: (1) every identifier reference resolves to
a `Symbol*` in the appropriate scope (global / declare / module /
proc / seq-or-parallel / function), (2) every `Expr` node's
`inferredType()` slot — reserved at M2 — is filled with a non-null
`TypeRef` denoting its inferred width / kind (`Bit`, `BitVector(N)`,
`Struct(name)`, `Memory(depth, element)`), and (3) every cross-scope
reference (`SUB.port` for a submodule port, `inst.finish()` for a
proc-method invocation per `N6`/`S21`, `func ic.ready { … }` for a
dotted-`func` definition per `N7`) resolves to the correct symbol in
the target scope. The Sema run also re-classifies AST nodes whose
identity was only-syntactic at M2 — most notably
control-terminal-name-as-1-bit-value occurrences (per `S27`),
which are tagged at the resolved-name level, and `proc`-method
invocations (per `S21`), which are recognized as built-in `finish` /
`invoke` calls rather than user-defined `func`s.

**Why this priority**: M4 dialect, M5 lowering, M6 CIRCT, M7
end-to-end, M8 formal, and M9 release all consume the **resolved**
AST — name resolution and width information are the contract those
layers depend on. Without resolved types, no lowering can build a
`hw::WireOp` of the right width or a `seq::CompRegOp` with the right
bit-count. Sema is also the milestone the *tooling* track gates on:
T2 (formatter), T3 (LSP skeleton), and T6 (lint framework) all read
the symbol table directly. Per
[`README.md`](../../README.md) §Roadmap, "**M3 is the unlock point**
— hitting M3 enables the bulk of the tooling track to start in
parallel with subsequent compiler work."

**Independent Test**: Build the project. Run `nslc` on the per-grammar
fixtures shipped at M2 (already covering every grammar production)
and assert that Sema completes without error and that every `Expr`
node in the resulting AST has a non-null `inferredType()`. For each
sub-section of [`docs/spec/nsl_lang.ebnf`](../../docs/spec/nsl_lang.ebnf)
that introduces a name kind (`§3` struct, `§4` declare ports + control
terminals, `§6` internals, `§7` defns), ship a focused fixture that
references each name from a sibling scope, and assert the resolved
`Symbol*` matches the declaration site by `SourceRange` equality.
Does not depend on US2 (per-`Sn` rejection) or US3 (multi-error
recovery) — this story is the well-formed-input path only.

**Acceptance Scenarios**:

1. **Given** a fixture defining a `module M` with a `wire w[8]` and a
   transfer `w = ...;`, **When** Sema runs, **Then** the
   `IdentifierExpr` for `w` resolves to a `WireSymbol` whose
   `declLoc` round-trips to the `wire w[8];` declaration, AND the
   `Expr` carrying `w`'s use site has `inferredType() ==
   BitVector(8)`.
2. **Given** a fixture with a `declare D { input clk; output q[16]; }`
   block and a `module M : D { … q = ...; }` where `q` is referenced
   in the module body, **When** Sema runs, **Then** the
   `IdentifierExpr` for `q` resolves to a `PortSymbol` (kind = output)
   declared in the matching `DeclareBlock`, AND the resolved
   `inferredType()` is `BitVector(16)`.
3. **Given** a fixture with a struct `struct S { a[4]; b[12]; }` and
   an instance `S inst;` in a module, **When** Sema runs, **Then**
   the `FieldAccessExpr` `inst.a` resolves to the `a` field of `S`
   with `inferredType() == BitVector(4)`, AND `inst.b` to
   `BitVector(12)`. The packed-layout MSB-first ordering (`S18`)
   is observable in the symbol's `FieldInfo` offset.
4. **Given** a fixture with a submodule instance `SubM sub(.p(x));`
   inside a parent module, **When** Sema runs, **Then** the `sub.p`
   reference resolves through the `SubmoduleSymbol`'s `templateDecl`
   to the corresponding `PortSymbol` in `SubM`'s declare scope.
5. **Given** any expression `8 # sig` where `sig` is an 8-bit wire,
   **When** Sema runs, **Then** the `SignExtendExpr`'s
   `inferredType()` is the requested width (here, the surrounding
   transfer's destination width, with `#` enforcing sign-extension
   semantics on the post-resolution width per Ref §0's
   "Estimation of bit width in operation" rules invoked by
   [`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md)
   line 856).
6. **Given** a fixture invoking `inst.finish()` from outside the
   `inst` proc body, **When** Sema runs, **Then** the
   `ControlCallStmt` is classified as a built-in proc method (per
   `S21` and `N6`) rather than a user-defined `func` call, and `inst`
   resolves to a `ProcSymbol` in the enclosing module scope.
7. **Given** any successful pass through Sema, **When** the
   diagnostic engine is queried, **Then** its diagnostic buffer is
   empty (zero errors, zero warnings unless the input legitimately
   triggers a non-error warning such as `S26` `function`-canonicalize
   or `N10` `label`-as-identifier).

---

### User Story 2 — Reject every `S1`–`S29` violation with a precise, source-locating diagnostic (Priority: P1)

A contributor parsing NSL source that *grammatically* parses but
*semantically* violates one of the constraints `S1`–`S29` enumerated
in [`docs/spec/nsl_lang.ebnf`](../../docs/spec/nsl_lang.ebnf) lines
826–1009 receives a single-line diagnostic of the form
`path:line:col: error: <message>` (per FR-025 of the M1 spec, re-used
unchanged), where the location is the offending construct's start and
the message names the constraint by spec citation
(e.g., `error: 'wire' may not have an initializer (S2)`). The
diagnostic message text for every `Sn` is *frozen* at M3 — the
fail-case test for that constraint cites the literal string, so any
future weakening or rename of the diagnostic is caught automatically
(per Constitution Principle VIII's `Sn`/`Nn`/`Pn` clause:
"The fail-case test MUST cite the specific diagnostic message string
that the constraint produces, so renaming or weakening the diagnostic
later is caught automatically").

**Why this priority**: The M3 row's named test gate is unambiguous —
"**One pass-case + one fail-case test per `S1`–`S29` (with diagnostic-
string assertion per Principle VIII rule for `Sn` constraints)**, all
green" — and per Constitution Principle VI it is **NON-NEGOTIABLE**
("VI (sema tests, NON-NEGOTIABLE)"). Shares P1 with US1 because both
are explicit M3 acceptance gates, and with US3 because per-`Sn`
diagnostics are useless if a single failure on one declaration
suppresses every later check.

**Independent Test**: For each of the 29 constraints, ship a passing
fixture pair under `test/sema/s<NN>/`: (i) `pass.nsl` containing a
construct that `Sn` accepts (asserted via Sema-clean run), and
(ii) `fail.nsl` containing the closest-possible construct that `Sn`
rejects, asserted via the literal diagnostic-message string. Where a
`Sn` carries a *warning-only* shape (e.g., `S26` `function` ≡ `func`
canonicalization), the fail-fixture asserts the warning text and a
zero exit code; where the `Sn` is genuinely an error, the fixture
asserts the error text and a non-zero exit. Coverage is
mechanically auditable: a CI guard verifies that `test/sema/s<NN>/`
exists and contains both files for every `NN ∈ {01..29}`. Does not
depend on US1's full resolution coverage (each fixture is small) or
US3's recovery (each fixture has exactly one error).

**Acceptance Scenarios** (one per `Sn` family — exhaustive list of all
29 is in FR-006's table):

1. **Given** an input with a `reg` named `foo__bar`, **When** Sema
   runs, **Then** an error diagnostic citing `S1` ("identifier may
   not contain '__'") is emitted at the identifier's source range
   and the run exits non-zero.
2. **Given** an input `wire w = 0;`, **When** Sema runs, **Then** an
   error diagnostic citing `S2` ("'wire' may not have an
   initializer; use 'reg' or struct-instance-reg") is emitted.
3. **Given** an input `reg r; r = 0;` (using `=` not `:=` on a reg),
   **When** Sema runs, **Then** an error citing `S3` is emitted with
   a fix-it hint suggesting `:=`.
4. **Given** an input where the dummy arg of a `func_in` is declared
   `output`, **When** Sema runs, **Then** an error citing `S4` is
   emitted at the dummy-arg declaration.
5. **Given** an input with a `seq { … }` block at the top of a module
   (not inside a `func` / `proc`), **When** Sema runs, **Then** an
   error citing `S7` is emitted at the `seq` keyword.
6. **Given** an input with `wire q = if (c) a;` (no `else`) at
   expression position, **When** Sema runs, **Then** an error citing
   `S14` is emitted with a fix-it hint suggesting `else`.
7. **Given** an input with a bit-slice index that is not a
   compile-time constant (e.g., `w[r:0]` where `r` is a reg), **When**
   Sema runs, **Then** an error citing `S15` is emitted at the
   index expression.
8. **Given** an input using `_display(...)` inside a module whose
   `declare` block has no `simulation` modifier, **When** Sema runs,
   **Then** an error citing `S17` is emitted at the call site.
9. **Given** an input with `return e;` whose width does not match the
   enclosing `func`'s return-value-terminal width, **When** Sema
   runs, **Then** an error citing `S22` (with both the expression
   width and the expected width in the message) is emitted.
10. **Given** an input declaring `func ic.ready` and using `function`
    as the keyword (per `S26`), **When** Sema runs, **Then** a
    *warning* (not error) citing `S26` is emitted suggesting the
    canonical `func` form, and the run exits zero.
11. **Given** an input with `goto S;` inside a `seq` block where `S`
    resolves to a `state_name` (not a label) — the `S25` cross-kind
    case — **When** Sema runs, **Then** an error citing `S25` is
    emitted at the `goto` statement.

---

### User Story 3 — Diagnose every Sema error in one Sema pass (multi-error reporting) (Priority: P1)

A contributor running Sema on input that contains *multiple
independent* `Sn` violations (e.g., a `wire` with an initializer in
one module *and* a `seq` outside a `func` in another, plus a width
mismatch in a `return` in a third) sees *all* the diagnostics in a
single Sema run, in source order, rather than a single-error bail
that suppresses every later check. Per the same principle adopted for
the parser at M2 (Clarifications session 2026-04-27 Q1 → Option A,
full multi-error recovery), Sema continues past each `Sn` failure to
keep diagnosing every later `Sn` site whose checking is independent
of the failed one.

**Why this priority**: Constitution Principle IV ("Diagnostics First")
plus the LSP-track requirement at T3 (`publishDiagnostics` reports
*all* errors per save, not first-only) plus the audited-corpus
ergonomics (a 12k-line audited NSL project with three early-stage
errors should report all three on one run, not require three
edit/recompile cycles) all point the same direction. Shares P1 with
US1/US2 because the M3 acceptance gate ("**all green**") implicitly
requires Sema not to crash or bail on any of the per-`Sn` test
inputs encountered during the corpus run.

**Independent Test**: (a) Provoke two independent `Sn` violations in
separate top-level modules and assert both diagnostics emit in source
order. (b) Provoke a `Sn` violation inside a `module` body whose
*non-failing* sibling declarations are still reachable, and assert
that those siblings' resolved types and symbol entries still appear
in the post-Sema AST. (c) Provoke a name-resolution failure (an
identifier with no declaration) and assert that *later* `Sn` checks
that depend on that identifier are *suppressed* (treating the symbol
as `Unresolved`) but `Sn` checks elsewhere in the compilation unit
are *unaffected* — the LSP user sees one resolution failure, not a
cascade of synthetic width-mismatch errors fed by the unresolved
symbol. Does not depend on US1's full grammar coverage or US2's
diagnostic-string accuracy — only on the recovery sets being
populated and the per-`Sn` checks being independent.

**Acceptance Scenarios**:

1. **Given** an input with two independent `Sn` violations in
   separate top-level modules (e.g., `S2` in module `A` and `S7` in
   module `B`), **When** Sema runs, **Then** **both** diagnostics
   are emitted in source order, the run exits non-zero, AND no
   later-stage emit (`-emit=mlir`, etc.) runs.
2. **Given** an input with a `Sn` violation inside one module's
   `module_item` and a *correct* sibling `module_item` after it,
   **When** Sema runs, **Then** the sibling's symbols still appear
   in the symbol table, AND the sibling's `Expr` nodes still have
   their `inferredType()` filled.
3. **Given** an input with a typo in an identifier (`foo` declared,
   `fooo` referenced), **When** Sema runs, **Then** a single
   "unresolved name 'fooo'" diagnostic is emitted at the reference
   site; downstream `Sn` checks that consume `fooo`'s
   `inferredType()` are suppressed (no cascading width-mismatch
   errors), AND `Sn` checks on identifiers elsewhere in the module
   still emit correctly.
4. **Given** any successful pass through Sema, **When** the
   diagnostic engine is queried, **Then** its diagnostic buffer is
   empty and the run reports success (zero errors).

---

### Edge Cases

- An empty compilation unit (zero top-level items). Sema MUST
  produce a fully-resolved (vacuously) `CompilationUnit` and exit
  successfully.
- A `module` whose body is empty. Sema MUST construct an empty
  module scope and exit successfully.
- A `declare D` block referenced by *zero* `module M : D` sites.
  Sema MUST register `D` in the global scope and emit no warning
  (M3 is not the lint layer; "unused declare" is a future T6 lint).
- A struct `struct S { … }` with zero members. Sema MUST register
  `S` as a `StructType` with `totalWidth() == 0`, and any later
  use of `S inst;` MUST resolve.
- A `proc` declared with `proc_name P;` but with no `state` body
  defined. Sema MUST emit a `S6`/`S11`-family error (depending on
  whether the proc requires at least one state for legal lowering)
  citing the spec line.
- The `function` keyword used in place of `func` (per `S26`
  parser-equivalence rule). Sema MUST emit a *warning* (not error),
  re-use the canonical name `func` in any later diagnostic that
  mentions the construct, and accept the input.
- The reserved keyword `label` used as a user identifier (per
  `N10` — flagged at M2 by the parser as a warning). Sema MUST
  accept the warning forwarded from M2 and not re-emit it.
- A `goto S;` whose target `S` is ambiguous between a label-name
  in the enclosing `seq` block and a `state_name` in the enclosing
  `proc` (the `S25` two-kind case). Sema MUST resolve by enclosing
  context (label-name preferred inside `seq`; `state_name` preferred
  inside `state` body) and emit a clear-diagnostic error if neither
  matches.
- A `mem M[D][W];` whose initializer is `{0}` (per `S24`'s "fewer
  values than depth → zero-fill"). Sema MUST accept this and treat
  the missing addresses as zero-initialized; per the
  paired-introspection resolution for constructive `Sn`
  (Clarifications session 2026-04-28 Q1 → Option B; see FR-013),
  the `pass.nsl` fixture is paired with a unit-test introspection
  assertion that `MemSymbol::initValues.size()` equals the declared
  depth with the trailing entries zero-filled, and the `fail.nsl`
  is the same input with the introspection-expected-value flipped.
  An overflow init (more values than depth) is a *separate* error
  case classified under the broader memory-init type-mismatch
  family rather than under `S24` itself.
- A `_init { … }` block at module top-level whose enclosing
  `declare` block has no `simulation` modifier (per `S29`). Sema
  MUST emit an `S29` error.
- A control-terminal name (`func_in` / `func_out` / `func_self` /
  `proc_name`) appearing in expression position (per `S27`). Sema
  MUST classify the reference as a 1-bit value tap, fill the
  `Expr::inferredType()` with `Bit` (width 1), and not emit any
  diagnostic.
- A bit-slice index expression that *grammatically* parses but
  evaluates to a non-integer constant (e.g., a string literal
  passed where an `_int` helper expected an integer). Sema MUST
  emit a `S15` diagnostic, *not* a generic type-mismatch
  diagnostic, so the message routes the user back to the spec.
- `nslc -emit=ast` invoked on a file that fails Sema. The driver
  MUST exit non-zero, MUST emit no AST on stdout (per FR-029-style
  "no partial output"), and MUST emit every Sema diagnostic on
  stderr (or via `--diagnostic-format=json`). When Sema succeeds,
  the printed AST is the *post-Sema* enriched form per FR-020 (Q2
  Option A); the parser-only AST is no longer a user-visible
  artifact through `nslc` at M3.

## Requirements *(mandatory)*

### Functional Requirements

**Library deliverables (M3 layer per [`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md) §3, lines 132–148):**

- **FR-001**: The build MUST produce the static library `nsl-sema` at
  the layer-6 position, exposing public headers `nsl/Sema/Sema.h`,
  `nsl/Sema/SymbolTable.h`, `nsl/Sema/TypeSystem.h`, depending only
  on `nsl-ast` (and transitively on `nsl-basic`). The "three public
  headers under one directory" pattern is allowed by Constitution
  Principle II for library-internal symbol/type/visitor surfaces
  (parallel to `nsl-ast`'s per-node-kind exception).
- **FR-002**: The library MUST be declared via the `add_nsl_library`
  macro from M0; its dependencies MUST be expressed exclusively via
  that macro's `DEPENDS` argument. `nsl-sema` MUST NOT depend on
  `nsl-dialect`, `nsl-lower`, or `nsl-driver` (Principle II layered
  structure: Sema does not consume the dialect or anything below it).
- **FR-003**: `nsl-sema` MUST NOT introduce any cyclic dependency on
  `nsl-parse`. Sema is *consumer* of the AST produced by parse, never
  the reverse — the parser at M2 must not gain a Sema-time check.
  CI MUST guard this with a static dependency-graph assertion.

**Symbol table (per [`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md) §6, lines 688–795):**

- **FR-004**: `nsl-sema` MUST define a `SymbolTable` class with the
  scope-stack semantics enumerated in design §6 ("Scope stack
  semantics" table, lines 786–793): Global / Declare / Module / Proc
  / Seq-or-Parallel / Function. Each scope kind MUST be opened and
  closed at the AST-traversal entry/exit of its corresponding node.
- **FR-005**: `nsl-sema` MUST define one `Symbol` subclass per
  declaration kind enumerated in design §6 lines 692–759:
  `PortSymbol`, `RegSymbol`, `WireSymbol`, `VariableSymbol`,
  `IntegerSymbol`, `MemSymbol`, `FuncInSymbol`, `FuncOutSymbol`,
  `FuncSelfSymbol`, `ProcSymbol`, `StateSymbol`, `SubmoduleSymbol`,
  `StructTypeSymbol`. Each carries the fields documented there
  (e.g., `PortSymbol::dir`, `RegSymbol::init`, `MemSymbol::depth`,
  `FuncInSymbol::args` and `ret`). Every `Symbol` carries a
  `SourceRange declLoc` round-trippable to the original (post-`#line`)
  declaration site.
- **FR-006**: Name resolution MUST walk outward from the current
  scope to the global scope; scoped references (`SUB.port`,
  `inst.finish`, `func ic.ready`) MUST resolve by first looking up
  the head identifier in the current scope, validating its kind, and
  then looking up the tail in the head's target scope (per design §6
  lines 794–795: "A scoped reference like `SUB.port` is resolved by
  looking up `SUB` in the current scope, confirming it's a
  `SubmoduleSymbol`, and then looking up `port` in the template's
  declare scope.").

**Type system (per [`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md) §6.x, lines 797–856):**

- **FR-007**: `nsl-sema` MUST define a `TypeSystem` class that
  interns `Type` values (`Bit`, `BitVector(N)`, `Struct(name)`,
  `Memory(depth, element)`, `Unresolved`) so that pointer equality
  implies type equality (per design §6.x line 799: "Types are
  immutable value objects interned in a `TypeSystem` so pointer
  equality implies type equality").
- **FR-008**: `nsl-sema` MUST run a width-inference pass that
  propagates widths top-down from transfer destinations to source
  expressions, per design §6.x line 856 ("Width inference is a
  single top-down pass that propagates widths from transfer
  destinations back to source expressions, using the rules in Ref
  §0's 'Estimation of bit width in operation.'"). The pass MUST
  fill the `Expr::inferredType()` slot reserved at M2 (per the M2
  spec FR-004) for every `Expr` node in the post-Sema AST.
- **FR-009**: Integer-typed sub-expressions MUST be resolved at
  Sema time per design §6.x line 856 ("Integer-typed sub-expressions
  are resolved at this point"). The structural-expansion integers
  (e.g., `generate`-loop indices, the `_int` preprocessor helper's
  results) are NOT in M3 scope — those are M5's
  `NSLExpandGeneratePass` (per design §9, lines 1048–1061).

**Per-`Sn` constraint checking (per [`docs/spec/nsl_lang.ebnf`](../../docs/spec/nsl_lang.ebnf) lines 826–1009; one pass + one fail per Constitution Principle VI / VIII):**

- **FR-010**: Sema MUST implement a check for every constraint in
  the table below, emitting a diagnostic of the documented severity
  at the offending construct's source range, with a message that
  cites the constraint by spec marker (e.g., `(S2)` in the message
  text):

  | `Sn` | Severity | Spec line | One-line summary |
  |---|---|---|---|
  | S1 | Error | `nsl_lang.ebnf:830` | `__` forbidden in identifiers |
  | S2 | Error | `nsl_lang.ebnf:832` | `wire` may not have an initializer |
  | S3 | Error (fix-it) | `nsl_lang.ebnf:835` | `=` vs `:=` LHS-kind rules |
  | S4 | Error | `nsl_lang.ebnf:838` | `func_in`/`func_out`/`func_self` dummy-arg directions |
  | S5 | Error | `nsl_lang.ebnf:843` | return-value-terminal direction is reversed |
  | S6 | Error | `nsl_lang.ebnf:848` | `proc_name` arguments must be `reg` |
  | S7 | Error (fix-it) | `nsl_lang.ebnf:850` | `seq`/`while`/`for` only inside func/proc |
  | S8 | Error | `nsl_lang.ebnf:854` | `while`/`for` only inside `seq` |
  | S9 | Error | `nsl_lang.ebnf:857` | for-loop var must be `reg` |
  | S10 | Error | `nsl_lang.ebnf:860` | `generate` loop var must be `integer` |
  | S11 | Error | `nsl_lang.ebnf:863` | `state_name` is proc-scoped |
  | S12 | Error | `nsl_lang.ebnf:866` | partial-LHS only on `variable` |
  | S13 | (Classification) | `nsl_lang.ebnf:871` | `alt` priority vs `any` parallel (paired-introspection per FR-013) |
  | S14 | Error (fix-it) | `nsl_lang.ebnf:874` | conditional-expression `else` mandatory |
  | S15 | Error | `nsl_lang.ebnf:877` | bit-slice indices must be compile-time constant |
  | S16 | Error | `nsl_lang.ebnf:880` | `param_int`/`param_str` only meaningful for HDL submodules |
  | S17 | Error | `nsl_lang.ebnf:884` | system tasks need `simulation` modifier |
  | S18 | (Layout) | `nsl_lang.ebnf:889` | struct MSB-first packing (paired-introspection per FR-013) |
  | S19 | (Lowering) | `nsl_lang.ebnf:892` | one-clock per goto/loop in seq (paired-introspection per FR-013; full enforcement deferred to M5/M6) |
  | S20 | Error | `nsl_lang.ebnf:896` | `interface` modifier explicit clk/rst names |
  | S21 | Error | `nsl_lang.ebnf:900` | proc methods `.finish()` / `.invoke()` rules |
  | S22 | Error | `nsl_lang.ebnf:931` | `return` width must match func's return-terminal |
  | S23 | (Layout) | `nsl_lang.ebnf:936` | reg width-omitted with init = 1-bit (paired-introspection per FR-013) |
  | S24 | (Layout) | `nsl_lang.ebnf:940` | mem partial init = zero-fill (paired-introspection per FR-013) |
  | S25 | Error | `nsl_lang.ebnf:944` | `goto` two kinds (label vs state) cross-kind reject |
  | S26 | **Warning** | `nsl_lang.ebnf:959` | `func` ≡ `function` synonym (canonical: `func`) |
  | S27 | (Classification) | `nsl_lang.ebnf:965` | control-terminal name as 1-bit value (paired-introspection per FR-013) |
  | S28 | Error | `nsl_lang.ebnf:986` | `first_state` positioning rules |
  | S29 | Error | `nsl_lang.ebnf:1001` | `_init` block placement |

  *Note: Severity column reflects what the M3 diagnostic emits.
  "Error" rejects the input. "Warning" accepts but flags. "Layout"
  / "Classification" / "Lowering" rows mark constraints that are
  *constructive* (they describe Sema's resolution behavior, not a
  shape Sema rejects); these ship paired pass + introspection test
  pairs per FR-013 (resolved in Clarifications session 2026-04-28
  Q1 → Option B), not paired pass + diagnostic-string pairs.*

  *Some `Sn` rows produce multiple frozen diagnostic-message
  variants — e.g., `S3` has 2 (`=` on reg vs `:=` on wire), `S4`
  has 3 (one per func kind), `S5` has 2, `S7` has 3 (`seq` /
  `while` / `for`), `S8` has 2, `S21` has 2 (bare vs dotted), `S22`
  has 3 (outside-func vs width-mismatch vs bare-with-terminal),
  `S25` has 2 (label vs state-name), `S28` has 2 — for a total of
  32 frozen messages covering the 23 error/warning rows. See
  [`contracts/diagnostic-string.contract.md`](./contracts/diagnostic-string.contract.md)
  for the full freeze surface.*

- **FR-011**: For every `Sn` whose severity is Error or Warning, the
  emitted diagnostic message text MUST cite the spec marker
  (e.g., `(S2)`) so a future reader can `grep` from the diagnostic
  back to the spec. The full message text is *frozen* at M3 by the
  fail-case fixture's literal-string assertion (per Constitution
  Principle VIII) — any rename or weakening is caught
  automatically.

- **FR-012**: Where mechanical fix-it hints are well-defined
  (per design §12 lines 1187–1196: `=` vs `:=` on a reg per `S3`,
  missing `else` per `S14`, `seq` outside `func`/`proc` per `S7`),
  the corresponding Sn diagnostic MUST attach a `FixItHint`
  carrying the replacement range and replacement text. The
  fail-case fixture asserts both the message text AND the
  `FixItHint` shape (range + replacement), so a future regression
  on either is caught.

- **FR-013**: For every `Sn` whose row is marked
  "(Layout)" / "(Classification)" / "(Lowering)" in FR-010's table
  — `S13`, `S18`, `S19`, `S23`, `S24`, `S27` — the M3 deliverable
  is the *constructive* behavior (e.g., `S18`: emit MSB-first
  packed layout into `StructTypeSymbol`; `S24`: zero-fill the
  missing `mem` addresses; `S27`: classify control-terminal names
  in expression position as 1-bit taps), and the per-`Sn` test
  pair follows the **paired pass + introspection** shape resolved
  in Clarifications session 2026-04-28 Q1 (Option B):
  `test/sema/s<NN>/pass.nsl` exercises the construct, paired with
  a unit-test introspection assertion against the post-Sema state
  (e.g., for `S18`: `StructTypeSymbol::fields[0].name` is the
  first-declared member; for `S24`: `MemSymbol::initValues.size()`
  equals declared depth with the trailing entries zero-filled; for
  `S27`: `Sema::classifyIdentifierExpr(controlTerminalRef)` returns
  the 1-bit-tap classifier kind), and `test/sema/s<NN>/fail.nsl`
  is the same `.nsl` input with the introspection-expected-value
  flipped so the test fails iff Sema diverges from the spec's
  constructive rule. The fail-fixture for these five `Sn` does
  NOT carry a diagnostic-string assertion — Principle VIII's
  diagnostic-string clause is, by construction, inapplicable to
  constraints that produce no diagnostic. The
  [`CLAUDE.md`](../../CLAUDE.md) §1 roll-up table MUST gain a
  footnote at M3 documenting which `Sn` ship paired-introspection
  vs. paired-diagnostic, so the carve-out is mechanically
  auditable from the spec/design coupling table.

**Diagnostic, recovery, and resolution requirements (per Constitution Principle IV / VIII; M1's diagnostic plumbing):**

- **FR-014**: Sema MUST emit every diagnostic exclusively via the
  `DiagnosticEngine` introduced at M1 (FR-024 of the M1 spec).
  Direct writes to `stderr` from inside `nsl-sema` are forbidden.
  Every diagnostic MUST render in the canonical form
  `<path>:<line>:<col>: <severity>: <message>` (FR-025 of M1).
- **FR-015**: Every Sema fail-case fixture under `test/sema/s<NN>/
  fail.nsl` MUST also assert the literal diagnostic message text,
  per Constitution Principle VIII's `Sn`/`Nn`/`Pn` clause. A test
  that asserts only the error count (and not the text) is rejected
  at code-review time.
- **FR-016**: Sema MUST recover from per-`Sn` failures using the
  **hybrid** strategy resolved in Clarifications session
  2026-04-28 Q3 → Option C: a single top-down **resolution pass**
  walks the AST once, emitting exactly one diagnostic per
  unresolved name and recording each unresolved subtree so
  downstream per-`Sn` checks can skip it; THEN a **per-`Sn` pass
  set** runs each constraint check over the full AST as an
  independent pass with no cross-`Sn` suppression. A `Sn` check
  encountering an `Unresolved`-typed subtree from the resolution
  pass MUST silently skip that subtree and continue. Multiple
  independent `Sn` violations in a single source file are ALL
  reported in a single Sema run; multiple unresolved names produce
  one diagnostic each (one per name, NOT one per use site). The
  recovery / skip behavior MUST be documented per check-site in
  `nsl-sema`'s implementation (a comment block at each emit-site
  naming the assumed-state on continuation, parallel to M2's
  parser recovery-set convention).
- **FR-017**: An unresolved name reference MUST produce **exactly
  one** "unresolved name `<X>`" diagnostic at the reference site,
  AND MUST NOT cascade into synthetic per-`Sn` errors driven by
  `<X>`'s missing `inferredType()`. The symbol's resolved type is
  recorded as `Unresolved`, and downstream `Sn` checks that consume
  it skip silently. This is a per-`Sn` independence guarantee — a
  single name typo in a 12k-line audited project must not produce
  hundreds of cascading width-mismatch errors.
- **FR-018**: The post-Sema AST MUST carry the resolved
  information additively (resolved `Symbol*` on every name-ref
  `Expr` node; resolved `TypeRef` on every `Expr::inferredType()`
  slot) WITHOUT mutating the parser-emitted shape. The M2 AST
  output is the same shape post-Sema; only the resolution slots
  fill in. No M2 fixture under `test/parse/grammar/` MUST require
  re-cutting because of M3.

**Driver / CLI surface (per [`README.md`](../../README.md) §Usage and the M2 spec note that `-emit=ast` is allowed to format-bump as later milestones add resolved information):**

- **FR-019**: The `nslc` driver MUST run Sema on every input that
  parses successfully (i.e., any `-emit=*` flag from `-emit=ast`
  forward triggers Sema; `-emit=tokens` does not, since it stops
  at lex). On Sema failure, the driver MUST exit non-zero and emit
  no later-stage output on stdout — diagnostics on stderr (or
  JSON to stdout for `--diagnostic-format=json` per the M1 spec).
- **FR-020**: The `-emit=ast` golden test corpus is re-cut at M3
  in place (per Clarifications session 2026-04-28 Q2 → Option A):
  the AST printer detects post-Sema input and prints
  `Expr::inferredType()` and the resolved `Symbol*` `declLoc`
  inline on each relevant node. Each printer line carries the
  node kind, the `SourceRange`, the kind-specific fields from
  M2, AND (post-Sema) the inferred-type value (e.g.,
  `IdentifierExpr w : BitVector(8) → decl@F:L:C`) on every name-ref
  and `Expr` node. The M2 parser-only `-emit=ast` golden is
  re-cut in the M3 patch as part of the deliverable; per Principle
  VII (spec/design coupling), the parser-fixture goldens under
  `test/parse/grammar/` are updated in the same patch as the
  printer-format change.
- **FR-021**: The `nslc` driver MUST inherit the M2 flag set
  (`-I <dir>`, `-D NAME=value`, `NSL_INCLUDE`,
  `--diagnostic-format=json`, `-emit=ast`, `-emit=tokens`) without
  modification. M3 adds **no new CLI flags** — the format bump on
  `-emit=ast` is silent at the flag level (the format change is
  observable only by inspecting output) per Q2's Option-A
  resolution.
- **FR-022**: The `nslc -emit=ast` code path MUST live behind a
  thin wrapper inside `nsl-driver` (per the M0/M1/M2 invariant
  that the driver remains ≤60 lines plus per-`-emit=*` glue). The
  wrapper invokes `Compilation::preprocess()` →
  `Compilation::parse()` → `Compilation::sema()` and prints
  `CompilationUnit*` via the AST-printer entry point in `nsl-ast`'s
  public header. The printer MUST handle both pre-Sema input
  (parser-only mode, in case of a future `--no-sema` debug flag or
  Sema-failure-with-partial-AST scenario) and post-Sema input
  (the default at M3 and beyond), with the post-Sema variant
  emitting the resolved-type / resolved-decl-loc enrichments
  per FR-020.

**Test gates (per Constitution Principles VI and VIII):**

- **FR-023**: The repository MUST carry one passing fixture
  (`test/sema/s<NN>/pass.nsl`) and one failing fixture
  (`test/sema/s<NN>/fail.nsl`) for every `NN ∈ {01..29}`. The fail
  fixture MUST assert the literal error/warning text via
  FileCheck-style or gtest equality. CI MUST mechanically guard
  the existence of both files under each `s<NN>/`.
- **FR-024**: For every `Sn` with a `FixItHint` per FR-012, the
  fail fixture MUST also assert the hint's replacement range and
  replacement text — so a regression on the fix-it shape is
  caught alongside a regression on the message.
- **FR-025**: The repository MUST carry a multi-error recovery
  corpus under `test/sema/recovery/` covering at minimum: (a) two
  independent `Sn` violations in separate top-level modules,
  (b) an unresolved name producing exactly one diagnostic without
  cascading synthetic errors, (c) a `Sn` violation in one
  `module_item` with a *correct* sibling `module_item` whose
  resolution and `inferredType()` are unaffected. Each fixture
  asserts the full diagnostic stream in source order in a single
  Sema run.
- **FR-026**: The repository MUST carry a name-resolution corpus
  under `test/sema/resolution/` covering each scope-kind from
  design §6's "Scope stack semantics" table (Global / Declare /
  Module / Proc / Seq-or-Parallel / Function), each scoped-name
  form (`SUB.port` for submodule ports, `inst.finish` for proc
  methods per `S21`/`N6`, `func ic.ready` definitions per `N7`),
  and each `Symbol` kind from FR-005.
- **FR-027**: The repository MUST carry a width-inference corpus
  under `test/sema/width/` covering each `Expr` form from
  `nsl_lang.ebnf §11` whose width is determined by Sema (and not
  parser-syntactic): unary, binary, conditional, concat, repeat,
  sign-extend `#`, zero-extend `'`, slice, field access, call,
  struct cast. Each fixture asserts the post-Sema
  `Expr::inferredType()` matches the expected `BitVector(N)` /
  `Bit` / `Struct` / `Memory` value.
- **FR-028**: Every fixture under `test/sema/` MUST be authored
  before its driving implementation (Principle VIII TDD); the
  test commit MUST be observed failing prior to the implementation
  commit being accepted. The TDD evidence path (failing-CI link
  in the PR description) is the standard mechanism.

**Determinism (Constitution Principle V):**

- **FR-029**: The post-Sema AST (resolved symbols + filled-in
  types) MUST be a pure function of (input AST, CLI flag list).
  No environment-derived inputs (CWD, mtime, locale, hostname,
  env vars other than `NSL_INCLUDE`) MAY influence resolution or
  width inference. Two `nslc -emit=ast` invocations on the same
  input under the same flag list MUST produce byte-identical
  stdout (this re-states M2's FR-030 and extends it to cover the
  post-Sema printout).
- **FR-030**: All collection types whose iteration order is part
  of any serialized output (the post-Sema AST, the diagnostic
  stream order, the symbol table dump if exposed for testing)
  MUST be deterministic (insertion-ordered or sorted) — no
  unordered_map iteration in serialization.
- **FR-031**: Symbol identity (the `Symbol*` recorded on each
  resolved name-ref `Expr`) MUST NOT leak raw memory addresses
  into any serialized output. Cross-references serialize via the
  symbol's `declLoc.start` byte offset (or a zero-based monotonic
  symbol-table index) — same convention as M2's FR-031 for AST
  cross-references.

### Key Entities

- **`Sema`**: top-level driver of the M3 pipeline stage; owns a
  `SymbolTable`, a `TypeSystem`, and a `DiagnosticEngine&`
  reference; entry point `SemaResult run(CompilationUnit&)`. Public
  type in `nsl-sema`.
- **`SymbolTable`**: scope stack with `enterScope` / `leaveScope` /
  `declare` / `lookup` / `lookupScoped` / `currentModule`, per
  design §6 lines 761–769. The scope stack mirrors NSL's
  syntactic nesting (Global / Declare / Module / Proc /
  Seq-or-Parallel / Function).
- **`Symbol`**: abstract base; one concrete subclass per declaration
  kind (FR-005), each carrying its `SourceRange declLoc`, its
  identifier name, its `kind()` enum tag, its `TypeRef type`, and
  per-kind fields (e.g., `PortSymbol::dir`, `FuncInSymbol::args`).
- **`TypeSystem`**: interns `Type` values so pointer equality
  implies type equality, per design §6.x lines 799–801. Hands out
  `TypeRef` (= `const Type*`) handles.
- **`Type`** / **`BitVectorType`** / **`StructType`** /
  **`MemoryType`** / **`Bit`** / **`Unresolved`**: the type kinds
  per design §6.x lines 802–836. Width-inference fills these into
  the AST `Expr::inferredType()` slot reserved at M2.
- **`SemaResult`**: returned by `Compilation::sema()`; carries
  ownership of the `SymbolTable` and `TypeSystem` so the post-
  Sema AST can be walked by later stages without re-running
  resolution.
- **`SourceRange`** (re-used from `nsl-basic` per M1): every
  `Symbol` and every `Sn` diagnostic's location uses the
  post-`#line` virtual coordinates.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: For every `NN ∈ {01..29}`, **at minimum** a
  `test/sema/s<NN>/pass.nsl` and a `test/sema/s<NN>/fail.nsl` exist
  (some `Sn` carry multi-variant `fail_<variant>.nsl` per
  [`contracts/diagnostic-string.contract.md`](./contracts/diagnostic-string.contract.md);
  baseline 58 fixtures + ~14 multi-variant ≈ 72 lit fixtures total),
  are authored before their implementation per Principle VIII TDD,
  pass on a green CI run, AND every fail fixture for the 23
  error/warning `Sn` asserts the literal diagnostic-message text;
  every fail fixture for the 6 constructive `Sn` (`S13`/`S18`/
  `S19`/`S23`/`S24`/`S27`) asserts the flipped-introspection-
  expected-value per Q1 Option B (`test_unit/constructive_sn_test/`)
  and Constitution Principle VIII's constructive carve-out (v1.6.0).
  0 of the 29 are unenforced; 100% test-pair coverage.
- **SC-002**: A diagnostic emitted by Sema matches the regex
  `^[^:]+:\d+:\d+: (error|warning|note): .+\(S\d+\)\.?$`
  (the message MUST cite `(SNN)` per FR-011), with the suffix
  `.` optional. 0% deviation tolerated.
- **SC-003**: Two consecutive `nslc -emit=ast` invocations on the
  same input under the same flag list produce byte-identical
  stdout — across both supported build types (Debug and Release)
  and both supported compilers (gcc and clang) (Principle V;
  FR-029; matches M0/M1/M2's reproducibility gates).
- **SC-004**: A multi-error fixture under `test/sema/recovery/`
  containing `K` independent `Sn` violations produces exactly `K`
  diagnostics (no fewer, no more — no cascading synthetic errors)
  in source order, in a single Sema run, for `K ∈ {2, 3, 5}`.
- **SC-005**: An unresolved-name fixture (a single name typo
  feeding `M ≥ 3` later use sites) produces exactly **one**
  "unresolved name" diagnostic at the typo site — not `M+1`
  cascading width-mismatch / type-mismatch errors driven by the
  typo's `Unresolved` type (FR-017).
- **SC-006**: A reviewer opening a red CI run from a Sema
  regression can identify the failing `Sn` constraint within 10
  seconds, by reading the failing diagnostic-string assertion
  alone, without inspecting the Sema source code (re-statement of
  M1 SC-008 / M2 SC-005 for M3 fixtures).
- **SC-007**: After M3 lands, the tooling-track gates that
  declare a hard dependency on M3 (T2 formatter, T3 LSP skeleton,
  T6 lint framework per [`README.md`](../../README.md)
  §Roadmap "Tooling track" rows) can begin implementation against
  the `nsl-sema` public-header surface without further compiler-
  track waits — this is the "M3 unlock" property cited in
  README's "M3 is the unlock point" callout (line 58).
- **SC-008**: 100% of files newly added under `lib/Sema/`,
  `include/nsl/Sema/`, and `test/sema/` carry the `Apache-2.0
  WITH LLVM-exception` SPDX header (M0 FR-010 hygiene re-stated
  for the M3 file set).
- **SC-009**: The `nsl-sema` library's only build-time
  dependencies are `nsl-ast` and `nsl-basic` (the layered
  structure of Principle II). A CI guard MUST verify this — no
  link-time edge from `nsl-sema` to `nsl-parse`, `nsl-dialect`,
  or any later layer.
- **SC-010**: Adding a new `Sn` (a hypothetical spec change in a
  later release) requires editing exactly one new fixture
  directory under `test/sema/s<NN>/`, one new check-site in
  `lib/Sema/`, one row in [`CLAUDE.md`](../../CLAUDE.md) §1's
  table, AND one row in `nsl_lang.ebnf` lines 826–1009 (per
  Principle I monotonic numbering) — no edit to the M3
  scaffolding itself (Principle II layer extensibility, applied
  to the Sema test corpus).

## Assumptions

- **Scope is the M3 row of `README.md` §Roadmap, the `nsl-sema`
  library, the per-`Sn` test corpus, and the driver glue to run
  Sema after parse for every `-emit=*` from `-emit=ast` forward.**
  No new lint rules — `W001`–`H009` are tooling-track per
  [`CLAUDE.md`](../../CLAUDE.md) §2.2 and ride T6/T7. No structural-
  expansion (M5). No MLIR dialect (M4). No Verilog backend (M6/M7).
- **The M3 deliverable does NOT replace M2's `-emit=ast` text
  format wholesale.** The post-Sema AST *shape* is the same as
  the parser-emitted shape — Sema fills in resolved-symbol and
  inferred-type slots, it does not introduce new node kinds. The
  *format* of the `-emit=ast` text dump bumps additively at M3
  per Clarifications session 2026-04-28 Q2 → Option A: each
  `Expr` line gains a post-`SourceRange` `: <type>` suffix and
  each name-ref gains a `→ decl@<file>:<line>:<col>` suffix. The
  M2 parser-fixture goldens under `test/parse/grammar/` are
  re-cut in the M3 patch (per Principle VII spec/design
  coupling).
- **The M2 spec's deferred surfaces — `S26` `function`-canonicalize
  warning, `N10` `label`-as-identifier warning forwarded from
  parser, the `Expr::inferredType()` slot reserved on every
  `Expr` — all land at M3 per their M2-deferral notes (M2 spec
  Assumptions paragraph 2 and FR-016/FR-017).** The parser at M2
  already accepts `function` and emits a parse-time warning for
  `label`; M3 re-emits or upgrades these as documented per `S26`
  / `N10`.
- **Constructive-shape `Sn` (the rows marked Layout / Classification
  / Lowering in FR-010's table — `S13`, `S18`, `S19`, `S23`, `S24`,
  `S27`) describe Sema's *resolution behavior*, not a shape Sema
  *rejects*.** Per Clarifications session 2026-04-28 Q1 → Option B,
  these `Sn` ship a paired pass + introspection test pair (see
  FR-013) instead of a paired pass + diagnostic-string test pair.
  Principle VIII's diagnostic-string clause is by construction
  inapplicable to constraints that produce no diagnostic, so this
  is a literal-VIII honoring choice, not a carve-out. The fail
  fixture is the same `.nsl` input with the introspection-expected
  value flipped, so the test fails iff Sema diverges from the
  spec's constructive rule. The
  [`CLAUDE.md`](../../CLAUDE.md) §1 roll-up table gains a M3-era
  footnote naming the six `Sn` that ship paired-introspection so
  the carve-out is auditable from the spec/design coupling table
  per Principle VII.
- **Multi-error recovery in Sema follows the same architectural
  principle as the parser at M2 (full multi-error per
  `M2 spec Clarifications session 2026-04-27 Q1 → Option A`),
  with the granularity resolved in Clarifications session
  2026-04-28 Q3 → Option C (hybrid): one top-down resolution pass
  followed by a per-`Sn` independent-pass set.** The unresolved-
  name suppression rule (FR-017) is unconditional: a single name
  typo MUST NOT cascade. Per-`Sn` checks gate on `Unresolved`-
  typed subtrees from the resolution pass and skip them silently;
  `Sn` checks elsewhere in the compilation unit are unaffected.
- **`SymbolTable` and `TypeSystem` are exposed as public headers
  (FR-001) so that the tooling-track libraries (T2 `nsl-fmt`, T3
  `nsl-lsp`, T6 `nsl-lint`) can consume them at link time without
  duplicating the implementation** (Constitution Principle II:
  all tools reuse `libNSLFrontend.a`). The two-header carve-out
  (`SymbolTable.h` + `TypeSystem.h` alongside the umbrella
  `Sema.h`) is the same pattern Principle II grants `nsl-ast`
  for its per-node-kind headers.
- **Audited-project ingestion (`P-VEN`) and golden VCDs (`P-VCD`)
  are out of scope** — they gate M7. CI's end-to-end and formal
  stages remain in the "wired-but-empty" state established at M0.
- **Tooling track** (T1–T12) implementation-of-rules is out of
  scope. M3's job is to publish the symbol/type surface; the rule
  implementations (W001 unused-decl, S001 width-mismatch, etc.)
  are routine PRs against the T6/T7 framework after M3 lands.
- **Reference host, build matrix, CI infrastructure, SPDX hygiene,
  `add_nsl_library`, `DiagnosticEngine`, `SourceManager`, `Lexer`,
  `Preprocessor`, `Parser`, AST node hierarchy** are M0/M1/M2
  deliverables and are taken as given. This spec inherits and
  does not re-justify them.

