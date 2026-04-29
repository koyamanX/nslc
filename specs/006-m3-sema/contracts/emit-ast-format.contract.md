<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: Post-Sema `-emit=ast` text format

**Owner**: `lib/AST/Printer.cpp` (extended at M3) +
`lib/Driver/EmitAST.cpp` (calls Sema before printing)
**Spec FRs**: FR-019, FR-020, FR-021, FR-022, FR-029
**Spec SCs**: SC-003, SC-007
**Constitutional anchors**: Principle V (deterministic pipeline; `-emit=` flag invariant)

This contract pins the byte-stable text format frozen at M3 by the
re-cut `-emit=ast` golden corpus. Per Clarifications session
2026-04-28 Q2 → Option A, M3 re-cuts the existing `-emit=ast`
golden in place — no new CLI flag is added (FR-021); the post-Sema
enrichments are additive.

## Invariant 1 — One flag covers both pre-Sema and post-Sema modes

**Statement**: `nslc -emit=ast foo.nsl` produces the **post-Sema**
AST whenever Sema succeeds (the M3 default). When Sema fails, the
driver exits non-zero and emits NO output (per "no partial output"
rule). There is NO new flag for the post-Sema view; the M2 flag is
extended additively.

**Rationale**: Q2 Option A's stated goal — minimum CLI surface,
maximum debuggability ("a contributor sees resolution data via
`nslc -emit=ast foo.nsl` without learning a new flag").

**Enforcement**:
- `tools/nslc/main.cpp` has no new switch case at M3.
- `test/Driver/emit-ast-no-new-flag.test` greps the help output and
  asserts that the M3 flag list is identical to M2's plus zero new
  flags.

## Invariant 2 — Additive type suffix on every `Expr` line

**Statement**: After M3, every `Expr`-kind node line in the
`-emit=ast` output gains a ` : <Type>` suffix between the
`SourceRange` and the kind-specific fields. The suffix format:

| `<Type>` rendering | When |
|---|---|
| `Bit` | `Expr::inferredType() == TypeSystem::bit()` |
| `BitVector(<N>)` | `Expr::inferredType()` is a `BitVectorType` of width `<N>` |
| `Struct(<name>)` | `Expr::inferredType()` is a `StructType` named `<name>` |
| `Memory(<depth> x <element>)` | `Expr::inferredType()` is a `MemoryType` of given depth and element rendering recursively (ASCII `x`, not Unicode `×`, for regex-parsability and locale-independence — see Invariant 7) |
| `Unresolved` | `Expr::inferredType() == TypeSystem::unresolved()` (the no-cascade marker per FR-017) |

**Rationale**: Research §7. The suffix is tail-anchored so
fixture-readability and regex-parsability of M2 lines is preserved.

**Enforcement**:
- The re-cut golden corpus under `test/parse/grammar/` and the new
  `test/sema/emit-ast-resolved/` corpus assert the format
  byte-exactly.
- A regex guard in `test_unit/printer_format_test/` asserts
  `^\S+ <[^>]+> .* : (Bit|BitVector\(\d+\)|Struct\(\S+\)|Memory\(\d+ x .+\)|Unresolved)`
  on every `Expr` line.

## Invariant 3 — Additive decl-loc suffix on every name-ref

**Statement**: After M3, every `IdentifierExpr` /
`FieldAccessExpr` (head identifier) / `ScopedName` (head identifier)
node line in the `-emit=ast` output gains a ` -> decl@<file>:<line>:<col>`
suffix after the type suffix from Invariant 2 (ASCII `->`, not
Unicode `→`, for regex-parsability — see Invariant 7). The
`<file>:<line>:<col>` is the resolved `Symbol*::declLoc.start`
rendered through the same `SourceRange` formatter as the per-line
range.

When the name is `Unresolved` (no resolved `Symbol*`), the suffix
is omitted entirely (the type suffix is `Unresolved`, which is
sufficient signal).

**Rationale**: Research §7. The suffix lets a reader of the AST
dump click straight to the declaration site.

**Enforcement**:
- The re-cut golden corpus asserts the suffix shape byte-exactly.
- `test_unit/printer_format_test/` exercises both resolved and
  unresolved name-ref paths.

## Invariant 4 — Pre-Sema mode preserved (printer detection rule)

**Statement**: When the printer is invoked on an AST whose `Expr`
nodes have `inferredType() == nullptr` (i.e., M2-style parser-only
output, or a hypothetical future `--no-sema` debug flag), the
printer emits the M2 format unchanged — no `:` suffix, no
`-> decl@…` suffix.

**Rationale**: FR-022 wraps both modes in one printer entry point
so future flags (Sema-failure-with-partial-AST, debug `--no-sema`,
etc.) work without printer surgery. The M2 fixtures still parse
correctly through the printer if invoked pre-Sema.

**Enforcement**:
- `test_unit/printer_format_test/` constructs a parser-only
  `CompilationUnit` (no Sema run) and asserts the output is the M2
  format byte-exactly.

## Invariant 5 — Byte-stable across runs (Principle V; FR-029)

**Statement**: Two `nslc -emit=ast` invocations on the same input
+ flag list MUST produce byte-identical stdout. Across both
supported build types (`Debug` / `Release`) and both supported
compilers (`gcc` / `clang`), all four outputs MUST match.

**Rationale**: FR-029, SC-003, SC-007, Principle V.

**Enforcement**:
- The reproducibility check in CI's build-matrix stage runs the
  two-invocation `diff` under all four (build × compiler)
  combinations.
- A direct counterpart of the M2 stability gate, extended to the
  post-Sema enrichments.

## Invariant 6 — Format-bump documented in same patch (Principle VII)

**Statement**: Any future change to the post-Sema printer format
(e.g., M5 adds expansion-residue annotations; T-track LSP adds
JSON output behind a new flag) MUST land the format change AND the
re-cut goldens AND a row in this contract in the same patch — never
as a "follow-up".

**Rationale**: Constitution Principle VII (spec/design coupling).
A drifted golden is a drifted contract; both must move together.

**Enforcement**:
- Code review checklist row: "Did this PR change `-emit=ast` post-
  Sema format? If yes, are goldens re-cut AND is this contract
  updated?"
- CI's diff against the goldens fails if format changed without
  golden re-cut.

## Invariant 7 — ASCII-only printer separators

**Statement**: The post-Sema enrichments use ASCII separators only:
the type-element separator in `Memory(<depth> x <element>)` is the
ASCII letter `x` (U+0078), and the decl-loc separator is the ASCII
sequence `->` (U+002D U+003E). Unicode `×` (U+00D7) and `→` (U+2192)
are forbidden in printer output.

**Rationale**: (a) Regex-parsability — downstream tooling (the LSP
semantic-tokens hover, future JSON-AST emit at T-track, custom
analysis scripts) parses the printer output with `^.* : <Type>(.*)$`
and `\-> decl@<loc>$` patterns; ASCII keeps the regexes simple and
locale-independent. (b) Encoding-stability — Unicode characters
risk locale-dependent rendering on terminals, and BOM-handling
complications in golden-diff machinery. (c) Consistency with the
existing M2 printer which is ASCII-only by convention. The contract
chose Unicode at original drafting; the implementation at commit
`420a593` used ASCII; this contract is amended to codify what
shipped, per Invariant 6's "format-bump documented in same patch"
rule applied retroactively to the just-landed Phase 3 PR.

**Enforcement**:
- Goldens under `test/sema/emit-ast-resolved/` and re-cut
  `test/parse/grammar/**/*.ast` use ASCII separators byte-exactly.
- A regex guard `EXPECT_FALSE(std::regex_search(output, "[×→]"))`
  in `test_unit/printer_format_test/` (or similar) catches any
  future drift to Unicode.
- The standard Determinism gate (Invariant 5) catches inadvertent
  encoding shifts because the byte-diff fails on any Unicode
  character introduction.
