<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: T5 Formatting API — Wire-Level Behaviour

**Branch**: `011-t5-lsp-formatting` | **Date**: 2026-05-12
**Anchors**: spec FR-001, FR-002, FR-003, FR-004, FR-007, FR-008,
FR-012, FR-013, FR-014, FR-014a, FR-014b, FR-016; SC-001/002/006/009/010

This contract freezes the wire-visible behaviour of the two new
LSP methods `textDocument/formatting` and
`textDocument/rangeFormatting` at T5. Any later T-track milestone
that modifies any frozen entry MUST update this contract in the
same PR (Principle VII coupling).

Companion contracts:

- [`config-resolution.contract.md`](./config-resolution.contract.md)
  — freezes the FR-005 family TOML-precedence behaviour (what
  `Configuration` is passed to `format_buffer`).
- [`text-edit-shape.contract.md`](./text-edit-shape.contract.md)
  — freezes the FR-006 single-whole-span `TextEdit` convention
  (how the formatter's output is encoded into the response).
- [`../../010-t3-lsp-skeleton/contracts/lsp-protocol.contract.md`](../../010-t3-lsp-skeleton/contracts/lsp-protocol.contract.md)
  §1.2 — amended in this PR to advertise the two new capabilities
  (Principle VII coupling per research.md R6).

---

## §1 Capability advertisement (amended T3 §1.2)

The T3 `lsp-protocol.contract.md` §1.2 canonical
`InitializeResult.capabilities` JSON gains exactly two entries:

```diff
 {
   "capabilities": {
+    "documentFormattingProvider": true,
+    "documentRangeFormattingProvider": true,
     "foldingRangeProvider": true,
     "textDocumentSync": {
       "change": 1,
       "openClose": true,
       "save": false,
       "willSave": false,
       "willSaveWaitUntil": false
     }
   },
   "serverInfo": {
     "name": "nsl-lsp",
     "version": "<NSLC_VERSION_STRING>"
   }
 }
```

After the diff is applied, the new canonical JSON is:

```json
{
  "capabilities": {
    "documentFormattingProvider": true,
    "documentRangeFormattingProvider": true,
    "foldingRangeProvider": true,
    "textDocumentSync": {
      "change": 1,
      "openClose": true,
      "save": false,
      "willSave": false,
      "willSaveWaitUntil": false
    }
  },
  "serverInfo": {
    "name": "nsl-lsp",
    "version": "<NSLC_VERSION_STRING>"
  }
}
```

Both `documentFormattingProvider` and
`documentRangeFormattingProvider` are advertised as the boolean
`true` form (not the `DocumentFormattingOptions` /
`DocumentRangeFormattingOptions` form), matching T3's pattern for
`foldingRangeProvider`. T5 does NOT support `workDoneProgress`
for formatting at the wire level.

The server MUST NOT advertise:

- `documentOnTypeFormattingProvider` (per FR-022).
- `documentRangeFormattingProvider` as a dynamic registration
  (no `client/registerCapability` invocation; the static
  capability is sufficient).

**Test**: `lifecycle_test::CapabilitiesExact` (existing T3 test,
extended) asserts byte-equality between the captured response and
the canonical JSON above (post `llvm::json::OStream`
canonicalization). The test fails if any current or future
T-track milestone adds a capability without updating this
contract in the same PR (SC-009 / Principle VII).

---

## §2 `textDocument/formatting` request

### §2.1 Request shape (accepted)

The server MUST accept any LSP-3.16-conformant
`DocumentFormattingParams`:

```json
{
  "jsonrpc": "2.0",
  "id": <integer or string>,
  "method": "textDocument/formatting",
  "params": {
    "textDocument": { "uri": "file:///..." },
    "options": {
      "tabSize": 4,
      "insertSpaces": true,
      "trimTrailingWhitespace": true,
      "insertFinalNewline": true,
      "trimFinalNewlines": true
    },
    "workDoneToken": "<optional>"
  }
}
```

Fields the server reads:

- `params.textDocument.uri` — REQUIRED. Used to look up the
  `NslTU` in TUScheduler.
- `params.options` — REQUIRED by LSP 3.16 schema but **content
  discarded** by T5 (FR-005 — Session 2026-05-12 Q1). The server
  reads the field's presence and shape (so a malformed
  `options` payload would still be rejected at the protocol
  layer per LSP-base error handling), but every nested key is
  ignored.
- `params.workDoneToken` — IGNORED (no `$/progress` notifications
  emitted at T5).

All other `DocumentFormattingParams` fields are ignored.

### §2.2 Response shape (per outcome)

The server MUST respond on exactly one of these four paths:

#### §2.2.1 Success — whole-document formatting

When `format_buffer` returns `Status::Success` with a non-empty
`formattedText` that differs from the input buffer:

```json
{
  "jsonrpc": "2.0",
  "id": <matching request id>,
  "result": [
    {
      "range": {
        "start": { "line": 0, "character": 0 },
        "end":   { "line": <documentLineCount>, "character": 0 }
      },
      "newText": "<formatted-text-verbatim>"
    }
  ]
}
```

`<documentLineCount>` is the number of newline-terminated lines
in `NslTU.current.contents` at dispatch time (i.e., one more than
the line of the last newline character).

#### §2.2.2 Success — already-canonical input

When `format_buffer` returns `Status::Success` with a
`formattedText` byte-identical to the input buffer:

```json
{
  "jsonrpc": "2.0",
  "id": <matching request id>,
  "result": []
}
```

Empty array — NOT a length-1 edit whose `newText` equals the
input (per FR-006a (ii)).

#### §2.2.3 Refused / Error — no formatting available

When `format_buffer` returns `Status::Refused` (parse error) or
`Status::Error` (range out of bounds, internal failure), OR when
the document URI is unknown to the server:

```json
{
  "jsonrpc": "2.0",
  "id": <matching request id>,
  "result": null
}
```

JSON `null`, NOT a JSON-RPC error response. The
`Refused` vs `Error` distinction is captured in the stderr log
record per FR-015, not on the wire (research.md R4).

#### §2.2.4 Cancelled

When the `$/cancelRequest` notification targets this request's
ID before the response is sent and the handler observes the
cancellation token at a poll point:

```json
{
  "jsonrpc": "2.0",
  "id": <matching request id>,
  "error": {
    "code": -32800,
    "message": "request cancelled"
  }
}
```

Per the LSP base protocol's `RequestCancelled` error code; same
shape T3 uses for `foldingRange` cancellation.

### §2.3 Lifetime and concurrency

- **Document not opened**: if `params.textDocument.uri` does not
  refer to an `NslTU` in TUScheduler, the response is the §2.2.3
  `null`, AND a WARN-level stderr log record is emitted per
  FR-014.
- **`didChange` mid-format**: per FR-014a, the in-flight format
  runs against the captured `NslTU.current` snapshot at
  dispatch time. The response carries the same content the
  caller would have received if `didChange` had not arrived;
  the client rebases per LSP convention.
- **`didClose` mid-format**: per FR-014b, the in-flight format
  completes normally; the response is sent. The client is
  expected to discard responses for closed documents.

---

## §3 `textDocument/rangeFormatting` request

### §3.1 Request shape (accepted)

Same as §2.1 with one additional required field:

```json
{
  "params": {
    "textDocument": { "uri": "file:///..." },
    "range": {
      "start": { "line": <integer>, "character": <integer> },
      "end":   { "line": <integer>, "character": <integer> }
    },
    "options": { ... },
    "workDoneToken": "<optional>"
  }
}
```

Both `params.range.start` and `params.range.end` MUST carry
`line` and `character` as zero-based integers. UTF-16 code-unit
offset semantics per T3 FR-004; T5 discards the column index
(see §3.2 below), so the position-encoding conversion is a no-op
at line granularity.

### §3.2 Range computation (per research.md R5)

The server converts the request's `Range` to a 1-indexed
`nsl::fmt::LineRange` via:

```text
firstLine = range.start.line + 1
if range.end.character == 0:
    lastLine = range.end.line
else:
    lastLine = range.end.line + 1
firstLine = max(1, min(firstLine, documentLineCount))
lastLine  = max(1, min(lastLine,  documentLineCount))
if firstLine > lastLine:
    return §3.3.3 null
```

The `end.character == 0 ⇒ exclusive` convention follows LSP's
position-pair semantics: a `Range` is `[start, end)` in
document-position order.

### §3.3 Response shape (per outcome)

Same four-way split as §2.2, with these range-specific tweaks:

#### §3.3.1 Success — range formatted

```json
{
  "result": [
    {
      "range": {
        "start": { "line": <firstLine - 1>, "character": 0 },
        "end":   { "line": <lastLine>,      "character": 0 }
      },
      "newText": "<formatted-text-for-the-range>"
    }
  ]
}
```

The `newText` is the substring of `format_buffer`'s
`formattedText` that corresponds to the line range
`[firstLine, lastLine]`. Per T2 FR-007, lines outside the range
are emitted byte-identical to the input — meaning
`format_buffer`'s output for those lines equals the input for
those lines, so the LSP response's `range` covering exactly the
edited region produces a buffer byte-identical to what the
whole-document formatter would have emitted for those lines.

#### §3.3.2 Success — already-canonical range

If the range is already canonically formatted (no changes within
`[firstLine, lastLine]`), the response is `[]` per §2.2.2.

#### §3.3.3 Refused / Error / Inverted range / Out-of-bounds — null

Same as §2.2.3 (`result: null`). Inverted range
(`firstLine > lastLine` after clamping) goes here, per FR-003.

#### §3.3.4 Cancelled

Same as §2.2.4 (`error: { code: -32800, ... }`).

---

## §4 Concurrency guarantees

- **Two `textDocument/formatting` requests for the same URI
  arriving back-to-back**: TUScheduler serializes per-URI worker
  frames; the second request sees the post-first-request buffer
  state if no `didChange` intervened. Both responses are sent
  in arrival order.
- **`textDocument/formatting` for URI A and
  `textDocument/rangeFormatting` for URI B in parallel**:
  TUScheduler parallelizes across URIs; both proceed
  concurrently on different workers.
- **`$/cancelRequest` for one of the two in-flight requests**:
  signals exactly that request's cancellation token; the other
  request is unaffected.

---

## §5 Logging contract (per FR-015 / FR-016)

For each format request, the server MUST emit log records per
T3 FR-020d–FR-020f stderr logging:

- On request arrival: one `INFO`-level record with the URI, the
  method name, and the request ID. Format: `[INFO]
  <ISO-8601-timestamp> textDocument/formatting uri=<uri>
  id=<id>`.
- On response emission: one `INFO`-level record with the URI,
  the outcome classification (`Success` / `RefusedParse` /
  `RefusedMalformedTOML` / `ErrorOutOfBounds` /
  `ErrorInvertedRange` / `Cancelled` / `UnknownDocument`), and
  the elapsed time in milliseconds. Format: `[INFO]
  <ts> textDocument/formatting uri=<uri> id=<id> outcome=<...>
  elapsed_ms=<N>`.
- On internal exception: one `ERROR`-level record with the
  exception type and message. Format: `[ERROR] <ts>
  textDocument/formatting uri=<uri> id=<id>
  exception=<type>: <message>`.

The server MUST NOT log the contents of `NslTU.current.contents`
or `FormatResult.formattedText` at any level (FR-016).

---

## §6 Spec cross-reference

| Spec FR / SC | This contract section |
|---|---|
| FR-001 | §2 (whole-document method) |
| FR-002 | §3 (range method) |
| FR-003 | §3.2 (range computation) |
| FR-004 | §1 (capability advertisement) |
| FR-007 | §2.2.3 / §3.3.3 (`null` on refusal) |
| FR-008 | §2.2.2 (empty array on empty document via Success path) |
| FR-012 | §2.2.4 / §3.3.4 (cancellation response) |
| FR-013 | §3.1 (UTF-16 position encoding at the wire) |
| FR-014 | §2.3 (unknown-document handling) |
| FR-014a | §2.3 (`didChange` mid-format) |
| FR-014b | §2.3 (`didClose` mid-format) |
| FR-015 | §5 (logging) |
| FR-016 | §5 (logging exclusion) |
| FR-019 | §1 (capability test) |
| SC-001 | §2.2.1 (whole-document Success path) |
| SC-002 | §3.3.1 (range Success path) |
| SC-006 | §2.2 / §3.3 (deterministic response shape) |
| SC-009 | §1 (capability assertion is byte-exact) |
| SC-010 | §2.2.4 / §3.3.4 (cancellation timing) |
