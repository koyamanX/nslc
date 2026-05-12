<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Phase 0 Research: T5 — LSP Formatting Integration

**Branch**: `011-t5-lsp-formatting` | **Date**: 2026-05-12

This document records the technology / approach decisions taken
before Phase 1 design, with rationale and rejected alternatives.
Decisions are referenced by ID (`R1`, `R2`, …) from `plan.md`,
the contract files, and `tasks.md`.

T5 inherits the bulk of its architectural choices from T2
(formatter) and T3 (LSP skeleton); this research focuses on the
seam between the two and the decisions that surfaced during
`/speckit-clarify` (Session 2026-05-12).

---

## R1: Handler placement — one file or two?

**Decision**: One C++ source file
(`lib/LSP/Features/Formatting.cpp`) implements both
`textDocument/formatting` and `textDocument/rangeFormatting`
handlers. Internal helpers — the Configuration resolver, the
LineRange computer, the TextEdit constructor — are file-static
and reused between the two handlers.

**Rationale**:

- The two methods share ~90% of their implementation. They differ
  only in (a) whether `LineRange` is passed to `format_buffer`
  and (b) the source of that range (the request's `Range` field
  vs. omitted entirely). Splitting them into two files would
  duplicate the Configuration resolver, the TOML-error
  side-channel emission, the response constructor, and the
  cancellation-token polling logic.
- The design-doc convention
  [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
  §8 names exactly `lib/LSP/Features/Formatting.cpp` — singular —
  for the formatting group. Keeping one file per method group
  matches the precedent and aligns with `clangd`'s pattern
  (`lib/clangd/SemanticHighlighting.cpp` covers both `tokens`
  and `tokens/delta`; `Format.cpp` covers `formatting` and
  `rangeFormatting`).
- spec FR-010 forbids edits to any T3 source file except
  `NslLSPServer.cpp` (dispatch + capability advertisement); a
  one-file deliverable satisfies the "confined to one new file"
  rule from FR-010 / User Story 4 Acceptance 1.

**Alternatives rejected**:

- **`Formatting.cpp` + `RangeFormatting.cpp`** — two files,
  duplicated helpers. Rejected for code-duplication and divergent-
  drift reasons.
- **Inline both handlers directly in `NslLSPServer.cpp`** — keeps
  the file count smaller but bloats `NslLSPServer.cpp` (which T3
  established as the dispatch + lifecycle file) and contradicts
  the design-doc's `Features/<Name>.cpp` decomposition.

---

## R2: Configuration resolver — read order, caching, malformed-TOML side effects

**Decision**: Per request, the resolver runs:

1. `nsl::fmt::discover_config(parentDirOf(documentURI))` — returns
   `std::optional<std::string>` (the TOML path) or `nullopt`.
2. If `nullopt`: resolved configuration is
   `nsl::fmt::default_configuration()`. Skip step 3.
3. Read the TOML file's bytes into a `std::string`. (T5 owns this
   filesystem read; T2's `parse_config_file` takes a buffer
   `StringRef`, not a path.) Call
   `nsl::fmt::parse_config_file(tomlBuffer, fileID, &config)`.
4. On `Status::Success`: use the parsed `config`.
5. On `Status::Refused` (syntactic TOML error) or `Status::Error`
   (semantic / range / type / unknown-key error): use
   `default_configuration()`, AND emit one
   `textDocument/publishDiagnostics` notification whose `uri` is
   the TOML file's `file://` URI carrying every diagnostic from
   `FormatResult.diagnostics` mapped via the T3 diagnostic-mapping
   seam.

**No cross-request caching** at T5 (FR-005b). Each request redoes
the filesystem walk and the TOML parse. Cost: O(directory depth)
stat calls for `discover_config`, plus one read+parse of the TOML
file if found. Both fast in practice.

**Rationale**:

- The Session 2026-05-12 clarifications fixed three policy points
  that together determine the resolver shape: Q1 → TOML wins
  (no `FormattingOptions` blending — keep the LSP `options` field
  off the resolver's inputs entirely); Q4 → malformed-TOML
  fallback + diagnostic (side-channel via `publishDiagnostics`,
  not coupled to the format response); the design-doc Wadler-
  Leijen pretty-printer accepts a single `Configuration` object
  without further fallback chains.
- Filesystem read of the TOML happens in T5 code (not T2) because
  T2's `parse_config_file` takes a buffer, not a path — by
  design (T2 spec FR-018: `parse_config_file` is a pure function
  of its buffer; filesystem access is the caller's
  responsibility). This keeps T2's API surface deterministic
  per its `format-api.contract.md` §6 "every function except
  `discover_config` is pure."
- Caching is deferred per FR-005b because at T5 there is no
  `workspace/didChangeWatchedFiles` plumbing to invalidate the
  cache when the TOML changes. Adding stale-by-default caching
  here would risk SC-006 byte-equivalence: two requests over an
  edited TOML could see different `Configuration`s, which would
  surface as a determinism bug under SC-006's "two runs over the
  same input" condition. Pre-T9, the per-request walk is fast
  enough.

**Alternatives rejected**:

- **Cache `Configuration` per document URI** — fast for repeated
  requests but stale on TOML edits without a file-watch
  invalidation channel. Defer to T9.
- **Cache `discover_config` result only** — partial caching, more
  complex than no caching. Defer.
- **Have T2 grow a `parse_config_path(StringRef path)` overload**
  — would push the filesystem read into `libNslFmt.a` and break
  T2's pure-function invariant. Rejected as a Principle II /
  Principle V cost not justified by T5's request frequency.
- **Refuse the format request on malformed TOML** — Session
  2026-05-12 Q4 explicitly considered and rejected this
  (Option C). The TOML-fallback path keeps the format request
  productive.

---

## R3: TextEdit shape on success — single whole-span vs. minimal diff

**Decision**: Single whole-span `TextEdit`. The response is a
`TextEdit[]` of length exactly 1 (on changed input) or 0 (on
already-canonical input). The single edit's `range` covers
`{start: (0, 0), end: (documentLineCount, 0)}` for
`textDocument/formatting`, or the whole-line-snapped range for
`textDocument/rangeFormatting`; `newText` is `FormattedText`
verbatim.

**Rationale**:

- Session 2026-05-12 Q2 resolved this to Option A (single
  whole-span) for reasons codified in the Assumptions block:
  byte-equivalent with `format_buffer`'s output by construction;
  ~5 lines of converter code; minimal-diff complexity adds
  divergence-drift risk against the FR-006a idempotence
  guarantee.
- LSP precedent supports either shape. clangd emits multiple
  TextEdits (it has access to clang-format's internal Replacement
  list, which is already a sparse diff). gopls emits a single
  whole-document TextEdit on `textDocument/formatting`
  (`server.go:Formatting()` → `source.AllImportsFixes()` →
  `computeTextEdits()` returns one edit for the whole file).
  rust-analyzer is similar to gopls. Both whole-span and minimal-
  diff are accepted LSP idioms; the FR-006a idempotence and
  trailing-newline constraints are what tip the choice toward
  whole-span here.
- The FR-006a (ii) edge — already-canonical input → empty
  `TextEdit[]` (not a length-1 edit with `newText == oldText`) —
  is a deliberate UX nicety: editors apply non-empty edits even
  when they're no-ops, which can disrupt cursor position or
  trigger autosave; the empty array signals "no work needed."

**Alternatives rejected**:

- **Minimal Myers-diff `TextEdit[]`** — Session 2026-05-12 Q2
  Option B. Adds complexity (the converter computes line-level
  diffs); risks idempotence drift (a small approximation in the
  diff can produce a non-idempotent result that nonetheless
  applies-to-correct output, masking the bug). Forward-compatible:
  a later T-track polish milestone may switch the converter
  output without changing the FR-006a contract.
- **Mixed strategy** (Q2 Option C — whole-span for `formatting`,
  Myers-diff for `rangeFormatting`) — two code paths to test.
  Not worth the maintenance cost at T5.

---

## R4: Refusal response — `null` vs. JSON-RPC error

**Decision**: Return `null` on every non-success outcome —
`Status::Refused` (parse error), `Status::Error`
(range-out-of-bounds, internal failure). Never use a JSON-RPC
error response (no `-32603 InternalError`, no custom server-
specific code) for these paths.

**Rationale**:

- Session 2026-05-12 Q3 resolved this to Option A. The parse
  error already surfaces via the T3 `publishDiagnostics`
  channel; emitting a JSON-RPC error response would produce a
  popup in most editors and double-report the same information.
- clangd, gopls, and rust-analyzer all return `null` on this
  path. The LSP base protocol §16.8 says "a server can decide
  to provide formatting on save and the client decides whether
  to apply the result" — i.e., `null` is the canonical "not
  available" response, distinct from "an error happened."
- The `Refused` vs. `Error` distinction is preserved in the
  stderr log record per FR-015 (one log line per request with
  the elapsed time and outcome classification), so debugging is
  not impaired.

**Alternatives rejected**:

- **JSON-RPC error on `Refused`** — Session 2026-05-12 Q3
  Option B. UX cost (popup) without information gain over the
  existing diagnostic channel.
- **Empty `TextEdit[]` on `Refused`** — Q3 Option C. Conflates
  "no work needed" (already canonical) with "couldn't format"
  (parse error). Debug-confusion cost; rejected.

---

## R5: Range computation — whole-line snap, clamp, inverted-range rejection

**Decision**: The range handler computes a 1-indexed
`nsl::fmt::LineRange` from the request's zero-based `Position`
start/end via this exact procedure:

```text
function computeLineRange(request_range, document_line_count):
    # 1. Snap to whole-line boundaries (column index ignored).
    first_line = request_range.start.line + 1            # 0-based → 1-based
    if request_range.end.character == 0:
        last_line = request_range.end.line               # end is exclusive at col 0
    else:
        last_line = request_range.end.line + 1           # end is inside the line

    # 2. Clamp to document bounds.
    first_line = max(1, min(first_line, document_line_count))
    last_line  = max(1, min(last_line, document_line_count))

    # 3. Reject inverted ranges.
    if first_line > last_line:
        return None                                      # → server replies null

    return LineRange{firstLine: first_line, lastLine: last_line}
```

**Rationale**:

- The "end is exclusive at column 0" convention matches LSP
  3.16's definition of a `Range`: a `Range` is `[start, end)` in
  document-position order, and `end.character == 0` means
  "before the first character of `end.line`" — i.e., the range
  does not include `end.line` itself. This is consistent with
  how editors render selections (selecting through the newline
  of line N puts the caret at line N+1 column 0; the visible
  highlight stops at line N).
- The clamp-then-reject ordering (clamp before checking
  inversion) ensures that an out-of-bounds range gets clamped
  to a valid range when possible (e.g., `start.line = 0, end.line
  = documentLineCount + 100` clamps to lines 1..documentLineCount,
  which is valid). Only an actually-inverted range
  (`start > end` after clamping) is rejected.
- LSP 3.16 does not specify what a server must do with an
  inverted range. Returning `null` matches the FR-007 "no
  formatting available" convention and avoids the popup risk of
  an error response.
- T2's `LineRange` precondition (per `format-api.contract.md` §4)
  requires `1 <= firstLine <= lastLine <= LineCount(buffer)`.
  The clamp ensures we never violate the precondition; the
  inverted-range rejection ensures we never call
  `format_buffer` with a `LineRange` that would itself trip the
  precondition.

**Alternatives rejected**:

- **Forward the unsnapped column-level range to a hypothetical
  column-aware formatter** — the T2 formatter does not have a
  column-aware mode (Wadler-Leijen pretty-printer is line-
  granular by design). Adding one would be a T2 contract change.
- **Reject mid-line ranges entirely** — `null` response when the
  request's range starts mid-line. UX-hostile (editors send
  mid-line ranges as a matter of course when the user selects a
  partial-line region for "Format Selection"). Snap-up is the
  expected behaviour for line-granular formatters.

---

## R6: Capability JSON amendment vs. new contract file

**Decision**: Amend the T3 contract
`specs/010-t3-lsp-skeleton/contracts/lsp-protocol.contract.md`
§1.2 in place. The new canonical `InitializeResult.capabilities`
JSON, with two new entries (`documentFormattingProvider: true`
and `documentRangeFormattingProvider: true`), replaces the T3
canonical JSON. T5 also writes its own contracts under
`specs/011-t5-lsp-formatting/contracts/` for the wire-level
behaviour of the two new methods (per Phase 1 below), but the
capability-shape contract stays anchored in T3's file —
T3 established that file as the canonical wire contract for the
LSP protocol layer, and T4/T5/T9/T10 all amend it as their
respective methods land.

**Rationale**:

- Principle VII spec/design coupling: the capability JSON is
  a single canonical shape that every T-track milestone amends.
  Forking it across multiple contract files would mean every
  later test that asserts byte-equality against the shape would
  have to know which T's contract is "current" — a maintenance
  hazard.
- T3's `lsp-protocol.contract.md` already names "T4, T5, T9, T10
  that modifies any frozen entry MUST update this contract in
  the same PR." T5 is the first such milestone to exercise that
  clause; setting the precedent of "amend in place" rather than
  "fork to a new file" matters for the T4/T9/T10 work that
  follows.
- The `lifecycle_test::CapabilitiesExact` assertion already
  reads its expected JSON from T3's contract file (via a
  fixture-extracted golden); updating the contract automatically
  updates the assertion. No T5-private capability test needed.

**Alternatives rejected**:

- **Fork a `specs/011-t5-lsp-formatting/contracts/lsp-protocol-
  capabilities-amended.contract.md`** — would require the
  lifecycle test to consult two contract files and stitch them
  together. Maintenance cost; rejected.
- **Snapshot the post-T5 capability JSON in T5's own contracts
  dir and leave T3's untouched** — same hazard as above, plus
  it would put the "current" JSON shape in a milestone-specific
  contract that future milestones don't know to consult.

---

## Summary of decisions

| ID | Decision | Anchors |
|---|---|---|
| R1 | One file for both handlers (`Features/Formatting.cpp`) | FR-010; design-doc §8 |
| R2 | Per-request Configuration resolver; no caching; malformed-TOML side-channel via `publishDiagnostics` | FR-005, FR-005a–c (Session 2026-05-12 Q1, Q4) |
| R3 | Single whole-span `TextEdit` on success; empty `TextEdit[]` on already-canonical | FR-006, FR-006a (Session 2026-05-12 Q2) |
| R4 | `null` on every non-success outcome; no JSON-RPC error response | FR-007 (Session 2026-05-12 Q3) |
| R5 | Whole-line snap (end at col 0 ⇒ exclusive); clamp before inversion check; inverted ⇒ `null` | FR-003; T2 `format-api.contract.md` §4 |
| R6 | Amend T3 `lsp-protocol.contract.md` §1.2 in place | FR-004; Principle VII; T3 contract self-reference |
