<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

---
description: "Task list for T3 — nsl-lsp Skeleton (Lifecycle, Document Sync, Diagnostics, Folding)"
---

# Tasks: T3 — `nsl-lsp` Skeleton

**Input**: Design documents from `/specs/010-t3-lsp-skeleton/`
**Prerequisites**: `plan.md` (required), `spec.md` (required), `research.md`, `data-model.md`, `contracts/`

**Tests**: Test tasks are **MANDATORY** per Constitution Principle
VIII (Test-First Development, NON-NEGOTIABLE). T3 introduces a new
test layer — LSP integration tests via gtest + subprocess
(`LspSession`) — for which Principle VI's per-layer accepted-driver
list does not constrain the choice (its enumeration is for compiler
test layers). Every user story below MUST land its integration-test
fixtures + test cases FIRST and observe the binaries FAILING against
the unchanged tree before any handler implementation lands. The
test gate stated verbatim in [`README.md`](../../README.md) §Roadmap
row T3 is materialized as `LifecycleSuite::README_TestGate_OpenErrorEditFix`
(T076) — when that test passes, T3's test gate is met.

**Organization**: Tasks are grouped by user story to enable
incremental MVP delivery. T3's MVP is US1+US2 together (the literal
README test gate); US3 (folding + cancellation) and US4
(architectural verification) build on top.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: User-story label (`[US1]`, `[US2]`, `[US3]`, `[US4]`)
- All file paths are relative to repo root unless noted

## Path Conventions

- New T3 files live under: `include/nsl/LSP/`, `lib/LSP/`, `tools/nsl-lsp/`, `test/lsp/`, `test/lsp/fixtures/`
- T3 amends: top-level `CMakeLists.txt`, `scripts/ci.sh`,
  `CLAUDE.md`, `docs/design/nsl_tooling_design.md`,
  `docs/CLAUDE.md` (T-track polish phase)

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create the directory tree, CMake skeletons, and the
empty `nsl-lsp` binary that exits 0. No real LSP behavior yet.

- [X] T001 Create the T3 directory tree: `mkdir -p include/nsl/LSP lib/LSP tools/nsl-lsp test/lsp/fixtures` and add `.gitkeep` placeholders so empty subdirs survive the initial commit
- [X] T002 [P] Author `lib/LSP/CMakeLists.txt` declaring `add_library(NSLLSP STATIC …)` (NOT `add_nsl_library`, which is hardcoded to the 9 §3 compiler-track layer names per `cmake/AddNSLLibrary.cmake` line 80–85; NSLLSP is a T-track tooling library, outside that table). Set `target_compile_features(NSLLSP PUBLIC cxx_std_17)`, `POSITION_INDEPENDENT_CODE ON`, `CXX_EXTENSIONS OFF`. Provide `target_include_directories(NSLLSP PUBLIC ${CMAKE_SOURCE_DIR}/include)`. Link via `target_link_libraries(NSLLSP PUBLIC nsl-driver)` — Principle II reuse: pulling `nsl-driver` transitively links the entire frontend (basic/preprocess/lex/ast/parse/sema/dialect/lower) per its `add_nsl_library` DEPENDS list. Sources list contains `Server.cpp` initially (T007 stub); other `.cpp` files added task-by-task in Phase 2 onwards.
- [X] T003 [P] Author `tools/nsl-lsp/CMakeLists.txt` declaring `add_executable(nsl-lsp main.cpp)` linking `NSLLSP` (PRIVATE). Apply the same `--whole-archive,$<TARGET_FILE:nsl-sema>,--no-whole-archive` link option pattern used by `tools/nslc/CMakeLists.txt` — without it, the linker drops the per-`Sn` constraint TUs in `lib/Sema/Constraints/` (which self-register at static-init time via `NSL_REGISTER_CONSTRAINT`) and Sema runs with an empty registry, producing zero diagnostics for any document — a silent FR-021 test-gate failure. Set `RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin` so `build/bin/nsl-lsp` lives alongside `nslc` and `nsl-opt` per `tools/nslc/CMakeLists.txt` precedent. `cxx_std_17`, `CXX_EXTENSIONS OFF`. Install rule gated by `NSL_INSTALL_DEV_TOOLS` is NOT applied — `nsl-lsp` is a T-track user-facing deliverable per Constitution Principle II §3, so an unconditional `install(TARGETS nsl-lsp RUNTIME DESTINATION bin)` is added.
- [X] T004 [P] Author `test/lsp/CMakeLists.txt` declaring four gtest binaries (`lifecycle_test`, `diagnostics_test`, `folding_test`, `cancellation_test`) and one test library (`LspSession`); each binary uses `gtest_discover_tests` with `TEST_PREFIX "lsp_"` so the resulting ctest target names are `lsp_lifecycle_test_*`, etc., per `scripts/ci.sh` stage 3 amendment naming
- [X] T005 [P] Author `test/lsp/lit.local.cfg.py` disabling lit's suffix-based discovery (matches `test/tooling/`'s precedent); this prevents lit from picking up `.nsl` fixtures that have no `RUN` line
- [X] T006 Author `include/nsl/LSP/Server.h` declaring exactly one public symbol — `int nsl::lsp::runStdioServer(int argc, char** argv);` — with SPDX header on line 1 and the include-guard convention from existing `include/nsl/Driver/Compilation.h`
- [X] T007 Author `lib/LSP/Server.cpp` providing a stub `runStdioServer` that returns 0 immediately (no I/O, no lifecycle); SPDX header on line 1
- [X] T008 Author `tools/nsl-lsp/main.cpp` (≤ 70 lines per Principle II thin-driver convention): SPDX header, `#include "nsl/LSP/Server.h"`, `int main(int argc, char** argv) { return nsl::lsp::runStdioServer(argc, argv); }` plus optional `--version` short-circuit mirroring `tools/nslc/main.cpp`'s pattern
- [X] T009 Wire CMake subdirectories: in top-level `CMakeLists.txt`, insert `add_subdirectory(lib/LSP)` between `add_subdirectory(lib)` and `add_subdirectory(tools)`. (Note: `lib/CMakeLists.txt` is documented as an aggregator for `add_nsl_library` §3 layer libraries only — `lib/LSP/` is a T-track tooling library outside that scope, so it's added from the top-level.) In `tools/CMakeLists.txt`, append `add_subdirectory(nsl-lsp)`. In `test/CMakeLists.txt`, append `add_subdirectory(lsp)` — gating happens at the top-level via `if(NSL_BUILD_TESTS)` (the project's actual flag, not `LLVM_BUILD_TESTS`; CMakeLists.txt:37 declares `option(NSL_BUILD_TESTS …)`). U2 finding from `/speckit-analyze` re-run is closed by this task.
- [X] T010 Run `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build --target nsl-lsp` inside the dev container; confirm the binary builds and `./build/bin/nsl-lsp </dev/null` exits 0 — **verified 2026-05-05**: 98/98 ninja steps green; `bin/nsl-lsp` produced; `--version` prints `nsl-lsp unknown` (configure-time string, matches `nslc --version` pattern in same env); `--help` prints contract-anchored usage; empty stdin returns exit 0 via stub `runStdioServer`.
- [X] T011 [P] Run `python3 scripts/check_spdx.py` against every newly added file; all pass — **verified 2026-05-05**: `spdx-check: 7 passed, 0 failed, 0 exempt`.

**Checkpoint**: Directory tree + CMake skeleton + stub binary build green; `nsl-lsp` exits 0 on no input. No LSP behavior yet.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Implement the foundational layer that every user story depends on — JSON-RPC framing, position-encoding utilities, the stderr logger, the include-path discovery, the cancellation-token primitive, the per-document state machine, the LSP-protocol layer's lifecycle handlers, and the `LspSession` test harness. Land lifecycle integration tests in this phase as test-first proof that the foundation is correct.

**⚠️ CRITICAL**: No user-story work can begin until this phase is complete — every later test starts with `initialize` → `initialized`, which depends on the lifecycle handlers landing here.

### 2a. Core utilities

- [X] T012 [P] Implement `lib/LSP/PositionEncoding.{h,cpp}` per data-model §2.6 / R6: `byteOffsetToLspPosition(StringRef line, size_t byteOffset)` and `lspPositionToByteOffset(StringRef line, uint32 character)`; ASCII fast-path short-circuit; SPDX headers
- [X] T013 [P] Implement `lib/LSP/Logger.{h,cpp}` per contract §7: stderr writer with ISO-8601-timestamp prefix, level filtering via `Logger::level()`, `NSL_LSP_LOG_LEVEL` env-var parser that exits non-zero on invalid value (FR-020e), `NSL_LSP_LOG` macro using `llvm::formatv`. Document-content blacklist per contract §7.5 (no API to log raw `didOpen.text` payloads)
- [X] T014 [P] Implement `lib/LSP/IncludeSearchPath.h` declaring the `IncludeSearchPath` struct per data-model §2.6; SPDX header
- [X] T015 Implement `lib/LSP/IncludeSearchPath.cpp` providing `IncludeSearchPath::fromEnv()` that reads `NSL_INCLUDE` per contract §8 (POSIX colon, Windows semicolon); empty / unset → empty `angle_paths`; INFO-log the resolved path per contract §8.4
- [X] T016 [P] Implement `lib/LSP/CancellationToken.h` per data-model §2.5: shared-pointer + atomic-bool + `isCancelled()`. Header-only (no .cpp); SPDX header
- [X] T017 [P] Implement `lib/LSP/RequestId.h` declaring `using RequestId = std::variant<int64_t, std::string>;` and helper hash/equality functors so `llvm::DenseMap<RequestId, …>` works

### 2b. Transport

- [X] T018 Implement `lib/LSP/JSONTransport.{h,cpp}` per data-model §2.1 / R1: `Content-Length:`-framed reader using a state machine that handles the four edge cases — incomplete header (block until more bytes), malformed JSON (log ERROR, skip message), partial body (block), EOF (return std::nullopt). `writeMessage` is mutex-protected per data-model §2.1
- [X] T019 Author a JSONTransport unit test (gtest) under `test/lsp/JSONTransport_test.cpp`: round-trip simple message, round-trip nested object, malformed-header rejection, malformed-JSON rejection, EOF detection. Lives in `test/lsp/CMakeLists.txt`'s test list

### 2c. Per-document state

- [X] T020 Implement `lib/LSP/NslTU.{h,cpp}` skeleton per data-model §2.4: `State` struct (version, contents, ast, diagnostics, symbols), `reparse(version, contents, includes)` stub that stores the inputs without actually parsing yet (real parse wiring in T040), `withState(fn)` accessor, `cancelInFlight()` stub
- [X] T021 Implement `lib/LSP/TUScheduler.{h,cpp}` skeleton per data-model §2.4: `llvm::StringMap<std::unique_ptr<NslTU>>` keyed on URI, `open(uri)`, `update(uri, version, contents, includes)`, `close(uri)`, `withState(uri, fn)`, `setOnDiagnostics(cb)`. Owns an `llvm::ThreadPool` constructed from `NSL_LSP_WORKERS` env var (default `min(hwconc, 4)` per R2; out-of-range value exits non-zero per contract §9)
- [X] T022 Implement `lib/LSP/NslServer.{h,cpp}` skeleton per data-model §2.3: owns a `TUScheduler` and an `IncludeSearchPath`; provides `openOrUpdate`, `close`, `withDiagnostics`, `foldingRange` (stub returning `{}`)

### 2d. Protocol layer + lifecycle handlers

- [X] T023 Implement `lib/LSP/NslLSPServer.{h,cpp}` skeleton per data-model §2.2: dispatch table (method-name → handler-fn), in-flight-request table (RequestId → CancellationToken), constructor takes `JSONTransport&` + `NslServer&`. `run()` enters the main read-loop
- [X] T024 Wire `NslLSPServer::onInitialize` per contract §1.1–§1.2: log INFO; build the canonical capabilities `llvm::json::Object` matching contract §1.2 byte-for-byte (post-canonicalization); set `initialized_ = false` until `initialized` notification arrives
- [X] T025 Wire `NslLSPServer::onInitialized` per contract §1.3: set `initialized_ = true`; log INFO
- [X] T026 Wire `NslLSPServer::onShutdown` per contract §5.1: drain pending parse work via TUScheduler; for every still-open document emit one final empty `publishDiagnostics`; respond `null`; mark `shutdown_received_ = true`. Subsequent non-`exit` requests respond with `InvalidRequest` (-32600)
- [X] T027 Wire `NslLSPServer::onExit` per contract §5.2: exit 0 if `shutdown_received_`, else exit 1
- [X] T028 Wire `NslLSPServer::run` main message loop: `transport_.readMessage()` → parse JSON-RPC envelope → if `id`+`method` route as request, if just `method` route as notification, if `id`+`result|error` (server-side: shouldn't occur at T3); reject pre-`initialized` non-`initialize`/`shutdown`/`exit` requests with `ServerNotInitialized` (-32002) per contract §1.3
- [X] T029 Wire `runStdioServer` in `lib/LSP/Server.cpp` to (a) initialize `Logger` from `NSL_LSP_LOG_LEVEL`, (b) build `IncludeSearchPath::fromEnv()`, (c) construct `JSONTransport(std::cin, std::cout)`, (d) construct `NslServer(includes)`, (e) construct `NslLSPServer(transport, server)`, (f) return `server.run()`. Top-level try/catch around the run loop logs ERROR on uncaught exception and returns 1

### 2e. Test harness

- [X] T030 Implement `test/lsp/LspSession.h` per harness contract §1: declarations for `LspEnvVars`, `LspSession` (constructor, destructor, sendRequest, sendNotification, waitForMessage/Response/Diagnostics, exitCode, capturedStderr); SPDX header
- [X] T031 Implement `test/lsp/LspSession.cpp` constructor + destructor: spawn `nsl-lsp` via `llvm::sys::ExecuteAndWait`-style pipe management or `llvm::sys::Process`; setup stdin/stdout/stderr pipes; pass `LspEnvVars` to child via inherited+override env. Destructor sends `shutdown`+`exit` if not already, waits up to 5 s, SIGKILL otherwise
- [X] T032 Implement `LspSession` send-side: `sendRequest(method, params)` allocates fresh int64 id, frames JSON-RPC, writes via stdin pipe, returns id. `sendNotification(method, params)` frames, writes
- [X] T033 Implement `LspSession` receive-side: background reader thread on stdout pipe reads `Content-Length` framing, parses each message into `llvm::json::Value`, deposits in a queue protected by mutex+condvar. `waitForMessage(timeout)` blocks; `waitForResponse(id, timeout)` filters by `id`; `waitForDiagnostics(timeout)` filters by `method == "textDocument/publishDiagnostics"`
- [X] T034 Implement `LspSession` stderr capture: separate background thread reads stderr pipe into an internal `std::string` until EOF; `capturedStderr()` returns the accumulated string (blocks until process exits)
- [X] T035 Implement `LspSession::exitCode()`: blocks until the subprocess terminates and returns its exit code
- [X] T036 [P] Author shared fixtures: `test/lsp/fixtures/clean_module.nsl` (one error-free module), `test/lsp/fixtures/empty.nsl` (zero bytes — verified via `wc -c == 0`)

### 2f. Lifecycle integration tests (test-first per Principle VIII)

> **Land tests T037–T041 FIRST and observe FAILING against the unchanged tree before T042–T047 implementation tasks.** Record the failing-state commit hash in the PR description per Principle VIII no-retrofitted-tests clause. (Most lifecycle handlers have stubs after T024–T028; the failure surface is the missing capability JSON, missing exit-code logic, etc.)

- [X] T037 Author `test/lsp/lifecycle_test.cpp` `LifecycleSuite::CapabilitiesExact`: define `kFrozenCapabilitiesJSON` literal matching contract §1.2 verbatim; spawn session, send `initialize`, assert byte-equal canonical JSON match. Land FAILING (capabilities object likely empty until T024 fully wires)
- [X] T038 Author `LifecycleSuite::ShutdownExit_Code0` (initialize → initialized → shutdown → exit → exitCode == 0); `LifecycleSuite::ExitWithoutShutdown_Code1` in same file. Land FAILING
- [X] T039 Author `LifecycleSuite::PreInitialized_RejectsRequest`: send `initialize`, do NOT send `initialized`, send `textDocument/foldingRange`; assert response carries error code `-32002` (`ServerNotInitialized`). Land FAILING
- [X] T040 Author `LifecycleSuite::InvalidLogLevel_ExitsNonZero`: spawn with `NSL_LSP_LOG_LEVEL=garbage`; assert process exits non-zero before `initialize` would respond; assert stderr identifies the bad value (regex match). Land FAILING
- [X] T041 Author `LifecycleSuite::NSLIncludeLoggedAtStartup`: spawn with `NSL_INCLUDE=/tmp/foo:/tmp/bar`; send `initialize`+`initialized`+`shutdown`+`exit`; assert `capturedStderr()` regex-matches `INFO .*include.*\\/tmp\\/foo.*\\/tmp\\/bar` per contract §8.4. Land FAILING
- [ ] T042 Run all `LifecycleSuite` tests via `ctest -R "lsp_lifecycle"`; observed FAILING; capture output to `${TMPDIR:-/tmp}/t3-lifecycle-red.txt` for the PR description

### 2g. CI integration

- [X] T043 Amend `scripts/ci.sh` stage 3 (unit-tests) to invoke `ctest --test-dir "$BUILD_DIR" -R "^lsp_" --output-on-failure` after the existing layer-test invocation; preserve existing exit-on-failure behavior; add a per-stage time-budget warning (informational) if combined wall-clock exceeds 30 s per SC-007
- [X] T044 Run `./scripts/ci.sh unit-tests` inside the dev container; verify the new `lsp_*` tests run, fail per T042 expectations, and the stage exits non-zero

### 2h. Make foundational tests PASS

- [X] T045 Make `LifecycleSuite::CapabilitiesExact` PASS — verify `NslLSPServer::onInitialize` produces the contract §1.2 canonical JSON byte-for-byte; canonicalize via `llvm::json::OStream` with `IndentSize = 0` and sorted keys (a small canonicalization helper in `lib/LSP/JsonCanonical.{h,cpp}` if needed)
- [X] T046 Make `LifecycleSuite::ShutdownExit_Code0` and `LifecycleSuite::ExitWithoutShutdown_Code1` PASS
- [X] T047 Make `LifecycleSuite::PreInitialized_RejectsRequest` PASS
- [X] T048 Make `LifecycleSuite::InvalidLogLevel_ExitsNonZero` PASS — `Logger::init` checks the env var, prints to stderr, calls `std::exit(1)` before `runStdioServer` proceeds
- [X] T049 Make `LifecycleSuite::NSLIncludeLoggedAtStartup` PASS — `runStdioServer` logs INFO after `IncludeSearchPath::fromEnv()` returns

**Checkpoint**: Foundation ready — JSON-RPC works, lifecycle tests all PASS, CI integration in place. User-story implementation can now begin.

---

## Phase 3: User Story 1 — NSL author sees Sema diagnostics live (Priority: P1) 🎯 MVP-half

**Goal**: Open a `.nsl` file with a Sema error in an LSP client; the server emits one `publishDiagnostics` notification carrying the Sema diagnostic at the originating `SourceRange`. Implements `textDocument/didOpen` and `publishDiagnostics`. Round-trips every M3 diagnostic-emitting `Sn` (23 of them) through the diagnostic-mapping seam.

**Independent Test**: `ctest -R "lsp_diagnostics_test\.SingleS01"` and `ctest -R "lsp_diagnostics_test\.CodeMapping"` pass — a fixture with one S1 violation produces exactly one LSP `Diagnostic` with `code = "S01"`, `severity = 1`, `source = "nsl-sema"`, and a `range` matching the offending identifier.

### Fixtures for User Story 1 (test-first)

> **Land fixtures T050–T056 FIRST.** They cannot trigger any test failure on their own (they're just `.nsl` files), but they're prerequisites for the test cases T057–T067.

- [X] T050 [P] [US1] Author `test/lsp/fixtures/s01_double_underscore.nsl` containing one occurrence of an `__`-bearing identifier per S1 (e.g., `module foo { reg foo__bar; }`); single error, no other diagnostics
- [X] T051 [P] [US1] Author the remaining 22 per-`Sn` fixtures `s02_<short>.nsl` … `s29_<short>.nsl` (skipping the 6 constructive `Sn`: S13/S18/S19/S23/S24/S27 per [`CLAUDE.md`](../../CLAUDE.md) §1 footnote). Each fixture triggers exactly one Sema constraint violation; cross-reference the M3 frozen diagnostic-string contract at [`specs/006-m3-sema/contracts/diagnostic-string.contract.md`](../006-m3-sema/contracts/diagnostic-string.contract.md) for shape
- [X] T052 [P] [US1] Author `test/lsp/fixtures/parse_error_missing_brace.nsl` — a module with an unterminated `{` block; surfaces a parser-level diagnostic (FR-017 in spec; `source = "nsl-parse"` in mapping)
- [X] T053 [P] [US1] Author `test/lsp/fixtures/preprocess_unresolved_include.nsl` containing `#include "nonexistent_file.nslh"`; surfaces a preprocessor-level diagnostic (FR-020c; `source = "nsl-preprocess"`)
- [X] T054 [P] [US1] Author `test/lsp/fixtures/utf8_comment.nsl` — a module with one S1 violation on a line that also contains a UTF-8 multi-byte comment string (e.g., `// 日本語 comment`); exercises the UTF-16 column conversion path
- [X] T055 [P] [US1] Author `test/lsp/fixtures/include_chain_main.nsl` + `test/lsp/fixtures/include_chain_helper.nslh` where `main.nsl` `#include`s `helper.nslh` and the diagnostic originates inside `helper.nslh`; exercises the `relatedInformation` include-from-notes path
- [X] T056 [P] [US1] Author `test/lsp/fixtures/two_errors_same_line.nsl` and `test/lsp/fixtures/two_errors_same_position.nsl` for the diagnostic-mapping sort-order tests

### Tests for User Story 1 (test-first)

> **Land tests T057–T067 FIRST and observe FAILING against the unchanged tree before T068–T072 implementation.** Tests fail at this point because `NslLSPServer::onDidOpen` is unimplemented — the server accepts the notification per the dispatch table but never schedules a parse, so no `publishDiagnostics` ever arrives, and `waitForDiagnostics` times out.

- [X] T057 [P] [US1] Author `test/lsp/diagnostics_test.cpp` skeleton (gtest fixture `DiagnosticsSuite`); add `EmptyArrayOnClean`: open `clean_module.nsl`, assert one `publishDiagnostics` arrives with empty `diagnostics` array
- [X] T058 [US1] Add `DiagnosticsSuite::SingleS01` per harness contract §4.1: open `s01_double_underscore.nsl`; assert exactly one diagnostic with `code = "S01"`, `severity = 1`, `source = "nsl-sema"`, `range` covers the offending identifier
- [X] T059 [US1] Add `DiagnosticsSuite::CodeMapping_S<NN>` parameterized over the 23 non-constructive `Sn` fixtures (one per row of [`contracts/diagnostic-mapping.contract.md`](./contracts/diagnostic-mapping.contract.md) §1); each variant asserts the `code` field matches `S<NN>`. Covers SC-002
- [X] T060 [US1] Add `DiagnosticsSuite::SortOrder_LineThenColumn` using `two_errors_same_line.nsl`; assert sort by `(range.start.line, range.start.character)`
- [ ] T061 [US1] Add `DiagnosticsSuite::SortOrder_SeverityOnTie` using `two_errors_same_position.nsl`; assert severity-ascending tiebreaker per contract §6
- [ ] T062 [US1] Add `DiagnosticsSuite::IncludeFromNotes` using `include_chain_main.nsl` + `include_chain_helper.nslh`; assert the diagnostic carries non-empty `relatedInformation` whose entries reference the helper's URI per contract §5
- [X] T063 [US1] Add `DiagnosticsSuite::ParseError` using `parse_error_missing_brace.nsl`; assert `source = "nsl-parse"`
- [X] T064 [US1] Add `DiagnosticsSuite::PreprocessError` using `preprocess_unresolved_include.nsl`; assert `source = "nsl-preprocess"` and `code` is the preprocessor's frozen ID for unresolved include (per FR-020c)
- [X] T065 [US1] Add `DiagnosticsSuite::UTF8Comment` using `utf8_comment.nsl`; assert the `range.start.character` is computed as a UTF-16 code-unit offset (verifies the conversion path in T012)
- [X] T066 [US1] Add `DiagnosticsSuite::Determinism_TwoRunsByteIdentical` per harness §4.2: spawn-and-drive the same fixture twice, capture both `publishDiagnostics` payloads as canonical JSON byte strings, assert byte-equal. Covers SC-003
- [X] T067 [US1] Run `ctest -R "lsp_diagnostics_test"` and observe ALL US1 tests FAILING; capture output to `${TMPDIR:-/tmp}/t3-us1-red.txt`

### Implementation for User Story 1

- [X] T068 [US1] Implement `lib/LSP/DiagnosticMapper.{h,cpp}` per [`contracts/diagnostic-mapping.contract.md`](./contracts/diagnostic-mapping.contract.md) §1–§7: `toLspDiagnostic` and `toLspDiagnosticArray` free functions; `code` lookup table (M3 message-prefix → `Sn` keyed); severity mapping; range conversion through `byteOffsetToLspPosition` (T012); `source` disambiguation via message-prefix heuristic per §4 with DEBUG-log fallback; `relatedInformation` materialization per §5
- [X] T069 [US1] Implement `NslTU::reparse` body: invoke `nsl::driver::Compilation` (existing M3 entry point) on the stored `contents` with the `IncludeSearchPath`'s angle paths and the document URI's parent directory as the quote-form root; capture the resulting `CompilationUnit`, `SymbolTable`, and `DiagnosticEngine.diagnostics()` into the `State` struct
- [X] T070 [US1] Wire `NslLSPServer::onDidOpen` per [`contracts/lsp-protocol.contract.md`](./contracts/lsp-protocol.contract.md) §2.1: parse `params.textDocument`, call `NslServer::openOrUpdate(uri, version, text)`, register a callback that on TUScheduler completion invokes `publishDiagnostics`. Handle the second-`didOpen`-on-same-URI WARN case
- [X] T071 [US1] Wire the diagnostics-publication callback path: TUScheduler calls `setOnDiagnostics` callback registered by NslServer → NslServer calls `NslLSPServer::publishDiagnostics(uri, version, diags)` → NslLSPServer maps via `toLspDiagnosticArray` and writes the `textDocument/publishDiagnostics` notification through the transport
- [X] T072 [US1] Run `ctest -R "lsp_diagnostics_test"`; iterate until ALL US1 tests PASS. Common failure modes: position-encoding off-by-one (line vs character), `code` lookup-table prefix mismatch, source-disambiguation heuristic returning wrong origin

**Checkpoint**: US1 complete. Open a `.nsl` file with any Sema error in any LSP client → red squiggle appears at the right location with the correct `code`/`severity`/`source`. README test gate is **half-met** (open-side) — the edit-side requires US2.

---

## Phase 4: User Story 2 — NSL author edits and watches diagnostics re-emit (Priority: P1) 🎯 MVP completion

**Goal**: Edit an open document; the server re-runs parse + sema and emits a fresh `publishDiagnostics` reflecting the new state. When the error is fixed, the diagnostics array becomes empty. Implements `textDocument/didChange` (Full sync per Q1) and `textDocument/didClose`. Together with US1, this completes the README test gate (FR-021).

**Independent Test**: `ctest -R "lsp_lifecycle_test\.README_TestGate"` passes — the literal test gate stated in [`README.md`](../../README.md) §Roadmap row T3.

### Tests for User Story 2 (test-first)

> **Land tests T073–T076 FIRST and observe FAILING.** Tests fail because `NslLSPServer::onDidChange` is unimplemented (the dispatch table accepts the notification but the handler is a stub).

- [X] T073 [US2] Add `DiagnosticsSuite::EditClearsResolvedDiagnostic`: open `s01_double_underscore.nsl`, assert one diagnostic; send `didChange` rewriting to a clean module; assert next `publishDiagnostics` has empty `diagnostics`. Land FAILING
- [X] T074 [US2] Add `DiagnosticsSuite::EditIntroducesError`: open `clean_module.nsl`, assert empty diagnostics; send `didChange` introducing an `__`-name; assert next `publishDiagnostics` has the new S1 diagnostic
- [X] T075 [US2] Add `DiagnosticsSuite::RapidEdits_OnlyLatestVersionPublished` (FR-008): open clean, send 5 `didChange`s rapidly with versions 2..6 each toggling between clean and one-error states; wait for the final `publishDiagnostics`; assert its `version == 6` and contents reflect the version-6 source. Add `DiagnosticsSuite::DidClose_FinalEmptyDiagnostics`: open `s01...`, observe diagnostic; send `didClose`; assert one final `publishDiagnostics` with empty `diagnostics`. Add `DiagnosticsSuite::IncrementalChangePayload_Rejected` (FR-006): send `didChange` whose `contentChanges[0]` carries a `range` field; assert no `publishDiagnostics` follows AND stderr contains an ERROR log line. Add `DiagnosticsSuite::StaleVersion_Ignored`: send `didChange` with version=1 after version=3 has been seen; assert WARN log + no diagnostic update
- [X] T076 [US2] Add `LifecycleSuite::README_TestGate_OpenErrorEditFix` per [`contracts/lsp-test-harness.contract.md`](./contracts/lsp-test-harness.contract.md) §4.1 — the literal materialization of the README test gate. Land FAILING
- [X] T077 [US2] Run `ctest -R "lsp_(diagnostics|lifecycle)_test"` and observe US2 tests FAILING; capture to `${TMPDIR:-/tmp}/t3-us2-red.txt`

### Implementation for User Story 2

- [X] T078 [US2] Wire `NslLSPServer::onDidChange` per contract §2.2: validate exactly-one `contentChanges` element with no `range` field (reject otherwise with ERROR log per FR-006); update `NslTU.contents` via `TUScheduler::update`; reject `version <= latest_version` with WARN log
- [X] T079 [US2] Implement TUScheduler stale-drop per FR-008: each pending reparse worker checks `latest_version` after completing; if the result's version < `latest_version`, drop it (do not invoke the diagnostics callback). Mutex-protect the version comparison
- [X] T080 [US2] Wire `NslLSPServer::onDidClose` per contract §2.3: cancel in-flight reparse, emit one final empty `publishDiagnostics` for the URI's last known version, remove the URI from the scheduler
- [X] T081 [US2] Add `version` field to `PublishDiagnosticsParams` (FR-009): the publish callback path threads the version that was diagnosed (NOT the latest version) through to the wire payload
- [X] T082 [US2] Run `ctest -R "lsp_(diagnostics|lifecycle)_test"`; iterate until ALL US2 tests PASS, including `README_TestGate_OpenErrorEditFix`

**Checkpoint**: US1+US2 complete = T3 README test gate met. MVP fully achieved. Editor-side smoke test (quickstart §4 / §5) should now light up red squiggles that disappear on edit-fix.

---

## Phase 5: User Story 3 — NSL author folds blocks (Priority: P2)

**Goal**: Editor sends `textDocument/foldingRange`; server returns one `FoldingRange` per multi-line block-opener AST node plus one per multi-line block comment. Cancellation works end-to-end (real cancellation per Q5).

**Independent Test**: `ctest -R "lsp_folding_test"` passes; `ctest -R "lsp_folding_test\.Cancellation"` completes within SC-010's 200 ms budget.

### Fixtures for User Story 3 (test-first)

- [X] T083 [P] [US3] Author `test/lsp/fixtures/module_with_blocks.nsl` covering all 16 block-opener productions per [`contracts/folding-range.contract.md`](./contracts/folding-range.contract.md) §1 (one occurrence each of `module`, `declare`, `func`, `proc`, `state`, `seq`, `alt`, `any`, `par`, `if`/`else`, `for`, `while`, `generate`, `_init`, `struct`); each block spans ≥ 2 source lines so a fold is emitted
- [X] T084 [P] [US3] Author `test/lsp/fixtures/multiline_block_comment.nsl` containing one `/* ... */` spanning ≥ 2 lines plus one single-line `// ...` plus one single-line `/* ... */` (the latter must NOT fold per §3)
- [X] T085 [P] [US3] Author `test/lsp/fixtures/single_line_blocks.nsl` containing only single-line `{...}` blocks — fold response must be `[]`
- [ ] T086 [P] [US3] Author `test/lsp/fixtures/include_adjusts_lines.nsl` (`#include "include_helper.nslh"`-only stub plus one `module` after the include) and `test/lsp/fixtures/include_helper.nslh` to verify FR-011 line-number resolution per §8 of the folding contract
- [ ] T087 [P] [US3] Author `test/lsp/fixtures/cancellation_target.nsl` constructed to produce ≥ 10000 AST nodes (e.g., a large `generate` loop unrolling many copies of a multi-line block); used by SC-010 cancellation test
- [X] T088 [P] [US3] Author `test/lsp/fixtures/large_file.nsl` ≥ 1500 lines for SC-004 latency budget
- [ ] T089 [P] [US3] Author `test/lsp/fixtures/folding_parse_error.nsl` — partial block-opener structure with a missing `}` so the M2 parser's recovery produces a partial AST; folding contract §6 requires we still return what was parsed

### Tests for User Story 3 (test-first)

- [X] T090 [US3] Author `test/lsp/folding_test.cpp` skeleton (`FoldingSuite` gtest fixture); add `AllBlockOpeners` per harness §4 / folding contract §8: open `module_with_blocks.nsl`, send `foldingRange`, assert response array length matches the 16 expected folds with the correct `(startLine, endLine)` pairs (sorted ascending). Land FAILING
- [X] T091 [US3] Add `FoldingSuite::SingleLineBlockNotFolded`, `MultiLineBlockComment` (asserts `kind = "comment"` literal), `MultiLineBlockComment_KindFieldExact` (asserts the literal four-character string `"comment"`, not `"Comment"`/`"COMMENT"`), `ZeroBasedLines`, `IncludeAdjustsLines` (post-`#line` per Principle IV), `SortOrder` (response is sorted by `(startLine, endLine)`), `Determinism_TwoRunsByteIdentical` per folding contract §8
- [X] T092 [US3] Add `FoldingSuite::ParseErrorRecovery` per folding contract §6: opens `folding_parse_error.nsl`; assert response is a valid JSON-RPC `result` (NOT `error`) carrying whatever folds the parser's recovery managed to recognize; assert no crash, no LSP error response
- [ ] T093 [US3] Add `FoldingSuite::Cancellation_Under200ms` per harness §4.5 and SC-010: open `cancellation_target.nsl`, send `foldingRange`, immediately send `$/cancelRequest`, assert response carries `error.code = -32800`, `error.message = "request cancelled"`, and elapsed time < 200 ms
- [X] T094 [US3] Add `DiagnosticsSuite::OpenLatency_Under250ms_For1500Lines` per harness §4.4 and SC-004: opens `large_file.nsl`, measures elapsed time to first `publishDiagnostics`, asserts < 250 ms; gates assertion on Linux x86_64 with ≥ 4 cores via `GTEST_SKIP_("slow runner")` heuristic on smaller hosts
- [X] T095 [P] [US3] Author `test/lsp/cancellation_test.cpp` for cancellation edge cases per FR-020j: `CancellationSuite::CancelCompletedRequest` (silently ignored, no log above DEBUG); `CancelNotificationId` (silently ignored — notifications have no id); `CancelNeverSeenRequest` (silently ignored); `CancelDuringFoldingThenNewFoldingSucceeds` (cancel in-flight, immediately re-request, second succeeds)
- [X] T096 [US3] Run `ctest -R "lsp_(folding|cancellation)_test"` and observe ALL US3 tests FAILING; capture to `${TMPDIR:-/tmp}/t3-us3-red.txt`

### Implementation for User Story 3

- [X] T097 [P] [US3] Implement `lib/LSP/FoldingRangeBuilder.{h,cpp}` per [`contracts/folding-range.contract.md`](./contracts/folding-range.contract.md) §1–§7: derive from `nsl::ast::ASTVisitor`; visit each block-opener node listed in §1; emit `FoldingRange{startLine, endLine, kind?}` if `endLine > startLine`; post-process M1 lexer's block-comment tokens for §3 multi-line block-comment folds; cancellation polling at every visited block-opener per §5
- [X] T098 [US3] Wire `NslServer::foldingRange(uri, token)`: read `NslTU::State.ast` via `TUScheduler::withState`; instantiate `FoldingRangeBuilder(unit, sm, token)`; return `builder.build()` (or empty vector if cancelled)
- [X] T099 [US3] Wire `NslLSPServer::onFoldingRange` per LSP-protocol contract §4: parse `params.textDocument.uri`, allocate fresh `CancellationToken`, register in InFlightTable keyed by request id, dispatch to `NslServer::foldingRange`; on completion send `result` response and remove from InFlightTable; on cancellation send `error{code: -32800, message: "request cancelled"}` per LSP-protocol contract §6.2
- [X] T100 [US3] Wire `NslLSPServer::onCancelRequest` per LSP-protocol contract §6.1: extract `params.id` (variant — int or string), look up in InFlightTable, flip the token's atomic; if not present, log DEBUG and ignore (FR-020j)
- [X] T101 [US3] Run `ctest -R "lsp_(folding|cancellation)_test"`; iterate until ALL US3 tests PASS

**Checkpoint**: All P1 + P2 user stories complete. Editor folding works; cancellation works end-to-end within SC-010 budget.

---

## Phase 6: User Story 4 — Architectural verification (Priority: P2)

**Goal**: Verify structurally that `nsl-lsp` reuses `libNSLFrontend.a` (no duplication), exposes a single public header, and provides clean extension points for T4/T5/T9/T10. Most of US4 is verified by construction through Phases 1–5; this phase adds one mechanical assertion script.

**Independent Test**: `ctest -R "lsp_architecture_test"` passes; the linker-map check exits 0.

### Tests for User Story 4

- [X] T102 [P] [US4] Author `test/lsp/architecture_test.cpp` `ArchitectureSuite::SinglePublicHeader`: at compile time, assert the `include/nsl/LSP/` directory contains exactly one header file (`Server.h`) — implemented via a generated header listing the directory contents at CMake-configure time; or as a CMake-time custom-target that fails the build if a second header appears
- [X] T103 [P] [US4] Author `scripts/lsp_link_audit.sh` (POSIX shell, SPDX header) that runs `nm --defined-only build/bin/nsl-lsp` and asserts that no `nsl::Lex::*`, `nsl::Parse::*`, `nsl::Sema::*`, `nsl::Preprocess::*`, `nsl::AST::*` symbol appears more than once in the binary; exit non-zero on duplicate. Wired into `scripts/ci.sh` stage 2 (static checks) per Principle IX
- [X] T104 [US4] Author `ArchitectureSuite::Linker_NoDuplicatedFrontendSymbols`: invokes `scripts/lsp_link_audit.sh` via `system()` and asserts exit code 0; covers SC-005

### Implementation for User Story 4

> US4 is structural — the implementation work was done in Phases 1–5. The only remaining work is gating-test wiring.

- [X] T105 [US4] Run `ctest -R "lsp_architecture_test"` and verify all US4 tests PASS. If any fail, that's a Principle II violation in the existing implementation requiring root-cause investigation
- [X] T106 [US4] Run `./scripts/ci.sh static-checks` to verify `scripts/lsp_link_audit.sh` is invoked and passes

**Checkpoint**: All four user stories complete. Constitution Principle II reuse verified mechanically.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Principle VII coupling updates, documentation, and final verification. Same-PR-as-implementation rule for the doc updates per Constitution Principle VII.

- [X] T107 [P] Update `CLAUDE.md` §2.1 LSP-method roll-up: rows for `publishDiagnostics` and `textDocument/foldingRange` annotated as "delivered at T3 (PR #N)" with the PR number filled at merge time
- [X] T108 [P] Update `docs/design/nsl_tooling_design.md` §3 with a `> **T3 status**: delivered.` annotation linking to `specs/010-t3-lsp-skeleton/contracts/`
- [X] T109 [P] Update `docs/CLAUDE.md` §3 task-→-section map: the existing "Implementing the LSP" entry stays valid; if line-range references in §§4–7 shifted due to T108's annotation, update them per Principle VII line-range maintenance rule
- [X] T110 [P] Update root `README.md` §Roadmap row T3 — mark T3 as delivered (status row) at PR merge time; previously-checked-off `M3` row stays as is
- [ ] T111 Run full local CI: `./scripts/ci.sh all` inside the dev container; verify all six stages green (build matrix → static checks → unit/layer → lowering → e2e → formal-when-applicable). The new `lsp_*` tests run in stage 3
- [ ] T112 Run determinism check: `./scripts/check_determinism.sh` (or equivalent stage cell) covering the two-run byte-identical assertion for `publishDiagnostics` per SC-003
- [ ] T113 Walk through `quickstart.md` §1–§9 verbatim inside the dev container; record which sections pass and any divergences between contract and implementation. Update quickstart and the relevant contract in lockstep if a divergence is found (Principle VII)
- [X] T114 Verify SC-007 30-second combined budget: `time ctest --test-dir build -R "^lsp_"`; record actual wall-clock; fail the merge gate if > 30 s
- [X] T115 [P] Verify no source-content blacklist violation per logging contract §7.5: grep `lib/LSP/` for any `Logger::log` call site that could pass `didOpen.text` or `didChange.contentChanges[0].text` as the message argument; assert none. Authored as a one-shot `scripts/check_lsp_log_content.sh` if static analysis warrants
- [X] T116 [P] Capture the binary size of `build/bin/nsl-lsp` for posterity (informational; no SC budget at T3) and record in PR description
- [X] T117 Author the PR per Constitution external-integrations + Principle VIII no-retrofitted-tests clause: include the failing-state commit hashes from `${TMPDIR:-/tmp}/t3-{lifecycle,us1,us2,us3}-red.txt` in the PR body to establish red→green progression

**Checkpoint**: T3 ready to merge.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: No dependencies — can start immediately.
- **Phase 2 (Foundational)**: Depends on Phase 1 completion. **BLOCKS all user stories** — every later test starts with `initialize`/`initialized`, which only exists after T024–T028.
- **Phase 3 (US1)**: Depends on Phase 2. Delivers half the README test gate.
- **Phase 4 (US2)**: Depends on Phase 3 (uses the diagnostic seam built in US1; the README test gate test combines both halves). Delivers the full README test gate at T076.
- **Phase 5 (US3)**: Depends on Phase 2 (for the ASTVisitor + lifecycle); does NOT depend on Phase 3/4 (folding is independent of diagnostic emission). Could start in parallel with Phase 3 if staffed.
- **Phase 6 (US4)**: Depends on Phases 1–5 — verifies structural properties of the completed implementation.
- **Phase 7 (Polish)**: Depends on all prior phases.

### User Story Dependencies

- **US1 (P1)**: Independent of US2/US3/US4. Delivers an MVP-half — open a file, see diagnostics; an editor with the binary plugged in already provides value at this point.
- **US2 (P1)**: Depends on US1's diagnostic seam (`DiagnosticMapper`, `NslTU::reparse`, the diagnostics-publication callback). Does NOT depend on US3/US4. Together with US1, delivers the README test gate.
- **US3 (P2)**: Depends on US1's `NslTU::reparse` to produce an AST (so the folding builder has something to walk). Does NOT depend on US2 (folding doesn't need edit-then-re-fold to work — `didOpen` produces an AST that's enough). Could start in parallel with US2 if staffed.
- **US4 (P2)**: Depends on all of US1+US2+US3 having shipped (verifies their structural properties).

### Within Each User Story

- Fixtures FIRST (so test cases have something to open).
- Tests SECOND (test-first per Principle VIII; observed FAILING against the unchanged tree).
- Implementation THIRD; iterate to GREEN.
- The XFAIL → GREEN progression is recorded in commit history (or in PR description if squash-merged).

### Parallel Opportunities

- All `[P]`-marked Setup tasks can run in parallel (T002, T003, T004, T005 — different CMake files; T011 is post-everything-exists).
- Phase 2's utility-creation tasks T012/T013/T014/T016/T017 (different files, no dependencies) run in parallel.
- Phase 2's lifecycle test-authoring tasks T037–T041 are mostly sequential (same `lifecycle_test.cpp` file) but can be authored in one task if the implementer prefers.
- Phase 3's fixture-authoring tasks T050–T056 all run in parallel (different `.nsl` files).
- Phase 5's fixture-authoring tasks T083–T089 all run in parallel.
- Phase 7's documentation-update tasks T107–T110 all run in parallel (different files).
- A two-developer team could ship US1 and US3 in parallel after Phase 2; US2 then sequences after US1.

---

## Parallel Example: Phase 2 utilities

```bash
# After T011, the four utility files can be authored in parallel:
Task: "T012 [P] Implement lib/LSP/PositionEncoding.{h,cpp}"
Task: "T013 [P] Implement lib/LSP/Logger.{h,cpp}"
Task: "T014 [P] Implement lib/LSP/IncludeSearchPath.h"
Task: "T016 [P] Implement lib/LSP/CancellationToken.h"
```

## Parallel Example: User Story 1 fixtures

```bash
# All seven fixture-authoring tasks run concurrently:
Task: "T050 [P] Author test/lsp/fixtures/s01_double_underscore.nsl"
Task: "T051 [P] Author 22 per-Sn fixtures s02..s29 (skipping constructive)"
Task: "T052 [P] Author test/lsp/fixtures/parse_error_missing_brace.nsl"
Task: "T053 [P] Author test/lsp/fixtures/preprocess_unresolved_include.nsl"
Task: "T054 [P] Author test/lsp/fixtures/utf8_comment.nsl"
Task: "T055 [P] Author test/lsp/fixtures/include_chain_main.nsl + helper"
Task: "T056 [P] Author test/lsp/fixtures/two_errors_same_{line,position}.nsl"
```

---

## Implementation Strategy

### MVP First (US1+US2 — the README test gate)

1. Complete Phase 1 (Setup).
2. Complete Phase 2 (Foundational — including lifecycle tests).
3. Complete Phase 3 (US1 — diagnostics on open). **STOP and VALIDATE**: `s01_double_underscore.nsl` opens with a red squiggle in any LSP client.
4. Complete Phase 4 (US2 — edit re-diagnoses). **STOP and VALIDATE**: `LifecycleSuite::README_TestGate_OpenErrorEditFix` PASS — T3's load-bearing test gate is met.
5. T3 is shippable as MVP at this point.

### Incremental Delivery

1. Setup + Foundational → foundation ready.
2. US1 → editor diagnostics on open (MVP-half).
3. US2 → README test gate passes (full MVP).
4. US3 → folding + cancellation (P2 features).
5. US4 → architectural assertion (P2 verification).
6. Polish → coupling docs + final CI green.

### Parallel Team Strategy

With two developers after Foundational (Phase 2) completes:

1. Developer A: US1 → US2 (sequential within the diagnostic seam).
2. Developer B: US3 (independent of the diagnostic seam; needs only the AST that US1's `NslTU::reparse` produces — Dev B can stub a minimal reparse for local development if waiting on Dev A's `NslTU::reparse` is undesirable).
3. Both meet at Phase 6 / 7.

---

## Notes

- `[P]` tasks = different files, no dependencies on incomplete tasks.
- `[Story]` label maps each task to the user story it serves.
- Each user story should be independently completable and testable.
- Verify tests fail (capture to `${TMPDIR:-/tmp}/t3-*-red.txt`) before implementing.
- Commit after each task or logical group; PR squash-merges still preserve the failing-state hash via the PR body.
- Stop at any checkpoint to validate independently.
- Avoid: same-file concurrent edits, cross-story dependencies that break independence, retrofitted tests, capability advertisement without contract update (Principle VII coupling).
