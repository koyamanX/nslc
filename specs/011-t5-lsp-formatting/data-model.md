<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Phase 1 Data Model: T5 — LSP Formatting Integration

**Branch**: `011-t5-lsp-formatting` | **Date**: 2026-05-12

This document catalogs the entities T5 introduces inside
`lib/LSP/Features/Formatting.cpp` (+ `.h`) and how they relate to
existing T2 and T3 types. Compared to the spec's "Key Entities"
section (which is stakeholder-facing), this document is
implementation-facing and records the C++17 shape, the existing
library types each entity wraps or consumes, and the lifetime /
ownership relationships.

---

## §1 LSP wire-protocol entities (consumed)

These shapes flow across the JSON-RPC channel. T5 reads them from
the protocol layer and emits responses; the shapes themselves are
defined by LSP 3.16 and are consumed unchanged.

### §1.1 `DocumentFormattingParams` (client → server, request)

| Field                                | Type                         | T5 use |
| ------------------------------------ | ---------------------------- | ------ |
| `textDocument.uri`                   | `string` (DocumentUri)       | Looks up the `NslTU` in TUScheduler; if not found, response is `null` + WARN log (FR-014). |
| `options`                            | `FormattingOptions`          | Read off the wire (LSP 3.16 requires the field) and **discarded**; content does NOT influence `Configuration` per FR-005 (Session 2026-05-12 Q1). |
| `options.tabSize`                    | `uinteger`                   | Ignored. |
| `options.insertSpaces`               | `boolean`                    | Ignored. |
| `options.trimTrailingWhitespace`     | `boolean?` (LSP 3.15+)       | Ignored. T2 formatter handles trailing whitespace per its rules; client preference does not override. |
| `options.insertFinalNewline`         | `boolean?` (LSP 3.15+)       | Ignored. T2 always emits exactly one trailing `\n` on non-empty output (R7 — T2 Session 2026-05-05). |
| `options.trimFinalNewlines`          | `boolean?` (LSP 3.15+)       | Ignored. Same reason. |
| `workDoneToken`                      | `ProgressToken?` (LSP 3.15+) | Ignored at T5; progress reporting is not implemented (no `$/progress` notifications emitted). |

### §1.2 `DocumentRangeFormattingParams` (client → server, request)

Same as §1.1 with one additional field:

| Field    | Type    | T5 use |
| -------- | ------- | ------ |
| `range`  | `Range` | Converted to a 1-indexed `nsl::fmt::LineRange` via the procedure in research.md R5 (snap to whole-line + clamp + reject inverted). |

### §1.3 `TextEdit[]` (server → client, response payload)

The response to a successful format request. Each `TextEdit`:

| Field      | Type      | T5 emission shape |
| ---------- | --------- | ----------------- |
| `range`    | `Range`   | For `formatting`: `{start: (0, 0), end: (documentLineCount, 0)}`. For `rangeFormatting`: `{start: (firstLine - 1, 0), end: (lastLine, 0)}` (zero-based, end-exclusive at column 0). |
| `newText`  | `string`  | The `nsl::fmt::FormatResult::formattedText` verbatim. |

Per FR-006 (Session 2026-05-12 Q2), the array length is **exactly
1** on changed input and **0** on already-canonical input. Never
> 1 at T5; minimal-diff decomposition is deferred.

### §1.4 `PublishDiagnosticsParams` (server → client, notification — side channel)

Emitted by the Configuration resolver on FR-005c (malformed
TOML). Reuses the T3 diagnostic-mapping seam.

| Field           | Type                | T5 emission shape |
| --------------- | ------------------- | ----------------- |
| `uri`           | `DocumentUri`       | The TOML file's `file://` URI — **NOT** the .nsl document's URI. |
| `version`       | `integer?`          | Omitted (T5 does not track TOML versions; the TOML is read from disk fresh per request). |
| `diagnostics`   | `Diagnostic[]`      | Each entry maps a `basic::Diagnostic` from `FormatResult.diagnostics` via the existing T3 `toLspDiagnostic(...)` helper, with `source = "nsl-fmt"`. |

---

## §2 T5-internal entities (defined by `lib/LSP/Features/Formatting.cpp/.h`)

### §2.1 `ResolvedConfiguration` (file-static)

A wrapper around `nsl::fmt::Configuration` that carries enough
context to emit the side-channel TOML diagnostic if needed.

```cpp
namespace nsl::lsp::detail {

struct ResolvedConfiguration {
    nsl::fmt::Configuration                config;
    std::optional<std::string>             tomlPath;       // file:// path, if discovered
    std::vector<basic::Diagnostic>         tomlDiagnostics; // empty on Success; populated on Refused/Error
    bool                                   tomlFallback;    // true iff malformed TOML triggered default_configuration() path
};

// Free function (file-static in Formatting.cpp):
ResolvedConfiguration resolveConfiguration(StringRef documentURI,
                                            basic::SourceManager &sm);

} // namespace nsl::lsp::detail
```

**Lifetime**: stack-allocated per format request. Discarded after
the request response is sent.

**Construction**: `resolveConfiguration` runs the R2 procedure
(discover → read+parse → fallback-on-error). On
`tomlFallback == true`, the caller is responsible for emitting
the `PublishDiagnosticsParams` notification before sending the
format response (so the editor's problems panel updates before
the user sees their formatted output).

### §2.2 `FormatRequestContext` (file-static)

A bundle of the per-request state captured at dispatch time —
the shape T3's TUScheduler returns when the handler calls
`withCurrentState(...)`.

```cpp
namespace nsl::lsp::detail {

struct FormatRequestContext {
    StringRef                              documentURI;
    int                                    documentVersion;   // NslTU.current.version at dispatch
    StringRef                              documentContents;  // NslTU.current.contents at dispatch
    std::optional<nsl::fmt::LineRange>     lineRange;         // nullopt for textDocument/formatting
    CancellationToken                     *cancelToken;       // T3-provided
};

} // namespace nsl::lsp::detail
```

**Lifetime**: lives inside the TUScheduler worker frame; freed
when the handler returns.

### §2.3 `FormattingResponse` (file-static enum)

Discriminated outcome of a format request, before JSON encoding.

```cpp
namespace nsl::lsp::detail {

enum class FormattingResponseKind {
    SingleEdit,        // FR-006: one TextEdit covering the whole span
    Empty,             // FR-006a (ii): already-canonical input
    Null,              // FR-007: refusal or error
    Cancelled,         // FR-012: $/cancelRequest fired
};

struct FormattingResponse {
    FormattingResponseKind  kind;
    TextEditPayload         singleEdit; // valid iff kind == SingleEdit
};

struct TextEditPayload {
    Position    rangeStart;
    Position    rangeEnd;
    std::string newText;
};

} // namespace nsl::lsp::detail
```

**Encoding**: a free function `encodeResponse(FormattingResponse)`
produces an `llvm::json::Value` for the protocol layer to write.
`SingleEdit` → `TextEdit[]` of length 1; `Empty` → `TextEdit[]`
of length 0; `Null` → `llvm::json::Value(nullptr)`; `Cancelled`
→ JSON-RPC error response with code `-32800` (RequestCancelled)
constructed by the protocol layer, not by this encoder.

---

## §3 Existing T2 entities (consumed unchanged)

These are reused from T2's `libNslFmt.a` via `include/nsl/Fmt/Fmt.h`.

| Symbol | T5 consumption |
| ------ | -------------- |
| `nsl::fmt::Configuration` (struct, 3 enums, 10 fields) | The `config` field of `ResolvedConfiguration`. |
| `nsl::fmt::LineRange` (struct, 1-indexed) | The `lineRange` field of `FormatRequestContext`; computed per R5. |
| `nsl::fmt::FormatResult` (struct: `Status`, `formattedText`, `diagnostics`) | The return value of `format_buffer`; case-split per R3/R4 to produce `FormattingResponse`. |
| `nsl::fmt::format_buffer(...)` | Called once per format request. |
| `nsl::fmt::parse_config_file(...)` | Called once per format request (when TOML is discovered). |
| `nsl::fmt::discover_config(...)` | Called once per format request from the document URI's parent dir. |
| `nsl::fmt::default_configuration()` | Called on no-TOML or malformed-TOML paths. |
| `nsl::fmt::emit_unified_diff(...)` | **Not consumed at T5.** The TextEdit converter does not need diff output (whole-span shape per R3). |
| `nsl::fmt::config_key_names()` | **Not consumed at T5.** No "did you mean" suggestion at T5. |
| `nsl::fmt::version_string()` | **Not consumed at T5.** T3 already advertises `serverInfo.version`. |

---

## §4 Existing T3 entities (consumed unchanged)

These are reused from T3's `libNSLLSP.a`. T5 does NOT modify any.

| Entity | T5 consumption |
| ------ | -------------- |
| `nsl::lsp::JSONTransport` | Untouched. Adds the new methods to the dispatch table only. |
| `nsl::lsp::NslLSPServer` | One dispatch-table entry per new method; one capability JSON key per new method in `onInitialize`. No other edits per FR-010. |
| `nsl::lsp::NslServer` | The "format-region seam" body (T3 FR-019) is implemented here at T5. If T3 left it as a stub, that stub is replaced; if T3 left it absent, it is added (one method on `NslServer`). |
| `nsl::lsp::TUScheduler` | `withCurrentState(...)` (or equivalent T3 accessor) is called from the format handler to read `NslTU.current.contents` + `current.version`. No new accessor added. |
| `nsl::lsp::NslTU` | Read-only access to `current.contents` + `current.version`. No new fields. |
| `nsl::lsp::CancellationToken` | The format handler polls the token at the FR-012 boundary points. T3-provided plumbing. |
| `nsl::lsp::toLspDiagnostic(const basic::Diagnostic&, …)` | The TOML-diagnostic mapping in FR-005c reuses this helper. |
| `nsl::lsp::LspSession` (test harness) | The three new test binaries (`formatting_test`, `range_formatting_test`, `format_cancellation_test`) construct `LspSession` instances exactly as T3's `lifecycle_test`/`diagnostics_test`/etc. do. |

---

## §5 Existing project-wide entities (consumed unchanged)

| Entity | T5 consumption |
| ------ | -------------- |
| `nsl::basic::Diagnostic` | Carried by `FormatResult.diagnostics` and re-mapped to LSP `Diagnostic` for the TOML side-channel emission. |
| `nsl::basic::SourceManager` | Used to construct `basic::FileID` values for `parse_config_file` and `format_buffer` calls (FR-014's "use `NslTU.current.contents` as input" requires a `FileID` for diagnostic source-range mapping). |
| `nsl::basic::FileID` | Passed to `format_buffer` and `parse_config_file`. |
| `llvm::json::Value` / `Object` / `Array` | All wire-level JSON encoding/decoding. Already pulled in via T3. |
| `llvm::StringRef` | Used pervasively. |

---

## §6 Entity ownership and lifetime diagram

```text
Per-request flow (synchronous within TUScheduler worker frame):

  request                                ┌─ on_didChange would update,
   │                                     │  but format runs against
   ▼                                     │  captured snapshot below
 ┌──────────────────────────────┐        │
 │ NslLSPServer dispatch table  │        │
 │  (T3, +2 entries from T5)    │        │
 └────────────┬─────────────────┘        │
              │                          │
              ▼                          │
 ┌──────────────────────────────┐        │
 │ NslServer::formatRegion(…)   │        │
 │  (T3 seam body, filled at T5)│        │
 └────────────┬─────────────────┘        │
              │                          │
              │ withCurrentState() ─────►│  reads NslTU.current.{contents, version}
              │                          │
              ▼                          │
 ┌──────────────────────────────┐        │
 │ FormatRequestContext         │  ◄─────┘  captured snapshot
 │  (file-static struct)        │
 └────────────┬─────────────────┘
              │
              ▼
 ┌──────────────────────────────┐
 │ resolveConfiguration(uri,sm) │
 │  → ResolvedConfiguration     │
 │     ├─ config                │
 │     ├─ tomlPath?             │
 │     ├─ tomlDiagnostics       │
 │     └─ tomlFallback          │
 └────────────┬─────────────────┘
              │
              │ if tomlFallback:
              │   emit publishDiagnostics(tomlPath URI, tomlDiagnostics)
              │     via existing T3 diagnostic-mapping seam
              │
              ▼
 ┌──────────────────────────────┐
 │ nsl::fmt::format_buffer(     │
 │   contents, config,          │
 │   fileID, lineRange?)        │
 │   → FormatResult             │
 └────────────┬─────────────────┘
              │
              │  cancellation token polled
              │  before/after this call
              │
              ▼
 ┌──────────────────────────────┐
 │ FormattingResponse           │
 │  kind ∈ {SingleEdit, Empty,  │
 │           Null, Cancelled}   │
 └────────────┬─────────────────┘
              │
              ▼
 ┌──────────────────────────────┐
 │ encodeResponse(response)     │
 │  → llvm::json::Value         │
 └────────────┬─────────────────┘
              │
              ▼
        response sent (JSON-RPC)
```

All T5-internal entities are stack-allocated within the worker
frame; nothing in T5 owns heap memory beyond the
`std::string newText` inside `TextEditPayload` (one allocation
per request). The TOML-side-channel `publishDiagnostics`
emission borrows the diagnostics from `tomlDiagnostics` for the
duration of the JSON encoding, then they are dropped with the
`ResolvedConfiguration`.

---

## §7 No new persistent state

T5 introduces zero new persistent state:

- No new fields on `NslTU`.
- No new caches in `TUScheduler`.
- No new server-global maps in `NslLSPServer`.
- No new threading primitives — reuses T3's worker pool.

This means TUScheduler's existing memory accounting and shutdown
sequence (per T3 `lsp-protocol.contract.md` §2.3 `didClose` and
`shutdown` handlers) continue to apply unchanged. FR-014b's
"in-flight format completes through `didClose`" is naturally
satisfied by the existing T3 reparse-completion semantics — the
worker frame finishes, the response is sent, the worker resources
are released; no T5-specific bookkeeping is needed.
