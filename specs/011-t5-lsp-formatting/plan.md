<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Implementation Plan: T5 — LSP Formatting Integration

**Branch**: `011-t5-lsp-formatting` | **Date**: 2026-05-12 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/011-t5-lsp-formatting/spec.md`

## Summary

Deliver tooling-track milestone **T5** — wire the T2 formatter
(`libNslFmt.a`) into the T3 LSP server (`nsl-lsp`) via exactly two
new LSP methods: `textDocument/formatting` (whole-document) and
`textDocument/rangeFormatting` (line-range). Per the
[`docs/CLAUDE.md`](../../docs/CLAUDE.md) §3 "Implementing the LSP"
routing entry, this milestone exists at the seam between two
delivered libraries and adds no new compiler-track functionality.

T5 ships:

1. **`lib/LSP/Features/Formatting.cpp`** (+ `.h`) — the two new
   method handlers per [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
   §8 directory layout. The implementation is ~150 LOC including
   the Configuration resolver, the TextEdit converter, and the
   request-dispatch wiring; it consumes `libNslFmt.a`'s frozen
   10-symbol public API and produces LSP `TextEdit[]` (or `null`)
   responses.
2. **One-line dispatch-table entries** in `lib/LSP/NslLSPServer.cpp`
   registering the two new methods. **Two-line capability JSON
   amendment** in `onInitialize` adding `documentFormattingProvider:
   true` and `documentRangeFormattingProvider: true`. No other
   `Server.cpp` edits permitted by spec FR-010.
3. **`test/lsp/formatting/`** + **`test/lsp/rangeFormatting/`** —
   two new fixture directories under the existing T3 LSP test
   layer. Three new integration-test binaries built on the T3
   `LspSession` harness: `formatting_test`, `range_formatting_test`,
   `format_cancellation_test`. Plus one new fixture file added to
   the existing `lifecycle_test` exercising the capability-JSON
   amendment per FR-019.
4. **In-place amendment** to
   [`specs/010-t3-lsp-skeleton/contracts/lsp-protocol.contract.md`](../010-t3-lsp-skeleton/contracts/lsp-protocol.contract.md)
   §1.2 — the canonical `InitializeResult.capabilities` JSON gains
   two new entries; the `lifecycle_test::CapabilitiesExact` byte-
   equality assertion is updated to match. This is the
   Principle VII spec/design coupling action; it lands in the same
   PR as the T5 implementation.

**Technical approach (single sentence)**: The two new LSP handlers
are thin glue around `nsl::fmt::format_buffer` — given an
`NslTU.current.contents` buffer (T3-provided) and a resolved
`nsl::fmt::Configuration` (computed once per request from
`.nsl-fmt.toml` discovery + fallback per FR-005a/c), call
`format_buffer`, convert the returned `FormattedText` to a single
whole-span LSP `TextEdit` (FR-006 — Session 2026-05-12), and emit
the response; refusals become `null` (FR-007), malformed TOML
emits a side-channel `publishDiagnostics` against the TOML URI
(FR-005c) but the format response itself is unaffected.

**Architecture decisions** (full rationale in [research.md](./research.md)):

- **Handler granularity**: one C++ source file
  (`lib/LSP/Features/Formatting.cpp`) implements both methods.
  Shared helpers — the Configuration resolver, the LineRange
  computer, the TextEdit constructor — are file-static.
  Per FR-010 the diff against the T3 baseline confirms zero
  edits to `Transport.cpp`, `Scheduler.cpp`, or the
  document-sync handlers.
- **Library boundary**: T5 adds **no new library**. `libNSLLSP.a`
  (the T3 library) grows by ~150 LOC; `libNslFmt.a` is consumed
  unchanged via its frozen 10-symbol `Fmt.h` header. The `nsl-lsp`
  binary's CMake target gains one new linked archive
  (`libNslFmt.a`); the existing `libNSLFrontend.a` link is
  inherited from T3. SC-007 (linker-map inspection) is the
  structural Principle II check.
- **Configuration resolution** (FR-005 family — Session 2026-05-12
  TOML wins): each request runs `nsl::fmt::discover_config` from
  the document URI's parent directory (skipped on non-file
  schemes — FR-005a). If found, `parse_config_file` produces a
  `Configuration` (Status::Success) or one of `Refused`/`Error`
  with diagnostics. On `Success` the configuration is used; on
  `Refused`/`Error` the server falls back to
  `default_configuration()` and emits one
  `textDocument/publishDiagnostics` notification against the TOML
  URI carrying the parse diagnostics (FR-005c). The format request
  itself proceeds with the fallback configuration and returns a
  normal `TextEdit[]` per FR-006. The LSP `FormattingOptions`
  object is *read off the wire* (LSP 3.16 protocol requires the
  field) but its contents are **discarded** (FR-005 resolution —
  Session 2026-05-12).
- **Range computation** (FR-003 whole-line snap + clamp): the
  range handler computes a 1-indexed `nsl::fmt::LineRange` from
  the request's zero-based `Position` start/end by (a) snapping
  `start` down to start-of-line (column → 0), (b) snapping `end`
  up to end-of-line (line + 1, column → 0), (c) clamping
  `firstLine` to `[1, documentLineCount]` and `lastLine` likewise,
  (d) rejecting inverted ranges (firstLine > lastLine) with a
  `null` response. The position-encoding conversion (UTF-16 ↔
  byte offset) is a no-op at line granularity per FR-013; only
  the column index, which we discard, would have needed
  conversion.
- **TextEdit shape** (FR-006 — Session 2026-05-12 single
  whole-span): on `Status::Success`, the response is exactly one
  `TextEdit` whose `range` covers the entire formatted span and
  whose `newText` is `FormattedText` verbatim. **No Myers-diff
  decomposition**. On already-canonical input (input equals
  `FormattedText` byte-for-byte), the response is an empty
  `TextEdit[]` per FR-006a (ii) — sparing the editor from a
  no-op edit.
- **Cancellation** (FR-012): per T3's existing `$/cancelRequest`
  plumbing, each formatting handler receives a cancellation
  token. The handler polls the token after the `DirectiveSplitter`
  pre-pass and after each NSL-fragment lex+parse iteration. On
  set, the handler returns `RequestCancelled` (`-32800`) per T3
  FR-020i. `$/cancelRequest` is the **only** mechanism that
  aborts an in-flight format; mid-format `didClose` per FR-014b
  (Session 2026-05-12) does not signal the handler.
- **Threading**: reuse T3's TUScheduler worker pool (per FR-011).
  Each format request is dispatched to the per-document worker
  via the same `withCurrentState(...)` access pattern T3 already
  exposes for `foldingRange`. The handler reads
  `NslTU.current.contents` + `NslTU.current.version` at dispatch
  time (FR-014a), runs `format_buffer` on the captured buffer,
  and packages the response.
- **Determinism** (Principle V / SC-006): `format_buffer` is a
  pure function of `(buffer, configuration, line_range)` per T2's
  `format-api.contract.md` §6. The Configuration resolver is
  deterministic given filesystem state at call time (the only
  non-pure call is `discover_config`, T2's documented exception).
  The TextEdit converter is deterministic (single allocation, no
  hash-map iteration). SC-006 thus holds by construction.

## Technical Context

**Language/Version**: C++17 across all new code (Constitution
Build/Code/Licensing Standards). No C++20 features.

**Primary Dependencies**:
- **`libNslFmt.a`** (T2-delivered) — consumed via the frozen
  10-symbol `include/nsl/Fmt/Fmt.h` public header. Adds a new
  link edge from the `nsl-lsp` binary target. No T2 contract
  changes; SC-005 byte-equivalence depends on this.
- **`libNSLFrontend.a`** (M0–M3-delivered) — already linked by
  T3's `nsl-lsp` via the `add_nsl_library` convention. Reused for
  `basic::Diagnostic`, `basic::FileID`, `basic::SourceManager`
  (the toml diagnostic mapping in FR-005c reuses these), and the
  existing T3 diagnostic-mapping seam.
- **`libNSLLSP.a`** (T3-delivered) — grows in place; T5 adds new
  source files (`lib/LSP/Features/Formatting.cpp`, `.h`) plus
  dispatch-table edits in `NslLSPServer.cpp`.
- **`llvm::json`** — already pulled in via `nsl-driver` and T3.
  Used for the `TextEdit[]` JSON encoding and the request/response
  parsing in the dispatch table.
- **No new external dependencies.** No new third-party libraries,
  no new transitive imports.

**Storage**: N/A — formatting is stateless per request.
TUScheduler caching of per-document state is inherited from T3
and not extended.

**Testing**:
- **Layer**: existing `test/lsp/` LSP integration test layer
  (T3-introduced). T5 adds two fixture sub-directories
  (`formatting/` and `rangeFormatting/`) and three new
  gtest-driven test binaries. The harness (`LspSession` helper
  spawning `nsl-lsp` over pipes) is reused unchanged.
- **Format**: each test case is a pair `(input.nsl, request.jsonl)`
  plus `(expected-edits.json | expected-formatted.nsl)` golden.
  The harness sends `initialize` → `didOpen` → the formatting
  request, captures the response, applies the returned
  `TextEdit[]` to the original buffer, and asserts byte-equality
  with the expected file.
- **Coverage** per FR-017a / FR-017b / FR-018 / FR-019 / FR-020:
  - `formatting_test`: ~10 fixtures (one per NSL-specific rule
    from `nsl_tooling_design.md` §5.3 plus parse-error refusal,
    already-canonical, empty document, malformed TOML).
  - `range_formatting_test`: ~6 fixtures (middle-range,
    mid-line snap, out-of-bounds clamp, inverted range,
    range-overlapping-directive, range-covering-whole-document).
  - `format_cancellation_test`: 1 large-fixture cancellation
    test per SC-010 / FR-020.
  - `lifecycle_test::CapabilitiesExact` (existing T3 test):
    extended to assert the new canonical capabilities JSON shape
    per FR-019 / SC-009.
  - **CLI ↔ LSP parity** (FR-018 / SC-005): for each fixture in
    `test/lsp/formatting/`, a parameterized test asserts that
    `nsl-lsp` `textDocument/formatting` produces output
    byte-identical to `nsl-fmt --stdin < fixture.nsl`. The CLI
    invocation runs as a subprocess inside the test binary.

**Target Platform**: Linux x86_64 (matches Principle IX build
matrix). The dev container `ghcr.io/koyamanx/nsl-nslc:dev`
provides clang/gcc/cmake/ninja/lit/gtest. No new platform.

**Project Type**: tooling library + binary (no new library —
extends `libNSLLSP.a` in place; no new tooling binary — extends
`nsl-lsp` in place).

**Performance Goals**:
- SC-004: `textDocument/formatting` arrival → `TextEdit[]`
  response emission under **300 ms** for files ≤ 1500 lines on
  a standard CI runner. Composes T2 SC-003's 250 ms `format_buffer`
  budget plus the T5 LSP wrapper overhead (~50 ms).
- SC-010: cancellation acknowledgment under **250 ms**.
- SC-011: combined T5 integration tests (FR-017a/b + FR-018
  parity + FR-019 capabilities + FR-020 cancellation) under
  **60 s** on CI.

**Constraints**:
- Constitution Principle II: no duplication of layout / Doc-IR /
  Wadler-Leijen / TOML-parser / CST-walker code in `nsl-lsp` —
  every formatting symbol resolves into `libNslFmt.a` per SC-007.
- Constitution Principle V: byte-identical `TextEdit[]` responses
  across two runs over the same input (SC-006).
- Constitution Principle VII: the T3 `lsp-protocol.contract.md`
  §1.2 capabilities JSON is amended in the same PR as the T5
  implementation; the `lifecycle_test::CapabilitiesExact` byte-
  equality assertion is updated to match (SC-009).
- LSP 3.16 floor (inherited from T3 — UTF-16 unconditionally; no
  `positionEncodings` advertisement; `Full` text sync).
- `Full` text sync only (inherited from T3) means the
  TUScheduler's `current.contents` is always the latest buffer
  — the format handler does not need to apply pending
  `contentChanges` deltas.
- `$/cancelRequest` is the only cancellation path; mid-format
  `didClose` per FR-014b (Session 2026-05-12) lets the format
  complete (the protocol layer sends the response; editor
  discards it).
- Malformed `.nsl-fmt.toml` per FR-005c emits a side-channel
  `publishDiagnostics` against the TOML URI; the format request
  itself proceeds with `default_configuration()`.
- No new `-emit=` flag (T5 is a server feature, not a compiler
  stage); inspectability lives in the LSP integration tests
  themselves, which are reproducible CLI artefacts.

**Scale/Scope**: 2 new LSP wire methods
(`textDocument/formatting`, `textDocument/rangeFormatting`) plus
the capability-JSON amendment that exposes them. One new C++
source file (`Formatting.cpp` + `Formatting.h`) plus dispatch +
capability edits in `NslLSPServer.cpp`. ~150 LOC of new
implementation, ~250 LOC of new test code (~15 fixtures across 3
test binaries), one contract-amendment PR-section update to
T3's `lsp-protocol.contract.md` §1.2. Total in-tree footprint
≤ 500 LOC.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

Per the project Constitution (`.specify/memory/constitution.md`
v1.7.0), the nine principles apply as follows. Status legend:
**PASS** — satisfied by construction or by an explicit plan
artefact; **N/A** — principle does not apply to this layer.

| # | Principle | Status | Notes |
|---|---|---|---|
| I | Spec Is Authoritative | **PASS** | T5 surfaces existing T2 formatter behaviour through an LSP wire format; no `docs/spec/*.ebnf` edit. The "no silent AST drops" sub-clause does not apply (T5 consumes the CST through `libNslFmt.a`, which is downstream of the parser). |
| II | Layered Library Architecture | **PASS** | T5 extends `libNSLLSP.a` (T3) in place and links `libNslFmt.a` (T2) without modification. Per Principle II §3, `nsl-lsp` is a user-facing tooling binary that reuses both shared libraries; SC-007 enforces this structurally via linker-map inspection. No new library is introduced; the single-public-header rule for `libNSLLSP.a` (T3's `include/nsl/LSP/Server.h`) is preserved. |
| III | Stock CIRCT Below `nsl` Dialect | **N/A** | T5 is upstream of MLIR/CIRCT; no dialect or CIRCT involvement. |
| IV | Source-Locating Diagnostics | **PASS** | The TOML-error diagnostic emission per FR-005c uses the existing T3 diagnostic-mapping seam (which already preserves `nsl::SourceLocation` → LSP `Range` through the include-from-notes path per Principle IV). `format_buffer` warnings (e.g., over-long line on `Success`) flow through the same seam. No new diagnostic infrastructure. |
| V | Inspectable, Deterministic Pipeline | **PASS** | SC-006 enforces byte-identical `TextEdit[]` across two runs over the same input. `format_buffer` is a pure function of `(buffer, configuration, line_range)` per T2's `format-api.contract.md` §6; the LSP wrapper adds no environment reads beyond `discover_config`'s documented filesystem dependency (FR-005a). JSON serialization via `llvm::json::Value` is deterministic. The `-emit=` flag does not extend to `nsl-lsp` — inspectability applies via the integration-test fixtures, which are reproducible CLI artefacts. |
| VI | Layered Test Discipline | **PASS** | T5 extends the existing T3 LSP integration test layer at `test/lsp/`. Per the per-layer accepted-driver list, tooling-track LSP testing uses gtest + subprocess as the conventional driver (the precedent T3 established). The audited-projects list is unchanged. |
| VII | Spec ↔ Design Coupling | **PASS** | T5 lands four coupling updates in the same PR: (a) the T3 `lsp-protocol.contract.md` §1.2 canonical capabilities JSON gains two entries; (b) `CLAUDE.md` §2.1 LSP-method roll-up rows for `formatting` and `rangeFormatting` change from "T5" projection to "delivered at T5"; (c) `CLAUDE.md` §2.3 Formatter roll-up row "LSP `formatting` / `rangeFormatting` integration" similarly; (d) `docs/design/nsl_tooling_design.md` §5.4 may gain a "T5 status" annotation (decision deferred to research.md R6). T5's own contracts live under `specs/011-t5-lsp-formatting/contracts/` and are referenced from the routing pointers per the established `docs/CLAUDE.md` §3 "Implementing the LSP" / "Working on the formatter" pattern. |
| VIII | Test-First Development | **PASS** | `tasks.md` requires all three new integration-test binaries (`formatting_test`, `range_formatting_test`, `format_cancellation_test`) plus the extended `lifecycle_test::CapabilitiesExact` to land first, observed failing against the unchanged tree (T5 has no `Formatting.cpp` and no advertised capabilities yet — the lifecycle test fails on the capability-shape mismatch; the formatting tests fail because the dispatch table has no handler and replies with the LSP `MethodNotFound` error). XFAIL→green progression is preserved in commit history. |
| IX | Continuous Integration & Delivery | **PASS** | The new T5 integration tests integrate into `./scripts/ci.sh` stage 3 (unit & layer tests). The harness exits non-zero on any assertion failure. SPDX headers on every new file. No bypass. The `nsl-lsp` binary remains a built-from-source artefact (no tagged release until M9); the T5 amendment does not change release packaging. |

**No Constitution violations.** No `Complexity Tracking` entries
needed.

## Project Structure

### Documentation (this feature)

```text
specs/011-t5-lsp-formatting/
├── plan.md                                # This file
├── spec.md                                # Feature specification (post-/speckit-clarify)
├── research.md                            # Phase 0 output — R1..R6 decisions
├── data-model.md                          # Phase 1 output — entity catalog
├── contracts/                             # Phase 1 output
│   ├── formatting-api.contract.md         # The two-method wire-level contract
│   ├── config-resolution.contract.md      # FR-005 / FR-005a / FR-005b / FR-005c TOML-precedence contract
│   └── text-edit-shape.contract.md        # FR-006 single-whole-span TextEdit convention
├── quickstart.md                          # Phase 1 output — local repro recipe
├── checklists/
│   └── requirements.md                    # Quality checklist from /speckit-specify
└── tasks.md                               # Phase 2 output (/speckit-tasks — NOT created by /speckit-plan)
```

In addition, T5 amends in place:

```text
specs/010-t3-lsp-skeleton/contracts/
└── lsp-protocol.contract.md               # §1.2 capabilities JSON gains 2 entries (Principle VII coupling)
```

### Source code (repository root)

```text
include/nsl/LSP/
└── Server.h                               # T3-delivered umbrella header (UNCHANGED — single-public-header rule preserved)

lib/LSP/
├── Transport.cpp                          # T3-delivered (UNCHANGED — FR-010)
├── Scheduler.cpp                          # T3-delivered (UNCHANGED — FR-010)
├── NslServer.cpp                          # T3-delivered (UNCHANGED except optionally exposing existing format-region seam body)
├── NslLSPServer.cpp                       # T3-delivered: gains 2 dispatch-table entries + 2 capability JSON keys
└── Features/
    ├── (T3 may or may not have a Features/ dir; created here if absent)
    └── Formatting.cpp                     # NEW — both handlers, ~150 LOC

lib/LSP/                                   # internal headers
└── Features/
    └── Formatting.h                       # NEW — internal interface (private to lib/LSP/)

tools/nsl-lsp/
└── main.cpp                               # T3-delivered (UNCHANGED — thin entry point)

test/lsp/
├── lifecycle/                             # T3-delivered — CapabilitiesExact assertion JSON updated
├── diagnostics/                           # T3-delivered (UNCHANGED)
├── folding/                               # T3-delivered (UNCHANGED)
├── cancellation/                          # T3-delivered (UNCHANGED — folding-range cancellation)
├── formatting/                            # NEW — ~10 fixture pairs (input.nsl + expected-formatted.nsl)
├── rangeFormatting/                       # NEW — ~6 fixture pairs (input.nsl + range.json + expected-formatted.nsl)
└── format_cancellation/                   # NEW — 1 large-file fixture for SC-010

test/lsp/_harness/                         # T3-delivered LspSession helper (UNCHANGED — reused)
```

**Structure Decision**: T5 follows the layout
[`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
§8 prescribes — `lib/LSP/Features/<MethodName>.cpp` per method
group. The new `Formatting.cpp` matches the convention the design
doc names verbatim ("`lib/LSP/Features/Formatting.cpp`"). No new
top-level directories; no new CMake `add_nsl_library` target;
existing `nsl-lsp` binary target gains one `target_link_libraries`
edge to `NslFmt`.

## Complexity Tracking

*No Constitution violations to justify.*

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| *(none)*  | — | — |
