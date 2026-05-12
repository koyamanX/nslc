<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Feature Specification: T3 — `nsl-lsp` Skeleton (Lifecycle, Document Sync, Diagnostics, Folding)

**Feature Branch**: `010-t3-lsp-skeleton`
**Created**: 2026-05-05
**Status**: Draft
**Input**: User description: "T3"

> **Roadmap anchor.** Tooling-track milestone **T3**, per
> [`README.md`](../../README.md) §Roadmap row T3 and
> [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
> §3 (Language Server). T3 is the first milestone of the LSP track:
> it lands the protocol skeleton plus the four "trivial" LSP methods
> (`publishDiagnostics`, `textDocument/foldingRange`) along with the
> document-sync lifecycle (`initialize`, `textDocument/didOpen`,
> `textDocument/didChange`, `textDocument/didClose`) that every later
> LSP feature layers on top of.
>
> **Compiler-track dependency.** T3 gates on **M3** (`nsl-sema`) per
> the same row of the Roadmap: the test gate is "open a file with a
> Sema error, observe diagnostic; edit, observe re-diagnose," which
> requires the M3 SymbolTable + TypeSystem + per-`Sn` constraint
> checks to be in place. M3 has been delivered (see
> [`CLAUDE.md`](../../CLAUDE.md) §1 Status as of 2026-04-30); this
> spec consumes that work via `libNSLFrontend.a` per Constitution
> Principle II's no-duplication rule.
>
> **Tooling-track unlock.** Every later LSP-feature milestone (T4,
> T5, T9, T10) gates on T3. The shared infrastructure introduced
> here — JSON-RPC transport, document-sync, the TUScheduler caching
> AST per file version, the diagnostic-mapping seam — is what those
> later milestones extend. T3 is therefore the architectural seam,
> not just a thin slice.
>
> **Out-of-scope feature deferral.** All other LSP methods listed in
> [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
> §3.2 are deferred to their respective milestones: `hover`,
> `definition`, `documentSymbol`, `semanticTokens`, `signatureHelp`
> → T4; `formatting`, `rangeFormatting` → T5; `references`,
> `completion`, `rename`, `codeAction` → T9; `inlayHint`,
> `prepareCallHierarchy` → T10. T3 stops at the four trivial methods
> plus lifecycle.

---

## Clarifications

### Session 2026-05-05

- Q: Does T3's server advertise `Full` text sync or `Incremental` text sync on the wire? → A: `Full` (`TextDocumentSyncKind.Full`); client sends entire new document text on every `didChange`.
- Q: How does `nsl-lsp` discover `#include` search paths at T3 (driver uses `-I <dir>` and `NSL_INCLUDE`; `workspace/*` methods are out of scope per FR-025)? → A: `NSL_INCLUDE` environment variable, read once at server startup; identical semantic to the CLI driver. Workspace-folder propagation deferred to T9.
- Q: What minimum LSP protocol version does `nsl-lsp` target? → A: **LSP 3.16** (December 2020). Implies UTF-16-only position encoding (no `positionEncodings` capability negotiation, which was introduced in 3.17); `PublishDiagnosticsParams.version` is available and used per FR-009. T4+ may raise the floor when a 3.17+ feature becomes load-bearing.
- Q: What logging discipline does T3 implement? → A: **stderr-only, plain text, with a single `NSL_LSP_LOG_LEVEL` env-var knob** (`error`/`warn`/`info`/`debug`; default `warn`). No LSP protocol surface (no `window/logMessage` at T3 — it lands when an editor packaging milestone needs it).
- Q: Does `nsl-lsp` implement `$/cancelRequest` handling at T3? → A: **Real cancellation**. T3 wires up `$/cancelRequest` to abort in-flight `foldingRange` work; the cancellation token threads from the protocol layer through the language-logic layer to the AST walk. Cancelled requests respond with the LSP `RequestCancelled` (`-32800`) error code per the LSP spec.

---

## User Scenarios & Testing *(mandatory)*

### User Story 1 — NSL author sees Sema diagnostics live in the editor (Priority: P1)

A hardware engineer authoring an NSL module in an LSP-capable editor
opens a `.nsl` file that contains a semantic error from the M3
constraint set (`S1`–`S29`) — for example, an `__` in an identifier
(`S1`), a `wire` declared with an initializer (`S2`), or a `seq`
block outside a `func`/`proc` body (`S7`). Within sub-second time
of opening the file, every Sema error is rendered as a diagnostic
(red squiggle in VS Code; `:lopen` quickfix list in Neovim; etc.)
at the originating NSL `SourceRange`. The editor reaches the
diagnostic without invoking the `nslc` driver — the LSP server
ran sema inside its own process via `libNSLFrontend.a`.

**Why this priority**: This is the entire user-visible value of T3
and is the test gate stated verbatim in
[`README.md`](../../README.md) §Roadmap row T3. Without it, T3 is
not done.

**Independent Test**: Launch `nsl-lsp` (stdin/stdout JSON-RPC),
send a hand-rolled `initialize` request followed by a
`textDocument/didOpen` notification carrying a fixture file
containing one or more Sema errors of known shape; observe a
`textDocument/publishDiagnostics` notification arriving on the
output stream within a bounded time, with `Diagnostic[]` content
that matches the M3 `DiagnosticEngine` output for the same input —
same severity, same code, same `range` (line / character —
zero-based — under whichever encoding the client negotiated:
`utf-8` / `utf-16` / `utf-32` per LSP `PositionEncodingKind`),
same message.

**Acceptance Scenarios**:

1. **Given** an NSL fixture containing a single `S1` violation
   (`reg foo__bar;`), **When** the client sends `initialize` and
   then `textDocument/didOpen` for that file, **Then** the server
   emits exactly one `publishDiagnostics` notification whose
   `diagnostics` array contains exactly one entry with severity
   `Error`, the expected `S1` diagnostic message text, and a
   `range` covering the offending identifier.
2. **Given** an NSL fixture containing two distinct Sema errors
   (`S1` plus `S2`), **When** the client opens it, **Then** the
   server emits one `publishDiagnostics` containing both entries,
   ordered by source location (line, then column), each with the
   correct code, severity, message, and range.
3. **Given** an NSL fixture with zero errors, **When** the client
   opens it, **Then** the server emits one `publishDiagnostics`
   with an empty `diagnostics` array (per the LSP spec: an empty
   array clears any previous diagnostics for that document).
4. **Given** an NSL fixture whose error originates in a region
   pulled in by `#include`, **When** the client opens it, **Then**
   the published `Diagnostic.range` and `Diagnostic.relatedInformation`
   identify the original physical file and offset that the M3
   `DiagnosticEngine` already plumbs through (`#line` round-trip
   per Principle IV).

---

### User Story 2 — NSL author edits and watches diagnostics re-emit live (Priority: P1)

Continuing from User Story 1: the author types into the open buffer
to fix the error. With every keystroke, the editor sends a
`textDocument/didChange` notification carrying the new content (or
the per-character incremental edit). The LSP server reparses the
file and emits a fresh `publishDiagnostics` whose contents reflect
the new state — when the error is resolved, the diagnostics array
becomes empty; when a different error appears mid-edit, that
diagnostic appears at the new location.

**Why this priority**: The test gate explicitly requires "edit,
observe re-diagnose" as the second half of the integration test.
This is what distinguishes a real LSP from a one-shot lint. Without
it, T3 fails the gate.

**Independent Test**: After User Story 1's `didOpen`, send a
sequence of `didChange` notifications transitioning the buffer
from "contains error E1" → "fixed, no errors" → "contains
different error E2" → "back to clean." Observe one
`publishDiagnostics` per state change with the correct contents.

**Acceptance Scenarios**:

1. **Given** an open document whose initial state contains one Sema
   error, **When** the client sends a `didChange` whose post-edit
   text resolves the error, **Then** the server emits a fresh
   `publishDiagnostics` with an empty `diagnostics` array.
2. **Given** an open document whose initial state is clean, **When**
   the client sends a `didChange` introducing a new Sema error,
   **Then** the server emits a `publishDiagnostics` containing
   exactly that error.
3. **Given** an open document and a rapid burst of `didChange`
   notifications (faster than the parse/sema turnaround time),
   **When** the burst settles, **Then** the most recent
   `publishDiagnostics` reflects the final document state — no
   stale diagnostics from intermediate states are left visible.
4. **Given** an open document, **When** the client sends a
   `textDocument/didClose`, **Then** the server emits a final
   `publishDiagnostics` with an empty `diagnostics` array (clearing
   the editor's state) and releases the document's translation-unit
   resources.

---

### User Story 3 — NSL author folds blocks to navigate large modules (Priority: P2)

The same author, having fixed the errors, now navigates a long
NSL file — a multi-hundred-line `module` containing several `declare`
blocks, multiple `func`/`proc`/`state` definitions, and nested
`seq`/`alt`/`any` blocks. The editor sends
`textDocument/foldingRange`; the LSP server returns one
`FoldingRange` per syntactic block opener (every `{ … }` pair that
spans more than one line), enabling the editor to collapse blocks
on demand.

**Why this priority**: Folding ranges are listed as "Trivial" in
[`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
§3.2 and pair naturally with the lifecycle methods at T3 — the
implementation is a tree walk over the M2 AST that's already
produced by the parse pass. Excluding folding from T3 would leave
the trivial-tier methods incomplete for one milestone-cycle. P2,
not P1, because diagnostics are what the milestone is named for.

**Independent Test**: Open a fixture file containing a `module`
with one `declare` block, one multi-line `func` definition, and
one nested `seq` block; send `textDocument/foldingRange`; verify
the response contains exactly four `FoldingRange` entries with
correctly identified `startLine`/`endLine` boundaries and `kind`
omitted (the LSP spec leaves `kind` optional and clients fall back
to the generic fold).

**Acceptance Scenarios**:

1. **Given** a fixture NSL file with a `module foo { ... }` block
   spanning lines 1–20, a `declare { ... }` block spanning lines
   2–8, a `func clk { ... }` spanning lines 10–18, and a nested
   `seq { ... }` spanning lines 13–17, **When** the client sends
   `textDocument/foldingRange`, **Then** the response contains
   exactly four `FoldingRange` entries with `startLine`/`endLine`
   matching those boundaries.
2. **Given** an NSL fixture containing a multi-line block comment
   (`/* … */` spanning ≥ 2 lines), **When** the client sends
   `textDocument/foldingRange`, **Then** the response includes a
   `FoldingRange` for the comment with `kind = "comment"`.
3. **Given** an NSL file containing only single-line blocks, **When**
   the client sends `textDocument/foldingRange`, **Then** the
   response is an empty array (single-line blocks are not foldable).
4. **Given** an NSL file with a parse error such that a block
   closer is missing, **When** the client sends
   `textDocument/foldingRange`, **Then** the server returns the
   folding ranges that the M2 parser's error-recovery managed to
   recognize and does **not** crash or return an LSP error response.

---

### User Story 4 — LSP server is the architectural seam every later T-track milestone reuses (Priority: P2)

A future tooling-track contributor lands T4, T5, T9, T10, or T11 by
adding a new method handler to `nsl-lsp`. They do not touch the
JSON-RPC transport, the lifecycle handling, the document store, the
TUScheduler, or the diagnostic-mapping plumbing. They register a
handler for the new LSP method, implement the method on the
`NslServer` language-logic layer, and the test fixture for their
milestone exercises only the new behaviour.

**Why this priority**: Constitution Principle II (single front-end
library; no duplicated lex/parse/sema) and the Roadmap structure
(T4 / T5 / T9 / T10 all gate on T3) mean T3 must deliver the
extension points cleanly, not just the four methods. If T4 has to
reshape the server layout, that's a T3 design defect. P2 because
it's verifiable structurally, but its test gate (FR-018 and SC-005
below) lives mostly inside the M2/M3 contracts, not the LSP
protocol.

**Independent Test**: A code-structure inspection: the LSP-method
dispatch table is a registry that adding a new method does not
require modifying core lifecycle code; the `NslServer` API exposes
the language operations (Sema diagnostics, AST access, symbol
table access) via narrow seams that future T4 / T5 / T9 / T10 work
can call without re-parsing. Concretely: a smoke build + a unit
test that registers a stub handler for an unrelated LSP method
must succeed without changing the lifecycle code, the transport,
or the TUScheduler. This is structural, not a runtime acceptance
test.

**Acceptance Scenarios**:

1. **Given** the T3 source layout, **When** a contributor adds a
   new method handler (e.g. for the future T4 `textDocument/hover`),
   **Then** the addition is confined to one new source file plus
   one registry entry; no edits to the JSON-RPC transport,
   document-sync handlers, or `NslServer` core API are required.
2. **Given** the T3 source layout, **When** a contributor inspects
   `NslServer`, **Then** the language-logic API exposes the
   `diagnostics(uri)` (T3-needed), AST-access seam (T4-needed), and
   format-region seam (T5-needed) such that none of those is
   reachable only via a private helper of another method.
3. **Given** T3 lands, **When** a contributor builds the
   `nsl-frontend` library tree, **Then** `nsl-lsp` links a single
   instance of `libNSLFrontend.a` and reuses
   `Lexer`/`Preprocessor`/`Parser`/`Sema`/`SymbolTable`/`TypeSystem`
   without re-implementing any of them — Constitution Principle II
   verified by linker map.

---

### Edge Cases

- **Initialize before any `didOpen`**: server responds correctly to
  an `initialize` request received as the first message and must
  not require a document to exist.
- **`didChange` for a document the server has not seen via
  `didOpen`**: per LSP, this is a client error; the server logs
  and ignores the notification (no crash, no error response —
  notifications cannot be replied to anyway).
- **`shutdown` followed by `exit`**: server tears down all
  translation units, flushes pending diagnostics (clear-arrays per
  US2 acceptance 4), and exits with code 0; an `exit` without prior
  `shutdown` exits with code 1 per the LSP spec.
- **Empty document opened**: zero diagnostics, zero folding ranges,
  no crash.
- **Document with parse error (not Sema)**: parser-level errors
  flow through the same `DiagnosticEngine` and surface as
  `publishDiagnostics`. Folding ranges return whatever the
  recovery-mode parser managed to recognise (per US3 acceptance 4).
- **Document with `#line` directive**: diagnostic ranges and
  folding ranges resolve to the user-visible (logical) source per
  N14 / P13 / Principle IV — the server does not leak post-include
  physical offsets to the client. (LSP `Position`s are zero-based;
  M3's `DiagnosticEngine` uses one-based — the seam translates.)
- **Rapid edits while reparse is in flight**: the in-flight reparse
  is allowed to complete or is cancelled; either way, the latest
  `didChange` is queued and reparsed once a worker is free, and
  only the diagnostics from the latest version are emitted (per
  US2 acceptance 3).
- **Document closed while reparse is in flight**: the worker
  finishes or is cancelled cleanly; no `publishDiagnostics` is
  emitted for a closed document.
- **`didChange` carrying incremental edits (range-based) vs full
  text**: the server advertises support for whichever encoding it
  implements via the `initialize` response's
  `textDocumentSync.change` capability; the client uses the
  advertised mode.
- **UTF-16 position encoding (always at T3)**: per FR-004 and the
  LSP 3.16 floor, the server uses UTF-16 unconditionally — no
  `positionEncodings` advertisement, no `utf-8` fallback. Internal
  arithmetic is byte-offset; the protocol-boundary seam converts
  to/from UTF-16 code units. Non-ASCII source content (UTF-8
  comments, string literals) exercises the conversion path; pure
  ASCII NSL source bypasses it.
- **Multiple files open simultaneously**: each gets its own
  translation unit; reparses run on a thread pool; diagnostics for
  one document never block diagnostics for another.
- **LSP `$/cancelRequest` for an in-flight request**: real
  cancellation per FR-020h–FR-020j. The protocol layer parses the
  notification, looks up the in-flight request by its `id`,
  signals its cancellation token, and the language-logic worker
  observes the token at the next polling point and aborts. The
  cancelled request's response carries the LSP-spec error code
  `RequestCancelled` (`-32800`) with an empty `result`. (Per
  Clarifications session 2026-05-05 Q5 → Option A; chosen over
  the simpler no-op stub to ensure the cancellation seam is
  exercised end-to-end at T3 rather than T9.)
- **Trailing-only edits inside a single `func`**: TUScheduler
  re-parses; T3 does not need incremental-parse correctness — the
  baseline is full reparse per
  [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
  §2.1 ("an initial implementation can do a full reparse — NSL
  files rarely exceed a few thousand lines").
- **Server crash**: structural — `nsl-lsp` is a long-running
  process; a crash kills the LSP session and the client falls back
  to no diagnostics. T3 does not introduce crash recovery; an
  unrecoverable internal error MUST surface as an `ERROR`-level
  stderr log record per FR-020d–FR-020f before the server exits
  non-zero. (The previously-considered alternative —
  `window/logMessage` — is deferred to T11 per FR-020g.)
- **Workspace folders not provided in `initialize`**: server
  operates in single-file mode; per-file translation units still
  work; cross-file features (deferred to T9 `references`) do not
  apply. `#include` resolution still works per FR-020a/FR-020b
  (env var + document-relative) — workspace folders are not the
  source of include paths at T3.
- **`NSL_INCLUDE` changes after server startup**: the server
  reads the env var **once** at startup; runtime changes do not
  take effect until the server restarts. (Editors that need a
  reload typically expose a "restart server" command.)
- **`#include` target outside `NSL_INCLUDE` and not relative to
  the open document**: surfaces as an unresolved-include
  diagnostic per FR-020c.

## Requirements *(mandatory)*

### Functional Requirements

#### Lifecycle (per LSP base protocol)

- **FR-001**: The server MUST handle the LSP base protocol
  initialization handshake — `initialize` request → response
  carrying server capabilities → `initialized` notification — per
  the LSP specification.
- **FR-002**: The server MUST respond to `shutdown` followed by
  `exit` by tearing down all open translation units, emitting one
  final empty `publishDiagnostics` per still-open document, and
  terminating with exit code 0. An `exit` received without a
  prior `shutdown` MUST cause termination with exit code 1.
- **FR-003**: The server's `initialize` response MUST advertise
  exactly the capabilities T3 implements — `textDocumentSync`
  with `change = TextDocumentSyncKind.Full` (`= 1`),
  `openClose = true`, and `foldingRangeProvider: true`. It MUST
  NOT advertise capabilities it does not implement (no
  `hoverProvider`, no `definitionProvider`, no
  `documentFormattingProvider`, etc.). T4 / T5 / T9 / T10 will
  extend the capabilities object as those milestones land. (Per
  Clarifications session 2026-05-05 Q1 → Option A: `Full` over
  `Incremental` chosen because the server reparses the full
  document anyway and the simpler wire contract suits the
  skeleton-milestone framing; an `Incremental` upgrade is
  forward-compatible and may land at T9 when file size makes wire
  bandwidth matter.)
- **FR-004**: The server MUST use **UTF-16** for every `Position`
  and `Range` value crossing the wire — i.e., `Position.character`
  is a UTF-16 code-unit offset within the line, per the LSP 3.16
  default. The server MUST NOT advertise the
  `general.positionEncodings` capability (introduced in LSP 3.17;
  out of scope per the LSP-version pin). Internal NSL source
  positions are byte offsets; the seam at the protocol boundary
  converts byte → UTF-16 code-unit on the way out and the reverse
  on the way in. (Per Clarifications session 2026-05-05 Q3 →
  Option A: LSP 3.16 floor chosen for maximum client
  compatibility; T4+ may raise the floor and re-introduce
  preferred-encoding advertisement when a 3.17+ feature becomes
  load-bearing.)

#### Document synchronization

- **FR-005**: The server MUST handle `textDocument/didOpen` by
  registering a translation unit for the document URI, storing the
  initial content and version, scheduling a parse + sema, and
  emitting `publishDiagnostics` once results are ready.
- **FR-006**: The server MUST handle `textDocument/didChange` by
  reading the full document text from the notification's
  `contentChanges[0].text` field (per FR-003's
  `TextDocumentSyncKind.Full`), updating the document version,
  scheduling a fresh parse + sema, and emitting
  `publishDiagnostics` once results are ready. The server MUST
  NOT receive incremental `{range, rangeLength?, text}` edits at
  T3; if a non-conforming client sends such a payload, the server
  MAY reject the notification with a structured log entry and
  ignore it (notifications cannot be replied to).
- **FR-007**: The server MUST handle `textDocument/didClose` by
  emitting one final empty `publishDiagnostics` to clear client-side
  state and releasing the document's translation-unit resources.
- **FR-008**: When multiple `didChange` notifications arrive faster
  than the parse-sema cycle for one document, the server MUST
  ensure that at most one `publishDiagnostics` per document
  reflects the **most recent** version received as of when the
  diagnostic was published. Diagnostics from intermediate states
  MUST NOT be published if a newer state has already been seen.
- **FR-009**: The server MUST track each open document's LSP
  version number and include the **document version that was
  diagnosed** in any state-tracking it exposes (e.g. via `version`
  field on `PublishDiagnosticsParams` per LSP 3.16+).

#### Diagnostics

- **FR-010**: For every parse + sema cycle that completes
  successfully, the server MUST emit exactly one
  `textDocument/publishDiagnostics` notification per document
  carrying the union of parser-level and Sema-level diagnostics
  produced by `libNSLFrontend.a`'s `DiagnosticEngine`, mapped to
  LSP `Diagnostic` shape: `severity` from `DiagnosticEngine`'s
  level, `range` from `SourceRange`, `code` from the diagnostic's
  ID (e.g. `S1`, `S2`, …, parser note `N5`, etc.), `message` from
  the formatted diagnostic string, `source` set to `nsl-sema` or
  `nsl-parse` to disambiguate origin.
- **FR-011**: Diagnostic `range` values MUST point to the
  user-visible NSL source — i.e. the `#line`-resolved logical
  location (Principle IV) — never to post-preprocess physical
  offsets.
- **FR-012**: When a previously-emitted diagnostic is resolved by a
  subsequent edit, the next `publishDiagnostics` MUST omit it; an
  empty `diagnostics` array clears all client-side diagnostics for
  that document.
- **FR-013**: Position values inside `Range` MUST be expressed in
  UTF-16 per FR-004, with line and character both zero-based per
  the LSP spec. For ASCII-only NSL source (the predominant case
  in the audited corpus), UTF-16 code-unit offsets equal byte
  offsets equal column numbers; the conversion seam is exercised
  only when non-ASCII characters appear (e.g., a UTF-8 comment
  or string literal).

#### Folding ranges

- **FR-014**: On `textDocument/foldingRange`, the server MUST
  return one `FoldingRange` per AST block that spans more than one
  source line — concretely: `module`, `declare`, `func`/`function`,
  `proc`, `state`, `seq`, `alt`, `any`, `par`, `if`/`else`,
  `for`, `while`, `generate`, and `_init` blocks (every
  block-opening production listed in `lang.ebnf §§5–8`).
- **FR-015**: For multi-line block comments (`/* … */` spanning
  ≥ 2 lines), the server MUST emit a `FoldingRange` with
  `kind = "comment"`.
- **FR-016**: `FoldingRange.startLine` and `endLine` MUST be
  zero-based per the LSP spec; `startCharacter` and
  `endCharacter` MAY be omitted (clients then fold whole-line).
- **FR-017**: When the document is unparseable (parse error), the
  server MUST return the folding ranges for whichever blocks the
  M2 parser's error-recovery managed to identify; it MUST NOT
  return an LSP error response.

#### Configuration discovery

- **FR-020a**: `nsl-lsp` MUST read the `NSL_INCLUDE` environment
  variable **once at server startup** (before processing the
  `initialize` request) and use its value as the angle-form
  `#include` search path for every document the server diagnoses
  during the session. Semantics MUST match the `nslc` driver's
  use of `NSL_INCLUDE` per `pp.ebnf §2.1`/P8 (colon-separated
  list of directories on POSIX, semicolon-separated on Windows).
  If `NSL_INCLUDE` is unset, the angle-form search path is empty.
  (Per Clarifications session 2026-05-05 Q2 → Option A: env-var
  semantic mirrors the CLI driver, preserves Constitution
  Principle II's no-duplication rule, adds zero LSP protocol
  surface, keeps workspace-level methods deferred to T9.)
- **FR-020b**: `nsl-lsp` MUST resolve quote-form `#include "…"`
  references relative to the directory containing the open
  document (the `textDocument.uri`'s parent directory) — same
  semantic as the CLI driver. The server MUST NOT accept
  additional `-I`-style search paths at T3; T9 may add a
  `workspace/configuration`-based mechanism when workspace-level
  methods land.
- **FR-020c**: An `#include` whose target cannot be resolved
  through FR-020a/FR-020b MUST surface as a diagnostic via the
  same `publishDiagnostics` channel as Sema diagnostics (FR-010),
  with `source = "nsl-preprocess"` and the diagnostic code from
  M1's preprocessor `DiagnosticEngine` output. The server MUST
  NOT silently substitute an empty file or skip the directive.

#### Server logging and observability

- **FR-020d**: `nsl-lsp` MUST emit operational log records to
  **stderr** as plain text — one record per line, with no
  embedded newlines. Each record MUST carry a level prefix
  (`ERROR`/`WARN`/`INFO`/`DEBUG`), a wall-clock timestamp in
  ISO-8601 format with whole-second precision, and a free-text
  message. (Per Clarifications session 2026-05-05 Q4 → Option A:
  stderr is universal across editors and CI harnesses, requires
  no LSP protocol surface, and matches the convention established
  by `libNSLFrontend.a`'s `DiagnosticEngine`.)
- **FR-020e**: The minimum log level MUST be controlled by the
  environment variable `NSL_LSP_LOG_LEVEL`, read once at server
  startup. Permitted values: `error`, `warn`, `info`, `debug`
  (case-insensitive). The default when unset MUST be `warn`. An
  invalid value MUST cause the server to exit non-zero before
  the LSP handshake begins, with a stderr message identifying
  the bad value.
- **FR-020f**: Required log content MUST include: every
  `initialize` request received (level `INFO`), every protocol
  framing error (level `ERROR`), every internal exception or
  unrecoverable error (level `ERROR`), and every TUScheduler
  parse-cycle failure (level `WARN`). Required content MUST NOT
  include the body of source documents (`textDocument/didOpen`
  / `didChange` payloads) at any level — source content stays
  server-internal even at `DEBUG` level.
- **FR-020g**: `nsl-lsp` MUST NOT emit `window/logMessage` LSP
  notifications at T3. (`window/logMessage` lands at the editor
  packaging milestone — T11 — when per-editor "LSP Log" panel
  population becomes the primary observability surface for
  end users.)

#### Cancellation

- **FR-020h**: `nsl-lsp` MUST handle the `$/cancelRequest`
  notification per the LSP base protocol: the notification's
  `params.id` identifies an in-flight client→server request whose
  worker MUST be signalled to abort. (Per Clarifications session
  2026-05-05 Q5 → Option A: real cancellation, not a no-op stub.)
- **FR-020i**: The protocol layer MUST issue a per-request
  cancellation token to every cancellable request handler at the
  point of dispatch; handlers MUST poll the token at coarse-grained
  abort points (at minimum: between AST traversal subtrees for
  `foldingRange`). On token-set, the handler MUST abort and the
  protocol layer MUST send a response with JSON-RPC error code
  `RequestCancelled` (`-32800`) and an LSP-spec error message
  (`"request cancelled"`).
- **FR-020j**: A `$/cancelRequest` for a request that has already
  completed, has never been seen, or is a notification (not a
  request) MUST be silently ignored — no error response, no log
  record above `DEBUG` level.

#### Architecture and reuse

- **FR-018**: `nsl-lsp` MUST link `libNSLFrontend.a` and reuse
  `Lexer`, `Preprocessor`, `Parser`, `Sema`, `SymbolTable`, and
  `TypeSystem` without re-implementation, satisfying Constitution
  Principle II's no-duplication rule.
- **FR-019**: The LSP-protocol layer (request parsing, response
  serialization, lifecycle dispatch, capability negotiation, JSON-RPC
  framing) and the language-logic layer (parse, sema, folding,
  diagnostics) MUST be separated such that adding a future LSP
  method (T4 `hover`, T5 `formatting`, T9 `completion`, T10
  `inlayHint`) does not require modifying the JSON-RPC transport
  or the document-sync handlers — verifiable per US4 acceptance 1.
- **FR-020**: The server MUST manage per-document translation
  units via a scheduler/threading layer (the TUScheduler pattern
  per [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
  §3.3) so that parse + sema work on one document does not block
  request handling on another, and so that AST-version-keyed
  caching is in place for T4 / T5 / T9 / T10 to use.

#### Test gate (mandatory per README §Roadmap T3)

- **FR-021**: An LSP integration test MUST exist that drives
  `nsl-lsp` over stdin/stdout (or an equivalent in-process JSON-RPC
  channel) by sending the lifecycle sequence `initialize` →
  `initialized` → `textDocument/didOpen` (file with one Sema error)
  → assert one `publishDiagnostics` with the expected diagnostic
  → `textDocument/didChange` (text edited to fix the error) →
  assert one `publishDiagnostics` with empty `diagnostics` →
  `shutdown` → `exit` → assert exit code 0.
- **FR-022**: A second integration test MUST exercise the
  `textDocument/foldingRange` request against a fixture that
  contains every block-opening production from `lang.ebnf §§5–8`
  and verify the response.
- **FR-023**: Both integration tests MUST run in the project's
  existing CI matrix (`scripts/ci.sh`) on every PR and MUST fail
  the run on any assertion failure. Determinism applies
  (Principle V): two consecutive runs over the same input MUST
  produce byte-identical `publishDiagnostics` payloads.

#### Boundaries

- **FR-024**: `nsl-lsp` MUST NOT implement any LSP method outside
  the T3 set: no `hover`, `definition`, `documentSymbol`,
  `semanticTokens`, `signatureHelp`, `formatting`, `rangeFormatting`,
  `references`, `completion`, `rename`, `codeAction`, `inlayHint`,
  or `prepareCallHierarchy`. Those methods are deferred to T4 / T5
  / T9 / T10 per `docs/design/nsl_tooling_design.md` §3.2.
- **FR-025**: `nsl-lsp` MUST NOT implement workspace-level methods
  (`workspace/didChangeConfiguration`, `workspace/symbol`,
  `workspace/executeCommand`, etc.) at T3. Per-file LSP methods
  are sufficient for the test gate; workspace features are
  out-of-scope until at least T9 (`references`) introduces the
  cross-file requirement.
- **FR-026**: `nsl-lsp` MUST NOT introduce its own lex/parse/sema
  code — Constitution Principle II's no-duplication rule applies.
  Any operation requiring AST or SymbolTable access MUST go
  through `libNSLFrontend.a`. This is enforceable by inspecting
  the `nsl-lsp` source tree for lexer/parser/sema source files
  outside `lib/Lex`, `lib/Preprocess`, `lib/Parse`, `lib/AST`,
  `lib/Sema`.
- **FR-027**: T3 MUST NOT include any tree-sitter or TextMate
  involvement; semantic-tokens-style highlighting that would
  consume the token-lattice from
  [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
  §2.3 is deferred to T4. T3 ships with diagnostic + folding only.
- **FR-028**: T3 MUST NOT add an editor-package shell (VS Code
  extension `package.json`, Neovim plugin, etc.) beyond what is
  needed for an integration smoke test. Editor packaging across
  Neovim / Emacs / Sublime is a T11 deliverable; the VS Code
  extension shell that consumes WASM tree-sitter is a T8
  deliverable.

### Key Entities

- **LSP server process (`nsl-lsp`)** — A long-running binary that
  speaks LSP over stdin/stdout (JSON-RPC framed per the LSP base
  protocol). Reuses `libNSLFrontend.a`. Lives at `tools/nsl-lsp/`
  in the repository tree per
  [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
  §8.
- **LSP-protocol layer** — The class (`NslLSPServer`, per the
  design doc) that parses incoming JSON-RPC requests into typed
  structs, dispatches them to typed handlers, serializes responses,
  and manages the lifecycle (initialize / initialized / shutdown
  / exit).
- **Language-logic layer** — The class (`NslServer`, per the
  design doc) that exposes a stateless API of language operations:
  `diagnostics(uri) → Diagnostic[]`, AST-access seams, folding
  computation. Future T4–T10 methods extend this layer.
- **Translation unit (`NslTU`)** — One per open document URI;
  holds the most-recently-seen content + version, the parsed
  CompilationUnit, the SymbolTable, and the diagnostic vector.
  Re-parses on `didChange`. Owned by the TUScheduler.
- **TUScheduler** — The threading + caching layer that owns the
  translation-unit map keyed by document URI, runs parse + sema
  on a worker pool, serializes writes per document, parallelizes
  reads across documents, and publishes diagnostics back to the
  protocol layer.
- **Diagnostic mapping seam** — The translation that converts
  `nsl::DiagnosticEngine` output (with `SourceLocation` and
  level) into the LSP `Diagnostic` shape (with `Range` in the
  negotiated position-encoding, `severity`, `code`, `message`,
  `source`). One-directional; no LSP-to-NSL mapping is needed
  at T3.
- **LSP integration test harness** — A test driver that spawns
  `nsl-lsp` as a subprocess (or instantiates it in-process),
  sends a deterministic JSON-RPC sequence, and asserts on the
  responses. Lives under `test/lsp/` per the existing
  `test/<layer>/` convention.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The `nsl-lsp` integration test from FR-021 passes on
  a clean checkout in CI: opening a fixture file with a single
  `S1` Sema error produces one `publishDiagnostics` containing
  exactly one diagnostic at the expected location; editing the
  file to fix the error produces a second `publishDiagnostics`
  with an empty diagnostics array. This is the test gate stated
  in [`README.md`](../../README.md) §Roadmap row T3.
- **SC-002**: 100% of the M3 Sema-constraint set (the 23 `Sn`
  constraints with locked diagnostic strings per Principle VIII —
  all of `S1`–`S29` minus the 6 constructive constraints `S13`,
  `S18`, `S19`, `S23`, `S24`, `S27`, per
  [`CLAUDE.md`](../../CLAUDE.md) §1) round-trip from
  `DiagnosticEngine` to LSP `Diagnostic` with the correct
  severity, code, range, and message — verified by parameterized
  fixture coverage in the LSP integration test.
- **SC-003**: When the same fixture file is opened twice in two
  separate `nsl-lsp` runs over identical input, the
  `publishDiagnostics` payloads are byte-identical (Principle V
  determinism). The diff between the two captured payloads MUST
  be empty.
- **SC-004**: Time from `textDocument/didOpen` (or `didChange`)
  arrival to the corresponding `publishDiagnostics` emission MUST
  be **under 250 ms** for an audited-corpus-sized NSL file (≤ 1500
  lines) on a standard CI runner. (Aspirational: the M3 sema pass
  itself runs well under this threshold; the LSP overhead must
  not dominate.) Files larger than 1500 lines are out of scope
  for the SC-004 budget.
- **SC-005**: The `nsl-lsp` binary links exactly one instance of
  `libNSLFrontend.a` (verifiable via the linker map): every
  lexer, preprocessor, parser, AST, sema, symbol-table, and
  type-system symbol resolves into the shared library, none into
  duplicated `nsl-lsp`-private code. (Constitution Principle II
  enforced structurally.)
- **SC-006**: A future contributor adding T4 (`hover`,
  `definition`, etc.) must not modify the JSON-RPC transport,
  the lifecycle handlers, or the TUScheduler — verifiable when T4
  lands by checking the diff against T3 source files. (Forward
  promise; not testable at T3 merge.)
- **SC-007**: The integration tests from FR-021 and FR-022
  complete in under **30 seconds** combined on a standard CI
  runner — fast enough that they run on every PR with negligible
  cost.
- **SC-008**: Capability advertisement is exact: the
  `initialize` response declares exactly
  `{textDocumentSync: …, foldingRangeProvider: true}` and no
  others. A test asserts this — every later T-track milestone
  that adds a capability must update both the implementation
  and that assertion in the same PR (Principle VII coupling).
- **SC-009**: When the FR-021 integration test fails, the
  captured stderr from the `nsl-lsp` subprocess MUST contain at
  least one `ERROR` or `WARN` log record identifying the failure
  cause — a silent-stderr crash MUST be impossible. (Verifiable
  by injecting a synthetic fault in a CI-only test mode and
  asserting the captured stderr matches a regex for the
  expected log line.)
- **SC-010**: A cancellation integration test MUST exercise the
  end-to-end seam from FR-020h–FR-020j: the harness sends a
  `foldingRange` request against an artificially-large fixture
  (constructed to push the AST-walk past the worker's polling
  threshold), immediately follows with a `$/cancelRequest`, and
  asserts that the response carries error code `RequestCancelled`
  (`-32800`) within an upper bound of 200 ms. This test gates
  the cancellation-seam quality at T3 rather than deferring its
  verification to T9.

## Assumptions

- **M3 is delivered.** This is reflected in
  [`CLAUDE.md`](../../CLAUDE.md) §1 ("Status as of 2026-04-30: M1,
  M2, M3, M4, and M5 (this branch, pass-standalone) are
  delivered"). T3 consumes M3 outputs (SymbolTable, TypeSystem,
  per-`Sn` constraint diagnostics) directly via
  `libNSLFrontend.a`. If M3's diagnostic API needs to be widened
  for T3's needs (e.g., to expose the diagnostic ID separately
  from its formatted message for FR-010's `code` field), that
  widening is in scope for T3 and is a Principle VII coupling
  concern (the spec/design coupling rule applies inside the
  compiler track too).
- **JSON-RPC framing.** The LSP base protocol's `Content-Length:
  N\r\n\r\n` framing is implemented inside `nsl-lsp` rather than
  pulled from an external dependency. (LLVM ships
  `llvm::json::Value` and a `JSONTransport`-style scaffold that
  may be reused.) Reading a vendored helper here is acceptable
  per Principle II's "code reuse, not code duplication" intent.
- **LSP protocol floor.** T3 targets **LSP 3.16** (December
  2020) as the minimum protocol version. This is the floor below
  which client compatibility is not promised. `nsl-lsp` does not
  advertise the `general.positionEncodings` capability (an LSP
  3.17 feature) — position values are UTF-16 unconditionally,
  per the LSP 3.16 default. T4+ may raise the floor and
  introduce `positionEncodings` (with `utf-8` preferred) when a
  3.17+ feature becomes load-bearing for a tooling milestone;
  that promotion is a Principle VII coupling concern at the
  time it lands.
- **Position encoding.** UTF-16 unconditionally at T3 per the
  protocol-floor pin. Internal `libNSLFrontend.a` source
  arithmetic is byte-offset; the protocol-boundary seam converts.
  Pure-ASCII NSL source (the predominant case in the audited
  corpus) makes the conversion a no-op; non-ASCII source content
  (UTF-8 comments, string literals) exercises the conversion
  path.
- **Concurrency baseline.** TUScheduler uses one worker per CPU
  core (or a fixed pool of 4, whichever is smaller) by default;
  the choice is plan-level. Determinism (SC-003) MUST hold
  regardless of the worker count — diagnostics for a given input
  do not depend on scheduling.
- **Incremental-parse posture.** T3 implements full re-parse on
  every `didChange` per
  [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
  §2.1. Incremental parsing is shared infrastructure listed in
  §2.1 but is not load-bearing for T3's test gate. If a future
  milestone (likely T9 or T10 once the file count grows) needs
  incremental parsing for performance, it adds it then; T3 does
  not block on it.
- **CST is not consumed at T3.** The CST layer per §2.4 is
  consumed by the formatter (T2/T5) and by clients that need
  comment-preserving operations. T3 ships diagnostics + folding,
  both of which use the AST + token positions only. T3 does not
  block on a CST being available.
- **Stable node IDs are not consumed at T3.** Per §2.2 stable
  node IDs prevent diagnostic flicker across reparses; at T3, the
  full `publishDiagnostics` array is re-emitted on every reparse
  and the LSP client (VS Code, Neovim, etc.) handles the diff —
  this is the LSP-spec norm and is sufficient for the test gate.
  Stable node IDs land naturally when an LSP feature needs them
  (T4 / T9).
- **Editor-side smoke test.** Per FR-028, T3 does not ship a VS
  Code extension shell; verification of the integration-with-a-real-editor
  claim is via a manual smoke test (drop `nsl-lsp` binary path
  into Neovim's `nvim-lspconfig` `cmd =` setting; confirm
  diagnostics appear). The CI integration test runs against
  `nsl-lsp` directly without an editor.
- **`NSL_INCLUDE` propagation from editors.** Modern LSP clients
  expose a per-server environment-variable map (VS Code:
  `serverEnvironmentVariables` in extension manifest or
  `cmd_env` in launch config; Neovim's `vim.lsp.start({cmd_env =
  {NSL_INCLUDE = "..."}})`; Emacs lsp-mode `lsp-server-trace`
  / process-environment binding). FR-020a's env-var semantic
  works through every supported editor's LSP-launch hook with no
  extension-specific code at T3. Editor packaging (T11) will
  document the recommended per-editor recipe.
- **Audited-corpus availability.** SC-004 references files from
  the audited corpus; the audited corpus is vendored at M7
  (P-VEN). T3 lands before M7; SC-004 is therefore verified
  against equivalently-sized hand-written fixtures or whichever
  subset of the audited corpus has already been vendored at the
  time T3 merges.
- **Constitutional anchors.** T3's acceptance is gated on
  Principles V (determinism — SC-003), VI (layered tests —
  FR-021/022 are the LSP test layer), VII (spec/design coupling
  — SC-008 capability assertion), VIII (TDD — fixtures written
  first, observed failing, then made green per the per-fixture
  workflow), and IX (CI green — FR-023). Constitution v1.7.0 is
  the current authority; no amendment is anticipated for T3.
