<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: CST shape (T2 freeze)

**Branch**: `010-t2-formatter-v0` | **Date**: 2026-05-04
**Plan**: [../plan.md](../plan.md) | **Spec**: [../spec.md](../spec.md)
**Data model**: [../data-model.md §1–§3](../data-model.md)
**Research**: [../research.md §1–§2, §7](../research.md)

This contract freezes the Concrete Syntax Tree (CST) shape that
`libNslFmt.a` consumes. The CST is internal to the formatter
library at T2 (no public-API export), but the invariants here
are load-bearing for every layout fixture and for FR-008
(idempotence) — so a contract is warranted.

---

## §1. Top-level CST structure

A CST root has type `nsl::fmt::SourceFile` and contains a
sequence of slices:

```
SourceFile
└── slices: vector<Slice>
    Slice = DirectiveTok | NSLFragmentRoot
```

- The `slices` vector is a 1:1 partition of the source-buffer
  bytes (no gaps, no overlaps — see [data-model §2](../data-model.md#§2-directive-splitter-libfmtdirectivesplitterhcpp)).
- `DirectiveTok` slices are leaves — the directive payload is
  preserved as `StringRef` (byte-for-byte) and is never
  recursed into.
- `NSLFragmentRoot` slices are interior — they wrap a
  `CSTNode` produced by the CST-mode parser pass on the
  fragment.
- **BOM bytes are NOT a Slice variant** (clarified Session
  2026-05-05 — Q1 strict refusal). The DirectiveSplitter
  treats BOM bytes as part of the leading NSLFragment slice
  (no special-case isolation); the lexer then fails to
  tokenise them and `format_buffer` returns `Status::Refused`
  per FR-012. Users with BOM-prefixed source must strip the
  BOM before formatting. The Phase-2a DirectiveSplitter
  comment about "BOM-prefix retention" is now PARTIAL:
  retention only happens via the leading-fragment rawText
  pass-through, which the LayoutRenderer never reaches in
  the Refused case.

---

## §2. Node taxonomy (interior CST nodes)

Every interior `CSTNode` corresponds 1:1 to an AST node in
`include/nsl/AST/`. The kind set is the union of all AST
node kinds defined in M2 (full list: see
`include/nsl/AST/`). The CST adds NO new productions — it
mirrors the AST.

```text
CSTNode::Kind ⊃ ASTNode::Kind   (set inclusion)
```

**No CST-only kinds exist at T2.** Adding one is a contract
change.

---

## §3. Per-node invariants (frozen)

For every `CSTNode n`:

| Invariant | Enforced by |
|---|---|
| `n.range != SourceRange{}` (Principle IV — every node has a precise byte range) | gtest `cst_invariants_test.cc` walks every node post-parse |
| `n.id` is unique within the parse (NodeID stability) | CSTBuilder's monotonic ID counter |
| Every child of `n` has a range strictly within `n.range` | gtest assertion (per-node walk) |
| `n.children` order matches source order | gtest assertion (per-node walk) |
| Every byte of `n.range` is covered by either a child node range, a Trivia in leadingTrivia/trailingTrivia, or directly by the node's own token | gtest assertion (no-byte-loss invariant) |
| `n.leadingTrivia` and `n.trailingTrivia` are vectors of `Trivia` (may be empty); never `nullptr` | type signature |
| `Trivia.text` is byte-for-byte from the source buffer | gtest assertion (lifetime tied to MemoryBuffer) |

**The "no-byte-loss invariant" is the load-bearing one for
idempotence.** If any source byte is missing from the CST, the
formatter cannot reproduce it on output, breaking FR-008.

---

## §4. Trivia attachment rules

For any token `t` between two non-trivia tokens `a` (preceding)
and `b` (following):

1. If `t` is whitespace-only: attach to `a.trailingTrivia` if
   `t` ends before a newline; attach to `b.leadingTrivia` if
   `t` follows a newline.
2. If `t` is a `LineComment` (`// …` or `# …` outside
   directives — note NSL's existing comment syntax; see
   `nsl_lang.ebnf` §14): attach to `a.trailingTrivia` if `t`
   appears on the same source line as `a`; otherwise attach to
   `b.leadingTrivia`.
3. If `t` is a `BlockComment` (`/* … */`): attach to
   `b.leadingTrivia` (block comments are visually associated
   with what follows them, per the design doc §5.3 rule 6).
4. If `t` is multiple newlines (blank-line trivia): attach to
   `b.leadingTrivia` so the formatter can decide blank-line
   policy (`blank_lines_between_modules` from §5.1) at planner
   time.
5. **Inline-comment carve-out** (clarified Session 2026-05-05 —
   Q2): if `t` is a `BlockComment` whose surrounding tokens `a`
   and `b` BOTH belong to the SAME statement (i.e., the
   parser's current production has not yet ended), tag the
   trivia as `Trivia::kind = InlineBlockComment` rather than
   the rule-3 default `BlockComment`. Same logic for inline
   `//` comments (rare, only valid if `t` is followed by a
   newline AND the next non-trivia token is on the next line
   continuing the same statement) → `InlineLineComment`. The
   LayoutPlanner emits `Inline*` trivia byte-for-byte at the
   same token-relative position rather than hoisting to
   leading/trailing line position. **Rationale**: hoisting
   loses the per-token semantic association (e.g., in
   `wire a + /* trace */ b` the `/* trace */` refers to the
   `+` operator, not the surrounding declaration).

**These rules are frozen for FR-009 / FR-010.** Every
`test/Fmt/rules/attached-comments/` fixture asserts the
expected attachment.

---

## §5. DirectiveTok shape

```cpp
struct DirectiveTok {
    enum class Opcode { Include, Define, Undef, Ifdef, Ifndef, If, Else, Endif, Line };
    Opcode             opcode;
    basic::SourceRange range;
    llvm::StringRef    rawText;   // entire directive line(s), byte-for-byte
};
```

- `rawText` includes the trailing newline of the directive line
  (or the final continuation line, if `\`-continued).
- `rawText` does NOT have its leading whitespace stripped — if
  the directive was indented (`    #include "foo.nsl"`), the
  indent is preserved.
- The formatter MUST emit `rawText` verbatim on output. It MUST
  NOT re-normalise the directive's internal whitespace, even if
  the user's directive payload uses non-canonical spacing.

**Spec mapping**: FR-012a (formatter MUST NOT reorder, deduplicate,
or syntactically rewrite directives).

---

## §6. NSL fragment processing

For each `NSLFragmentRoot` slice produced by the directive
splitter:

1. The fragment's source range is fed to a fresh
   `nsl::parse::Parser` instance with `emitCST_ = true`.
2. The parser uses the existing M2 grammar productions
   unchanged.
3. The CSTBuilder is populated as a side effect; the AST is
   discarded (the formatter does not need it).
4. Trivia between the fragment's first token and the previous
   `DirectiveTok` (if any) is attached to the fragment's
   leading-trivia surface.
5. Parse errors WITHIN a fragment refuse the entire file
   (FR-012); the formatter does NOT attempt to format other
   fragments in the same file.

---

## §7. `%IDENT%` splice handling (research §7)

`%IDENT%` splices are recognised by the M1 lexer as a single
`tok::IdentSplice` token. The CSTBuilder records the splice as
a leaf `NSLToken` node with `kind = tok::IdentSplice` and
`lexeme = "%FOO%"` (byte-for-byte from source). The
LayoutPlanner treats `IdentSplice` exactly like an identifier:
it never breaks across lines, never modifies the lexeme, and
participates in `align_struct_members` / `align_case_arrows`
column calculations using `lexeme.size()`.

**Spec mapping**: FR-011 (preserve numeric literal forms;
extends to `%IDENT%` splices by parity).

---

## §8. Round-trip invariant (idempotence root cause)

Define `serialize(CSTNode root)` as the byte-for-byte
reconstitution of source from a CST: emit every leaf token
followed by its trailing trivia, plus the leading trivia of
the next leaf, in source order. Then:

```
For every accepted SourceFile sf:
    serialize(parse_cst(sf.bytes)) == sf.bytes      (CST round-trip)
```

This invariant is the *enabling* condition for FR-008
idempotence. The CSTBuilder MUST satisfy it; the formatter
MAY then choose a different (canonical) serialisation, and
re-parsing the canonical form MUST yield a CST whose
canonical serialisation is itself.

**Test**: `test_unit/Fmt/directive_splitter_test.cc::CSTRoundTrip`
asserts `serialize(parse_cst(s)) == s` for every fixture in
`test/Fmt/idempotence/synthetic/` and (eventually, when M7
lands) `test/Fmt/idempotence/audited/`.

---

## §9. Forbidden CST shapes

The following CST shapes are bugs and MUST be caught by
gtest assertions in `cst_invariants_test.cc`:

- A node whose `range` is `{}` (empty / unknown source range).
- A node whose children's ranges overlap.
- A node whose children's ranges have a gap not covered by
  trivia.
- A `DirectiveTok` whose `rawText.size() != range.size()`.
- A `Trivia.text.data()` pointing outside the source
  `MemoryBuffer`.
- A second `CSTBuilder` instance per `Parser` (lifetime
  violation).

---

## Spec cross-reference

| Spec FR / SC | This contract section |
|---|---|
| FR-008 (idempotence) | §3 (no-byte-loss) + §8 (round-trip) |
| FR-009 / FR-010 (NSL-specific rules + comment preservation) | §4 (trivia attachment) |
| FR-011 (literal preservation) | §3 (lexeme byte-fidelity) + §7 (IdentSplice) |
| FR-012 (refuse on parse error) | §6 step 5 |
| FR-012a (raw-source parse + opaque directives) | §1, §5 |
| Principle I (no silent AST drops) | §3 children-completeness invariant + §2 taxonomy inclusion |
| Principle II (no-duplication) | §6 (existing parser used unchanged) |
| Principle IV (source-locating diagnostics) | §3 (every node has SourceRange) |
| Principle V (determinism) | §6 (parse is deterministic) |
