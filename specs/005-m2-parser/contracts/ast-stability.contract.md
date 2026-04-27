<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: AST stability and determinism

**Owner**: `lib/AST/Printer.cpp` (printer walker) + every
`include/nsl/AST/<NodeKind>.h` (data-member ordering) +
`lib/AST/NodeKindNames.cpp` (enum-to-string table)
**Spec FRs**: FR-018, FR-030, FR-031, FR-032
**Spec SCs**: SC-003, SC-007
**Constitutional anchors**: Principle IV (source-locating); Principle V (deterministic pipeline)

This contract pins the invariants that make the AST a reliable
fixture target. Violations are testable in CI's `unit-tests`
stage and locally via `scripts/ci.sh unit-tests`.

## Invariant 1 — `SourceRange` per node (Principle IV, FR-018)

**Statement**: Every concrete `ASTNode` subclass MUST be
constructible only with a non-empty `SourceRange`. The base
`ASTNode` constructor is protected and takes `(NodeKind, SourceRange)`;
no subclass may default-construct a `SourceRange`.

**Rationale**: Principle IV mandates that "every AST node ... MUST
carry an NSL `SourceRange`." A node with a default-empty range
would silently break diagnostic location attribution downstream.

**Enforcement**:
- The `ASTNode(NodeKind, SourceRange)` constructor is protected;
  the default `ASTNode()` is `= delete`d.
- `test_unit/ast_node_test/` exercises every concrete node's
  constructor with a non-empty range.
- The `Printer` asserts `loc().valid()` per node before emitting;
  in `NDEBUG` builds the assertion is a `__builtin_unreachable()`
  hint to the optimizer.

## Invariant 2 — Byte-identical printer output across runs (Principle V, FR-030)

**Statement**: Given identical input bytes and identical CLI flag
list, two `nslc -emit=ast` invocations MUST produce byte-identical
stdout.

**Rationale**: Principle V's deterministic-pipeline rule and
SC-003 / SC-007.

**Enforcement**:
- `test/Driver/emit-ast.test` runs `nslc -emit=ast` twice on the
  same input and `diff`s the outputs; any difference is a CI
  failure.
- The reproducibility check in CI (build matrix stage 1) runs
  the test under both `Debug` and `Release` and both `gcc` and
  `clang` toolchains; all four outputs must match.

## Invariant 3 — No pointer-derived data in printer output (FR-031)

**Statement**: The printer's output MUST NOT contain raw
pointer values, hex addresses (`0x[0-9a-f]+`), or any value
derived from `&` or `reinterpret_cast<uintptr_t>`.

**Rationale**: FR-031 verbatim. Pointer addresses vary across
runs (ASLR, allocator randomization) and would break Invariant 2.

**Enforcement**:
- A regex-based check in `test_unit/ast_printer_test/`:
  `EXPECT_FALSE(std::regex_search(output, "0x[0-9a-f]+"))`.
- Cross-references between AST nodes serialize as
  `ref=<path>:<line>:<col>` of the target's
  `SourceRange::start` (per data-model §6).

## Invariant 4 — Deterministic collection iteration in printer (FR-032)

**Statement**: Every collection iterated by the printer MUST be
iteration-order-deterministic. Permitted: `std::vector`, sorted
`std::map<Key, V>`, `llvm::MapVector` (insertion-ordered).
Forbidden: `std::unordered_map`, `std::unordered_set`,
`llvm::DenseMap` *unless* the printer iterates a sorted view of
the keys.

**Rationale**: FR-032 verbatim. Hash-map iteration order varies
across builds (different hash seeds, allocator alignment, etc.)
and would break Invariant 2.

**Enforcement**:
- Code review during PR (M0/M1 precedent).
- `scripts/check_layering.py` extension MAY grep for
  `std::unordered_` under `lib/AST/Printer.cpp`; if found,
  fail the build with a guidance message.

## Invariant 5 — Visitor exhaustiveness at link time (FR-005)

**Statement**: A `class Foo : public ASTVisitor` that fails to
override any pure-virtual `visit(T&)` MUST fail to link.

**Rationale**: FR-005 verbatim. Adding a new node kind without
teaching the printer (or any future visitor) about it should be
caught at build time, not at runtime.

**Enforcement**:
- `ASTVisitor` declares one `virtual void visit(T&) = 0;` per
  concrete node kind in `NodeKind.def`.
- The printer's `class PrinterVisitor : public ASTVisitor`
  overrides every method explicitly; `test_unit/ast_visitor_test/`
  instantiates it to confirm linkage.
- If a contributor adds a node kind to `NodeKind.def` without
  teaching the printer, the printer's `.cpp` fails to link, and
  CI's build-matrix stage catches it on the very first run.

## Invariant 6 — Node-kind name stability (FR-022 stdout schema, FR-028)

**Statement**: The string emitted by the printer for each node
kind MUST equal the `NodeKind` enumerator name (e.g.,
`CompilationUnit`, `BinaryExpr`). Renaming an enumerator is a
schema-bumping change requiring a same-patch golden re-cut.

**Rationale**: FR-022's stdout schema commits to enumerator-name
output. FR-028 mandates coordinated golden updates on AST node
additions.

**Enforcement**:
- `lib/AST/NodeKindNames.cpp` derives the name table from
  `NodeKind.def` via X-macro (research §6); the table cannot
  drift from the enum.
- Golden test `test/Driver/emit-ast.test` asserts the exact
  enumerator names in the output.

## Invariant 7 — Format extensibility without breaking changes (FR-028)

**Statement**: Adding a new node-line field (e.g.,
`(Expr ... type=bit<8>)`) MUST be additive — existing field
positions and key names MUST NOT shift. New fields appear at the
**end** of the existing field list (after all M2 fields).

**Rationale**: Schema-unstable across milestones (per
[`nslc-emit-ast.contract.md`](./nslc-emit-ast.contract.md)
§Stability), but additive within a milestone keeps M2 fixtures
forward-compatible with M3 etc. when the goldens are re-cut.

**Enforcement**:
- The printer's per-node-kind formatter functions list fields in
  declaration order; each new field is appended.
- Golden re-cuts at M3+ verify the additive property by
  comparing field-position-by-position before accepting.

## Invariant 8 — Source-range round-trip (Principle IV, M1 chain)

**Statement**: For any node N in a printed AST, parsing the
`<source-range>` field back yields a `SourceRange` whose
`start.fileID()`, `start.offset()`, `end.fileID()`, and
`end.offset()` agree (modulo `#line`-virtual ↔ physical
translation) with the value `N->loc()` returned at print time.

**Rationale**: Principle IV chains: M1 guarantees post-`#line`
virtual coordinates resolve correctly through `SourceManager`;
M2 prints those virtual coordinates; M3+ re-parses them as a
handle into the original buffer for incremental work.

**Enforcement**:
- `test_unit/ast_printer_test/` round-trips a parsed `SourceRange`
  through the printer's format string and asserts equality with
  the original.

## Forbidden printer behaviors

- **Embedded timestamps** in output (no `time(0)`, no
  `__DATE__`).
- **Locale-dependent output** (no `std::locale`-aware
  formatting; integer-to-string uses `llvm::itostr` /
  `std::to_chars` in the C locale).
- **Hostname / username / path-prefix-leakage** in output (no
  `getenv("HOME")`, no `/Users/<name>` paths leaking through —
  `<path>` is whatever the `SourceManager` resolved, which is
  what the user passed on the command line).
- **Build-path leakage** (no `__FILE__` macros embedded;
  `__FILE__` is for diagnostic plumbing, not user output).

## Self-test snippet

```cpp
// test_unit/ast_printer_test/determinism.cpp
TEST(ASTPrinter, ByteIdenticalAcrossInvocations) {
  auto cu = parseFixture("hello.nsl");
  std::string out1, out2;
  llvm::raw_string_ostream s1{out1}, s2{out2};
  nsl::ast::print(*cu, s1);
  nsl::ast::print(*cu, s2);
  EXPECT_EQ(out1, out2);
}
```

This test failing is a Principle V violation, not a flake — the
PR that introduces the regression must be reverted.
