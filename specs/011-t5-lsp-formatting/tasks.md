<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Tasks: T5 — LSP Formatting Integration

**Input**: Design documents from `/specs/011-t5-lsp-formatting/`
**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md, contracts/

**Tests**: Test tasks are **MANDATORY** for this project per Constitution Principle VIII (Test-First Development, NON-NEGOTIABLE). Every user story includes test tasks at the LSP integration test layer per Constitution Principle VI. Tests MUST be written and observed FAILING before the corresponding implementation tasks begin.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3, US4)
- File paths are absolute or repo-rooted

## Path Conventions

Per [plan.md](./plan.md) §"Project Structure":

- **Source code**: `lib/LSP/Features/Formatting.cpp` + `Formatting.h`, with one dispatch-table edit and one capability-JSON edit in the existing `lib/LSP/NslLSPServer.cpp` (T3-delivered).
- **Tests**: `test/lsp/formatting/`, `test/lsp/rangeFormatting/`, `test/lsp/format_cancellation/`, plus the existing `test/lsp/lifecycle/` (extended).
- **Build**: extends the existing `nsl-lsp` CMake target; adds one `target_link_libraries(... NslFmt ...)` line; no new top-level CMake module.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project plumbing for T5's new source file, new test directories, and new linker edge.

- [ ] T001 Add `target_link_libraries(nsl-lsp PRIVATE NslFmt)` line to `lib/LSP/CMakeLists.txt` so the `nsl-lsp` binary links `libNslFmt.a`; verify with `cmake --build build --target nsl-lsp` that the existing T3 build still succeeds (the new link edge is unused until Phase 2 lands)
- [ ] T002 [P] Create test directory structure: `test/lsp/formatting/` (with empty `CMakeLists.txt` registering an empty `formatting_test` target), `test/lsp/rangeFormatting/`, `test/lsp/format_cancellation/`; register each new test binary in `test/lsp/CMakeLists.txt`
- [ ] T003 [P] Add SPDX header (`<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->` for markdown; `// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception` for C++) skeleton to every new file created in Phases 1–7; the project's existing `scripts/check_spdx.sh` CI gate enforces this on every PR per Principle IX stage 2

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Wire the dispatch table, capability advertisement, and contract amendment so the rest of the work has a place to land. The handler bodies stay stubs returning `null` — Phase 3 fills them.

**⚠️ CRITICAL**: No user-story work can begin until this phase is complete. The capability test in T006/T009 demonstrates the Principle VIII red→green progression at the foundational level.

- [ ] T004 Amend `specs/010-t3-lsp-skeleton/contracts/lsp-protocol.contract.md` §1.2 in place — replace the canonical `InitializeResult.capabilities` JSON with the new shape per [`contracts/formatting-api.contract.md`](./contracts/formatting-api.contract.md) §1 (adds `documentFormattingProvider: true` and `documentRangeFormattingProvider: true`); record the amendment in a new "Amendment 2026-05-12 (T5)" subsection at the top of the file noting the Principle VII coupling
- [ ] T005 Create `include/nsl/LSP/Features/Formatting.h` declaring the two free-function entry points in namespace `nsl::lsp::detail`: `llvm::json::Value onFormatting(...)` and `llvm::json::Value onRangeFormatting(...)`, taking `NslServer&`, the request JSON params, and the `CancellationToken*` per [`data-model.md`](./data-model.md) §2.2; place under `lib/LSP/Features/` (internal header per Principle II — `Features/` is not exposed in the public umbrella `Server.h`)
- [ ] T006 Create `lib/LSP/Features/Formatting.cpp` with both entry-point stubs returning `llvm::json::Value(nullptr)` (the FR-007 / §2.2.3 `null` shape); include the new header from T005 and add `// TODO(T5): implement per contracts/formatting-api.contract.md` comments inside each stub
- [ ] T007 Add two dispatch-table entries in `lib/LSP/NslLSPServer.cpp` mapping `"textDocument/formatting"` to `nsl::lsp::detail::onFormatting` and `"textDocument/rangeFormatting"` to `nsl::lsp::detail::onRangeFormatting`; keep the dispatch insertion alphabetical (or matching whatever T3 convention exists — adopt T3's pattern verbatim, do NOT introduce a new ordering)
- [ ] T008 Add two capability JSON keys in `lib/LSP/NslLSPServer.cpp::onInitialize`'s capability builder: `documentFormattingProvider: true` and `documentRangeFormattingProvider: true`, placed alphabetically before `foldingRangeProvider` per the diff in [`contracts/formatting-api.contract.md`](./contracts/formatting-api.contract.md) §1
- [ ] T009 Update the existing `lifecycle_test::CapabilitiesExact` expected JSON in `test/lsp/lifecycle/` (the golden file the assertion reads — locate via `grep -r "foldingRangeProvider" test/lsp/lifecycle/`) to match the amended T3 contract §1.2 canonical JSON; run `ctest -R "lifecycle_test.CapabilitiesExact"` and **observe the test PASSING** (the test was previously red against the T3-shape JSON between T004 and T008; it goes green here) — this is the foundational Principle VIII red→green progression

**Checkpoint**: Foundation ready — the server advertises both capabilities, dispatches both methods, but every format request returns `null` because the handlers are stubs. User-story implementation can now begin in parallel.

---

## Phase 3: User Story 1 — Format the open buffer with one keystroke (Priority: P1) 🎯 MVP

**Goal**: `textDocument/formatting` returns a single whole-buffer `TextEdit` whose application produces output byte-identical to `nsl-fmt --stdin < buffer.nsl`. Test gate: spec FR-017a / SC-001 (the test gate stated verbatim in [`README.md`](../../README.md) §Roadmap row T5).

**Independent Test**: Launch `nsl-lsp`; send `initialize → initialized → didOpen` (fixture with non-canonical whitespace) → `textDocument/formatting`; assert the returned `TextEdit[]` applied to the original buffer produces output byte-identical to `nsl-fmt --stdin < fixture.nsl`.

### Tests for User Story 1 (MANDATORY per Constitution Principle VIII) ⚠️

> **Write these tests FIRST. They MUST be observed FAILING against the unchanged tree (Phase 2 stubs return `null` — every fixture's `apply(textEdits, input) == golden` assertion fails because `textEdits == null`) before any implementation task in this story begins.**

- [ ] T010 [P] [US1] Author 10 fixture pairs under `test/lsp/formatting/` covering the 6 NSL-specific rules from [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md) §5.3 (`alt-arrow.nsl`, `struct-member.nsl`, `proc-args.nsl`, `bitslice-concat.nsl`, `operator-spacing.nsl`, `attached-comment.nsl`) plus four boundary fixtures (`parse-error.nsl` — bad token; `already-canonical.nsl`; `empty.nsl`; `malformed-toml.nsl` plus `.nsl-fmt.toml`); each fixture pairs `input.nsl` with the matching CLI-produced golden `expected-formatted.nsl` (generate via `./build/bin/nsl-fmt --stdin < input.nsl > expected-formatted.nsl` — these are the same fixtures the T2 SC-002 audited-corpus idempotence test would accept)
- [ ] T011 [P] [US1] Implement `test/lsp/formatting/formatting_test.cpp` (gtest binary) using the T3 `LspSession` harness from `test/lsp/_harness/` — parameterized over every fixture pair from T010; the test (a) launches `nsl-lsp`, (b) sends `initialize → initialized → didOpen → textDocument/formatting`, (c) captures the response, (d) applies the returned `TextEdit[]` to the input buffer, (e) asserts byte-equality against the golden; per [`contracts/formatting-api.contract.md`](./contracts/formatting-api.contract.md) §2.2 each fixture maps to one of the four response outcomes (SingleEdit / Empty / Null / Cancelled) — parameterize the assertions accordingly
- [ ] T012 [US1] Build with `cmake --build build --target formatting_test` and run `ctest -R "formatting_test" --output-on-failure`; **observe every fixture FAILING** because Phase 2 stubs return `null` (parse-error and malformed-TOML fixtures will accidentally pass — they expect `null` — so document this in a comment explaining the partial green is expected at this checkpoint and will reverse once the handler is implemented)

### Implementation for User Story 1

- [ ] T013 [P] [US1] Implement `resolveConfiguration(StringRef documentURI, basic::SourceManager &sm) -> ResolvedConfiguration` as a file-static helper in `lib/LSP/Features/Formatting.cpp` per [`contracts/config-resolution.contract.md`](./contracts/config-resolution.contract.md) §2 — the resolver runs `discover_config` (skipped on non-file URI per §3), reads the TOML if found, calls `parse_config_file`, and returns a `ResolvedConfiguration` carrying the resolved `Configuration`, the TOML path, the diagnostics array (empty on Success), and the `tomlFallback` boolean (true iff malformed-TOML triggered fallback to `default_configuration()`); the helper MUST NOT consult `params.options` per FR-005
- [ ] T014 [P] [US1] Implement `encodeTextEditWholeBuffer(StringRef originalBuffer, StringRef formattedText) -> llvm::json::Value` as a file-static helper in `lib/LSP/Features/Formatting.cpp` per [`contracts/text-edit-shape.contract.md`](./contracts/text-edit-shape.contract.md) §2 — returns an empty JSON array if `originalBuffer == formattedText` (§2.1), otherwise a length-1 `TextEdit[]` with `range = {start: (0,0), end: (documentLineCount, 0)}` and `newText = formattedText` (§2.2); `documentLineCount` is the count of newline-terminated lines in `originalBuffer`
- [ ] T015 [P] [US1] Implement `emitTomlDiagnostics(NslLSPServer &server, StringRef tomlPath, ArrayRef<basic::Diagnostic> diags)` as a file-static helper in `lib/LSP/Features/Formatting.cpp` per [`contracts/config-resolution.contract.md`](./contracts/config-resolution.contract.md) §7 — constructs and sends one `textDocument/publishDiagnostics` notification whose `uri` is the TOML file's `file://` URI, mapping each `basic::Diagnostic` via the existing T3 `toLspDiagnostic(...)` helper with `source = "nsl-fmt"`; the notification MUST be sent before the format response per the §7 ordering rule; also emit a "clear" notification (empty `diagnostics` array) on Success-after-fallback to remove stale entries
- [ ] T016 [US1] Replace the `onFormatting` stub body in `lib/LSP/Features/Formatting.cpp` with the full implementation (depends on T013, T014, T015): (a) look up `NslTU` by URI — return `null` + WARN log on miss (FR-014); (b) call `resolveConfiguration`; (c) if `tomlFallback` and `tomlDiagnostics` non-empty, call `emitTomlDiagnostics`; (d) call `nsl::fmt::format_buffer(contents, config, fileID, std::nullopt)`; (e) case-split per [`contracts/formatting-api.contract.md`](./contracts/formatting-api.contract.md) §2.2: Success+changed → T014 whole-buffer edit, Success+unchanged → empty array, Refused/Error → `null`; (f) return the JSON value
- [ ] T017 [US1] Add cancellation polling in `onFormatting` at the FR-012 boundary — between `resolveConfiguration` and `format_buffer`, AND immediately after `format_buffer` returns — per [`contracts/formatting-api.contract.md`](./contracts/formatting-api.contract.md) §2.2.4: if the token is set, return a sentinel that the protocol layer converts to JSON-RPC error `-32800 RequestCancelled`; the cancellation-token plumbing is T3-delivered (`CancellationToken` argument already passed to the handler in T005's signature)
- [ ] T018 [US1] Add logging records per FR-015 / [`contracts/formatting-api.contract.md`](./contracts/formatting-api.contract.md) §5 — one `INFO` on request arrival (URI + method + id), one `INFO` on response (URI + id + outcome ∈ {Success, RefusedParse, RefusedMalformedTOML, ErrorOutOfBounds, Cancelled, UnknownDocument} + elapsed_ms), and one `ERROR` on internal exception; use the T3-delivered logging facility (`nsl::lsp::log_info`, `nsl::lsp::log_error` — locate via `grep -r "log_info\|log_error" lib/LSP/`); MUST NOT log buffer contents (FR-016)
- [ ] T019 [US1] Re-run `ctest -R "formatting_test" --output-on-failure` and **observe every fixture PASSING** — the 6 NSL-rule fixtures, `already-canonical`, `empty`, `parse-error`, and `malformed-toml` all assert the expected response shape per their parameterized response-outcome assignment; total wall-clock < 60 s (contribution to SC-011)

**Checkpoint**: User Story 1 fully functional. `textDocument/formatting` produces byte-equivalent output to `nsl-fmt --stdin`. Independent test: drop a fixture into `test/lsp/formatting/` with a hand-curated golden and the parameterized test exercises it without code changes.

---

## Phase 4: User Story 2 — Format only a selection (Priority: P2)

**Goal**: `textDocument/rangeFormatting` returns a single line-range-scoped `TextEdit` whose application changes only the selected lines; lines outside the range are byte-identical to the input. Test gate: spec FR-017b / SC-002.

**Independent Test**: Open a multi-line fixture; send `textDocument/rangeFormatting` with a `Range` covering only the middle third; assert the response is a `TextEdit[]` whose application yields exactly the same output as `nsl-fmt --stdin --range L1:L2 < fixture.nsl`.

### Tests for User Story 2 (MANDATORY per Constitution Principle VIII) ⚠️

> **Write these tests FIRST. Observed failing before any implementation: Phase 3 left `onRangeFormatting` as the T006 stub returning `null`, so every fixture's `apply(textEdits, input) == golden` assertion fails.**

- [ ] T020 [P] [US2] Author 6 fixture pairs under `test/lsp/rangeFormatting/` covering: `middle-range.nsl` + `range.json` (lines 5..15 of a 30-line fixture); `mid-line-snap.nsl` (range starts at column 5 of line 3, ends at column 10 of line 8 — should snap to whole-line); `out-of-bounds-clamp.nsl` (range end past document end); `inverted-range.nsl` (start.line > end.line — expects `null`); `range-overlapping-directive.nsl` (range covers a `#include` line plus surrounding NSL — directive is byte-preserved per T2 FR-012a); `range-covering-whole-document.nsl` (range covers lines 1..N — equivalent to whole-document); each fixture has `input.nsl`, `range.json`, and `expected-formatted.nsl` (the CLI-produced golden — generate via `./build/bin/nsl-fmt --stdin --range L1:L2 < input.nsl > expected-formatted.nsl`)
- [ ] T021 [P] [US2] Implement `test/lsp/rangeFormatting/range_formatting_test.cpp` (gtest binary) — parameterized over every fixture pair from T020; reads `range.json` for the request's `Range` parameter; the test asserts both (a) the response shape per [`contracts/formatting-api.contract.md`](./contracts/formatting-api.contract.md) §3.3 (SingleEdit / Empty / Null per parameterized outcome) and (b) byte-equality between `apply(textEdits, input)` and the golden; the `inverted-range.nsl` fixture asserts `result === null`; the `range-overlapping-directive.nsl` fixture asserts the directive line is byte-preserved in the post-edit buffer
- [ ] T022 [US2] Build with `cmake --build build --target range_formatting_test` and run `ctest -R "range_formatting_test" --output-on-failure`; **observe every fixture FAILING** (except `inverted-range.nsl`, which accidentally passes against the stub `null` — document this partial green per T012's pattern)

### Implementation for User Story 2

- [ ] T023 [P] [US2] Implement `computeLineRange(const Range &requestRange, int documentLineCount) -> std::optional<nsl::fmt::LineRange>` as a file-static helper in `lib/LSP/Features/Formatting.cpp` per [research.md R5](./research.md) and [`contracts/formatting-api.contract.md`](./contracts/formatting-api.contract.md) §3.2 — snap to whole-line (treat `end.character == 0` as exclusive), clamp to `[1, documentLineCount]`, return `nullopt` on inverted post-clamp ranges
- [ ] T024 [P] [US2] Extend `encodeTextEditWholeBuffer` (T014) into `encodeTextEditForRange(StringRef originalBuffer, StringRef formattedText, LineRange range) -> llvm::json::Value` in `lib/LSP/Features/Formatting.cpp` per [`contracts/text-edit-shape.contract.md`](./contracts/text-edit-shape.contract.md) §3 — compute the byte offsets of `firstLine` and `lastLine+1` in both buffers; if the slices are byte-identical, return `[]` (§3.2); otherwise return a length-1 `TextEdit[]` with `range = {start: (firstLine - 1, 0), end: (lastLine, 0)}` and `newText` set to the formatted slice (§3.3); refactor T014 to share the underlying byte-offset / slice-extraction logic with this function — avoid two divergent implementations
- [ ] T025 [US2] Replace the `onRangeFormatting` stub body in `lib/LSP/Features/Formatting.cpp` with the full implementation (depends on T013, T015, T017, T018, T023, T024): same flow as T016 but (a) parse `params.range` into a `Range`, (b) call `computeLineRange` — on `nullopt`, return `null` + WARN log per FR-003 / §3.3.3; (c) call `format_buffer(contents, config, fileID, lineRange)`; (d) on Success, call `encodeTextEditForRange` (T024); (e) reuse T013's `resolveConfiguration`, T015's `emitTomlDiagnostics`, T017's cancellation polling, and T018's logging verbatim — file-static helpers shared with `onFormatting`
- [ ] T026 [US2] Re-run `ctest -R "range_formatting_test" --output-on-failure` and **observe every fixture PASSING** — the 6 range fixtures all satisfy their parameterized outcome assertions

**Checkpoint**: User Story 2 fully functional. `textDocument/rangeFormatting` produces correct line-range edits. Both US1 and US2 share the resolver, the TOML side-channel, the cancellation polling, and the logging — no duplicated code paths.

---

## Phase 5: User Story 3 — CLI ↔ LSP byte-equivalence (Priority: P2)

**Goal**: For every fixture under `test/lsp/formatting/`, the LSP `textDocument/formatting` response applied to the input buffer is byte-identical to `nsl-fmt --stdin < input.nsl`. Test gate: spec FR-018 / SC-005 — the gate that delivers [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md) §10's "Save-on-format produces byte-identical output" promise.

**Independent Test**: Pick any fixture; run both `nsl-fmt --stdin < input.nsl` and an `nsl-lsp` round-trip via `LspSession`; the two captured outputs are byte-identical.

### Tests for User Story 3 (MANDATORY per Constitution Principle VIII) ⚠️

> **Write this test FIRST. Observed failing if either US1 or US2 has subtly diverged from the CLI — the parity assertion catches drift the per-fixture goldens might miss (e.g., if a fixture's golden was hand-edited and no longer matches the CLI's actual output).**

- [ ] T027 [P] [US3] Add a parameterized `CLI_LSP_Parity` test group to `test/lsp/formatting/formatting_test.cpp` (extends T011, does not create a new binary): for each fixture in `test/lsp/formatting/` AND `test/lsp/rangeFormatting/`, the test (a) spawns `./build/bin/nsl-fmt --stdin < input.nsl` (and for range fixtures, also `--range L1:L2`) capturing the CLI's stdout; (b) drives `nsl-lsp` via `LspSession` to produce the LSP `TextEdit[]` response; (c) applies the LSP response to the input buffer; (d) asserts byte-equality between the CLI's stdout and the LSP-applied buffer; the test SKIPs fixtures whose expected outcome is `null` (parse-error, malformed-TOML cases — both paths produce `null` / non-zero-exit, parity is trivial); the test uses `llvm::sys::ExecuteAndWait` for the CLI subprocess (already a transitive dep)

### Implementation for User Story 3

User Story 3 has no implementation tasks beyond what US1 and US2 deliver. SC-005 byte-equivalence holds by construction:

- The Configuration resolver (T013) consults only `.nsl-fmt.toml` + defaults — same inputs as the CLI's resolver.
- The `format_buffer` call (T016, T025) passes the same buffer + configuration + line range as the CLI invokes.
- The TextEdit converter (T014, T024) emits `formattedText` verbatim — no transformation between formatter output and editor-applied buffer.

T027's parameterized assertion is the verification, not an implementation.

- [ ] T028 [US3] Build with `cmake --build build --target formatting_test` and run `ctest -R "formatting_test.CLI_LSP_Parity" --output-on-failure`; **observe every applicable fixture PASSING** — if any fixture fails, US1 or US2 has drifted from CLI behaviour; investigate and re-do the responsible implementation task (NOT this test task — the test is the assertion, not the fix)

**Checkpoint**: SC-005 byte-equivalence verified across the entire fixture set. Save-on-save in editors will produce CI-passing output.

---

## Phase 6: User Story 4 — Architectural seam verification (Priority: P3)

**Goal**: T5 lands without modifying any T3 lifecycle plumbing. Specifically, `Transport.cpp`, `Scheduler.cpp`, and `Server.cpp`'s `onDidOpen`/`onDidChange`/`onDidClose` handlers are byte-identical to their T3 baseline. The new code is confined to `lib/LSP/Features/Formatting.cpp` (+`.h`) plus dispatch-table and capability-JSON edits in `NslLSPServer.cpp`. Test gates: spec FR-010 / SC-008; structural Principle II / SC-007 linker-map check.

**Independent Test**: `git diff master...HEAD --name-only` returns only an allowlist of paths; `nm` on `libNSLLSP.a` finds no formatter symbols.

### Tests for User Story 4 (MANDATORY per Constitution Principle VIII) ⚠️

> **Write these tests FIRST. They MUST be observed FAILING against the T3 baseline (or against an intentionally broken T5 candidate that touched a forbidden file) before being committed as the structural guard.**

- [ ] T029 [P] [US4] Implement `scripts/audit_lsp_t5_seam.sh` — a shell script that (a) computes `git merge-base HEAD master`, (b) lists files changed since that base with `git diff --name-only <base> HEAD`, (c) intersects against a forbidden-paths allowlist (`lib/LSP/Transport.cpp`, `lib/LSP/Scheduler.cpp`, `lib/LSP/NslServer.cpp`, and `lib/LSP/NslLSPServer.cpp`'s `onDidOpen`/`onDidChange`/`onDidClose` handlers — for the latter, grep the file for the function bodies and assert their bytes are unchanged from `<base>`), (d) exits non-zero if any forbidden path is in the changed set or any of the named handlers has changed; the script outputs a clear diagnostic listing the offending path(s); to validate the test itself: run it once against a temporary commit that deliberately touches `Transport.cpp` and confirm exit code is non-zero, then revert
- [ ] T030 [P] [US4] Implement `scripts/audit_lsp_no_formatter_duplication.sh` per [`quickstart.md`](./quickstart.md) §7 — runs `nm --defined-only --extern-only build/lib/libNSLLSP.a` and asserts via `grep -v` that none of these symbols are defined there: `format_buffer`, `parse_config_file`, `discover_config`, `LayoutPlanner`, `LayoutRenderer`, `Doc::`, `Wadler`; the same `nm` over `build/lib/libNslFmt.a` must DEFINE those symbols (positive check); exits non-zero on any structural violation
- [ ] T031 [US4] Wire both scripts (T029, T030) into `scripts/ci.sh` stage 2 (static checks per Principle IX); **observe the integrated CI run failing** if the scripts themselves have bugs, by deliberately introducing a transient violation (touch `Transport.cpp` with a no-op whitespace change in a throwaway commit; observe T029 fails CI; revert the commit; CI goes green)

### Implementation for User Story 4

User Story 4 has no production-code implementation tasks. The structural guarantees are properties of the rest of T5's implementation, verified by T029/T030/T031's scripts.

- [ ] T032 [US4] Final structural review at PR-creation time — eyeball the `git diff master...HEAD --name-only` output and confirm the changed-files list is a strict subset of: `lib/LSP/Features/Formatting.cpp`, `lib/LSP/Features/Formatting.h` (or `include/nsl/LSP/Features/Formatting.h`), `lib/LSP/NslLSPServer.cpp` (only dispatch table + `onInitialize` edits), `lib/LSP/CMakeLists.txt` (only the `target_link_libraries` line), `test/lsp/formatting/`, `test/lsp/rangeFormatting/`, `test/lsp/format_cancellation/`, `test/lsp/lifecycle/` (only the expected-JSON golden update), `test/lsp/CMakeLists.txt`, `scripts/audit_lsp_t5_seam.sh`, `scripts/audit_lsp_no_formatter_duplication.sh`, `scripts/ci.sh`, `specs/010-t3-lsp-skeleton/contracts/lsp-protocol.contract.md` (Principle VII coupling — §1.2 amendment), `specs/011-t5-lsp-formatting/*`, `CLAUDE.md` (active-feature pointer)

**Checkpoint**: User Story 4 structural guard in place. Future T-track milestones (T4 / T9 / T10) that add LSP methods will trigger T029 if they touch the wrong files, catching architectural drift at PR time.

---

## Phase 7: Cancellation Acceptance Test (FR-020 / SC-010)

**Purpose**: One additional acceptance test that exercises the SC-010 cancellation seam end-to-end. This is FR-020 in the spec — a stand-alone test gate, not part of any individual user story's independent test. Placed in its own phase so it cleanly gates the T5 milestone close.

> **Note**: The cancellation *behaviour* is delivered by T017 (`$/cancelRequest` polling in `onFormatting`) and T025 (the same polling in `onRangeFormatting` via the shared helper). T033/T034 are the end-to-end acceptance test for FR-020.

- [ ] T033 [P] Author a single fixture under `test/lsp/format_cancellation/large.nsl` — an artificially-large NSL file (≥ 5000 lines, constructed by replicating an audited sample module) that pushes `format_buffer` past the cancellation poll threshold; pair it with `expected-response.json` documenting the expected JSON-RPC error response: `{error: {code: -32800, message: "request cancelled"}}`
- [ ] T034 Implement `test/lsp/format_cancellation/format_cancellation_test.cpp` (gtest binary) using `LspSession`: (a) drives `initialize → initialized → didOpen` (large fixture from T033), (b) sends `textDocument/formatting` with `id: 42`, (c) **immediately** sends `$/cancelRequest` with `params: {id: 42}` (no wait between the two), (d) captures responses for ≤ 250 ms wall-clock, (e) asserts the response for id 42 is a JSON-RPC error with `code: -32800` per [`contracts/formatting-api.contract.md`](./contracts/formatting-api.contract.md) §2.2.4; build and run via `ctest -R "format_cancellation_test"` and **observe PASSING** — total elapsed time (cancellation arrival → response arrival) under SC-010's 250 ms budget

**Checkpoint**: SC-010 end-to-end verified. The cancellation seam works from `$/cancelRequest` arrival through to `RequestCancelled` response within the timing budget.

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Documentation roll-up updates per Principle VII, performance verification, and final CI run.

- [ ] T035 [P] Update `CLAUDE.md` §2.1 LSP-methods roll-up: the two rows for `textDocument/formatting` and `textDocument/rangeFormatting` change from "T5" (projection) to "T5 — **delivered**"; the introductory paragraph above the table gains one sentence noting T5's delivery date matches the merge date of this PR
- [ ] T036 [P] Update `CLAUDE.md` §2.3 Formatter roll-up: the row "LSP `formatting` / `rangeFormatting` integration" changes from "T5" to "T5 — **delivered**"
- [ ] T037 [P] Update [`docs/CLAUDE.md`](../../docs/CLAUDE.md) §3 "Implementing the LSP" task entry — add a one-line "T5 delivered as of 2026-MM-DD; the three contracts under `specs/011-t5-lsp-formatting/contracts/` freeze the wire-visible behavior of `textDocument/formatting` / `textDocument/rangeFormatting`. T9/T10 extend the dispatch table in `lib/LSP/NslLSPServer.cpp`." sentence after the T3-delivery note
- [ ] T038 [P] Update [`docs/CLAUDE.md`](../../docs/CLAUDE.md) §3 "Working on the formatter" task entry — add a one-line "T5 wires the formatter into `nsl-lsp` via `textDocument/formatting` and `textDocument/rangeFormatting`. The LSP wrapper is a thin layer per `specs/011-t5-lsp-formatting/contracts/`; the layout engine itself is untouched." sentence after the existing T2 entry
- [ ] T039 [P] Add a `## Performance` parameterized test group to `test/lsp/formatting/formatting_test.cpp` exercising SC-004 — for a 1500-line audited-sample fixture, assert the median dispatch-to-response latency across 10 trials is under 300 ms; use the SC-004 fixture name `perf-1500.nsl` (generate by concatenating audited samples up to ≥ 1500 lines)
- [ ] T040 Run `scripts/ci.sh all` end-to-end inside the dev container and confirm a green exit code; record the runtime and confirm SC-011's < 60 s budget for the combined T5 integration tests holds
- [ ] T041 Run the [`quickstart.md`](./quickstart.md) §3 (`textDocument/formatting`), §4 (`textDocument/rangeFormatting`), and §9 (malformed-TOML walkthrough) manual smoke tests; capture the stderr log output and inspect for the expected `INFO` records per FR-015; record evidence in the PR description per the project's [`CONTRIBUTING.md`](../../CONTRIBUTING.md) §7 checklist
- [ ] T042 Update `specs/011-t5-lsp-formatting/spec.md` status field from "Draft" to "Delivered"; update `specs/011-t5-lsp-formatting/checklists/requirements.md` Notes section with the T5 delivery commit hash

---

## Dependencies & Execution Order

### Phase dependencies

- **Setup (Phase 1)**: No dependencies — start immediately.
- **Foundational (Phase 2)**: Depends on Setup completion. T004 (contract amendment) can run in parallel with T005–T008 (build/code); T009 depends on T004 + T008 (golden update + capability code together).
- **User Story 1 (Phase 3)**: Depends on Foundational. The MVP — once green, T5 has delivered its primary spec FR-001 / SC-001 promise.
- **User Story 2 (Phase 4)**: Depends on Foundational + T013/T014/T015/T017/T018 from US1 (shared helpers). Effectively serialized after US1's implementation tasks, but the US2 fixtures (T020, T021) can be authored in parallel with US1's implementation (US1's T011 doesn't conflict with US2's T021 — different binaries).
- **User Story 3 (Phase 5)**: Depends on Foundational + US1 + US2 — the parity test exercises both methods.
- **User Story 4 (Phase 6)**: Depends on **nothing** at the code level — the structural scripts can be authored in parallel with US1/US2/US3. T031 (CI wiring) must land before T040 (final CI run).
- **Cancellation acceptance (Phase 7)**: Depends on Foundational + T017/T025 (the cancellation polling implementations).
- **Polish (Phase 8)**: Depends on US1 + US2 + US3 + US4 + Cancellation all green.

### Story-level dependencies

- **US1 (P1)** — depends on Foundational. No upstream story dependency. ✅ MVP.
- **US2 (P2)** — depends on Foundational + US1's shared helpers (T013, T014, T015, T017, T018). T020/T021 can be authored before US1 is implementation-complete.
- **US3 (P2)** — depends on US1 + US2 (verifies both via parity). The parity test (T027) can be drafted in parallel but only runs green after T019 + T026.
- **US4 (P3)** — depends on **nothing**. Structural scripts are independent of handler code.

### Within each user story

- Tests authored first (per Constitution Principle VIII) and observed failing.
- Helpers (`resolveConfiguration`, `encodeTextEdit*`, `computeLineRange`) before handlers.
- Handler stubs from Phase 2 are replaced with real bodies — not added alongside.
- Cancellation polling and logging are added as final implementation steps, since they touch the handler's full path.

### Parallel opportunities

- **Phase 1**: T002 / T003 in parallel with T001.
- **Phase 2**: T004 (contract amendment) / T005 (header) / T006 (stub `.cpp`) in parallel; T007 (dispatch) / T008 (capability) sequential within `NslLSPServer.cpp` (same file).
- **Phase 3**: T010 (fixtures) / T011 (test binary) authored in parallel; T013 / T014 / T015 implementation in parallel (different file-static helpers); T016 / T017 / T018 sequential within the same `onFormatting` function body.
- **Phase 4**: T020 / T021 in parallel with each other AND with Phase 3 work; T023 / T024 in parallel; T025 sequential.
- **Phase 5**: T027 in parallel with US1/US2 — observable green only after both complete.
- **Phase 6**: T029 / T030 in parallel; T031 sequential after both.
- **Phase 8**: T035 / T036 / T037 / T038 / T039 all in parallel (different files); T040 / T041 / T042 sequential after parallel batch.

---

## Parallel Example: User Story 1

```bash
# After Phase 2 checkpoint, launch in parallel:

# Test authoring (independent files):
Task: "Author 10 fixture pairs under test/lsp/formatting/"
Task: "Implement test/lsp/formatting/formatting_test.cpp (gtest binary)"

# Then observe failing, then launch implementation helpers in parallel:
Task: "Implement resolveConfiguration() in lib/LSP/Features/Formatting.cpp"
Task: "Implement encodeTextEditWholeBuffer() in lib/LSP/Features/Formatting.cpp"
Task: "Implement emitTomlDiagnostics() in lib/LSP/Features/Formatting.cpp"

# Then sequentially:
Task: "Replace onFormatting stub body with full implementation (T013 + T014 + T015 wired together)"
Task: "Add cancellation polling in onFormatting"
Task: "Add logging records in onFormatting"

# Observe green:
Task: "Re-run ctest -R formatting_test"
```

---

## Parallel Example: Cross-Story Parallelism

```bash
# Once Phase 2 is checkpoint-green, three parallel tracks can proceed:

# Track A — US1 (the MVP):
Task: "Phase 3 tasks T010..T019"

# Track B — US4 (structural; independent of code):
Task: "Phase 6 tasks T029..T032"

# Track C — Phase 8 documentation prep:
Task: "Phase 8 task T037: docs/CLAUDE.md §3 LSP routing entry update"
Task: "Phase 8 task T038: docs/CLAUDE.md §3 formatter routing entry update"

# Tracks A and B converge at Phase 7+8.
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational (CRITICAL — blocks all stories; lands the capability JSON amendment + stub dispatch)
3. Complete Phase 3: User Story 1
4. **STOP and VALIDATE**: Run `ctest -R formatting_test`; capture timing and assert SC-001 + SC-004 satisfied
5. Land as a sub-PR or a single feature PR with US2/US3/US4 to follow in the same merge

### Incremental Delivery (if splitting across PRs)

1. **PR 1 — Foundation**: Phase 1 + Phase 2. Lands a stubbed `nsl-lsp` that advertises both capabilities and returns `null` for every format request. `lifecycle_test::CapabilitiesExact` passes; nothing else for formatting works yet but no regression.
2. **PR 2 — MVP (US1)**: Phase 3. `formatting_test` green for all 10 fixtures. SC-001 satisfied. This is the merge that delivers the milestone's primary value.
3. **PR 3 — Range (US2 + US3)**: Phase 4 + Phase 5. Adds `rangeFormatting` and the CLI/LSP parity check.
4. **PR 4 — Structural Guard + Cancellation + Polish (US4 + Phase 7 + Phase 8)**: Lands the audit scripts, the SC-010 test, and the documentation roll-up.

Recommended: **single PR** delivering Phases 1–8 atomically. T5 is small (~500 LOC + tests) and the user-story decomposition serves planning clarity, not merge granularity.

### Parallel Team Strategy

With multiple developers:

1. **Developer A**: Phases 1–2 (setup + foundational), then Phase 3 (US1 MVP).
2. **Developer B**: starts on Phase 6 (US4 structural scripts — independent of code) in parallel with Developer A's Phase 1–2, then takes Phase 4 (US2) after Phase 3's implementation helpers (T013/T014/T015) are in.
3. **Developer C**: starts Phase 8 documentation tasks (T037, T038) in parallel; takes Phase 7 (cancellation) after Phase 3+4 land; final integration on Phase 8 polish.

Estimated calendar time with three developers: ~3–5 working days, including the Principle VIII observed-failing checkpoints.

---

## Notes

- [P] tasks = different files, no dependencies on incomplete tasks.
- [Story] label maps each implementation task to its user story for traceability; setup, foundational, cancellation, and polish phases have NO story label per the template convention.
- Each user story is independently completable and testable.
- Verify tests fail before implementing (Constitution Principle VIII). The Phase 2 sub-checkpoint (T009 observes the capability test going red→green inside Phase 2) is the foundational TDD example; Phase 3 (T012 → T019) is the canonical user-story TDD example.
- Commit after each task or logical group; use `Co-Authored-By:` trailer when AI-assisted per Constitution Development Workflow §"AI attribution."
- Stop at any checkpoint to validate the story independently.
- The Principle VII coupling action (T004 — amending the T3 contract in place) is what closes the forward-edge T3 itself flagged ("T4, T5, T9, T10 that modifies any frozen entry MUST update this contract in the same PR") — this is the first time the clause is exercised; set the precedent of in-place amendment for T9/T10 to follow.
