<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Feature Specification: T5 — LSP Formatting Integration (`textDocument/formatting` + `rangeFormatting`)

**Feature Branch**: `011-t5-lsp-formatting`
**Created**: 2026-05-12
**Status**: Draft
**Input**: User description: "T5"

> **Roadmap anchor.** Tooling-track milestone **T5**, per
> [`README.md`](../../README.md) §Roadmap row T5 and
> [`docs/CLAUDE.md`](../../docs/CLAUDE.md) §3 "Implementing the LSP".
> T5 is the milestone that wires the T2 formatter (`libNslFmt.a`)
> into the T3 LSP server (`nsl-lsp`) by implementing exactly two
> new LSP methods: `textDocument/formatting` (whole-document) and
> `textDocument/rangeFormatting` (line-range). Per
> [`CLAUDE.md`](../../CLAUDE.md) §2.1, both methods are classified
> "Low difficulty" because the heavy lifting already lives in the
> two prerequisite libraries.
>
> **Prerequisite dependencies.** T5 gates on **T2** and **T3**, both
> already delivered:
>
> - **T2** ([`specs/010-t2-formatter-v0/`](../010-t2-formatter-v0/))
>   shipped `libNslFmt.a` with a frozen 10-symbol public API
>   ([`format-api.contract.md`](../010-t2-formatter-v0/contracts/format-api.contract.md))
>   whose `nsl::fmt::format_buffer(...)` entry point accepts an
>   optional `LineRange` parameter — already shaped for both
>   `textDocument/formatting` (no `LineRange`) and `textDocument/
>   rangeFormatting` (with `LineRange`). T2 spec FR-019 and SC-005
>   committed to "at most ~30 lines of glue" at T5; this spec
>   delivers that glue.
> - **T3** ([`specs/010-t3-lsp-skeleton/`](../010-t3-lsp-skeleton/))
>   shipped the `nsl-lsp` server skeleton (JSON-RPC transport,
>   `initialize`/`shutdown`/`exit`, document sync via
>   `TextDocumentSyncKind.Full`, diagnostics, folding ranges,
>   `$/cancelRequest`, `NSL_LSP_LOG_LEVEL` stderr logging).
>   T3 spec FR-019 (architecture-and-reuse) promised a
>   **format-region seam** on the language-logic layer (`NslServer`)
>   such that T5 needs only to register a handler — no JSON-RPC
>   transport or TUScheduler changes required.
>
> **What T5 does NOT do.** T5 ships exactly the two formatting
> methods. Every other deferred LSP method
> (`hover`/`definition`/`documentSymbol`/`semanticTokens`/
> `signatureHelp` → T4; `references`/`completion`/`rename`/
> `codeAction` → T9; `inlayHint`/`prepareCallHierarchy` → T10) is
> out of scope. T5 also does NOT introduce
> `documentOnTypeFormattingProvider` (format-on-typing) — that
> method is not on the [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
> §3.2 list and is excluded by FR-024.
>
> **Constitutional anchors.** Principle II (no duplication —
> `nsl-lsp` reuses `libNslFmt.a` rather than re-rolling layout
> logic), Principle V (determinism — formatting output is a pure
> function of `(source, configuration)` so two LSP sessions over the
> same input emit byte-identical `TextEdit[]`), Principle VI (test
> discipline — LSP-layer integration tests written first against
> the FR-001/FR-002 wire shape), Principle VII (spec/design coupling
> — T5 amends the T3 `lsp-protocol.contract.md` §1.2 capabilities
> JSON in the same PR that lands the implementation), Principle IX
> (CI green — the T5 integration tests run in the existing
> `scripts/ci.sh` matrix).

---

## Clarifications

### Session 2026-05-12

- Q: When the editor sends `FormattingOptions {tabSize: 2, insertSpaces: false}` and the repo has a `.nsl-fmt.toml` with `indent = 4`, which one wins? → A: **TOML wins.** Project style overrides editor preference (clang-format model). `nsl-lsp` resolves `nsl::fmt::Configuration` exclusively from the discovered `.nsl-fmt.toml` (or `nsl::fmt::default_configuration()` if none is discoverable from the document URI). The LSP `FormattingOptions` object is *received* on the wire (it is a required field of `DocumentFormattingParams` per LSP 3.16) but its content is **ignored** for the purposes of computing the formatter `Configuration`. SC-005's byte-equivalence with `nsl-fmt --stdin` is thereby preserved trivially: the CLI ignores editor preferences entirely, and so does the LSP.
- Q: On `Status::Success` from `format_buffer`, should the server emit a single whole-span `TextEdit` or a minimal Myers-diff `TextEdit[]`? → A: **Single whole-span `TextEdit`.** The response is a `TextEdit[]` of length exactly 1, whose `range` covers the entire input span (`{start: {line: 0, character: 0}, end: <one past last line>}` for `textDocument/formatting`; the expanded whole-line range per FR-003 for `textDocument/rangeFormatting`) and whose `newText` is the full formatter output verbatim. Byte-identical to `format_buffer`'s `formattedText` by construction; idempotence and trailing-newline guarantees flow through unchanged. The minimal-diff path is forward-compatible and may land at a later T-track polish milestone without breaking the FR-006a byte-equivalence contract.
- Q: On `Status::Refused` from `format_buffer` (parse error), what does the LSP server send back? → A: **Return `null`.** LSP convention for "formatting not available for this document"; the editor leaves the buffer untouched and the user sees the existing parse-error diagnostic via the T3 `publishDiagnostics` channel. No JSON-RPC error response (which would produce a popup and double-report the error). The same `null` response applies to `Status::Error` (range-out-of-bounds, internal failure); the distinction between `Refused` and `Error` is captured in the stderr log record per FR-015, not on the wire.
- Q: What does the LSP server do when `.nsl-fmt.toml` is malformed (T2 `parse_config_file` returns `Status::Refused` for syntactic errors or `Status::Error` for semantic errors like `indent = "potato"`)? → A: **Fall back to `default_configuration()` for the current format request, AND emit a `publishDiagnostics` notification against the TOML file's URI** carrying the diagnostics returned by `parse_config_file` (mapped to the LSP `Diagnostic` shape per the existing T3 diagnostic-mapping seam). The format request proceeds with the default configuration and returns a normal `TextEdit[]` per FR-006. Rationale: matches `clang-format`'s tolerant posture (a broken `.clang-format` doesn't refuse to format `.cpp`); keeps the format request productive while signalling the TOML breakage; reuses the existing diagnostic channel rather than coupling the format-response shape to TOML health.
- Q: If a `textDocument/didClose` arrives for a document while a `textDocument/formatting` request for the same URI is in flight on a TUScheduler worker, what happens? → A: **Let the in-flight format complete and send its response.** The TUScheduler worker finishes its normal path; the protocol layer sends the response (`TextEdit[]` on Success, `null` on Refused/Error per FR-007); the editor discards the response because the document has been closed. Wasted CPU is bounded by SC-004's 300 ms budget. Rationale: zero new cancellation plumbing in `Formatting.cpp`; `Server.cpp`'s `onDidClose` is not modified (FR-010 architectural seam preserved); matches T3's "worker finishes or is cancelled cleanly" posture for in-flight reparse on `didClose`. The explicit `$/cancelRequest` path remains the only mechanism that aborts an in-flight format.

---

## User Scenarios & Testing *(mandatory)*

### User Story 1 — NSL author formats the open buffer with one keystroke (Priority: P1)

A hardware engineer is editing a `.nsl` module in an LSP-capable
editor (VS Code, Neovim, Emacs, or Sublime Text 4) where the editor
is configured to use `nsl-lsp` as the language server (per T3's
manual-smoke-test recipe). The buffer contains valid NSL with
non-canonical whitespace: mixed tabs and spaces, missing operator
spacing, misaligned `alt` arrows, an over-long `proc_name` argument
list on one line. The author invokes the editor's "Format Document"
command (default keybind: `Shift+Alt+F` in VS Code,
`:lua vim.lsp.buf.format()` in Neovim, etc.). The editor sends
`textDocument/formatting` to `nsl-lsp`. Within sub-second time, the
server returns a `TextEdit[]` that rewrites the buffer to the
canonical NSL style (per [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
§5.3 rules — alt/any case alignment, struct member alignment,
proc_name argument-list wrapping, bit-slice/concat spacing,
operator spacing, attached-comment preservation). The buffer
updates immediately; the diff matches what `nsl-fmt -i foo.nsl`
would have produced from the command line.

**Why this priority**: This is the entire user-visible value of T5
and is the test gate stated verbatim in
[`README.md`](../../README.md) §Roadmap row T5 ("Format on save in
editor produces same output as `nsl-fmt` CLI."). Without it, T5 is
not done.

**Independent Test**: Launch `nsl-lsp` over stdin/stdout. Send
`initialize` → `initialized` → `textDocument/didOpen` (fixture file
with known non-canonical whitespace) → `textDocument/formatting`.
Assert the response is a `TextEdit[]` whose application to the
original buffer produces a string byte-identical to the same
fixture run through `nsl-fmt --stdin`.

**Acceptance Scenarios**:

1. **Given** an NSL fixture with mixed tabs/spaces, missing operator
   spacing, and misaligned `alt` arrows, **When** the client opens
   it and sends `textDocument/formatting`, **Then** the response is a
   `TextEdit[]` whose application to the buffer produces output
   byte-identical to `nsl-fmt --stdin < fixture.nsl`.
2. **Given** an NSL fixture that is already canonically formatted,
   **When** the client sends `textDocument/formatting`, **Then** the
   response is an empty `TextEdit[]` (no changes; LSP convention).
3. **Given** an NSL fixture containing both a Sema error (e.g., an
   `S1` violation `reg foo__bar;`) and non-canonical whitespace
   elsewhere, **When** the client sends `textDocument/formatting`,
   **Then** the server returns `null` and a corresponding
   diagnostic surfaces (or has already surfaced) via the existing
   T3 `publishDiagnostics` channel. Formatting refuses on parse
   error per T2 FR-012 (strict refusal); Sema errors do NOT block
   formatting because the input still lexes and parses — see Edge
   Cases below for the exact rule.
4. **Given** the buffer's preprocessor directives (`#include`,
   `#define`, `%IDENT%` splices), **When** the formatter runs,
   **Then** those directives appear byte-identical in the formatted
   output per T2 FR-012a (directive lines are opaque CST tokens).

---

### User Story 2 — Author formats only a selection inside a long module (Priority: P2)

The same author is working on a 500-line `module` and has just
edited one `proc_name` definition (lines 230–250). They don't want
to format the whole file — that would touch unrelated regions and
churn the diff in their PR. They select lines 230–250 in their
editor (or place the cursor inside that range) and invoke the
editor's "Format Selection" command. The editor sends
`textDocument/rangeFormatting` with the selected `Range`. The
server returns a `TextEdit[]` that rewrites only those lines; every
other line in the file is preserved byte-identical (per T2 FR-007
`--range` semantics: "Lines outside the range MUST be emitted
byte-identical to the input").

**Why this priority**: P2 because whole-document formatting (User
Story 1) is sufficient for daily editor use; range formatting is
the polish that prevents diff churn on partial-file edits. Cannot
be cut entirely because (a) the LSP method `textDocument/
rangeFormatting` is on the [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
§3.2 list with the same "Low" difficulty rating, (b) the T2
formatter already accepts a `LineRange` parameter so the lower-half
wiring is free, and (c) editors expose "Format Selection" by
default — its absence would be a visible regression vs. C/C++ LSPs.

**Independent Test**: Open a multi-line fixture; send
`textDocument/rangeFormatting` with a `Range` covering only the
middle third of the file; assert the response is a `TextEdit[]`
whose application yields exactly the same output as `nsl-fmt
--stdin --range L1:L2 < fixture.nsl` where L1/L2 are the 1-indexed
line numbers covering the selected `Range`.

**Acceptance Scenarios**:

1. **Given** a 500-line fixture with non-canonical whitespace
   throughout, **When** the client sends `textDocument/
   rangeFormatting` with a `Range` covering lines 230 to 250
   (zero-based LSP positions), **Then** the response is a
   `TextEdit[]` whose application changes only lines 230–250 and
   every other line stays byte-identical.
2. **Given** a `Range` that starts mid-line (not at column 0),
   **When** the client sends `textDocument/rangeFormatting`,
   **Then** the server expands the range to whole-line boundaries
   (snap `start` down to start-of-line, `end` up to end-of-line)
   before forwarding to the formatter, and the `TextEdit[]`
   response uses the expanded whole-line span. (NSL formatting
   does not preserve column-level positioning of partial lines —
   the formatter operates on line-granular layout per T2's
   Wadler-Leijen pretty-printer.)
3. **Given** a `Range` whose start or end falls outside the
   document (e.g., end-line exceeds the line count), **When** the
   client sends `textDocument/rangeFormatting`, **Then** the server
   clamps the range to the document bounds and proceeds (per the
   LSP recovery convention; T2's `LineRange` precondition forbids
   out-of-bounds, so the LSP layer clamps before calling).
4. **Given** an empty `Range` (`start == end`), **When** the client
   sends `textDocument/rangeFormatting`, **Then** the server
   returns an empty `TextEdit[]` (no edits; the range covers no
   content).

---

### User Story 3 — Editor format-on-save produces identical output to `nsl-fmt --check` in CI (Priority: P2)

A team has CI configured with `nsl-fmt --check` as a required PR
gate (per T2 User Story 2). Individual contributors enable
"format on save" in their editors (VS Code:
`"editor.formatOnSave": true` plus `"[nsl]": {"editor.defaultFormatter":
"nsl-lsp"}`; equivalent settings in Neovim's `BufWritePre` autocmd
and Emacs's `before-save-hook`). When a contributor saves a `.nsl`
file, the editor emits `textDocument/formatting`, applies the
returned `TextEdit[]`, then writes the buffer to disk. Files
formatted via "format on save" pass `nsl-fmt --check` in CI without
any further fixup commits.

**Why this priority**: P2 because it depends on User Story 1 (whole
document formatting) and on the T2 → T5 byte-equivalence guarantee
(SC-005 below). This is the wire-up that delivers
[`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
§10's bullet "Save-on-format that produces byte-identical output
whether it ran on their machine, a CI runner, or through
`textDocument/formatting` via the LSP."

**Independent Test**: A CI integration test runs a fixture file
through three paths: (a) `nsl-fmt --stdin`, (b) `nsl-fmt -i` then
read back, (c) `nsl-lsp` `textDocument/formatting` request →
apply `TextEdit[]` to original buffer. Assert all three produce
byte-identical output.

**Acceptance Scenarios**:

1. **Given** a fixture file, **When** the same file is formatted
   via the CLI (`nsl-fmt --stdin < fixture.nsl`) and via the LSP
   server (`textDocument/formatting`), **Then** the two resulting
   buffers are byte-identical.
2. **Given** an editor configured with "format on save" pointing
   at `nsl-lsp`, **When** the user saves a non-canonical `.nsl`
   file, **Then** the saved-to-disk content matches what
   `nsl-fmt -i` would have produced from the command line.
3. **Given** a project CI that runs `nsl-fmt --check` on every
   committed `.nsl` file, **When** every contributor's editor uses
   `nsl-lsp` format-on-save, **Then** the `--check` step never
   reports a diff caused by editor-vs-CLI drift.

---

### User Story 4 — Architectural seam: T5 does not modify any T3 lifecycle plumbing (Priority: P3)

A future tooling-track contributor lands T5 by registering exactly
two new method handlers on the existing `NslLSPServer` dispatch
table and adding one new source file (`lib/LSP/Features/
Formatting.cpp`, per [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
§8 directory layout) plus the `documentFormattingProvider` and
`documentRangeFormattingProvider` entries in the `InitializeResult`
capabilities JSON. They do not modify the JSON-RPC transport, the
TUScheduler, the document-sync handlers (`didOpen`/`didChange`/
`didClose`), the diagnostic-mapping seam, the folding-range handler,
the cancellation plumbing, or the logging layer.

**Why this priority**: P3 because it gates structural quality
rather than runtime behaviour, but it is the verification that T3's
FR-019 ("format-region seam") was actually load-bearing — i.e., T3
shipped real extension points, not a stub. If T5 has to reshape the
LSP layer, that's a T3 design defect; if T5 lands cleanly, T3's
architecture is vindicated and T4 / T9 / T10 inherit the same
discipline.

**Independent Test**: A code-structure inspection at T5 merge: the
diff against the T3 baseline confirms that (a) the JSON-RPC
transport (`Transport.cpp`) is untouched, (b) the TUScheduler
(`Scheduler.cpp`) is untouched, (c) the lifecycle handlers
(`Server.cpp`'s `onInitialize`/`onShutdown`/`onExit`) are untouched
**except** for the addition of the two new capability advertisement
entries, and (d) the new code is confined to one new file
`lib/LSP/Features/Formatting.cpp` plus one registry entry in the
dispatch table.

**Acceptance Scenarios**:

1. **Given** the T5 PR diff against the T3 baseline, **When** a
   reviewer inspects which files changed, **Then** the new code is
   confined to: one new `Formatting.cpp` (+ optional header), one
   registry edit in the dispatch table, one capability JSON edit
   in `Server.cpp`'s `onInitialize`, and the `lsp-protocol.contract.md`
   amendment (per Principle VII coupling). No other LSP source
   file is modified.
2. **Given** the T5 build, **When** a contributor inspects the
   linker map for `nsl-lsp`, **Then** every formatting-related
   symbol resolves into `libNslFmt.a` and `libNSLFrontend.a`; no
   duplicated layout logic exists inside `nsl-lsp` private code
   (Constitution Principle II verified structurally).
3. **Given** the T5 server's `InitializeResult` capabilities JSON,
   **When** a test asserts byte-equality against the new canonical
   shape (T3's §1.2 contract JSON with two new entries:
   `documentFormattingProvider: true` and
   `documentRangeFormattingProvider: true`), **Then** the
   assertion passes — and any later T-track milestone that adds a
   capability must update the canonical JSON in the same PR
   (Principle VII coupling re-anchored).

---

### Edge Cases

- **Parse-error input**: when the buffer fails to lex+parse (per
  T2 FR-012 strict refusal — BOM, vendor pragmas, top-level
  system-task expressions, malformed identifiers), `format_buffer`
  returns `Status::Refused`. The LSP server MUST treat this as
  a "no edits available" response: return `null` (per LSP spec
  for "formatting not available for this document") so the editor
  does not silently truncate the buffer to empty. The
  `DiagnosticEngine` output for the same parse error is already
  surfacing via the T3 `publishDiagnostics` channel — the
  formatter does NOT double-emit the diagnostic via its own
  response.
- **Sema-error input** (`S1`–`S29` from M3 — distinct from parse
  errors): the input still lexes and parses, so `format_buffer`
  proceeds and returns `Status::Success`. The formatter's
  `nsl::fmt::Configuration` is applied; the formatted output may
  still contain the same Sema error, which the existing T3
  `publishDiagnostics` channel reports independently. **The
  formatter does NOT silently rewrite around or "fix" Sema
  errors** — that's the linter's job (T6/T7) via `codeAction`
  (T9), not the formatter.
- **Empty document**: `format_buffer` returns `Status::Success`
  with an empty string (per T2 SC-002's empty-input acceptance);
  the LSP layer returns an empty `TextEdit[]` (no changes).
- **Document with only directives** (no NSL fragments): per T2
  FR-012a, directives are opaque tokens; output equals input;
  empty `TextEdit[]`.
- **Range overlapping a directive line**: directives are opaque
  tokens — the LSP layer expands the range to whole-line boundaries
  (per User Story 2 acceptance 2) and passes the whole-line range
  to `format_buffer`; the directive line round-trips byte-identical
  inside the formatted output (T2 FR-012a is preserved through the
  range path).
- **Range covering the entire document**: equivalent to
  `textDocument/formatting`; the LSP server MAY collapse this case
  to a `format_buffer` call without a `LineRange` (performance
  optimisation) but the visible behaviour MUST be identical.
- **Range with `start.line > end.line`** (inverted): the LSP
  server logs an `ERROR`-level record (per T3 FR-020d–FR-020f
  stderr logging) and responds with `null` (no edits). The
  JSON-RPC error response is NOT used — `null` is the LSP
  convention for "no formatting available."
- **Concurrent edit during format**: if a `didChange` arrives after
  the format request has been dispatched but before the response
  is sent, the in-flight format is run against the version
  captured at dispatch time. The returned `TextEdit[]` is keyed to
  that captured version. LSP clients re-base text edits against
  the current buffer version per the spec; if the rebase fails
  (i.e., the buffer has diverged too far), the client drops the
  edits and the user re-invokes "Format Document." T5 does NOT
  attempt server-side rebase or retry.
- **Cancellation during format**: `$/cancelRequest` (already wired
  by T3) targets the formatting request by ID. The formatting
  handler MUST poll its cancellation token at a coarse-grained
  point — at minimum, between the `DirectiveSplitter` pre-pass
  and the per-fragment lex+parse loop — and abort with
  `RequestCancelled` (`-32800`) per T3 FR-020h–FR-020j if signalled.
  Mid-`LayoutPlanner` cancellation is NOT required at T5; the
  granularity matches T3's `foldingRange` worker.
- **`didClose` arrives while format is in flight**: per FR-014b
  (clarified Session 2026-05-12), the in-flight format completes
  along its normal path and the protocol layer sends the response
  (`TextEdit[]` on Success, `null` per FR-007 on Refused/Error);
  the editor discards the response because the document is
  closed. Wasted CPU is bounded by SC-004's 300 ms budget. The
  `Server.cpp` `onDidClose` handler is NOT modified to signal the
  formatting handler — that would couple `onDidClose` to
  `Formatting.cpp` and violate FR-010's architectural seam. The
  explicit `$/cancelRequest` path remains the only mechanism that
  aborts an in-flight format.
- **`FormattingOptions.tabSize` / `insertSpaces` provided by client**:
  the LSP client sends `FormattingOptions {tabSize, insertSpaces, ...}`
  on every formatting request (required field per LSP 3.16). Per
  the FR-005 resolution (Session 2026-05-12 — TOML wins), the
  server reads and discards this object. Its content does NOT
  influence the formatter's `Configuration`. The user setting
  `tabSize = 2` in their VS Code preferences has zero effect on
  `.nsl` formatting; the project's `.nsl-fmt.toml` is authoritative.
- **No `.nsl-fmt.toml` discoverable from the document's URI**: the
  server uses `nsl::fmt::default_configuration()` (the T2-defined
  built-in defaults, per T2 FR-014 / [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
  §5.1). The client's `FormattingOptions` is still discarded per
  FR-005 — there is no fallback path that consults it.
- **Malformed `.nsl-fmt.toml`** (syntactic TOML error OR semantic
  error like `indent = "potato"`, `max_line_length = -1`, unknown
  key): per FR-005c (clarified Session 2026-05-12), the server
  falls back to `default_configuration()` for the current format
  request and emits a `publishDiagnostics` against the TOML file's
  URI carrying the `parse_config_file` diagnostics (with
  `source = "nsl-fmt"`). The format request response is a normal
  `TextEdit[]` — the TOML breakage is decoupled from the format
  response so the user can keep working while they fix the config.
- **Document URI uses a non-file scheme** (e.g., `untitled:` for
  unsaved buffers): the server cannot run `discover_config` —
  there is no filesystem path. The server uses
  `nsl::fmt::default_configuration()` directly and proceeds.
- **`textDocumentSync.change` is `Full`** (per T3 §1.2): the server
  already has the current buffer text in `NslTU.current.contents`.
  Formatting uses that buffer directly — there is no need to
  re-read from disk, and the URI's filesystem state is irrelevant
  to the format input (only to TOML discovery).
- **Formatter timing exceeds the cancellation poll interval**:
  per T2 SC-003, formatting a 1000-line NSL file completes in
  under 250 ms on dev-container hardware. The LSP wrapper's
  overhead (JSON-RPC framing + TextEdit conversion) is bounded by
  SC-004 below at +50 ms. A `$/cancelRequest` arriving at any
  point during the request observes the cancellation token at the
  next poll point.
- **`textDocument/willSave` / `willSaveWaitUntil`**: these
  capabilities are explicitly NOT advertised by T5 — only
  `documentFormattingProvider` and `documentRangeFormattingProvider`
  are added. Editors that use willSaveWaitUntil for "format on
  save" fall back to client-side formatting (i.e., send a regular
  `textDocument/formatting` from a `BufWritePre` autocmd or
  before-save-hook). This matches `clangd` / `rust-analyzer`
  practice.
- **TextEdit shape on success**: the server returns a single
  `TextEdit` (array length 1) whose `range` covers the entire
  input (whole-buffer formatting) or the expanded whole-line
  range (range formatting), and whose `newText` is the
  formatter's output (per FR-006 — clarified Session 2026-05-12).
  On already-canonical input, the response is an empty array
  (length 0). The server does NOT emit a minimal Myers-diff
  `TextEdit[]` at T5; the minimal-diff path is forward-compatible
  and may land at a later T-track polish milestone.

## Requirements *(mandatory)*

### Functional Requirements

#### LSP method surface

- **FR-001**: `nsl-lsp` MUST handle the LSP request
  `textDocument/formatting` per LSP 3.16 base protocol. The
  request's `DocumentFormattingParams` carries `textDocument.uri`
  (the document to format) and `options` (FormattingOptions —
  `tabSize`, `insertSpaces`, plus client-extensible keys). The
  response MUST be either a `TextEdit[]` (success — possibly
  empty) or `null` (no formatting available; see FR-007).
- **FR-002**: `nsl-lsp` MUST handle the LSP request
  `textDocument/rangeFormatting` per LSP 3.16 base protocol. The
  request's `DocumentRangeFormattingParams` adds `range` (the
  selection — zero-based `Position` start/end in UTF-16 code units
  per T3 FR-004). The response MUST be either a `TextEdit[]`
  (success — possibly empty) or `null` (no formatting available;
  see FR-007).
- **FR-003**: For `textDocument/rangeFormatting`, the server MUST
  expand the request's `Range` to whole-line boundaries before
  forwarding to `format_buffer` (snap `start` down to
  start-of-line, `end` up to end-of-line). The resulting
  whole-line span is converted to a 1-indexed `nsl::fmt::LineRange`
  per the T2 format-api contract §3. Out-of-bounds endpoints are
  clamped to the document's first/last line; an inverted range
  (`start > end`) is rejected with a `null` response and an
  `ERROR`-level stderr log record.
- **FR-004**: The `InitializeResult.capabilities` JSON MUST add
  exactly two new entries to the T3 contract's §1.2 canonical
  shape: `documentFormattingProvider: true` and
  `documentRangeFormattingProvider: true`. No other capability
  changes. The amendment MUST land in the same PR as the T5
  implementation (Principle VII coupling). The T3
  `lsp-protocol.contract.md` §1.2 JSON MUST be updated in the
  same change.

#### Configuration resolution

- **FR-005**: For each formatting request, the server MUST resolve
  `nsl::fmt::Configuration` exclusively from the project-level
  configuration source (clarified Session 2026-05-12 — TOML wins).
  Specifically: (a) if `.nsl-fmt.toml` is discoverable per FR-005a,
  the resolved `Configuration` is the result of
  `nsl::fmt::parse_config_file(...)` on that file; (b) otherwise,
  the resolved `Configuration` is `nsl::fmt::default_configuration()`.
  The LSP `FormattingOptions` object received in the request's
  `params.options` field MUST be ignored for the purposes of
  computing the `Configuration` — the server reads and discards it
  to satisfy the LSP 3.16 protocol shape, but its content does
  NOT influence the formatter's output. The resolved
  `Configuration` MUST be byte-identical to what `nsl-fmt --stdin`
  would compute for the same document path on disk; SC-005's
  byte-equivalence guarantee depends on this.
- **FR-005a**: TOML discovery MUST use `nsl::fmt::discover_config`
  per the T2 format-api contract §4 — walk upward from the
  directory containing the open document's URI. For document URIs
  that have no filesystem path (e.g., `untitled:` scheme), TOML
  discovery is skipped and `nsl::fmt::default_configuration()`
  applies.
- **FR-005b**: TOML discovery results MUST NOT be cached across
  requests at T5. (Caching is a T9-or-later concern when
  `workspace/didChangeWatchedFiles` lands; pre-T9, each request
  redoes the filesystem walk, which is fast — `discover_config` is
  O(directory depth) and one stat call per ancestor.)
- **FR-005c**: When `parse_config_file` returns either
  `Status::Refused` (syntactic TOML error) or `Status::Error`
  (semantic error — invalid value, unknown key, range violation)
  for a discovered `.nsl-fmt.toml`, the server MUST (clarified
  Session 2026-05-12 — fall back to defaults + diagnostic):
  (a) use `nsl::fmt::default_configuration()` as the resolved
  `Configuration` for the current format request and proceed
  normally per FR-001 / FR-002 / FR-006;
  (b) emit one `textDocument/publishDiagnostics` notification
  whose `uri` is the TOML file's `file://` URI (NOT the .nsl
  document's URI), carrying every diagnostic from
  `FormatResult.diagnostics` mapped to LSP `Diagnostic` shape via
  the existing T3 diagnostic-mapping seam — same conversion path
  as Sema and parse-error diagnostics, with `source` set to
  `nsl-fmt` to disambiguate origin;
  (c) emit one `WARN`-level stderr log record per FR-020d–FR-020f
  identifying the TOML file path and the first diagnostic message.
  The format request response itself MUST NOT signal the TOML
  error — the response is a normal `TextEdit[]` per FR-006 (or
  `null` per FR-007 if the .nsl document also fails to parse for
  its own reasons). This decoupling lets the user fix their TOML
  in a separate editor tab while continuing to format `.nsl`
  files.

#### Output shape

- **FR-006**: On `Status::Success` from `format_buffer`, the
  server MUST construct the `TextEdit[]` response as a single
  `TextEdit` (array length exactly 1) whose `range` covers the
  entire formatted span and whose `newText` is the entire
  formatter output verbatim (clarified Session 2026-05-12 —
  single whole-span TextEdit). Specifically:
  - For `textDocument/formatting`: `range.start = {line: 0,
    character: 0}` and `range.end = {line: <documentLineCount>,
    character: 0}` (a position one past the last line per LSP
    convention for end-of-document); `newText = formattedText`.
  - For `textDocument/rangeFormatting`: `range` is the
    whole-line-snapped span per FR-003 (start of `firstLine`,
    end of `lastLine + 1`); `newText` is the formatter's output
    for that line range (per `nsl::fmt::format_buffer`'s
    `LineRange`-restricted output, which already preserves
    out-of-range lines byte-identical).
- **FR-006a**: The server MUST satisfy: (i) applying the
  `TextEdit[]` to the original buffer produces a buffer
  byte-identical to `format_buffer`'s `formattedText`; (ii) on
  idempotent input (already-canonical), the `TextEdit[]` MUST be
  empty (no-op response — array length 0, not a length-1 array
  whose `newText` equals the input — to spare the editor from
  applying a no-op edit and disrupting cursor state); (iii) the
  trailing-newline guarantee from T2 (`formattedText` ends with
  exactly one `\n` on non-empty output) is preserved through the
  edit.
- **FR-007**: On `Status::Refused` from `format_buffer` (parse
  error), the server MUST respond with **`null`** (clarified
  Session 2026-05-12 — return null on refusal). This is the LSP
  3.16 convention for "no formatting available for this document"
  — the editor leaves the buffer untouched, and the user sees
  the existing parse-error diagnostic via the T3
  `publishDiagnostics` channel (no double-reporting). The server
  MUST NOT emit a JSON-RPC error response (e.g., `-32603
  InternalError`) on parse-error refusal — that would produce a
  popup in most editors and add no information beyond what
  `publishDiagnostics` already surfaces. The same `null` response
  MUST be used for `Status::Error` outcomes (range-out-of-bounds,
  internal failure, etc.). The distinction between `Refused` and
  `Error` is captured in the stderr log record per FR-015, not
  on the wire.
- **FR-008**: On `Status::Success` with an empty `formattedText`
  (empty document input), the server MUST return an empty
  `TextEdit[]` (not `null` — the document IS formattable, just
  contains nothing).

#### Architecture and reuse

- **FR-009**: `nsl-lsp` MUST link `libNslFmt.a` in addition to
  `libNSLFrontend.a` (already linked by T3). The formatter logic
  is reused, not duplicated, satisfying Constitution Principle II.
  The linker map for `nsl-lsp` MUST resolve every
  layout-engine / Doc-IR / Wadler-Leijen / TOML-parser /
  CST-walker symbol into `libNslFmt.a`.
- **FR-010**: The two new handlers MUST be implemented in a single
  new source file `lib/LSP/Features/Formatting.cpp` (per
  [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
  §8). The file MUST NOT modify the JSON-RPC transport
  (`Transport.cpp`), the TUScheduler (`Scheduler.cpp`), the
  document-sync handlers (`Server.cpp`'s `onDidOpen` /
  `onDidChange` / `onDidClose`), or the diagnostic-mapping seam.
  Allowed edits to `Server.cpp` are limited to: (a) capability
  advertisement in `onInitialize`, (b) registry entries in the
  dispatch table for the two new methods.
- **FR-011**: The handlers MUST execute on the TUScheduler worker
  pool — same scheduler T3 uses for `foldingRange` — to ensure
  that a long-running format request does not block the protocol
  thread or other documents' diagnostics. Reuse is structural; T5
  does NOT introduce a parallel thread pool.
- **FR-012**: The handlers MUST receive a per-request
  cancellation token from the protocol layer (per T3 FR-020i) and
  MUST poll the token at, at minimum, the boundary between the
  `DirectiveSplitter` pre-pass and the per-fragment lex+parse
  loop. On cancellation, the response MUST carry JSON-RPC error
  code `RequestCancelled` (`-32800`) per T3 FR-020i.

#### Position-encoding seam

- **FR-013**: Per T3 FR-004's UTF-16 position pin, the server MUST
  treat all `Position` and `Range` values on the wire as UTF-16
  code-unit offsets. For ASCII-only NSL source (the predominant
  case in the audited corpus), UTF-16 offsets equal byte offsets
  and the conversion is a no-op. For non-ASCII content (UTF-8
  comments, string literals), the byte-offset arithmetic that
  `nsl::fmt::LineRange` and `format_buffer` use internally MUST
  agree with the LSP-side UTF-16 arithmetic at line granularity
  (line numbers are encoding-agnostic). The whole-line snapping
  in FR-003 sidesteps the column-encoding issue.

#### Source document handling

- **FR-014**: The handlers MUST use `NslTU.current.contents` as
  the input to `format_buffer` (the same buffer T3's diagnostics
  pipeline uses). The handlers MUST NOT re-read the document from
  disk. The handlers MUST NOT format a document the server has
  never seen via `didOpen` — the response is `null` in that case,
  plus a `WARN`-level stderr log record.
- **FR-014a**: The handlers MUST tag the captured `NslTU.version`
  at dispatch time and key the response to that version. If a
  `didChange` arrives between dispatch and response, the in-flight
  format continues against the captured version; the LSP client
  is responsible for rebasing the returned `TextEdit[]` against
  the buffer's current state (LSP spec convention). The server
  does NOT cancel the in-flight format on `didChange` (distinct
  from the `$/cancelRequest` path, which is explicit).
- **FR-014b**: If a `textDocument/didClose` notification arrives
  for a URI while a formatting request for that URI is in flight
  (clarified Session 2026-05-12 — let in-flight format complete),
  the in-flight format MUST proceed along its normal path. The
  protocol layer MUST send the response (`TextEdit[]` on Success,
  `null` per FR-007 on Refused/Error) regardless of close state;
  the LSP client is expected to discard responses for closed
  documents per the LSP convention. The `Server.cpp` `onDidClose`
  handler MUST NOT be modified to signal the formatting handler's
  cancellation token — that coupling would violate FR-010's
  architectural seam (FR-010 forbids `onDidClose` edits). The
  explicit `$/cancelRequest` path per FR-012 remains the only
  mechanism that aborts an in-flight format.

#### Logging and observability

- **FR-015**: The handlers MUST emit log records per T3 FR-020d–
  FR-020f stderr logging: every formatting request received
  (level `INFO` with URI and elapsed time on completion); every
  `Refused` outcome (level `INFO` with parse-error count); every
  internal exception (level `ERROR`); every cancellation (level
  `INFO`).
- **FR-016**: The handlers MUST NOT log the formatted output text
  body or the input source body at any level (matches T3
  FR-020f's source-content exclusion).

#### Test discipline (Principle VI)

- **FR-017**: An LSP-layer integration test MUST drive `nsl-lsp`
  via stdin/stdout JSON-RPC and exercise both new methods:
  - **FR-017a** (`textDocument/formatting`): lifecycle
    (`initialize` → `initialized` → `didOpen` with non-canonical
    fixture → `textDocument/formatting`) → assert the returned
    `TextEdit[]`, when applied to the original buffer, produces a
    string byte-identical to the matching `nsl-fmt --stdin <
    fixture.nsl` golden output. Repeat for the
    already-canonical input (empty `TextEdit[]`), the empty
    document, and the parse-error input (`null` per FR-007 or
    error per FR-007's chosen alternative).
  - **FR-017b** (`textDocument/rangeFormatting`): lifecycle →
    `didOpen` with a multi-line fixture →
    `textDocument/rangeFormatting` with a middle-of-file `Range`
    → assert the returned `TextEdit[]` matches `nsl-fmt --stdin
    --range L1:L2 < fixture.nsl`, plus the surrounding lines are
    untouched. Repeat for the mid-line range (whole-line snap),
    the out-of-bounds range (clamp), and the inverted range
    (`null` per FR-003).
- **FR-018**: A CLI ↔ LSP parity test MUST assert that for at
  least one fixture per NSL-specific rule in
  [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
  §5.3 (six rules), the LSP `textDocument/formatting` response
  applied to the input buffer is byte-identical to
  `nsl-fmt --stdin < fixture.nsl`. This is the SC-005 verification
  test.
- **FR-019**: A capability-advertisement test MUST assert the
  exact `InitializeResult.capabilities` JSON shape per FR-004 —
  byte-equality with the new canonical JSON in the amended T3
  `lsp-protocol.contract.md` §1.2. This is the
  `lifecycle_test::CapabilitiesExact` test from T3's contract,
  extended with the two new entries.
- **FR-020**: A cancellation integration test MUST exercise the
  end-to-end seam: send `textDocument/formatting` against an
  artificially-large fixture (constructed to push past the
  handler's polling threshold), immediately follow with a
  `$/cancelRequest`, and assert the response carries error code
  `RequestCancelled` (`-32800`) within an upper bound of 250 ms.
  Mirrors T3's SC-010 cancellation test for `foldingRange`.
- **FR-021**: All T5 integration tests MUST run in the project's
  existing CI matrix (`scripts/ci.sh`) on every PR and MUST fail
  the run on any assertion failure. Determinism applies
  (Principle V): two consecutive runs over the same input MUST
  produce byte-identical `TextEdit[]` payloads.

#### Boundaries (what T5 does NOT do)

- **FR-022**: `nsl-lsp` MUST NOT implement
  `documentOnTypeFormattingProvider` (LSP `textDocument/
  onTypeFormatting` — format-on-typing). This method is not on
  the [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
  §3.2 list. Adding it would require a per-keystroke formatter
  invocation that the Wadler-Leijen layout engine is not tuned
  for; it is a forward-looking extension, not a T5 deliverable.
- **FR-023**: T5 MUST NOT introduce any new LSP method outside
  `textDocument/formatting` and `textDocument/rangeFormatting`.
  All other deferred methods (`hover`/`definition`/`documentSymbol`/
  `semanticTokens`/`signatureHelp` → T4; `references`/`completion`/
  `rename`/`codeAction` → T9; `inlayHint`/
  `prepareCallHierarchy` → T10) stay deferred.
- **FR-024**: T5 MUST NOT introduce its own formatter code paths.
  All layout decisions go through `nsl::fmt::format_buffer` from
  `libNslFmt.a`. No alternative configuration parser, no
  alternative pretty-printer, no formatter-private CST walker
  inside `nsl-lsp`. Verifiable by inspecting the diff for any new
  layout-engine code outside `Formatting.cpp`'s thin wiring.
- **FR-025**: T5 MUST NOT implement workspace-level configuration
  methods (e.g., `workspace/didChangeConfiguration`,
  `workspace/configuration`). TOML discovery uses the per-document
  filesystem walk per FR-005a; workspace-level configuration
  remains deferred to T9 (when `references` introduces the
  cross-file requirement).

### Key Entities

- **LSP formatting handler (`onFormatting`)** — the new handler
  registered on the `NslLSPServer` dispatch table for
  `textDocument/formatting`. Lives in `lib/LSP/Features/
  Formatting.cpp`. Calls `nsl::fmt::format_buffer` with no
  `LineRange` and converts the result to `TextEdit[]`.
- **LSP range-formatting handler (`onRangeFormatting`)** — the
  new handler for `textDocument/rangeFormatting`. Same file.
  Calls `format_buffer` with a `LineRange` derived from the
  request's `Range` (per FR-003 whole-line snap + clamp).
- **Configuration resolver** — the per-request routine that
  combines `discover_config` and, if a TOML is found,
  `parse_config_file`; otherwise falls back to
  `default_configuration`. On a malformed-TOML outcome, falls
  back to `default_configuration` AND emits a `publishDiagnostics`
  against the TOML URI per FR-005c. Implements FR-005 (TOML wins
  — Session 2026-05-12; LSP `FormattingOptions` is read and
  discarded) and FR-005c (malformed TOML → fall back + diagnostic
  — Session 2026-05-12). Stateless; called once per request. (Per
  FR-005b, no cross-request caching at T5.)
- **TextEdit converter** — the routine that takes
  `FormatResult::formattedText` and produces the LSP
  `TextEdit[]` response per the FR-006 shape. Stateless; pure
  function of `(originalBuffer, formattedText, range)`.
- **NslTU (existing T3 entity, reused unchanged)** — per
  [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
  §3.3 and T3's `Server.h` definition. The new handlers read
  `current.contents` and `current.version` via the TUScheduler's
  `withAST(...)` callback (or the equivalent withCurrentState
  accessor — T3 contract terminology).
- **LSP integration test harness (existing T3 entity, extended)**
  — lives under `test/lsp/` per T3 spec. T5 adds new fixture
  directories (`test/lsp/formatting/`, `test/lsp/rangeFormatting/`)
  containing paired `(input.nsl, expected.json)` and
  `(input.nsl, expected-formatted.nsl)` files.
- **`lsp-protocol.contract.md` §1.2 (existing T3 contract,
  amended)** — the canonical `InitializeResult.capabilities` JSON
  gains two entries (`documentFormattingProvider: true` and
  `documentRangeFormattingProvider: true`) in the same PR. The
  `lifecycle_test::CapabilitiesExact` assertion is updated to
  match.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The T5 integration test from FR-017a passes on a
  clean checkout in CI: opening a fixture file with known
  non-canonical whitespace and sending `textDocument/formatting`
  produces a `TextEdit[]` whose application to the buffer
  yields output byte-identical to the same fixture's CLI
  golden file. This is the test gate stated verbatim in
  [`README.md`](../../README.md) §Roadmap row T5 ("Format on
  save in editor produces same output as `nsl-fmt` CLI.").
- **SC-002**: The FR-017b integration test passes: for every
  middle-of-file `Range` in a multi-line fixture, the response
  matches `nsl-fmt --stdin --range L1:L2`, and every line
  outside the range is byte-identical to the input. Verified
  across at least three fixtures (small/medium/large file size).
- **SC-003**: 100% of the six NSL-specific formatting rules from
  [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
  §5.3 round-trip from `nsl::fmt::format_buffer` through the LSP
  `TextEdit[]` response with byte-identical output to the CLI —
  verified by parameterized FR-018 parity tests.
- **SC-004**: Time from `textDocument/formatting` arrival to
  `TextEdit[]` response emission MUST be under **300 ms** for an
  audited-corpus-sized NSL file (≤ 1500 lines) on a standard CI
  runner. (Composes T2 SC-003's 250 ms budget for
  `format_buffer` itself plus T5's LSP wrapper overhead. Files
  larger than 1500 lines are out of scope for the SC-004 budget.)
- **SC-005**: For every fixture in `test/lsp/formatting/`, the
  formatted output produced by the LSP server is byte-identical
  to `nsl-fmt --stdin < fixture.nsl`. The diff between the two
  captured outputs MUST be empty. This is the
  CLI-vs-LSP-byte-equivalence guarantee that
  [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
  §10's "Save-on-format that produces byte-identical output"
  bullet anchors.
- **SC-006**: Determinism — when the same fixture is opened
  twice across two separate `nsl-lsp` runs over identical input,
  the `textDocument/formatting` response payloads are
  byte-identical (Principle V; mirrors T3 SC-003).
- **SC-007**: The `nsl-lsp` binary links exactly one instance of
  `libNslFmt.a` (verifiable via the linker map): every layout
  engine, Doc-IR, Wadler-Leijen, TOML-parser, and CST-walker
  symbol resolves into the shared library, none into duplicated
  `nsl-lsp`-private code. (Constitution Principle II enforced
  structurally; the same structural check T3 SC-005 anchored for
  `libNSLFrontend.a`.)
- **SC-008**: The T5 PR diff against the T3 baseline confirms
  the User Story 4 architectural-seam claim: zero edits to
  `Transport.cpp`, `Scheduler.cpp`, or `Server.cpp`'s
  `onDidOpen` / `onDidChange` / `onDidClose` handlers. The only
  `Server.cpp` edits are the capability advertisement in
  `onInitialize` and the dispatch-table entries for the two new
  methods. (Verifiable at T5 merge; rolls forward the T3 SC-006
  "future contributor must not modify lifecycle" promise.)
- **SC-009**: Capability advertisement is exact — the
  `InitializeResult.capabilities` JSON declares exactly
  `{documentFormattingProvider: true,
  documentRangeFormattingProvider: true, foldingRangeProvider:
  true, textDocumentSync: …}` and no others. The
  `lifecycle_test::CapabilitiesExact` test (amended from T3) is
  the assertion. (Principle VII coupling at T5 merge — any
  capability addition by a later T-track milestone updates this
  test in the same PR.)
- **SC-010**: The cancellation integration test from FR-020
  exercises the end-to-end seam: `textDocument/formatting`
  followed immediately by `$/cancelRequest` returns a response
  with error code `RequestCancelled` (`-32800`) within an upper
  bound of 250 ms. Mirrors T3 SC-010 for `foldingRange`.
- **SC-011**: The combined T5 integration tests (FR-017a +
  FR-017b + FR-018 parity + FR-019 capabilities + FR-020
  cancellation) complete in under **60 seconds** on a standard
  CI runner. Together with T3's existing tests (< 30 seconds per
  T3 SC-007), the LSP test layer stays well under the 5-minute
  CI budget [`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md)
  §14 anchors.

## Assumptions

### Prerequisites delivered (no clarification needed)

- **T2 is delivered**, including the frozen 10-symbol
  `Fmt.h` public API per
  [`format-api.contract.md`](../010-t2-formatter-v0/contracts/format-api.contract.md).
  `nsl::fmt::format_buffer` accepts an optional `LineRange`
  parameter — the exact shape T5 needs. T5 consumes this API
  surface unchanged; if a T5 implementation detail forces a
  signature change to the T2 contract, that is itself a
  Principle VII coupling concern and requires a coordinated
  amendment to both contracts in the same PR.
- **T3 is delivered**, including the JSON-RPC transport,
  TUScheduler, document-sync handlers, diagnostic-mapping seam,
  folding-range handler, `$/cancelRequest` plumbing,
  `NSL_LSP_LOG_LEVEL` stderr logging, and the
  `lsp-protocol.contract.md` capabilities JSON.
- **NslServer / NslLSPServer split.** Per the design doc class
  diagram (§3.4), the language-logic layer (`NslServer`) exposes
  the format-region seam promised by T3 FR-019 ("`+formatRange(…)`"
  on the class diagram); the protocol layer (`NslLSPServer`)
  dispatches the two new methods to that seam. T5 implements the
  seam's body (or removes the seam's stub if T3 left one); T3
  promised the architectural shape, not the runtime
  implementation.

### Reasonable defaults adopted (no clarification needed)

- **`textDocument/willSaveWaitUntil` is NOT advertised.** Editors
  that use willSaveWaitUntil for format-on-save fall back to
  client-side formatting (a `BufWritePre`/`before-save-hook`
  autocmd that issues a regular `textDocument/formatting`).
  Matches `clangd` / `rust-analyzer` practice. Adding
  willSaveWaitUntil at a later milestone is a forward-compatible
  capability addition; it is not needed for the SC-001 test gate.
- **`documentOnTypeFormattingProvider` is NOT advertised.** Same
  rationale as FR-022; the Wadler-Leijen layout engine in
  `libNslFmt.a` is not tuned for per-keystroke invocation, and
  the method is not on the
  [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
  §3.2 list.
- **TextEdit shape**: single whole-span `TextEdit` per FR-006
  (clarified Session 2026-05-12). Rationale: (i) byte-identical
  with `format_buffer`'s output by construction (no Myers-diff
  approximation errors), (ii) minimal code (the converter is one
  allocation + one `TextEdit{}`), (iii) the cursor-disruption
  argument for Myers-diff is weak for whole-document formatting
  (most editors preserve cursor anyway). The minimal-diff path
  can be added at a later T-track polish milestone without
  breaking the FR-006a contract.
- **Refusal response**: return `null` per FR-007 (clarified
  Session 2026-05-12). Rationale: the parse error already
  surfaces via the T3 `publishDiagnostics` channel; emitting an
  LSP error popup in addition would be double-reporting and
  worsen UX. The `null` response matches `clangd`, `rust-analyzer`,
  and `gopls` practice on this path.
- **Range whole-line snapping**: per FR-003, mid-line ranges are
  expanded to whole-line boundaries before forwarding to
  `format_buffer`. NSL formatting is line-granular by design (the
  Wadler-Leijen pretty-printer operates on line-level breaks);
  column-granular range formatting would require a second
  formatter mode that is not on the design doc's roadmap.
- **TOML discovery is per-request, not cached**: per FR-005b.
  Caching is deferred to T9 with workspace methods.
- **Build target**: T5 builds inside the
  `ghcr.io/koyamanx/nsl-nslc:dev` container only (per project
  memory `project_build_environment.md`); host-machine builds
  are not a supported configuration.

### Scope boundaries

- **Format-on-typing is OUT of scope.** Per FR-022; if it lands,
  it lands at a later T-track milestone.
- **Workspace-level configuration is OUT of scope.** Per FR-025;
  TOML discovery is per-document at T5.
- **Editor extension packaging is OUT of scope.** Per T3 FR-028;
  the VS Code extension shell that consumes WASM tree-sitter is
  a T8 deliverable, and Neovim / Emacs / Sublime packaging is a
  T11 deliverable. T5 ships server-side wiring only; editors
  consume `nsl-lsp` via the manual smoke-test recipe T3
  documents.
- **Save-time hooks (`textDocument/didSave`) are NOT relied on.**
  The format-on-save flow runs entirely through the existing
  `textDocument/formatting` request issued by the editor's
  `BufWritePre` / `before-save-hook` autocmd. `didSave` is
  unrelated to formatting at T5.

### Dependencies on existing systems

- **Reuses `libNslFmt.a`** (per FR-009 / FR-024 — the entire
  formatter logic) and **`libNSLFrontend.a`** (per T3 FR-018 —
  already linked by the LSP server). T5 does NOT introduce a
  third library; the [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
  §7 "LSP composes shared libraries" picture is preserved.
- **Reuses T3's JSON-RPC transport, TUScheduler, lifecycle
  handlers, cancellation plumbing, and stderr logging.** Per
  FR-010 / FR-011 / FR-012 / FR-015. T5 does NOT modify any of
  these layers; it only adds handlers.
- **CI runs in GitHub Actions** with the existing clang and gcc
  cells (per the M0 reproducibility-CI scaffolding). The T5
  integration tests run in the existing `scripts/ci.sh` matrix
  (per FR-021).
