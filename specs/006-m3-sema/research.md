<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Research: M3 — Sema (`nsl-sema`)

**Branch**: `006-m3-sema` | **Date**: 2026-04-28
**Plan**: [plan.md](./plan.md)

This file resolves every Technical Context decision with a
**Decision / Rationale / Alternatives considered** entry, mirroring
the pattern established in
[`specs/005-m2-parser/research.md`](../005-m2-parser/research.md).
Each decision is anchored in the Constitution, the design docs, the
spec FRs, or the prior-milestone precedent — no decision is made on
"engineering taste" alone.

---

## 1. Pass strategy: hybrid resolution + per-`Sn` walks

**Decision**: Implement Sema as **two stages**:

1. `ResolutionPass` — a single top-down `ASTVisitor` walk that
   (a) opens/closes `Scope`s at AST node entry/exit per the
   scope-stack semantics in design §6 lines 786–793,
   (b) constructs and `declare()`s one `Symbol` per declaration
   site (FR-005),
   (c) resolves every `IdentifierExpr` / `FieldAccessExpr` /
   `ScopedName` to a `Symbol*` (FR-006),
   (d) folds in width inference for every `Expr` (per design
   §6.x line 856 "Width inference is a single top-down pass"),
   (e) emits exactly one "unresolved name 'X'" diagnostic per
   distinct `X` and tags every consumer subtree with the
   `Unresolved` `TypeRef` so per-`Sn` walkers can skip silently
   (FR-017, no-cascade).

2. `ConstraintCheckPass` — a fan-out of N independent walkers,
   one per constraint family in `lib/Sema/Constraints/S<NN>_*.cpp`.
   Each walker visits only the AST node kinds relevant to its
   `Sn` (e.g., `S02_WireNoInit.cpp` walks only `WireDecl` nodes;
   `S07_SeqInsideFuncProc.cpp` walks only `SeqBlock`/`WhileBlock`/
   `ForBlock` nodes). Each walker that encounters an
   `Unresolved`-typed subtree skips it silently. The 23
   error/warning walkers emit a `Diagnostic` carrying the frozen
   message text + optional `FixItHint`; the 6 constructive
   walkers (`S13`/`S18`/`S19`/`S23`/`S24`/`S27`) write
   introspection-observable state (e.g., `StructTypeSymbol::fields`
   in MSB-first order; `MemSymbol::initValues` zero-padded;
   `Sema::classifyIdentifierExpr` returning the 1-bit-tap kind)
   that the gtest unit tests assert on.

**Rationale**: Resolves Clarifications session 2026-04-28 Q3 →
Option C (hybrid) verbatim. Matches design §6.x line 856 ("Width
inference is a single top-down pass") for the inference half;
preserves per-`Sn` independence for the constraint half. The
no-cascade guarantee (FR-017) is achieved structurally — by the
time `ConstraintCheckPass` walkers run, every name has either
resolved successfully (so its `inferredType()` is real) or been
tagged `Unresolved` (so the walkers skip). One typo therefore
produces exactly one diagnostic, period.

**Alternatives considered**:

- *Pure per-`Sn` continue (Q3 Option A)*: each `Sn` re-walks the
  AST, resolution is duplicated 29× and unresolved-name detection
  produces 29 cascading errors per typo. **Rejected** because it
  violates FR-017 by construction unless every walker is also
  taught to detect-and-skip unresolved symbols (which is exactly
  what Option C does, but framed as a separate pass for clarity).
- *Pure per-construct continue (Q3 Option B)*: a single error on
  one declaration suppresses all later checks on that *same*
  declaration. **Rejected** because the spec's US3 ("Diagnose every
  Sema error in one Sema pass") and SC-004 ("`K` independent `Sn`
  violations produce exactly `K` diagnostics") require independence
  across constraints, which Option B violates.
- *Single-pass interleaved walk*: combine resolution and per-`Sn`
  in one visitor. **Rejected** because the per-`Sn` checks form a
  natural extension surface (adding `S30` later is a single new
  source file under `Constraints/`, per spec SC-010); a monolithic
  visitor would bloat `Sema.cpp` and entangle unrelated checks.

---

## 2. Symbol table data structure

**Decision**: Implement `SymbolTable` as a stack of
`std::unique_ptr<Scope>`, where each `Scope` carries
`llvm::DenseMap<llvm::StringRef, Symbol*>` for O(1) lookup and a
`std::vector<Symbol*>` for deterministic insertion-order iteration
(per FR-030). `lookup(name)` walks the stack outward; `lookupScoped(head, tail)`
first calls `lookup(head)`, validates the resolved kind
(`SubmoduleSymbol` for `SUB.port`; `ProcSymbol` for `inst.finish`;
`StructTypeSymbol` for `inst.field`), then re-enters that target's
scope to look up `tail`.

**Rationale**: Matches design §6 lines 761–779 verbatim. `DenseMap`
gives sub-microsecond lookup at the typical 10–50 symbols-per-scope
density of an audited NSL project; the parallel `std::vector`
guarantees deterministic iteration order (FR-030) without
sacrificing lookup speed. `unique_ptr<Scope>` avoids the
recursive-iterator-invalidation pitfalls of a flat vector under
nested scope creation.

**Alternatives considered**:

- *Flat `llvm::SmallVector<Scope, 8>` with `DenseMap` lookup
  inside each `Scope`*: simpler ownership but reallocation on
  push could invalidate prior `Scope*` references that the
  `ResolutionPass` holds during nested-scope traversal.
  **Rejected** for ownership safety.
- *Single global `DenseMap<{ScopeId, Name}, Symbol*>`*:
  flatter and cache-friendlier but loses the natural outward-walk
  semantics of nested scopes. **Rejected** because the
  `lookupScoped` flow (which walks into a *different* scope rooted
  at the head symbol's target) becomes awkward.

---

## 3. Type system interning

**Decision**: `TypeSystem` interns `Type` values such that
pointer equality implies type equality (per design §6.x line 799).
Implementation: one cache per concrete `Type` subclass keyed on the
type's structural fields:

- `BitVectorType` keyed on `uint64_t width` →
  `llvm::DenseMap<uint64_t, std::unique_ptr<BitVectorType>>` per
  design §6.x line 849.
- `StructType` keyed on `(StringRef name, ArrayRef<FieldInfo>)` —
  but since struct names are unique per compilation unit (enforced
  by the `ResolutionPass` declaring each `StructTypeSymbol` in the
  global scope), the cache key reduces to the name alone.
- `MemoryType` keyed on `(uint64_t depth, TypeRef element)` since
  `element` is itself an interned pointer.
- `Bit` is a singleton (`bitSingleton_` per §6.x line 848).
- `Unresolved` is a singleton (added at M3 — design §6.x doesn't
  enumerate it, but it's necessary for the no-cascade guarantee
  and so is part of the M3 type-system surface).

**Rationale**: Matches design §6.x lines 840–851 verbatim. The
pointer-equality contract is what allows `Sema::equal(TypeRef,
TypeRef)` to be `a == b` (line 846) — every check downstream of
Sema can use raw pointer comparison without round-tripping through
a structural-equal predicate.

**Alternatives considered**:

- *Hash-cons `Type` values without owning storage*: simpler but
  the `Type` instance lifetimes become tangled with the
  `Compilation` lifetime. **Rejected** because owning the cache
  inside `TypeSystem` is the design-doc choice.
- *Lazy interning (intern only on `==` query)*: defers cost but
  loses the simple `a == b` invariant — two structurally-equal
  `Type*` could be `!=`. **Rejected** because it breaks the FR-007
  contract.

---

## 4. Width-inference algorithm

**Decision**: Top-down inheritance from transfer destination
widths to source-expression widths, per design §6.x line 856.
Implementation: `ResolutionPass` carries a "context width" stack;
when it enters a `TransferStmt::lhs` it computes the LHS's width
(from the `Symbol*`'s declared width), pushes that as the context
width, recurses into `TransferStmt::rhs`, and on every
`Expr` node that carries an inherent width (literal, slice, call,
sign-extend `#`, zero-extend `'`, struct cast) it sets
`inferredType()` to the inherent width; on every `Expr` that is
context-dependent (binary, conditional, concat) it inherits the
context width; on every name-ref it looks up the resolved
`Symbol*`'s `type` field. The Ref §0 "Estimation of bit width in
operation" rules govern the concrete width assignment per operator
kind — this is documented in the per-`Expr`-form width fixtures
under `test/sema/width/` (FR-027) so the rules are the test corpus,
not an implementation comment.

**Rationale**: Top-down was the design-doc choice (line 856); it
matches NSL's transfer-driven semantics where the LHS terminal
width is the authoritative width for the assignment, and the RHS
must fit. Bidirectional (Hindley-Milner-style) inference is
overkill — NSL has no inferred-from-context type variables that
would benefit, and unification cost would inflate Sema runtime
without payoff.

**Alternatives considered**:

- *Bottom-up inference* (compute every `Expr` width from leaves):
  natural for typed languages with rich expressions but produces
  the wrong answer for context-sensitive operators like
  zero-extend `'` and sign-extend `#` whose result width is set
  by the *enclosing* context, not the operand. **Rejected** for
  semantic-correctness reasons (would force a second top-down
  fix-up pass anyway).
- *Bidirectional inference*: combines the two, gaining nothing
  for NSL. **Rejected** as over-engineering.

---

## 5. FixItHint format for the four mechanical fix-its

**Decision**: Three `FixItHint` entries per FR-012, plus one
optional fourth:

| `Sn` | Fix-it shape | Replacement text |
|---|---|---|
| `S3` | `replaceRange = TransferStmt::eqOpRange` | `:=` (when LHS is a reg, was `=`) or `=` (when LHS is a wire, was `:=`) |
| `S7` | `replaceRange = SeqBlock::seqKwRange` | `seq` is *removed* and the contained statements re-flowed inline (heavier; emit only when the enclosing scope allows them; otherwise omit fix-it and emit error-only) |
| `S14` | `insertRange = ConditionalExpr::endRange` | ` else <expr>` template stub — the user fills in `<expr>` |

The shape uses M1's `FixItHint{ SourceRange range, std::string replacement }`
struct (per design §12 lines 1187–1191). For `S14` the range is a
zero-width insert at the conditional's tail; the replacement begins
with a leading space.

The optional fourth fix-it is `S26` `function` → `func`
canonicalization. **Decision**: include it (warning-class fix-it).
The replacement range is the `function` keyword's `SourceRange`,
the replacement text is `func`. This makes the warning useful as
an LSP `codeAction` at T9 without further plumbing.

**Rationale**: Mechanical fix-its are exactly the cases where the
fix is uniquely-determined by the rule — design §12 lines 1187–1196
explicitly call out `S3` (`=` vs `:=`), `S7` (`seq` outside func/proc),
and `S14` (missing `else`) as such. `S26` is a clean fourth case
because the canonical-form rewrite is mechanical.

**Alternatives considered**:

- *Defer all fix-its to T9 (LSP codeAction)*: simpler M3 surface
  but the LSP would need to recompute them from the diagnostic
  alone, duplicating work. **Rejected** because the fix-it data is
  trivially constructed at the diagnostic-emission site and the
  cost of carrying it through `DiagnosticEngine` is one
  `std::vector<FixItHint>` move.
- *Include fix-its for *every* rule with a unique fix*: `S2`
  (wire→reg), `S22` (return-width truncation), etc. **Rejected**
  for M3 — these aren't *mechanical* fixes (they require user
  judgment about the intent), and embedding them as fix-its would
  encourage users to accept the wrong fix.

---

## 6. Introspection-API surface for paired-pass + introspection (Q1 Option B)

**Decision**: For the 6 constructive `Sn` (`S13`, `S18`, `S19`,
`S23`, `S24`, `S27`), expose the post-Sema observable as a
public-header method on the relevant entity:

| `Sn` | Observable | API entry point |
|---|---|---|
| `S13` | classification of `alt` vs `any` block | `AltBlock::cases()` returns `ArrayRef<Case>` in priority order; `AnyBlock::cases()` returns `ArrayRef<Case>` in declaration order. The unit test asserts the priority-vs-parallel semantic by comparing the Sema-classified node kind (`AltBlock` vs `AnyBlock`) against the parser-emitted shape. |
| `S18` | struct member packing | `StructTypeSymbol::fields()` returns `ArrayRef<FieldInfo>` in MSB-first order; `FieldInfo::offset` is the bit position from MSB. The unit test asserts `fields[0].offset == totalWidth - fields[0].width` (first declared = highest bit). |
| `S19` | one-clock-per-goto in `seq` | At M3, ship a stub: `SeqBlock::clockBudget()` returns the number of `goto`s + back-edge transitions; the unit test asserts the count matches the spec's clock-counting rule. The full timing-semantic enforcement is M5/M6 lowering; this stub is the M3 contract. |
| `S23` | reg-omitted-width with init = 1-bit | `RegSymbol::type()` returns `BitVectorType{1}` when `RegDecl::width == nullopt && RegDecl::init != nullopt`. The unit test asserts pointer equality with `TypeSystem::bitVector(1)`. |
| `S24` | mem partial-init zero-fill | `MemSymbol::initValues()` returns `ArrayRef<uint64_t>` of size `MemSymbol::depth()`; trailing entries beyond the user's init list are zero-filled. The unit test asserts `initValues[user_count..depth]` is all zero. |
| `S27` | control-terminal-name as 1-bit value | `Sema::classifyIdentifierExpr(IdentifierExpr&) → ClassifierKind ∈ {Value, ControlTerminalTap, …}` returns `ControlTerminalTap` for any `IdentifierExpr` whose resolved `Symbol*` is a `FuncInSymbol` / `FuncOutSymbol` / `FuncSelfSymbol` / `ProcSymbol` and whose context is expression-position. The unit test asserts the classifier kind on a fixture-input expression tree. |

These methods are stable (per `sema-stability.contract.md`
Invariant 4) and form the test surface for the 6 constructive `Sn`
unit tests under `test_unit/constructive_sn_test/` (per
Clarifications Q1 → Option B).

**Rationale**: A public observable is the only honest way to
satisfy "paired pass + introspection" — the unit test cannot
reach into private state. Each method has obvious utility outside
the test (e.g., the LSP T4 hover feature consumes
`StructTypeSymbol::fields()`'s offset; the formatter T2 doesn't
need it but it's cheap to expose), so the introspection surface is
*re-used*, not test-only scaffolding.

**Alternatives considered**:

- *Test-only friend access*: violates encapsulation and bloats the
  test infrastructure. **Rejected**.
- *Print the introspection state into `-emit=ast`*: would couple
  the introspection contract to the printer's text format, and the
  printer is not the right surface for structural assertions
  (introspection wants typed values, not regex-able text).
  **Rejected**.

---

## 7. `-emit=ast` post-Sema format spec (Q2 Option A)

**Decision**: The post-Sema `-emit=ast` printer adds two
additive enrichments to each line, both keyed on whether the
node has resolution information:

1. **Type suffix on every `Expr`**: ` : <Type>` after the
   `SourceRange`. `<Type>` is one of:
   - `Bit` for the singleton `BitType`
   - `BitVector(N)` for `BitVectorType{N}`
   - `Struct(<name>)` for `StructType`
   - `Memory(<depth> × <element>)` for `MemoryType`
   - `Unresolved` when the resolution pass tagged the subtree

2. **Decl-loc suffix on every name-ref**: ` → decl@<file>:<line>:<col>`
   after the type suffix. The `<file>:<line>:<col>` is the resolved
   `Symbol*::declLoc.start` rendered through the same `SourceRange`
   formatter as the per-line range.

Example post-Sema line:

```
IdentifierExpr <foo.nsl:12:7-12:9> w : BitVector(8) → decl@foo.nsl:5:5
```

The pre-Sema printer (no resolution data) omits both suffixes —
this is the M2 format unchanged. The printer detects the mode by
checking `Expr::inferredType() != nullptr`.

**Rationale**: Resolves Clarifications Q2 → Option A. Additive
enrichments preserve diff-readability of the existing M2 fixtures
under `test/parse/grammar/` (re-cut in this same patch but with
visually-obvious deltas), and the per-line shape stays parseable
by the same regex that parses M2 lines (the `: <Type>` and `→ decl@…`
suffixes are tail-anchored).

**Alternatives considered**:

- *JSON output*: M2 spec Clarifications Q2 already deferred this
  to T-track LSP design. **Not in scope** at M3.
- *Multi-line per-node (separate type/decl-loc lines)*: would more
  than double the printer's output volume on large fixtures.
  **Rejected** for fixture-readability.

---

## 8. Sema header layout (Principle II §3 exception)

**Decision**: `nsl-sema` exposes **three** umbrella public
headers — `Sema.h`, `SymbolTable.h`, `TypeSystem.h` — under
`include/nsl/Sema/`. This is a deviation from Principle II's
"single public header per library" rule, but it falls under the
*spirit* of the §3 exception that grants `nsl-ast` per-node-kind
headers.

Why it falls under that spirit:

- The three Sema umbrellas are **structurally analogous** to the
  three concerns in design §6: the entry-point + result type
  (`Sema.h`), the symbol-table machinery (`SymbolTable.h`), and
  the type-system machinery (`TypeSystem.h`).
- Each umbrella exposes a *cohesive* surface; combining them into
  one `Sema.h` would create a 1000+-line header that dominates
  every consumer's compile time (`#include <nsl/Sema/Sema.h>` would
  pull in every `Symbol` subclass + every `Type` subclass + the
  `Sema` engine itself).
- Tooling-track libraries (T2 `nsl-fmt`, T3 `nsl-lsp`, T6
  `nsl-lint`) consume *only* the symbol/type sub-surfaces, not the
  `Sema` engine — splitting allows them to `#include
  <nsl/Sema/SymbolTable.h>` without dragging in the resolution-pass
  visitors.
- The three-header pattern matches how production compiler
  Sema libraries are typically organized (clang's `Sema/`
  directory exposes `Sema.h`, `Lookup.h`, `Initialization.h`, … for
  the same reason).

**Constitutional posture**: Principle II §3 lists `nsl-ast` as
the explicit exception. Strictly read, M3's three-header layout
*expands* the exception set. Two posture options:

(A) **Treat as already-allowed** by reading Principle II §3's
    rationale ("a class hierarchy that benefits from per-kind
    separation") as covering Sema's three-axis decomposition by
    analogy. The `docs/CLAUDE.md` §3 task-section map for "Sema"
    already lists three concerns separately (lines 688–795 SymbolTable,
    797–856 TypeSystem, 1260–1270 Testing) so the design doc
    pre-figures the split.

(B) **Amend Principle II §3** in this same patch to read
    "`nsl-ast` and `nsl-sema` are exempt; other libraries follow
    the single-header rule."

**Decision**: Posture **(A)**. The amendment overhead is large
relative to the change; the analogy reading is supported by the
design doc and by clang's precedent; and Principle II's rationale
("zero semantic drift, cheap incremental reparse") is unaffected
by header count. Document the analogy in the spec Assumptions
paragraph (already done — the M3 spec Assumptions paragraph 6
calls this out as "the same pattern Principle II grants `nsl-ast`
for its per-node-kind headers").

If a constitutional reviewer disputes the analogy, the fallback is
posture (B) — a 1-line constitutional amendment in the M3 PR.

**Alternatives considered**:

- *Single `nsl/Sema/Sema.h` header*: 1000+ lines, violates
  compile-time hygiene, and forces every tooling consumer to drag
  in the engine. **Rejected**.
- *Split into more headers* (one per `Symbol` subclass à la
  `nsl-ast`): no per-kind benefit since `Symbol` subclasses are
  thin and inter-related. **Rejected**.

---

## 9. Diagnostic-message text format (FR-011 / FR-015)

**Decision**: Every error/warning diagnostic from Sema uses the
following template:

```
<path>:<line>:<col>: <severity>: <human-readable message> (S<NN>)
```

The `(S<NN>)` suffix is the spec marker per FR-011. Example:

```
foo.nsl:5:5: error: 'wire' may not have an initializer; use 'reg' instead (S2)
foo.nsl:8:3: error: '=' targets a wire, output, inout, variable, or integer; use ':=' for reg (S3)
```

The per-`Sn` literal text is frozen at M3 by the `s<NN>/fail.nsl`
fixture's FileCheck `// expected-error:` directive (or the gtest
`EXPECT_EQ` on the rendered text). See
`contracts/diagnostic-string.contract.md` for the full table of
the 23 frozen messages.

**Rationale**: The spec marker suffix gives a reader the spec line
for free (`grep '(S2)' nsl_lang.ebnf` lands on line 832).
Principle VIII's "diagnostic message string MUST be cited"
(`Sn`/`Nn`/`Pn` clause) is satisfied by the literal-string
assertion in every fail fixture.

**Alternatives considered**:

- *Prefix the marker* (`error: (S2) 'wire' may not...`): less
  readable; the marker is metadata, not part of the natural-
  language message. **Rejected**.
- *Standalone marker on a `note:` line*: adds an extra diagnostic
  per error, doubling the diagnostic-stream size for no
  user-visible benefit. **Rejected**.

---

## 10. Relationship to M1 / M2 forwarded warnings (`N10`, `S26`)

**Decision**: M2's parser already emits two warnings:

- `N10`: the reserved keyword `label` used as a user identifier.
- `S26`: `function` keyword as a synonym for `func`.

At M3, **Sema does NOT re-emit these warnings**. The parser-emitted
warnings are forwarded through `DiagnosticEngine` and visible to
the user; Sema's job is to *accept* both spellings (canonical
`func` and the legacy `function`; the `label` identifier) without
adding a second warning. Per spec Assumptions paragraph 3 ("The
parser at M2 already accepts `function` and emits a parse-time
warning for `label`; M3 re-emits or upgrades these as documented
per `S26` / `N10`."), the resolution is "no upgrade" — the
warning's severity, text, and location are M2's responsibility
unchanged.

If a future spec change wants the warning to be Sema-time instead
(for richer context, e.g., suggesting renamed identifiers), that's
a routine PR — just move the diagnostic emission site.

**Rationale**: Avoids duplicate diagnostics on the same source
location. Honors the M2 spec's stated boundary.

**Alternatives considered**:

- *Re-emit at Sema with richer context*: would produce two
  diagnostics per `label` use site. **Rejected**.
- *Suppress at parser, emit only at Sema*: requires the parser to
  defer the diagnostic to a later stage — tangles the layered
  design. **Rejected**.

---

## 11. CMake target name + linkage

**Decision**: `add_nsl_library(nsl-sema ...)` per the M0 macro.
Sources listed under `lib/Sema/` in the `CMakeLists.txt`. The
`DEPENDS` argument lists `nsl-ast nsl-basic` only — *not*
`nsl-parse` (FR-002, FR-003, layered-architecture invariant). The
per-`Sn` constraint sources under `lib/Sema/Constraints/S<NN>_*.cpp`
are *internal* compilation units (no public headers); each
registers its visitor at static-init time via a registration
macro defined in `Sema.h`'s implementation detail.

**Rationale**: Standard pattern from M0/M1/M2. The
`scripts/check_layering.py` extension (per plan §"Project
Structure" and FR-003) verifies the no-edge-to-`nsl-parse`
constraint at build time.

**Alternatives considered**:

- *One-source-file Sema*: would inline every per-`Sn` checker
  into `Sema.cpp`, ballooning that file beyond the comprehensible
  size. **Rejected**.
- *Per-`Sn` static library*: adds 29 link units for no benefit.
  **Rejected**.

---

## 12. Test-driver split: lit vs gtest

**Decision**:

- **lit** (under `test/sema/`): all per-`Sn` `pass.nsl` and
  `fail.nsl` fixtures *for the 23 error/warning rows* (using
  `// expected-error:` / `// expected-warning:` directives that
  FileCheck consumes); the multi-error recovery corpus
  (`test/sema/recovery/`); the per-scope resolution corpus
  (`test/sema/resolution/`); the per-`Expr`-form width corpus
  (`test/sema/width/`); the post-Sema `-emit=ast` golden corpus
  (`test/sema/emit-ast-resolved/` plus the re-cut M2 corpus under
  `test/parse/grammar/`).
- **gtest** (under `test_unit/`): the 6 constructive-`Sn`
  introspection assertions (per Clarifications Q1 Option B); the
  `SymbolTable` scope-stack invariants; the `TypeSystem` interning
  contract; the `ResolutionPass` no-cascade unit tests (FR-017).

**Rationale**: lit + FileCheck is Constitution Principle VI's
mandated driver for diagnostic-bearing fixtures (the artifact under
test is the diagnostic *text*; lit's `// expected-error:` syntax is
the standard idiom). gtest is the right tool for C++ assertions on
internal state — the introspection observables for the constructive
`Sn` are typed values (an `ArrayRef<FieldInfo>` for `S18`, a
classifier-kind enum for `S27`), not text.

**Alternatives considered**:

- *gtest-only*: would force every diagnostic-text assertion through
  a C++ harness with hard-coded fixture strings, losing the
  `.nsl` source's role as the executable spec. **Rejected**.
- *lit-only*: would force the constructive-`Sn` introspection
  through a textual proxy (e.g., a debug print of `MemSymbol::initValues`),
  duplicating the printer surface. **Rejected**.

---

## Post-Design Constitution Re-Check

After Phase 1 design (data-model.md, contracts/, quickstart.md;
see plan.md §"Project Structure"), every gate from the initial
Constitution Check still passes:

- **Principle II (header layout)**: posture (A) recorded above
  (§8); no amendment required.
- **Principle V (determinism)**: every collection iteration in
  the printer / diagnostic stream / post-Sema AST walk is
  insertion-ordered or sorted (per §3 `TypeSystem` decision).
- **Principle VI (test discipline)**: 58 per-`Sn` fixtures + 6
  introspection unit tests + ~28 resolution/width/recovery
  fixtures cover the constraint surface and the engine surface.
- **Principle VII (spec/design coupling)**: no `docs/spec/*.ebnf`
  edits; one `CLAUDE.md` (project root) §1 footnote planned for
  the 6 constructive `Sn` carve-out (auditable record).
- **Principle VIII (TDD)**: the 58 per-`Sn` fixtures + 6
  introspection assertions are the test-first artifacts; the 23
  diagnostic-string assertions freeze the message text per the
  `Sn`/`Nn`/`Pn` clause.

**Result**: PASSES post-design. No Complexity Tracking entries
needed.
