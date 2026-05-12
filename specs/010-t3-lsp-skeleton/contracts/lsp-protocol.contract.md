<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: LSP Protocol — Lifecycle, Sync, Capabilities, Cancellation, Logging

**Branch**: `010-t3-lsp-skeleton` | **Date**: 2026-05-05
**Anchors**: spec FR-001 through FR-009, FR-020d–j, FR-024–028; SC-001/003/008/009/010

This contract freezes the wire-visible behavior of `nsl-lsp` for
the four LSP methods plus lifecycle / cancellation / logging
machinery. Any later T-track milestone (T4, T5, T9, T10) that
modifies any frozen entry MUST update this contract in the same
PR (Principle VII coupling).

---

## Amendment log

### Amendment 2026-05-12 (T5 — `011-t5-lsp-formatting`)

§1.2 canonical `InitializeResult.capabilities` JSON gains two
new keys per the T5 capability advertisement requirement (T5 spec
FR-004): `documentFormattingProvider: true` and
`documentRangeFormattingProvider: true`. Keys inserted
alphabetically before `foldingRangeProvider`. The MUST-NOT-advertise
list in §1.2 loses two entries (`documentFormattingProvider`,
`documentRangeFormattingProvider`) since both are now advertised;
T5 spec FR-022 keeps `documentOnTypeFormattingProvider` on the
MUST-NOT list.

This amendment is the **first exercise of the T3 coupling clause**
("Any later T-track milestone that modifies any frozen entry MUST
update this contract in the same PR — Principle VII coupling")
introduced when T3 landed. It sets the precedent of in-place
amendment for T4 / T9 / T10 to follow when their respective
methods land. The amendment lands in the same PR as the T5
implementation under `specs/011-t5-lsp-formatting/`.

The `lifecycle_test::CapabilitiesExact` assertion's expected JSON
(in `test/lsp/lifecycle_test.cpp::buildExpectedCapabilities()`)
is updated in the same change to match the new shape.

---

## §1 Initialize handshake

### §1.1 Initialize request — accepted shape

The server MUST accept any LSP-3.16-conformant `InitializeParams`.
It MUST NOT reject the request based on missing optional fields.
Fields the server reads:

- `processId` — logged at `INFO`.
- `clientInfo.name`, `clientInfo.version` — logged at `INFO`.

All other `InitializeParams` fields are ignored at T3 (per FR-025
workspace-method exclusion and FR-004 UTF-16 pin).

### §1.2 Initialize response — exact capabilities

The server's `InitializeResult` MUST be the canonical JSON below
(post-canonicalization with sorted keys, no trailing whitespace,
LF line endings). `<NSLC_VERSION_STRING>` interpolates the
existing `NSLC_VERSION_STRING` macro.

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

`textDocumentSync.change == 1` is `TextDocumentSyncKind.Full` per
LSP 3.16 §General. `foldingRangeProvider == true` /
`documentFormattingProvider == true` /
`documentRangeFormattingProvider == true` are the boolean form
(the `FoldingRangeOptions` / `DocumentFormattingOptions` /
`DocumentRangeFormattingOptions` forms are allowed but not used).

The server MUST NOT advertise: `hoverProvider`, `definitionProvider`,
`semanticTokensProvider`, `signatureHelpProvider`,
`documentSymbolProvider`, `referencesProvider`, `completionProvider`,
`renameProvider`, `codeActionProvider`, `inlayHintProvider`,
`callHierarchyProvider`, `documentOnTypeFormattingProvider`
(per T5 spec FR-022), or any `workspace.*` capability.

The server MUST NOT advertise `general.positionEncodings` (LSP 3.17
feature; out of scope per the LSP 3.16 floor).

**Test**: `lifecycle_test::CapabilitiesExact` asserts byte-equality
between the captured response and the canonical JSON above (post
`llvm::json::OStream` canonicalization).

### §1.3 `initialized` notification

The server MUST accept the `initialized` notification as a no-op
acknowledgment. It MUST NOT process any other request before
`initialized` is received (per LSP spec; the server returns
`ServerNotInitialized` `-32002` for any pre-`initialized` request
other than `initialize` itself, `shutdown`, and `exit`).

---

## §2 Document synchronization

### §2.1 `textDocument/didOpen`

Body: `params.textDocument = {uri, languageId, version, text}`.

On receipt:
1. Register an `NslTU` for `uri` if one does not already exist.
2. Set `NslTU.current.contents = text`, `version = version`.
3. Schedule a parse + sema.
4. On parse + sema completion, emit one `publishDiagnostics`
   notification (§3 below).

The server MUST NOT process two `didOpen` notifications for the
same URI without an intervening `didClose`. If a second `didOpen`
for an already-open URI arrives, the server treats it as a
`didChange` from the new payload and logs a `WARN`.

### §2.2 `textDocument/didChange`

Body: `params.textDocument = {uri, version}`,
`params.contentChanges = [{text}]`.

The server MUST accept a `contentChanges` array of length exactly
1, whose element is `{text: <full new document text>}` (no `range`,
no `rangeLength`). This matches `TextDocumentSyncKind.Full` per §1.2.

On receipt of a `contentChanges` element with a `range` field, the
server MUST log an `ERROR` and ignore the notification (per FR-006:
notifications cannot be replied to anyway).

On a valid `didChange`:
1. Replace `NslTU.current.contents` with `contentChanges[0].text`.
2. Update `NslTU.latest_version` to `params.textDocument.version`.
3. If a worker is currently reparsing `NslTU` at an older version,
   the in-flight reparse continues but its result is discarded
   when complete (per FR-008).
4. Schedule a fresh parse + sema at the new version.
5. On completion, emit one `publishDiagnostics` (§3).

If the new `version` is `≤ NslTU.latest_version`, the server logs a
`WARN` and ignores the notification (LSP-spec recovery for an
out-of-order client).

### §2.3 `textDocument/didClose`

Body: `params.textDocument = {uri}`.

On receipt:
1. Cancel any in-flight reparse for this URI (best-effort).
2. Emit one final `publishDiagnostics` for this URI with an
   empty `diagnostics` array and the URI's last-known version
   (per FR-007 / FR-012; clears the editor's state).
3. Remove the URI's `NslTU` from the scheduler.

### §2.4 Document version tracking (FR-009)

Every `publishDiagnostics` notification MUST include a `version`
field whose value is the LSP document version that produced the
diagnostics. If a stale parse cycle's results are about to be
published but the document has since been updated to a newer
version, the publication is dropped (FR-008).

---

## §3 `publishDiagnostics` shape

```json
{
  "uri": "<document-uri>",
  "version": <int>,
  "diagnostics": [
    {
      "range": { "start": {"line": <int>, "character": <int>}, "end": {…} },
      "severity": <1|2|3|4>,
      "code": "<S01|S02|…|N05|…|P03|…>",
      "source": "<nsl-sema|nsl-parse|nsl-preprocess>",
      "message": "<formatted message>",
      "relatedInformation": [
        {
          "location": { "uri": "<related-uri>", "range": {…} },
          "message": "<note message>"
        },
        …
      ]
    },
    …
  ]
}
```

- `range.{start,end}.line` and `.character` are zero-based LSP
  positions (UTF-16 code-unit offsets per FR-013 / R6).
- `severity` mapping: `Error → 1`, `Warning → 2`, `Note → 3`
  (Information). The Severity-3-as-Note carve-out exists because
  LSP has no separate "note" severity; LSP `Information` is the
  closest match and matches clangd's convention.
- `code` is a stable string ID per
  [`diagnostic-mapping.contract.md`](./diagnostic-mapping.contract.md)
  §1; it is NOT free-form.
- `source` is one of three string literals, set by the diagnostic's
  origin (sema / parse / preprocess).
- `message` is the formatted diagnostic string from
  `nsl::Diagnostic.message` (no LSP-specific reformatting).
- `relatedInformation` carries the `is_include_from_note` notes
  (Principle IV `#line` round-trip).

Diagnostics MUST be sorted by `(range.start.line, range.start.character, severity)`
ascending. Determinism (Principle V / SC-003) requires this sort.

---

## §4 `textDocument/foldingRange`

Request body: `{textDocument: {uri}}`.
Response: `FoldingRange[]` per
[`folding-range.contract.md`](./folding-range.contract.md).

The handler is **cancellable** per §6 below.

---

## §5 Lifecycle: `shutdown`, `exit`

### §5.1 `shutdown`

Request, no params. The server:
1. Marks itself shutting-down.
2. Drains pending parse + sema work.
3. For every still-open document, emits one final empty
   `publishDiagnostics`.
4. Sends a `null` response per LSP spec.

After `shutdown`, the server MUST reject every subsequent request
with `InvalidRequest` (`-32600`) until `exit` is received.

### §5.2 `exit`

Notification, no params.
- If `shutdown` was previously received: terminate with exit code 0.
- If `exit` arrives without prior `shutdown`: terminate with exit
  code 1.

The exit MUST NOT depend on any in-flight worker; outstanding
reparse cycles are abandoned.

---

## §6 `$/cancelRequest`

Notification body: `{id: <request-id>}`.

### §6.1 Server-side semantics

1. Look up the in-flight-request entry for `id`.
2. If present: flip the entry's `CancellationToken.flag`. The
   handler will observe the token at its next polling point and
   abort.
3. If the request has already responded, has never been seen, or
   is a notification (no `id`): silently ignore (per FR-020j).

### §6.2 Cancelled-request response shape

```json
{
  "jsonrpc": "2.0",
  "id": <id>,
  "error": {
    "code": -32800,
    "message": "request cancelled"
  }
}
```

`-32800` is the LSP-spec `RequestCancelled` code. The `message`
field is the literal string `"request cancelled"`.

### §6.3 Cancellable T3 requests

| Method                          | Cancellable? |
| ------------------------------- | ------------ |
| `initialize`                    | No (one-shot) |
| `shutdown`                      | No |
| `textDocument/foldingRange`     | **Yes** (FR-020h–j; SC-010) |

Notifications (`didOpen`, `didChange`, `didClose`,
`publishDiagnostics`, `$/cancelRequest`, `initialized`, `exit`) are
not cancellable by definition (no response to send).

### §6.4 Polling discipline

`FoldingRangeBuilder` polls the cancellation token at minimum once
per visited block-opener AST node (FR-020i). On token-set, the
handler aborts and the protocol layer sends the §6.2 response
within an upper bound of 200 ms (SC-010 budget).

---

## §7 Logging contract

### §7.1 Output destination

`stderr`, plain text, one record per line, line-buffered, no
embedded newlines in any record.

### §7.2 Record format

```
<ISO-8601-timestamp> <LEVEL> <message>
```

- Timestamp: UTC, whole-second precision, ISO-8601
  (`YYYY-MM-DDThh:mm:ssZ`).
- Level: one of the literals `ERROR`, `WARN`, `INFO`, `DEBUG`.
- Message: free-text; no embedded `\n`. If the runtime constructs a
  message containing `\n`, the logger MUST replace each `\n` with
  the literal two-character sequence `\n` (escape it).

### §7.3 Level threshold

`NSL_LSP_LOG_LEVEL` env var, read once at server startup (before
the LSP handshake). Permitted values (case-insensitive):
`error` / `warn` / `info` / `debug`. Default: `warn`.

Records below the threshold are dropped (not buffered).

### §7.4 Required content (FR-020f)

| Event                                    | Level   |
| ---------------------------------------- | ------- |
| `initialize` request received            | `INFO`  |
| `initialized` notification received      | `INFO`  |
| `shutdown` request received              | `INFO`  |
| `exit` notification received             | `INFO`  |
| Protocol framing error (bad Content-Length, malformed JSON, etc.) | `ERROR` |
| Internal exception / unrecoverable error | `ERROR` |
| TUScheduler parse-cycle failure          | `WARN`  |
| `didChange` with incremental payload (rejected) | `ERROR` |
| `didChange` with stale version (ignored) | `WARN`  |
| Worker pool started, n workers           | `DEBUG` |

### §7.5 Forbidden content (FR-020f)

The body of `textDocument/didOpen` or `didChange` payloads
(i.e., the source-code text) MUST NOT appear in any log record at
any level — not even at `DEBUG`. Document URIs MAY appear; document
versions MAY appear.

### §7.6 No `window/logMessage` at T3

Per FR-020g, the server MUST NOT emit `window/logMessage` LSP
notifications at T3. T11 will land that channel as part of editor
packaging.

---

## §8 `NSL_INCLUDE` env-var contract (FR-020a)

### §8.1 Read timing

Read **once**, before the LSP handshake begins. Subsequent changes
to the env var have no effect on the running server.

### §8.2 Format

POSIX: colon-separated list of directories.
Windows: semicolon-separated list of directories.

### §8.3 Empty / unset

If unset or empty, the angle-form `#include` search path is empty.
Quote-form `#include "…"` resolution still works (it uses the
document's parent directory regardless of `NSL_INCLUDE`).

### §8.4 Logging

The resolved `IncludeSearchPath` MUST be logged at `INFO` level on
server startup, after `NSL_INCLUDE` is parsed but before the
`initialize` handler runs.

---

## §9 Process exit codes

| Scenario                                         | Exit code |
| ------------------------------------------------ | --------- |
| `shutdown` then `exit` (clean)                   | 0         |
| `exit` without prior `shutdown`                  | 1         |
| Unrecoverable internal error (uncaught exception, OOM, etc.) | non-zero (typically 1) |
| Invalid `NSL_LSP_LOG_LEVEL` value at startup     | non-zero (typically 1; before the LSP handshake) |
| `NSL_LSP_WORKERS` value outside `[1, 64]`        | non-zero |
| stdin EOF before `shutdown`                      | 1 (treat as abnormal client termination; log `WARN`) |

---

## §10 Forward-compatibility commitments

When T4 / T5 / T9 / T10 land:

- Adding capabilities to the §1.2 InitializeResult MUST update the
  canonical JSON in this contract in the same PR. The
  `lifecycle_test::CapabilitiesExact` test will fail otherwise
  (Principle VII coupling enforced mechanically).
- Adding new request methods MUST register their handlers in
  `NslLSPServer::dispatch()` and add per-method tests under
  `test/lsp/`. The protocol layer's dispatch table is the
  extension point; lifecycle / sync / capability advertisement
  MUST NOT need modification.
- Adding new notification methods follows the same pattern.
- Raising the LSP protocol floor (e.g., to 3.17 to introduce
  `positionEncodings` advertisement) MUST update §1, §3, and the
  Clarifications session record in `spec.md` in the same PR.
