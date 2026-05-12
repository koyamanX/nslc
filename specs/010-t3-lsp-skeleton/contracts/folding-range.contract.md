<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: Folding-Range Computation (AST → LSP `FoldingRange[]`)

**Branch**: `010-t3-lsp-skeleton` | **Date**: 2026-05-05
**Anchors**: spec FR-014 through FR-017, FR-022; US3

This contract freezes how `nsl-lsp` produces folding-range output
from a parsed `nsl::CompilationUnit`. Implemented in
`lib/LSP/FoldingRangeBuilder.{h,cpp}`:

```cpp
namespace nsl::lsp {
class FoldingRangeBuilder : public nsl::ast::ASTVisitor {
public:
    FoldingRangeBuilder(const nsl::CompilationUnit& unit,
                         const nsl::SourceManager& sm,
                         CancellationToken cancel);

    std::vector<FoldingRange> build();   // returns sorted folds
};
}
```

The builder walks `unit` once, tests each block-opener AST node
against §1, emits one `FoldingRange` per qualifying node, and
post-processes the M1 token stream for multi-line block comments
(§3).

---

## §1 Block-opener AST nodes that emit folds

The set of AST nodes that produce a fold (FR-014). Each row names
the AST node class and the EBNF production it represents; a fold
is emitted when `node.sourceRange().begin.line < node.sourceRange().end.line`
(strictly less — single-line blocks are NOT folded, FR-016).

| AST node              | EBNF production       | LSP `kind`     |
| --------------------- | --------------------- | -------------- |
| `ModuleBlock`         | `module-block §5`     | (omitted)      |
| `DeclareBlock`        | `declare-block §4`    | (omitted)      |
| `FuncDefn`            | `func-defn §7`        | (omitted)      |
| `ProcDefn`            | `proc-defn §7`        | (omitted)      |
| `StateDefn`           | `state-defn §7`       | (omitted)      |
| `SeqBlock`            | `seq-block §8`        | (omitted)      |
| `AltBlock`            | `alt-block §8`        | (omitted)      |
| `AnyBlock`            | `any-block §8`        | (omitted)      |
| `ParallelBlock`       | `par-block §8`        | (omitted)      |
| `IfStmt` (if-body)    | `if-statement §8`     | (omitted) — `if`'s then-block; emits a fold IFF the then-block spans ≥ 2 lines |
| `IfStmt` (else-body)  | `if-statement §8`     | (omitted) — separate fold for the else-block when present |
| `ForBlock`            | `for-block §8`        | (omitted)      |
| `WhileBlock`          | `while-block §8`      | (omitted)      |
| `StructuralGenerate`  | `generate §8`         | (omitted)      |
| `InitBlockStmt`       | `_init §10`           | (omitted)      |
| `StructDecl`          | `struct §3`           | (omitted)      |

`StructInstDecl` (struct instantiations) and other Decl nodes that
do not span a brace-delimited block do NOT emit folds — they're
declarations, not blocks.

The omitted `kind` field defaults to LSP's "generic code-block"
fold per the LSP 3.16 spec.

---

## §2 `FoldingRange` JSON shape

```json
{ "startLine": <int>, "endLine": <int>, "kind"?: "comment" }
```

- `startLine` = `sourceRange().begin.line - 1` (zero-based per LSP).
- `endLine` = `sourceRange().end.line - 1` (zero-based per LSP).
- `startCharacter` and `endCharacter` are omitted (clients fold
  whole-line per LSP 3.16 §FoldingRange).
- `kind` is set to `"comment"` for §3 block-comment folds; omitted
  otherwise (FR-015 / FR-016).

LSP 3.17 introduced `kind: "imports"` for import-block folds and
`kind: "region"` for marker-region folds; T3 produces neither
(LSP 3.16 floor; no preprocessor `#region`-style markers in NSL).

---

## §3 Block-comment folds

Block comments (`/* … */` per `lang.ebnf §14`) are not in the
AST — they are trivia consumed by the M1 lexer. The
`FoldingRangeBuilder` consults the M1 token stream (or a token
lattice exposed by the parser) for `BlockComment` tokens whose
start and end lines differ.

For each such token whose `SourceRange.begin.line <
SourceRange.end.line`:

```json
{ "startLine": <begin-line - 1>, "endLine": <end-line - 1>, "kind": "comment" }
```

Single-line block comments (`/* foo */` on one line) and line
comments (`// …`) are NOT folded.

---

## §4 Sort order

The builder MUST return folds sorted by `(startLine, endLine)`
ascending. Determinism (Principle V) requires this; client behavior
on unsorted folds is undefined per LSP spec.

The implementation accomplishes this by inserting folds into a
`std::vector` during the AST walk (which is deterministic, depth-first,
left-to-right per `ASTVisitor` contract) and then doing a single
final `std::sort` pass before returning. The sort is stable in the
`(startLine, endLine)` key (no third-key tie-breaking is required
because two folds cannot have identical `startLine` AND `endLine`
unless they are the same source span — a parser invariant from M2).

---

## §5 Cancellation

The handler is cancellable per FR-020h–j. Polling discipline
(FR-020i):

1. Before visiting each block-opener AST node, the builder reads
   `cancel.isCancelled()`.
2. If set, it discards any in-progress folds and aborts; the
   protocol layer responds with `RequestCancelled` (`-32800`).

The polling cost is one `std::atomic<bool>::load(memory_order_acquire)`
per visited node — well below 100 ns per call on any modern CPU.
SC-010's 200 ms cancellation-acknowledgment budget is met by the
fact that even a 10,000-node AST traversal hits ≥ one polling
point every < 1 ms in practice; the actual budget is dominated by
the time between when `$/cancelRequest` is dequeued and when the
worker thread next checks the token.

---

## §6 Parse-error recovery (FR-017)

When the M2 parser's error-recovery produces a partial AST, every
recognized block-opener still carries valid `SourceRange`. The
builder walks whatever the parser produced and emits folds for
the recognized blocks; unrecognized regions contribute no fold.

The builder MUST NOT crash, throw, or return an LSP error response
on a partial AST (FR-017). The integration test
`folding_test::ParseErrorRecovery` covers this — it opens a
fixture with a missing `}` and asserts the response is a valid
JSON-RPC `result` (not an `error`) carrying the partial-fold
array.

---

## §7 Empty / no-fold cases (FR-016)

| Document                                   | Response |
| ------------------------------------------ | -------- |
| Empty file                                 | `[]`     |
| File with no `{…}` blocks at all (e.g., a `#include`-only stub) | `[]`     |
| File where every block fits on one line    | `[]`     |
| File whose only multi-line block is a `_init` containing a single one-line `_display(...)` call | one fold for the `_init`'s outer `{…}` |

---

## §8 Test plan

| Test                                                      | What it asserts                          |
| --------------------------------------------------------- | ---------------------------------------- |
| `folding_test::AllBlockOpeners`                           | Fixture with one of every §1 production: response contains exactly N folds, sorted, with correct line ranges |
| `folding_test::SingleLineBlockNotFolded`                  | Single-line `{…}`: response is `[]` |
| `folding_test::MultiLineBlockComment`                     | `/* line1\nline2 */`: response includes a fold with `kind = "comment"` |
| `folding_test::MultiLineBlockComment_KindFieldExact`      | `kind` value is the literal string `"comment"` (not `"Comment"` / `"COMMENT"`) |
| `folding_test::ParseErrorRecovery`                        | FR-017: missing-brace fixture returns a valid response with partial folds |
| `folding_test::Cancellation`                              | SC-010: large fixture + immediate `$/cancelRequest` → response is `RequestCancelled` within 200 ms |
| `folding_test::Determinism_TwoRunsByteIdentical`          | SC-003: two runs produce byte-identical fold arrays |
| `folding_test::SortOrder`                                 | Folds are sorted by `(startLine, endLine)` |
| `folding_test::ZeroBasedLines`                            | A `module` starting on physical line 1 has `startLine = 0` |
| `folding_test::IncludeAdjustsLines`                       | A `module` after a 5-line `#include` block has `startLine = 5` (post-`#line` substitution per Principle IV) |

---

## §9 Forward-compatibility commitments

- Adding a new block-opening AST node (e.g., a future `case`
  block) MUST extend §1's table in the same PR that adds the
  node, with a new fixture under `test/lsp/fixtures/`.
- LSP 3.17's `kind: "imports"` / `"region"` are forward-compatible
  additions; if T-track ever raises the protocol floor to 3.17,
  this contract gains rows for those `kind` values.
- Adding `startCharacter` / `endCharacter` to fold output is
  forward-compatible (clients ignore unknown fields); not
  warranted at T3.
