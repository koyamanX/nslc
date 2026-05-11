<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Quickstart: Driving `nsl-lsp` Locally

**Branch**: `010-t3-lsp-skeleton` | **Date**: 2026-05-05

This walkthrough exercises T3's deliverables end-to-end inside the
project's dev container. It assumes the milestone is implemented;
its purpose is to be the canonical "show me it works" recipe a
contributor or reviewer can run after a clean build.

> **Reproduction environment**: Per the project Constitution
> (Principle V — deterministic pipeline) and the existing
> precedent ([`README.md`](../../README.md) "Building"), all
> commands run inside the dev container
> `ghcr.io/koyamanx/nsl-nslc:dev`. The host is not a supported
> environment.

---

## 1. Build

```bash
docker run --rm -v "$PWD:/work" -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  sh -c '
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build -j --target nsl-lsp lifecycle_test diagnostics_test folding_test cancellation_test
  '
```

After the build, the binary lives at `build/bin/nsl-lsp` and the
four integration-test binaries live at `build/bin/<test_name>`.

---

## 2. Run the test gate (FR-021 / SC-001)

```bash
docker run --rm -v "$PWD:/work" -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  sh -c '
    ctest --test-dir build -R "^lsp_" --output-on-failure
  '
```

Expected output: all four LSP integration test binaries pass;
combined wall-clock ≤ 30 s (SC-007).

Per-test verbose mode:

```bash
docker run --rm -v "$PWD:/work" -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  ./build/bin/lifecycle_test --gtest_filter='LifecycleSuite.README_TestGate*' --gtest_print_time=1
```

This prints the literal materialization of the
[`README.md`](../../README.md) §Roadmap row T3 test gate: open
→ S01 diagnostic → edit → empty diagnostics → shutdown → exit.

---

## 3. Drive `nsl-lsp` by hand (no editor required)

The simplest way to interact with `nsl-lsp` is with `printf` +
`nc`-style framing. Save a small driver to a temporary file:

```bash
cat > /tmp/lsp_drive.py <<'EOF'
import json, sys, subprocess

def frame(obj):
    body = json.dumps(obj).encode("utf-8")
    return f"Content-Length: {len(body)}\r\n\r\n".encode("ascii") + body

p = subprocess.Popen(
    ["./build/bin/nsl-lsp"],
    stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    env={"NSL_LSP_LOG_LEVEL": "debug",
         "NSL_INCLUDE": "/work/test/lsp/fixtures",
         "PATH": "/usr/bin:/bin"})

p.stdin.write(frame({"jsonrpc": "2.0", "id": 1,
                      "method": "initialize", "params": {}}))
p.stdin.write(frame({"jsonrpc": "2.0",
                      "method": "initialized", "params": {}}))
p.stdin.write(frame({"jsonrpc": "2.0",
                      "method": "textDocument/didOpen",
                      "params": {"textDocument":
                          {"uri": "file:///example.nsl",
                           "languageId": "nsl",
                           "version": 1,
                           "text": "module foo { reg bad__name; }"}}}))
p.stdin.flush()
# read responses until publishDiagnostics arrives
# (omitted: full read loop; see test/lsp/LspSession.cpp for a
# production-grade reader)
EOF
docker run --rm -v "$PWD:/work" -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  python3 /tmp/lsp_drive.py
```

Expected behavior: stderr shows three INFO-level log lines
(`initialize received`, `initialized received`, `worker pool
started`), then a `publishDiagnostics` notification arrives on
stdout carrying one `Diagnostic` with `code = "S01"`.

---

## 4. Drive from VS Code (manual smoke test)

T3 does not ship a VS Code extension shell (per FR-028; T11 will).
For a manual smoke test, install the LSP-launch helper extension
[`vscode-lsp-debug`](https://marketplace.visualstudio.com/items?itemName=daohu527.vscode-lsp-debug)
or any equivalent. Point it at `build/bin/nsl-lsp`:

```jsonc
// .vscode/settings.json — local only, not committed
{
  "lsp-debug.servers": [
    {
      "id": "nsl",
      "command": "${workspaceFolder}/build/bin/nsl-lsp",
      "languageIds": ["nsl"],
      "env": {
        "NSL_INCLUDE": "${workspaceFolder}/test/lsp/fixtures",
        "NSL_LSP_LOG_LEVEL": "info"
      }
    }
  ]
}
```

Open any `.nsl` file with a Sema error; confirm a red squiggle
appears. Edit the file to fix the error; confirm the squiggle
disappears. Inspect the editor's "LSP Log" panel — it will be
empty (per FR-020g, T3 sends no `window/logMessage`); the
`NSL_LSP_LOG_LEVEL=info` records appear in the editor's
"Output → vscode-lsp-debug" panel via stderr capture instead.

---

## 5. Drive from Neovim (built-in LSP)

```lua
-- ~/.config/nvim/after/ftplugin/nsl.lua  -- local only
vim.lsp.start({
  name = 'nsl-lsp',
  cmd = { vim.fn.getcwd() .. '/build/bin/nsl-lsp' },
  cmd_env = {
    NSL_INCLUDE        = vim.fn.getcwd() .. '/test/lsp/fixtures',
    NSL_LSP_LOG_LEVEL  = 'info',
  },
  root_dir = vim.fn.getcwd(),
})
```

Open any `.nsl` file. Diagnostics appear in `:lopen`. `zM` /
`zR` exercise folding ranges. `:LspLog` shows the captured stderr.

---

## 6. Inspect the capability advertisement (SC-008)

Send `initialize` and pretty-print the result:

```bash
docker run --rm -v "$PWD:/work" -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  sh -c '
    printf "Content-Length: 58\r\n\r\n%s" \
      "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}" |
    ./build/bin/nsl-lsp |
    sed -n "s/^Content-Length: [0-9]*\r\?$//; /^{/p" |
    python3 -m json.tool
  '
```

Expected output: the canonical JSON from
[`contracts/lsp-protocol.contract.md`](./contracts/lsp-protocol.contract.md)
§1.2 — `textDocumentSync` with `change=1, openClose=true` and
`foldingRangeProvider: true` and nothing else.

---

## 7. Determinism check (SC-003)

```bash
docker run --rm -v "$PWD:/work" -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  sh -c '
    ./build/bin/diagnostics_test \
      --gtest_filter="DiagnosticsSuite.Determinism_TwoRunsByteIdentical" \
      --gtest_print_time=1
  '
```

The test runs the full lifecycle twice and asserts byte-identical
captured `publishDiagnostics`. Any nondeterminism (pointer-derived
ordering, hash-map iteration order, embedded timestamps) fails
this test.

---

## 8. SC-004 latency check (≤ 250 ms / 1500 lines)

```bash
docker run --rm -v "$PWD:/work" -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  ./build/bin/diagnostics_test \
    --gtest_filter="DiagnosticsSuite.OpenLatency_Under250ms_For1500Lines" \
    --gtest_print_time=1
```

This test exercises the SC-004 budget on a 1500-line fixture. On
a slow runner (< 4 cores) the test may skip with
`GTEST_SKIP_("slow runner")`; in that case the budget is
nominally still met but not enforced.

---

## 9. SC-010 cancellation check (≤ 200 ms)

```bash
docker run --rm -v "$PWD:/work" -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  ./build/bin/folding_test \
    --gtest_filter="FoldingSuite.Cancellation_Under200ms" \
    --gtest_print_time=1
```

Sends a `foldingRange` request against a 10000-node fixture
followed immediately by `$/cancelRequest`; asserts the
`RequestCancelled` (`-32800`) response arrives within 200 ms.

---

## 10. Troubleshooting

| Symptom                                                      | Diagnosis                                                                                                                                |
| ------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------- |
| `nsl-lsp` exits with code 1 immediately on startup           | Check `NSL_LSP_LOG_LEVEL` value; only `error`/`warn`/`info`/`debug` are valid. Stderr will identify the bad value.                       |
| Diagnostics for `#include`'d files are not appearing         | `NSL_INCLUDE` is empty or wrong. Server logs the resolved include path at `INFO` level on startup.                                       |
| `publishDiagnostics` arrives but `code` field is missing     | The diagnostic's message prefix doesn't match any row in [`contracts/diagnostic-mapping.contract.md`](./contracts/diagnostic-mapping.contract.md) §1. Check the `nsl-sema`/`nsl-parse`/`nsl-preprocess` lookup table. |
| Folding ranges returned for the wrong line numbers           | `#line` directive interaction; the contract preserves user-visible (logical) source per Principle IV. If the directive is being ignored, that's a regression bug — file it. |
| Test hangs at `waitForDiagnostics`                           | Server probably exited before publishing; check `capturedStderr()` for an `ERROR` log record.                                            |
| `lifecycle_test::CapabilitiesExact` fails after a feature edit | A new capability was advertised without updating [`contracts/lsp-protocol.contract.md`](./contracts/lsp-protocol.contract.md) §1.2. Update both in the same PR (Principle VII).                                          |

---

## 11. What T3 does NOT deliver (forward-pointers)

| Feature                        | Lands at | Anchor                                                    |
| ------------------------------ | -------- | --------------------------------------------------------- |
| `hover`, `definition`, `documentSymbol`, `semanticTokens`, `signatureHelp` | T4 | `nsl_tooling_design.md` §3.2 |
| `formatting`, `rangeFormatting` | T5      | `nsl_tooling_design.md` §3.2 + §5                         |
| `references`, `completion`, `rename`, `codeAction` | T9 | `nsl_tooling_design.md` §3.2 |
| `inlayHint`, `prepareCallHierarchy` | T10  | `nsl_tooling_design.md` §3.2                              |
| Editor packaging (Neovim / Emacs / Sublime / VS Code shell) | T11 | `nsl_tooling_design.md` §4.4 |
| `window/logMessage`            | T11      | per FR-020g                                               |
| `workspaceFolders`, `workspace/configuration`, `workspace/symbol` | T9 | per FR-025 |
| LSP 3.17+ features (`positionEncodings`, etc.) | undecided | per Q3 / Clarifications |
| `Incremental` text sync        | T9 (or earlier if size pressure) | per Q1 / Clarifications |
| Stable node IDs / incremental parse | undecided | `nsl_tooling_design.md` §2.1 / §2.2; T9 if needed |
