<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract — VS Code Language Configuration

**Feature**: `009-t1-textmate-grammar`
**Phase**: 1 (design / contracts)
**Date**: 2026-05-04

This contract freezes the `editors/vscode/language-configuration.json`
schema for T1 — every field T1 ships, the value/shape it carries,
and the rationale a reader needs to decide whether a future change
is constitution-compatible.

The reference schema for the file is VS Code's own
`vscode.LanguageConfiguration` interface, documented at
`vscode.languageserver-types`. T1 uses only the stable subset.

---

## §1 Top-level keys (frozen at T1 PR-landing)

```json
{
  "_comment_top": "SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception. JSON has no comment syntax; see specs/009-t1-textmate-grammar/contracts/language-config.contract.md for the rationale and field-by-field reasoning.",
  "comments":          { … },
  "brackets":          [ … ],
  "autoClosingPairs":  [ … ],
  "surroundingPairs":  [ … ],
  "wordPattern":       "…",
  "indentationRules":  { … }
}
```

**`_comment_top`** is the SPDX-header carrier per research.md §2.
The string MUST start with `"SPDX-License-Identifier: Apache-2.0
WITH LLVM-exception"` so `scripts/check_spdx.py` recognises it.

VS Code ignores unknown top-level keys; `_comment_top` does not
affect editor behaviour.

---

## §2 `comments` (FR-012)

```json
"comments": {
  "lineComment":   "//",
  "blockComment":  ["/*", "*/"]
}
```

| Field | Required value | Rationale |
|---|---|---|
| `lineComment` | `"//"` | Per `nsl_lang.ebnf §14`'s `line_comment` production. |
| `blockComment[0]` | `"/*"` | Per `nsl_lang.ebnf §14`'s `block_comment` open. |
| `blockComment[1]` | `"*/"` | Per same. Block comments are non-nestable but `comments.blockComment` does not encode that — the grammar's begin/end rule does (see grammar-coverage.contract.md §4). |

VS Code uses these to drive the comment-toggle commands tested by
spec User Story 2 acceptance scenarios 2 and 3.

---

## §3 `brackets` (FR-013)

```json
"brackets": [
  ["{", "}"],
  ["[", "]"],
  ["(", ")"]
]
```

| Pair | Used by | Rationale |
|---|---|---|
| `{` `}` | block delimiters per `nsl_lang.ebnf §§4–9` (declare / module / func / proc bodies; alt / any / seq / if blocks); generate-loop bodies §8 | Standard. |
| `[` `]` | width specifications and bit slices per `nsl_lang.ebnf §11` | Standard. |
| `(` `)` | function-call argument lists, parenthesised expressions per `nsl_lang.ebnf §11`; declare/module port lists §4 | Standard. |

**Single quote `'` is intentionally absent.** The Verilog-sized
literal `8'hFF` (per `nsl_lang.ebnf §13`) consumes `'` as a
literal-form delimiter; treating `'` as a bracket-pair would
auto-close the literal mid-parse and break authoring (see spec
Edge Cases). Documented as a non-default choice here so the
rationale survives future contributor curiosity.

---

## §4 `autoClosingPairs` (FR-013)

```json
"autoClosingPairs": [
  { "open": "{",  "close": "}" },
  { "open": "[",  "close": "]" },
  { "open": "(",  "close": ")" },
  { "open": "\"", "close": "\"", "notIn": ["string", "comment"] }
]
```

| Pair | `notIn` | Rationale |
|---|---|---|
| `{` `}`, `[` `]`, `(` `)` | (none) | Standard. |
| `"` `"` | `["string", "comment"]` | Don't auto-close inside an existing string (avoid `""` triggering re-close) or inside a comment (where the user's typing literal characters, not delimiters). |

Single-quote `'` deliberately omitted (same Verilog-sized-literal
rationale as §3).

---

## §5 `surroundingPairs` (FR-013)

```json
"surroundingPairs": [
  ["{", "}"],
  ["[", "]"],
  ["(", ")"],
  ["\"", "\""]
]
```

The four pairs that wrap a selection when the user types the
opening character with a non-empty selection. Single quote `'`
deliberately omitted (same rationale).

---

## §6 `wordPattern` (FR-014)

```json
"wordPattern": "(-?\\d*\\.\\d\\w*)|([^\\`\\~\\!\\@\\#\\%\\^\\&\\*\\(\\)\\-\\=\\+\\[\\{\\]\\}\\\\\\|\\;\\:\\'\\\"\\,\\.\\<\\>\\/\\?\\s]+)"
```

VS Code's recommended `wordPattern` form, identical to the
TypeScript grammar's. Two alternations:

1. `-?\d*\.\d\w*` matches numeric literals like `-0.5e10` (used
   by VS Code's "select word" command). This alternation is
   loose enough to span Verilog-sized literals without breaking
   them (`8'hFF` is treated as one word because none of `'`,
   the digits, or the hex chars are in the negation class).
2. The negation-class alternation matches identifiers — every
   character that is not whitespace and not a punctuation
   metacharacter. Aligns with `nsl_lang.ebnf §13`'s `identifier`
   production: leading letter or underscore, followed by
   letters / digits / underscores.

The `wordPattern` is consumed by VS Code's word-selection,
double-click, and rename-affordance commands; T1 ships the
stable shape, refinement deferred to T9 (LSP rename) where the
real word boundaries are known semantically.

---

## §7 `indentationRules` (FR-015)

```json
"indentationRules": {
  "increaseIndentPattern": "^.*\\{[^}\"']*$",
  "decreaseIndentPattern": "^\\s*\\}"
}
```

| Rule | Pattern | Trigger |
|---|---|---|
| `increaseIndentPattern` | `^.*\{[^}"']*$` | Line ends with an unmatched `{` (and not closed on the same line, and not inside a string or single-quoted literal). Next line indents one level deeper. |
| `decreaseIndentPattern` | `^\s*\}` | Line begins with `}` (after optional whitespace). VS Code dedents this line one level relative to its parent. |

Conservative — covers the common case (`{` open, `}` close on
its own line). NSL-specific richer rules (smart indent for
`alt:` arms, hanging arguments in `func` declarations) are
deferred to T5 (LSP `formatting`) and T2 (`nsl-fmt`). At T1, the
goal is "not actively wrong"; `nsl-fmt` will normalise on save
once T5 ships.

---

## §8 Fields T1 explicitly does NOT include

| Field | Reason |
|---|---|
| `onEnterRules` | Richer indent behaviour (e.g. continuing a `//` line comment after Enter) belongs with T5 (LSP `formatting`) where the formatter has structural context. |
| `folding.markers` | Code-folding range computation is T3 (LSP `foldingRange`); T1 can fall back to VS Code's default brace-based folding without extra config. |
| `colorizedBracketPairs` / `bracketPairColorization` | VS Code default is fine. |
| `autoCloseBefore` | VS Code default is fine; NSL has no characters that should suppress auto-close in unusual ways. |
| `__characterPairs` (private) | VS Code internal; not part of the public schema. |

---

## §9 Versioning

| Change | Constitution-compatible | Action required |
|---|---|---|
| Add a new bracket pair (e.g. for a future struct-pattern syntax) | Yes — informational extension | Update §3, §4, §5; mention in spec §FR-013 |
| Drop the `_comment_top` SPDX header | **No** — Principle IX SPDX rule | Reject |
| Auto-close single quote `'` | **No** — breaks Verilog-sized literals | Reject |
| Add `'` to `surroundingPairs` only (not `autoClosingPairs`) | Conditional | Acceptable: surrounding-pair triggers only with selected text, not free typing — does not break literal authoring. Update §5; document in spec Edge Cases. |
| Tighten `wordPattern` to match `nsl_lang.ebnf §13`'s exact identifier production | Yes — refinement | Update §6 with the specific regex; add unit-style test under `test/tooling/textmate/word-pattern/` |
| Add `onEnterRules` | Yes when T5 lands | Defer; do not add at T1 |
