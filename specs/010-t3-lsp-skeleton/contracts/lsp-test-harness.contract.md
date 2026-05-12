<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: LSP Integration Test Harness

**Branch**: `010-t3-lsp-skeleton` | **Date**: 2026-05-05
**Anchors**: spec FR-021, FR-022, FR-023; SC-001, SC-003, SC-007, SC-009, SC-010

This contract freezes the shape of the LSP integration test
harness — the in-tree gtest helper that drives `nsl-lsp` as a
subprocess and asserts on its responses. The harness is the
mechanical implementation of the test gate stated in
[`README.md`](../../../README.md) §Roadmap row T3.

---

## §1 `LspSession` API

The harness lives at `test/lsp/LspSession.{h,cpp}` and provides:

```cpp
namespace nsl::lsp::test {

struct LspEnvVars {
    std::optional<std::string> nsl_include;
    std::optional<std::string> nsl_lsp_log_level;
    std::optional<std::string> nsl_lsp_workers;
    // unset entries inherit the test process's environment
};

class LspSession {
public:
    explicit LspSession(LspEnvVars env = {});
    ~LspSession();   // sends shutdown + exit if not already

    // --- send side ---

    // Sends a JSON-RPC request and returns the request id.
    int64_t sendRequest(llvm::StringRef method, llvm::json::Value params);

    // Sends a JSON-RPC notification.
    void sendNotification(llvm::StringRef method, llvm::json::Value params);

    // --- receive side ---

    // Block (with timeout) for the next incoming response or
    // notification. Returns `std::nullopt` on timeout or reader EOF;
    // otherwise returns the parsed JSON-RPC envelope.
    std::optional<llvm::json::Value> waitForMessage(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(2000));

    // Block for the response to a specific request id. Returns
    // `std::nullopt` on timeout or reader EOF.
    std::optional<llvm::json::Value> waitForResponse(
        int64_t id,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(2000));

    // Block for the next publishDiagnostics notification (any URI).
    // Returns `std::nullopt` on timeout or reader EOF.
    std::optional<llvm::json::Value> waitForDiagnostics(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(2000));

    // --- subprocess control ---

    int exitCode();   // blocks until the process exits; returns its code

    // Captured stderr of the subprocess. Available after exit.
    std::string capturedStderr();

private:
    // implementation detail: pipes, llvm::sys::Process handle, mutex,
    // background reader thread for stdout demux.
};

} // namespace nsl::lsp::test
```

### §1.1 Lifecycle

`LspSession::LspSession()` spawns `nsl-lsp` (path resolved via
ctest's `CMAKE_BINARY_DIR/bin/nsl-lsp`) with the configured
environment. The constructor returns once the subprocess has
started and pipes are open; it does NOT auto-send `initialize` —
each test does that explicitly so that lifecycle-error scenarios
can be tested directly.

`~LspSession()` sends `shutdown` + `exit` if the test did not
already, then waits for the process to terminate. If the process
does not exit within 5 seconds, the destructor sends SIGKILL and
records a test failure.

### §1.2 Threading model

The constructor spawns one background thread that reads from
the subprocess's stdout pipe, frames JSON-RPC envelopes, and
deposits them into an internal queue. `waitForMessage` /
`waitForResponse` / `waitForDiagnostics` block on a condition
variable until the queue produces a matching envelope or the
timeout expires.

This design lets a test send a notification (which has no
response) and then later block for a `publishDiagnostics`
notification that will arrive asynchronously.

### §1.3 Timeout discipline

Default timeout is 2000 ms — generous for the SC-004 250 ms
budget but tight enough that a hung server fails the test in
under a minute even at the SC-007 cap of 30 s.

Each `waitForX` API takes a timeout override for tests that have
a known reason to expect a longer or shorter wait (e.g.,
`folding_test::Cancellation` uses 500 ms — well under the 2-s
default but well over the 200 ms SC-010 budget so the test fails
loudly if the budget is missed).

### §1.4 Stderr capture

Stderr is captured into an internal buffer continuously. After
the subprocess exits, `capturedStderr()` returns the full string.
Tests use this for SC-009 (`ERROR`/`WARN` log records on failure)
and FR-020d–g coverage.

---

## §2 Test-binary layout

Four test binaries under `test/lsp/`:

| Binary                | Coverage                                                  |
| --------------------- | --------------------------------------------------------- |
| `lifecycle_test`      | FR-001/002/003 (initialize, shutdown, exit, capabilities), §6.4 cancellation-protocol coverage at the lifecycle level |
| `diagnostics_test`    | FR-005/006/007/010/011/012/013, FR-020c (#include diagnostics), SC-001, SC-002, SC-003 |
| `folding_test`        | FR-014/015/016/017/022, SC-010 cancellation budget |
| `cancellation_test`   | FR-020h/i/j cross-cutting cancellation behavior (multi-request, race conditions) |

Each binary registers itself with ctest via the existing
`add_executable + gtest_discover_tests` CMake pattern.

---

## §3 Fixture corpus layout

```text
test/lsp/fixtures/
├── README.md                                # short index of fixture purpose
├── empty.nsl                                # 0-byte file
├── clean_module.nsl                         # one module, no errors
├── s01_double_underscore.nsl                # SC-002 / S01
├── s02_wire_with_init.nsl                   # SC-002 / S02
├── …                                        # (one per Sn with a diagnostic)
├── s29_init_block_placement.nsl             # SC-002 / S29
├── parse_error_missing_brace.nsl            # FR-017
├── preprocess_unresolved_include.nsl        # FR-020c
├── module_with_blocks.nsl                   # folding_test::AllBlockOpeners
├── multiline_block_comment.nsl              # folding_test::MultiLineBlockComment
├── single_line_blocks.nsl                   # folding_test::SingleLineBlockNotFolded
├── include_adjusts_lines.nsl                # folding_test::IncludeAdjustsLines
├── include_helper.nslh                      # included by include_adjusts_lines.nsl
├── large_file.nsl                           # ≥1500 lines for SC-004 budget
├── cancellation_target.nsl                  # ≥10000 nodes for SC-010 budget
└── utf8_comment.nsl                         # diagnostics_test::UTF8Comment
```

Each fixture is a plain `.nsl` source file — no embedded RUN
lines, no FileCheck directives. The expected JSON shapes are
constructed in the corresponding `*_test.cc` source files (or
loaded from sibling `.expected.json` files for very large
expectations).

---

## §4 Per-test contract excerpts

### §4.1 The "test gate" — FR-021

```cpp
TEST_F(LifecycleSuite, README_TestGate_OpenErrorEditFix) {
    LspSession s;
    auto initId = s.sendRequest("initialize", llvm::json::Object{});
    auto initResp = s.waitForResponse(initId);
    ASSERT_TRUE(initResp.has_value());
    ASSERT_TRUE(initResp->getAsObject()->getObject("result"));

    s.sendNotification("initialized", llvm::json::Object{});

    s.sendNotification("textDocument/didOpen", llvm::json::Object{
        {"textDocument", llvm::json::Object{
            {"uri",        "file:///s01.nsl"},
            {"languageId", "nsl"},
            {"version",    1},
            {"text",       "module foo { reg foo__bar; }"} }}}); // S01

    auto diag1 = s.waitForDiagnostics();
    ASSERT_TRUE(diag1.has_value());
    auto* arr = diag1->getAsObject()->getObject("params")
                    ->getArray("diagnostics");
    EXPECT_EQ(arr->size(), 1u);
    EXPECT_EQ(arr->at(0).getAsObject()->getString("code"), "S01");

    s.sendNotification("textDocument/didChange", llvm::json::Object{
        {"textDocument",    llvm::json::Object{{"uri", "file:///s01.nsl"},
                                                 {"version", 2}}},
        {"contentChanges",  llvm::json::Array{
             llvm::json::Object{{"text", "module foo { reg foo_bar; }"}} }}});

    auto diag2 = s.waitForDiagnostics();
    ASSERT_TRUE(diag2.has_value());
    auto* arr2 = diag2->getAsObject()->getObject("params")
                     ->getArray("diagnostics");
    EXPECT_EQ(arr2->size(), 0u);

    auto shutId = s.sendRequest("shutdown", llvm::json::Value{nullptr});
    s.waitForResponse(shutId);
    s.sendNotification("exit", llvm::json::Value{nullptr});
    EXPECT_EQ(s.exitCode(), 0);
}
```

This is the literal materialization of the README-§Roadmap-row-T3
test gate. Its name (`README_TestGate_OpenErrorEditFix`)
identifies the role explicitly.

### §4.2 Determinism — SC-003

```cpp
TEST_F(DiagnosticsSuite, Determinism_TwoRunsByteIdentical) {
    auto runOnce = [](){
        LspSession s;
        // ... full sequence as above, capture publishDiagnostics ...
        return capturedDiagnosticsBytes;
    };
    auto a = runOnce();
    auto b = runOnce();
    EXPECT_EQ(a, b);   // byte-equal
}
```

### §4.3 Capability assertion — SC-008

```cpp
TEST_F(LifecycleSuite, CapabilitiesExact) {
    LspSession s;
    auto id = s.sendRequest("initialize", llvm::json::Object{});
    auto resp = s.waitForResponse(id);
    ASSERT_TRUE(resp.has_value());
    auto* caps = resp->getAsObject()->getObject("result")
                     ->getObject("capabilities");
    ASSERT_NE(caps, nullptr);

    // Canonicalize and compare against the frozen contract JSON.
    std::string actual = canonicalize(*caps);
    EXPECT_EQ(actual, kFrozenCapabilitiesJSON);  // contract §1.2
}
```

`kFrozenCapabilitiesJSON` is a `constexpr` string literal in
`lifecycle_test.cpp` matching the contract §1.2 verbatim.

### §4.4 SC-004 latency budget

```cpp
TEST_F(DiagnosticsSuite, OpenLatency_Under250ms_For1500Lines) {
    LspSession s;
    initializeSession(s);
    auto t0 = std::chrono::steady_clock::now();
    s.sendNotification("textDocument/didOpen", largeFileParams());
    s.waitForDiagnostics();    // default 2 s timeout — but we measure tighter
    auto elapsed = std::chrono::steady_clock::now() - t0;
    EXPECT_LT(elapsed, std::chrono::milliseconds(250));
}
```

This test runs at SC-004 budget granularity. CI runners vary; on
slow runners the test may be skipped via `GTEST_SKIP_("slow runner")`
based on a `lscpu`-derived heuristic (TBD; conservative default
is to gate the assertion on Linux x86_64 with ≥ 4 cores and skip
otherwise — the spec's "standard CI runner" caveat).

### §4.5 Cancellation budget — SC-010

```cpp
TEST_F(FoldingSuite, Cancellation_Under200ms) {
    LspSession s;
    initializeSession(s);
    s.sendNotification("textDocument/didOpen", cancellationTargetParams());
    auto reqId = s.sendRequest("textDocument/foldingRange",
                                {{"textDocument", {{"uri", URI}}}});
    s.sendNotification("$/cancelRequest", {{"id", reqId}});

    auto t0 = std::chrono::steady_clock::now();
    auto resp = s.waitForResponse(reqId,
                                    std::chrono::milliseconds(500));
    auto elapsed = std::chrono::steady_clock::now() - t0;

    ASSERT_TRUE(resp.has_value());
    auto* err = resp->getAsObject()->getObject("error");
    ASSERT_NE(err, nullptr);
    EXPECT_EQ(err->getInteger("code").value(), -32800);
    EXPECT_LT(elapsed, std::chrono::milliseconds(200));
}
```

---

## §5 CI integration

### §5.1 `scripts/ci.sh` stage 3 amendment

Stage 3 (unit & layer tests) gains a sub-step that runs the four
LSP integration tests via ctest. The sub-step exits non-zero on
any test failure; the stage exits non-zero on sub-step failure
(existing convention).

### §5.2 SC-007 budget enforcement

The four binaries combined MUST complete in < 30 s on the
project's standard CI runner. The CI cell exposes a per-stage
time budget via the existing `time` invocation; a regression past
30 s fails the cell. (Strict assertion; not a warning.)

### §5.3 SC-009 stderr-on-failure

When any LSP integration test fails, the harness's
`capturedStderr()` string is dumped via `RecordProperty("stderr",
captured)` so ctest's XML output captures it. The CI runner
prints it via `--output-on-failure`. The harness MUST verify
that at least one `ERROR` or `WARN` log record appears in the
captured stderr by regex match against `^[^ ]+ (ERROR|WARN) .+$`.

---

## §6 Forward-compatibility commitments

- Adding a new test (e.g., a T4 hover test) MUST follow this
  contract's `LspSession` API. The harness API is forward-stable;
  T4+ tests reuse it without modification.
- Adding new env-var configuration (e.g., a future
  `NSL_LSP_WORKSPACE_ROOT` once T9 lands workspace-folder support)
  MUST extend `LspEnvVars` in this contract in the same PR.
- The `waitForX` timeout defaults are tunable via test-suite-level
  configuration if needed for future slow-CI cells; the contract
  does not freeze the literal millisecond values, only the budget
  semantics (SC-004 250 ms, SC-010 200 ms, SC-007 30 s combined).
