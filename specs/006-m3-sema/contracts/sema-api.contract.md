<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: Sema public-API surface

**Owner**: `include/nsl/Sema/Sema.h` (entry-point + `SemaResult`),
`include/nsl/Sema/SymbolTable.h` (Symbol hierarchy + Scope stack),
`include/nsl/Sema/TypeSystem.h` (Type hierarchy + interner)
**Spec FRs**: FR-001, FR-002, FR-003, FR-004, FR-005, FR-006, FR-007, FR-008
**Spec SCs**: SC-007, SC-009
**Constitutional anchors**: Principle II (layered architecture); Principle V (deterministic pipeline)

This contract pins the public-API invariants every consumer of
`nsl-sema` (the M3 driver glue, the M4+ later-stage layers, and the
T-track tooling libraries `nsl-fmt` / `nsl-lsp` / `nsl-lint`) can
rely on across patches.

## Invariant 1 — Three public headers, single library (Principle II §3 analogy; FR-001)

**Statement**: `nsl-sema` exposes exactly three public headers under
`include/nsl/Sema/`: `Sema.h`, `SymbolTable.h`, `TypeSystem.h`. No
other public headers are added under `include/nsl/Sema/` without a
spec amendment + entry in this contract.

**Rationale**: Mirrors the design-doc decomposition (§6 SymbolTable,
§6.x TypeSystem, §11 Compilation/driver-glue surface). Splitting the
three concerns reduces consumer compile cost (a tooling library that
needs only `SymbolTable` does not pull in the resolution-pass
visitors). See research.md §8 for the Principle II §3 posture.

**Enforcement**:
- `find include/nsl/Sema -maxdepth 1 -name '*.h'` MUST produce
  exactly three lines: `Sema.h`, `SymbolTable.h`, `TypeSystem.h`.
- A CI guard in `scripts/check_layering.py` (M2 introduced; M3
  extends) asserts the three-header count.
- Adding a fourth header is a spec amendment and a row in this
  contract — never a routine PR.

## Invariant 2 — Layered dependency: only `nsl-ast` and `nsl-basic` (FR-002, FR-003)

**Statement**: The `nsl-sema` link target depends on `nsl-ast` and
`nsl-basic` and **nothing else**. No edges to `nsl-parse`,
`nsl-dialect`, `nsl-lower`, `nsl-driver`, or any later layer. The
parser does not gain a Sema-time check (no reverse edge).

**Rationale**: Constitution Principle II's downward-only flow.
Tooling reuse (`libNSLFrontend.a`) is contingent on this — a tool
that wants Sema must not also drag in the parser.

**Enforcement**:
- `scripts/check_layering.py` parses the CMake-emitted target graph
  and asserts no `nsl-sema → {nsl-parse, nsl-dialect, nsl-lower,
  nsl-driver}` edge.
- Spec SC-009 mechanically gates this on every CI run.

## Invariant 3 — `Sema::run()` is the single public entry point (FR-008, FR-019)

**Statement**: External callers invoke Sema **only** through
`Sema::run(CompilationUnit&) → SemaResult`. The internal pass
visitors (`ResolutionPass`, `ConstraintCheckRegistry`, the per-`Sn`
visitors) are private impl details under `lib/Sema/` and are NOT
exposed in any public header.

**Rationale**: Keeps the public surface minimal; lets the pass
arrangement (Q3 hybrid; research §1) evolve without breaking
consumers.

**Enforcement**:
- `include/nsl/Sema/Sema.h` declares only `Sema`, `SemaResult`,
  `Sema::run`, and the engine constructor; no pass-class names
  appear.
- `test_unit/sema_api_test/` greps the post-preprocess header for
  forbidden symbols.

## Invariant 4 — Stable introspection API for the 6 constructive `Sn` (Q1 Option B)

**Statement**: Per Clarifications session 2026-04-28 Q1 → Option B
and research.md §6, six observable methods are public-stable across
patches:

| `Sn` | API entry point | Return |
|---|---|---|
| `S13` | `AltBlock::cases()` / `AnyBlock::cases()` | `ArrayRef<Case>` (priority-ordered for `alt`, declaration-ordered for `any`) |
| `S18` | `StructTypeSymbol::fields()` / `StructTypeSymbol::totalWidth()` | `ArrayRef<FieldInfo>` MSB-first; `uint64_t` total bits |
| `S19` | `SeqBlock::clockBudget()` | `uint64_t` (count of `goto`s + back-edge transitions; M3 stub, full enforcement M5/M6) |
| `S23` | `RegSymbol::type()` | `TypeRef` — `BitVectorType{1}` when width omitted with init |
| `S24` | `MemSymbol::initValues()` | `ArrayRef<uint64_t>` of size `depth()` (zero-padded) |
| `S27` | `Sema::classifyIdentifierExpr(IdentifierExpr&)` | `ClassifierKind ∈ {Value, ControlTerminalTap, …}` |

**Rationale**: These are the test surface for the constructive
`Sn` (per Q1 Option B); they're also reusable by tooling (T4 LSP
hover consumes `StructTypeSymbol::fields()`).

**Enforcement**:
- `test_unit/constructive_sn_test/` exercises every entry point.
- Removing or renaming any of these methods is a spec amendment
  + row update in this contract.

## Invariant 5 — `TypeSystem::equal(a, b) == (a == b)` (FR-007)

**Statement**: `TypeSystem::equal` is implemented as raw pointer
comparison. Two structurally-equal `Type*` MUST compare equal by
pointer; two structurally-different `Type*` MUST compare unequal.

**Rationale**: Design §6.x line 846. Pointer equality is what
makes downstream type-checks O(1) without re-running structural
comparison.

**Enforcement**:
- `test_unit/type_system_test/` asserts `bv8_a == bv8_b` for two
  invocations of `bitVector(8)` and `bv8 != bv16` for different
  widths.
- The `BitType` and `UnresolvedType` singletons (one each per
  `TypeSystem` instance) are re-used across all `bit()` /
  `unresolved()` calls.

## Invariant 6 — Ownership transfer to `SemaResult`

**Statement**: After `Sema::run()` returns, the `SymbolTable` and
`TypeSystem` ownership is *moved* into `SemaResult`. The `Sema`
instance's internal `symbols_` / `types_` members are nulled-out;
re-invoking `run()` on the same `Sema` is a no-op (or asserts in
debug builds).

**Rationale**: Later-stage consumers (`-emit=ast` printer at M3,
`-emit=mlir` at M5+) walk the post-Sema AST without re-running
resolution; they need stable ownership of the symbol/type stores
for the lifetime of the `CompilationUnit` they consume.

**Enforcement**:
- `test_unit/sema_lifecycle_test/` asserts that `SemaResult::symbols`
  and `SemaResult::types` are non-null after `run()`, and that a
  second `run()` invocation triggers an assertion.

## Invariant 7 — `DiagnosticEngine` is the only diagnostic surface (FR-014)

**Statement**: `Sema` writes every diagnostic through the
`DiagnosticEngine&` reference passed to its constructor. Direct
writes to `stderr` from any `nsl-sema` source file are forbidden.

**Rationale**: Constitution Principle IV. Single diagnostic surface
makes the LSP, the JSON formatter, and the human formatter
interchangeable.

**Enforcement**:
- A CI grep guard against `std::cerr` / `fprintf(stderr` /
  `llvm::errs()` in `lib/Sema/**.cpp` (extend M2's M2 grep guard
  for `nsl-parse`).

## Invariant 8 — Public API symbols are documented per the LLVM doc-comment style

**Statement**: Every public class, method, and free function in
`Sema.h` / `SymbolTable.h` / `TypeSystem.h` carries a Doxygen-style
`///` comment naming its purpose, its preconditions, and its
postconditions where non-trivial.

**Rationale**: The headers are the authoritative consumer
documentation; T-track tooling will read them. Constitution
Build/Code/Licensing standards (LLVM-style) require this for public
surfaces.

**Enforcement**:
- A CI grep guard verifies every `public:` member in the three
  Sema headers is preceded by at least one `///` comment.
