<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: Sema stability and determinism

**Owner**: `lib/Sema/Sema.cpp` (engine), `lib/Sema/ResolutionPass.cpp`
(resolution + width inference), `lib/Sema/SymbolTable.cpp` (scope stack),
`lib/Sema/TypeSystem.cpp` (interner)
**Spec FRs**: FR-016, FR-017, FR-029, FR-030, FR-031
**Spec SCs**: SC-003, SC-004, SC-005
**Constitutional anchors**: Principle IV (source-locating); Principle V (deterministic pipeline); Principle VIII (TDD)

This contract pins the invariants that make Sema's output a
reliable fixture target — both as a stage in the `-emit=ast`
post-Sema printer and as the input to every later milestone (M4
dialect, M5/M6 lowering, M7 end-to-end). Violations are testable
in CI's `unit-tests` and `lowering` stages and locally via
`scripts/ci.sh`.

## Invariant 1 — Pure function from input AST + flags (Principle V, FR-029)

**Statement**: `Sema::run(CompilationUnit&) → SemaResult` is a pure
function of (AST contents, CLI flag list). No environment-derived
input (CWD, mtime, locale, hostname, env vars other than
`NSL_INCLUDE`) MAY influence resolution, type inference, or the
emitted diagnostic stream.

**Rationale**: Principle V's deterministic-pipeline rule and FR-029.
A Sema run that produces different post-AST state on a different
host invalidates the audited-corpus regression at M7.

**Enforcement**:
- `test/sema/emit-ast-resolved/` runs `nslc -emit=ast` twice on the
  same input and `diff`s outputs; any difference is a CI failure.
- The reproducibility check in CI (build matrix stage 1) runs the
  test under both `Debug` and `Release` and both `gcc` and `clang`
  toolchains; all four outputs MUST match.

## Invariant 2 — Deterministic collection iteration in serialized output (FR-030)

**Statement**: Every collection iterated by the post-Sema
`-emit=ast` printer or by the diagnostic stream MUST be
iteration-order-deterministic. Permitted: `std::vector`, sorted
`std::map<Key, V>`, `llvm::MapVector` (insertion-ordered),
`std::vector<Symbol*>` (matches `Scope::declOrder`). Forbidden in
any printer / serializer iteration:
`std::unordered_map`, `std::unordered_set`, `llvm::DenseMap`
*unless* the emitter iterates a sorted view of the keys.

**Rationale**: FR-030 verbatim. `TypeSystem` uses `llvm::DenseMap`
for *lookup* (Sema-internal, not serialized); when the `-emit=ast`
printer needs to enumerate types (e.g., a struct's fields in order),
it iterates the `StructTypeSymbol::fields()` `ArrayRef`, which is
the deterministic source.

**Enforcement**:
- A CI grep guard in `scripts/check_determinism.py` flags every
  `unordered_map`, `unordered_set`, or `DenseMap` literal that
  appears in `lib/Sema/*.cpp` AND is not protected by a sorted-view
  iteration.
- Two-build determinism gate (Principle V; M0/M1/M2 fixture)
  catches escapes.

## Invariant 3 — Pointer equality implies type equality (FR-007)

**Statement**: For any two `TypeRef a, b` returned from the same
`TypeSystem` instance: `a == b ⟺ structurally_equal(a, b)`.

**Rationale**: Design §6.x line 799. The interning contract is what
makes downstream type-checks O(1) without round-tripping through a
structural-equal predicate.

**Enforcement**:
- `test_unit/type_system_test/` asserts `ts.bitVector(8) ==
  ts.bitVector(8)` and `ts.bitVector(8) != ts.bitVector(16)`.
- A property-based test enumerates 100 random struct/memory
  combinations and asserts the contract.

## Invariant 4 — Stable introspection API across patches (Q1 Option B)

**Statement**: The six introspection-API entry points listed in
`sema-api.contract.md` Invariant 4 (`AltBlock::cases()`,
`StructTypeSymbol::fields()`, `SeqBlock::clockBudget()`,
`RegSymbol::type()`, `MemSymbol::initValues()`,
`Sema::classifyIdentifierExpr`) are stable across patches: their
signatures, return types, and observable behaviors do not change
without a spec amendment.

**Rationale**: These are the test-fixture surface for the 6
constructive `Sn` per Clarifications Q1 → Option B. Renaming or
weakening one would silently break the per-`Sn` test pair.

**Enforcement**:
- `test_unit/constructive_sn_test/` is the freeze-fixture; failures
  point directly at the offending API change.
- A code-review checklist row in the M3 PR template (and forward)
  for "Did this PR change a Q1-Option-B-stable API?"; YES requires
  spec amendment.

## Invariant 5 — No pointer-derived data in serialized output (FR-031)

**Statement**: The post-Sema `-emit=ast` printer's output and every
diagnostic message MUST NOT contain raw pointer values, hex
addresses (`0x[0-9a-f]+`), or any value derived from `&` or
`reinterpret_cast<uintptr_t>`.

**Rationale**: FR-031 verbatim. Pointer addresses vary across runs
(ASLR, allocator randomization) and would break Invariant 1.

**Enforcement**:
- A regex-based check in `test/sema/emit-ast-resolved/` and
  `test_unit/diagnostic_format_test/`:
  `EXPECT_FALSE(std::regex_search(output, "0x[0-9a-f]+"))`.
- Cross-references between AST nodes / Symbols serialize via the
  target's `declLoc.start` byte offset rendered as
  `<file>:<line>:<col>`, NEVER as a `Symbol*` value.

## Invariant 6 — No-cascade guarantee for unresolved names (FR-017)

**Statement**: A single `IdentifierExpr` whose target name is
unresolved produces **exactly one** "unresolved name '<X>'"
diagnostic, even if `<X>` is referenced at `M ≥ 2` use sites in the
compilation unit. Downstream `Sn` checks that consume the
unresolved subtree skip silently — no synthetic width-mismatch /
type-mismatch / direction-mismatch diagnostics fed by the
unresolved symbol's `Unresolved` `TypeRef`.

**Rationale**: FR-017 verbatim, plus spec SC-005 ("`M`-use-site
typo produces exactly one diagnostic"). The clangd-style LSP
ergonomics depend on this; a cascading-diagnostics Sema is
unusable in `publishDiagnostics`.

**Enforcement**:
- `test/sema/recovery/unresolved_cascade.nsl` is a fixture with
  one typo + 5 use sites; FileCheck asserts exactly one
  `unresolved name` diagnostic.
- `test_unit/resolution_pass_test/` constructs an AST with a
  typo'd identifier and 5 unresolved use sites and asserts
  `diag_engine.errorCount() == 1`.

## Invariant 7 — Multi-error reporting in a single Sema run (FR-016, SC-004)

**Statement**: Multiple independent `Sn` violations in a single
source file are ALL reported in a single Sema run, in source order.
For `K ∈ {2, 3, 5}` independent violations, exactly `K` diagnostics
are emitted (no fewer, no more — and no cascading synthetic
errors).

**Rationale**: FR-016 + SC-004 + spec US3 ("Diagnose every Sema
error in one Sema pass"). Hybrid recovery (Q3 Option C) makes this
structurally true: per-`Sn` walkers are independent, so a `S2`
failure on one `wire` does not block the `S7` check on a `seq`
block elsewhere.

**Enforcement**:
- `test/sema/recovery/multi_K2.nsl`, `multi_K3.nsl`, `multi_K5.nsl`
  fixtures assert the expected diagnostic count via FileCheck
  `// expected-error:` directive count.

## Invariant 8 — Symbol identity does not leak across Sema runs

**Statement**: A `Symbol*` returned from `SymbolTable::lookup(...)`
during one `Sema::run()` invocation MUST NOT be passed to or
compared with a `Symbol*` from a *different* `Sema::run()`
invocation. The `SemaResult::symbols` ownership transfer
(`sema-api.contract.md` Invariant 6) implicitly guarantees this —
each `Compilation` gets its own `SymbolTable` instance.

**Rationale**: Pointer-equality contracts only hold within a single
`SymbolTable` instance. An LSP that runs Sema repeatedly over an
edited document gets fresh `SymbolTable`s per run.

**Enforcement**:
- Documented in `Sema.h`'s public Doxygen comment on `SemaResult`.
- An LSP-tier test at T3 will exercise this when it lands; M3 ships
  the documentation and the contract row only.
