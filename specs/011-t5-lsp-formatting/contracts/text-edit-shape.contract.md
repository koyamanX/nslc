<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: T5 TextEdit Encoding — Single Whole-Span Convention

**Branch**: `011-t5-lsp-formatting` | **Date**: 2026-05-12
**Anchors**: spec FR-006, FR-006a, FR-008; SC-001, SC-005, SC-006

This contract freezes the FR-006 single-whole-span `TextEdit`
encoding — exactly how `nsl::fmt::FormatResult::formattedText`
is converted into the LSP `TextEdit[]` response payload.
Resolved per Session 2026-05-12 Q2 (single whole-span over
Myers-diff).

Companion contracts:

- [`formatting-api.contract.md`](./formatting-api.contract.md)
  — wire shape that wraps the encoded `TextEdit[]`.
- [`config-resolution.contract.md`](./config-resolution.contract.md)
  — produces the `Configuration` consumed by `format_buffer`.

---

## §1 Input

| Symbol | Source | Notes |
| ------ | ------ | ----- |
| `originalBuffer` | `NslTU.current.contents` | The buffer text captured at dispatch time per FR-014a. |
| `formattedText` | `FormatResult.formattedText` | T2's output. Guaranteed by T2 contract §4 to end with exactly one `\n` when non-empty; empty when input was empty. |
| `lineRange` | `std::optional<LineRange>` | `nullopt` for `textDocument/formatting`; populated for `textDocument/rangeFormatting` (per `formatting-api.contract.md` §3.2). |
| `documentLineCount` | computed from `originalBuffer` | One more than the line of the last newline character (LSP convention). |

---

## §2 Encoding — `textDocument/formatting` (whole document)

When `lineRange` is `nullopt`:

### §2.1 No-change case (already canonical)

If `originalBuffer == formattedText` byte-for-byte, the response
is:

```json
"result": []
```

An empty `TextEdit[]`. NOT a length-1 edit whose `newText`
equals the input — per FR-006a (ii), the empty array signals
"no work needed" to the editor and spares it from applying a
no-op edit that could disrupt cursor state.

### §2.2 Change case (whole-buffer replacement)

If `originalBuffer != formattedText`, the response is:

```json
"result": [
  {
    "range": {
      "start": { "line": 0, "character": 0 },
      "end":   { "line": <documentLineCount>, "character": 0 }
    },
    "newText": "<formattedText verbatim>"
  }
]
```

The `range` covers the entire document by LSP convention:
`start` is the beginning of the first line, `end` is one
position past the last line's last character (encoded as
`line: <documentLineCount>, character: 0` — the position before
the first character of a hypothetical line `documentLineCount`).
LSP clients interpret this as "replace the entire document with
`newText`."

The `newText` is `formattedText` verbatim — no transformation,
no canonicalization, no escape-sequence processing. T2's
trailing-newline guarantee flows through.

---

## §3 Encoding — `textDocument/rangeFormatting` (line range)

When `lineRange` is populated (`firstLine`, `lastLine`,
1-indexed inclusive):

### §3.1 Identifying the changed slice

T2's `format_buffer` with a non-null `LineRange` produces a
`formattedText` whose:

- Lines `1..firstLine-1` are byte-identical to the input
  (T2 FR-007 "Lines outside the range MUST be emitted
  byte-identical to the input").
- Lines `firstLine..lastLine` carry the canonical formatting.
- Lines `lastLine+1..documentLineCount` are byte-identical to
  the input.

To produce the LSP response, the encoder computes the
**byte offset** of the start of line `firstLine` (call it
`startByte`) and the byte offset of the start of line
`lastLine + 1` (call it `endByte`) in `originalBuffer`. The
slice `formattedText[startByte..endByte_formatted]` (where
`endByte_formatted` is the matching offset in `formattedText`)
is the new content for the line range.

### §3.2 No-change case (range already canonical)

If `originalBuffer[startByte..endByte] ==
formattedText[startByte..endByte_formatted]`, the response is:

```json
"result": []
```

Same convention as §2.1.

### §3.3 Change case (range replacement)

```json
"result": [
  {
    "range": {
      "start": { "line": <firstLine - 1>, "character": 0 },
      "end":   { "line": <lastLine>,      "character": 0 }
    },
    "newText": "<formattedText[startByte..endByte_formatted] verbatim>"
  }
]
```

The `range` covers exactly lines `firstLine..lastLine` (1-indexed
inclusive) translated to LSP zero-based positions
`(firstLine - 1, 0)` to `(lastLine, 0)`. The `newText` ends with
exactly one `\n` (which is the line terminator of line `lastLine`
in `formattedText`).

---

## §4 Idempotence (FR-006a (i))

Applying the encoded `TextEdit[]` to `originalBuffer` MUST
produce a buffer byte-identical to `formattedText`.

For the whole-document case (§2.2), this is trivial: the single
edit replaces the entire document with `formattedText`, so the
result is `formattedText` exactly.

For the range case (§3.3), idempotence holds because T2
guarantees the out-of-range lines are byte-identical to the
input; the LSP edit replaces only the in-range bytes; the
out-of-range bytes in `originalBuffer` are untouched and equal
the corresponding bytes in `formattedText`. The concatenation
matches.

The integration tests (FR-018 / SC-005) verify this directly:
for each fixture, apply the LSP response's `TextEdit[]` to the
input and compare byte-for-byte against `nsl-fmt --stdin <
fixture.nsl`.

---

## §5 Trailing-newline preservation (FR-006a (iii))

T2 guarantees `formattedText` ends with exactly one `\n` on
non-empty output (T2 contract §4 / Session 2026-05-05 Q3 — R7
trailing-newline policy).

For the whole-document case, the encoded `TextEdit.newText`
inherits this property — it IS `formattedText` verbatim.

For the range case, the encoded `TextEdit.newText` ends with
the line-terminating `\n` of the last line in the range. The
out-of-range tail in `originalBuffer` is preserved by the LSP
client when it applies the edit, so the resulting buffer's
trailing newline is whatever `originalBuffer`'s trailing
newline was — which equals `formattedText`'s trailing newline
because T2 preserves out-of-range bytes.

---

## §6 Determinism (Principle V / SC-006)

The encoding is deterministic given `(originalBuffer,
formattedText, lineRange)`. No hash-map iteration, no time
source, no allocator-dependent ordering. The single string
allocation for `TextEdit.newText` is a deterministic copy of
`formattedText` (or its slice).

SC-006 (byte-identical responses across runs) holds because:

- T2's `format_buffer` is a pure function of its inputs
  (T2 contract §6 — Principle V).
- The configuration resolver is deterministic given the
  filesystem state at request time (research.md R2 / §8 of
  `config-resolution.contract.md`).
- The range computation is a pure function of the request's
  `Range` and `documentLineCount` (research.md R5).
- The encoding (this contract) is a pure function of its inputs.

The composition is therefore deterministic.

---

## §7 Wire-shape examples (frozen — used by integration tests)

### §7.1 Whole-document, changed

Input buffer (input.nsl, 8 bytes including trailing newline):

```text
a=1;\nb=2;\n
```

Suppose `format_buffer` returns:

```text
a = 1;\nb = 2;\n
```

Encoded response:

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": [
    {
      "range": {
        "start": { "line": 0, "character": 0 },
        "end":   { "line": 2, "character": 0 }
      },
      "newText": "a = 1;\nb = 2;\n"
    }
  ]
}
```

`documentLineCount` is `2` (two newline-terminated lines).

### §7.2 Whole-document, unchanged

Input buffer equal to `format_buffer` output. Encoded response:

```json
{
  "jsonrpc": "2.0",
  "id": 43,
  "result": []
}
```

### §7.3 Range, changed (lines 2..2 only)

Input buffer (3 lines):

```text
a=1;\nx=2;\nc=3;\n
```

Suppose `format_buffer` with `LineRange{firstLine: 2, lastLine: 2}`
returns:

```text
a=1;\nx = 2;\nc=3;\n
```

Encoded response:

```json
{
  "jsonrpc": "2.0",
  "id": 44,
  "result": [
    {
      "range": {
        "start": { "line": 1, "character": 0 },
        "end":   { "line": 2, "character": 0 }
      },
      "newText": "x = 2;\n"
    }
  ]
}
```

The LSP client replaces lines 2..2 (zero-based row 1) with the
new content; lines 1 and 3 remain untouched.

---

## §8 Spec cross-reference

| Spec FR / SC | This contract section |
|---|---|
| FR-006 | §2, §3 (encoding shape) |
| FR-006a (i) | §4 (idempotence) |
| FR-006a (ii) | §2.1, §3.2 (empty array on no-change) |
| FR-006a (iii) | §5 (trailing-newline preservation) |
| FR-008 | §2.1 (empty document → empty TextEdit[]) |
| SC-001 | §2.2 (whole-document Success encoding matches CLI) |
| SC-005 | §4 + §7 (byte-equivalence verified by integration tests) |
| SC-006 | §6 (determinism by composition) |
| Principle V | §6 (no env-derived state in the encoder) |
