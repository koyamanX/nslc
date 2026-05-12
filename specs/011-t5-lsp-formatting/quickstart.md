<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Quickstart: Driving the T5 LSP Formatting Methods Locally

**Branch**: `011-t5-lsp-formatting` | **Date**: 2026-05-12

This walkthrough exercises T5's deliverables end-to-end inside
the project's dev container. It assumes the milestone is
implemented; its purpose is to be the canonical "show me it
works" recipe a contributor or reviewer can run after a clean
build.

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
    cmake --build build -j --target nsl-lsp nsl-fmt formatting_test range_formatting_test format_cancellation_test lifecycle_test
  '
```

After the build, the binaries live at:

- `build/bin/nsl-lsp` (T3-delivered, extended in T5)
- `build/bin/nsl-fmt` (T2-delivered, used by parity tests)
- `build/bin/formatting_test` (NEW — FR-017a)
- `build/bin/range_formatting_test` (NEW — FR-017b)
- `build/bin/format_cancellation_test` (NEW — FR-020 / SC-010)
- `build/bin/lifecycle_test` (T3-delivered, extended in T5 for SC-009)

---

## 2. Run the T5 test gate (FR-017a / FR-017b / FR-018 / FR-019 / FR-020)

```bash
docker run --rm -v "$PWD:/work" -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  sh -c '
    ctest --test-dir build -R "^lsp_(formatting|range_formatting|format_cancellation|lifecycle)$" --output-on-failure
  '
```

Expected output: all four matching tests pass; combined
wall-clock ≤ 60 s (SC-011).

Per-test verbose mode:

```bash
docker run --rm -v "$PWD:/work" -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  ./build/bin/formatting_test --gtest_filter='*' --gtest_color=no
```

---

## 3. Manual end-to-end smoke test — `textDocument/formatting`

This is the SC-001 walkthrough: drive `nsl-lsp` directly with a
hand-rolled JSON-RPC envelope sequence and observe the
`TextEdit[]` response.

### 3.1 Prepare a test fixture

```bash
cat > /tmp/smoke.nsl <<'EOF'
module Foo {
declare{
input  a [8];
input b[8];
output y  [8];
}
y=a+b;
}
EOF
```

### 3.2 Construct the JSON-RPC envelope sequence

```bash
cat > /tmp/smoke.jsonl <<'EOF'
Content-Length: 122

{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"processId":null,"capabilities":{}}}
Content-Length: 51

{"jsonrpc":"2.0","method":"initialized","params":{}}
Content-Length: 200

{"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///tmp/smoke.nsl","languageId":"nsl","version":1,"text":"module Foo {\ndeclare{\ninput  a [8];\ninput b[8];\noutput y  [8];\n}\ny=a+b;\n}\n"}}}
Content-Length: 122

{"jsonrpc":"2.0","id":2,"method":"textDocument/formatting","params":{"textDocument":{"uri":"file:///tmp/smoke.nsl"},"options":{"tabSize":4,"insertSpaces":true}}}
Content-Length: 51

{"jsonrpc":"2.0","id":3,"method":"shutdown"}
Content-Length: 39

{"jsonrpc":"2.0","method":"exit"}
EOF
```

> The harness used by `formatting_test` constructs these envelopes
> programmatically; this manual form exists only for the
> walkthrough.

### 3.3 Drive the server

```bash
docker run --rm -v "$PWD:/work" -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  sh -c './build/bin/nsl-lsp < /tmp/smoke.jsonl > /tmp/smoke-out.txt 2>/tmp/smoke-stderr.txt'
```

### 3.4 Inspect the response

```bash
cat /tmp/smoke-out.txt
```

Expected: a stream of `Content-Length:`-framed JSON messages.
The response to request id `2` is the formatting response per
`formatting-api.contract.md` §2.2.1 — a single `TextEdit`
covering the whole document with `newText` matching what
`nsl-fmt --stdin < /tmp/smoke.nsl` produces.

The stderr log records (per FR-015):

```bash
cat /tmp/smoke-stderr.txt
```

Expected: one `INFO` record per arriving request, one `INFO`
record per outgoing response (with `outcome=Success
elapsed_ms=<N>` for the formatting request), and the standard
lifecycle records.

---

## 4. Manual end-to-end smoke test — `textDocument/rangeFormatting`

Same flow as §3 with `params.range` populated. Construct a
larger fixture and request formatting of lines 3..5 only:

```bash
cat > /tmp/range.nsl <<'EOF'
module Bar {
declare{
input  a [8];
input b[8];
output y  [8];
}
y=a+b;
}
EOF
```

The relevant request envelope is:

```json
{
  "jsonrpc": "2.0",
  "id": 4,
  "method": "textDocument/rangeFormatting",
  "params": {
    "textDocument": {"uri": "file:///tmp/range.nsl"},
    "range": {
      "start": {"line": 2, "character": 0},
      "end":   {"line": 5, "character": 0}
    },
    "options": {"tabSize": 4, "insertSpaces": true}
  }
}
```

Expected response: a single `TextEdit` per `text-edit-shape.contract.md`
§3.3 covering lines 3..5 (1-indexed inclusive) of the buffer.
Lines outside that range are untouched in the resulting buffer.

---

## 5. SC-009 — Capability advertisement verification

The `lifecycle_test::CapabilitiesExact` assertion (T3, extended
at T5) verifies that the `InitializeResult.capabilities` JSON
matches the new canonical shape from
`../010-t3-lsp-skeleton/contracts/lsp-protocol.contract.md` §1.2
(amended) byte-for-byte. To run just that test:

```bash
docker run --rm -v "$PWD:/work" -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  ./build/bin/lifecycle_test --gtest_filter='*CapabilitiesExact*'
```

Expected: 1 test passes. The captured `result.capabilities` JSON
matches the canonical shape with `documentFormattingProvider:
true` and `documentRangeFormattingProvider: true` present.

---

## 6. SC-005 — CLI ↔ LSP parity check

The FR-018 / SC-005 parity test iterates every fixture in
`test/lsp/formatting/`, runs both the CLI (`nsl-fmt --stdin <
fixture.nsl`) and the LSP server (driving via `LspSession`),
applies the LSP response's `TextEdit[]` to the input buffer,
and asserts byte-for-byte equality between the two outputs.

To run just this test:

```bash
docker run --rm -v "$PWD:/work" -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  ./build/bin/formatting_test --gtest_filter='*CLI_LSP_Parity*'
```

Expected: ~10 parameterized cases (one per fixture) all pass.

---

## 7. SC-007 — Principle II structural check (linker map)

```bash
docker run --rm -v "$PWD:/work" -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  sh -c '
    cd build && \
    nm --defined-only --extern-only ./lib/libNSLLSP.a 2>/dev/null | grep -E "(format_buffer|parse_config_file|discover_config|LayoutPlanner|Wadler)" && echo "FAIL: formatter symbols in libNSLLSP.a" || echo "PASS: no formatter symbols in libNSLLSP.a (Principle II)"
  '
```

Expected: `PASS: no formatter symbols in libNSLLSP.a (Principle II)`

The `nsl-lsp` binary's effective symbol set should resolve
every formatter symbol into `libNslFmt.a`, not into
`libNSLLSP.a`:

```bash
docker run --rm -v "$PWD:/work" -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  sh -c '
    cd build && \
    nm --defined-only --extern-only ./lib/libNslFmt.a 2>/dev/null | grep -c "format_buffer\|parse_config_file\|discover_config" 
  '
```

Expected: non-zero count (at least 3, one per named symbol).

---

## 8. SC-010 — Cancellation timing

```bash
docker run --rm -v "$PWD:/work" -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  ./build/bin/format_cancellation_test --gtest_filter='*' --gtest_color=no
```

Expected: 1 test passes; assertion confirms the
`RequestCancelled` (`-32800`) response arrives within 250 ms of
the `$/cancelRequest` notification.

---

## 9. Manual TOML side-channel walkthrough (FR-005c)

This walkthrough demonstrates the malformed-TOML fallback path
per `config-resolution.contract.md` §6 / §7.

### 9.1 Set up a fixture with a broken TOML

```bash
mkdir -p /tmp/t5-toml-demo
cat > /tmp/t5-toml-demo/.nsl-fmt.toml <<'EOF'
indent = "potato"
max_line_length = -1
EOF

cat > /tmp/t5-toml-demo/demo.nsl <<'EOF'
module Demo { declare { input a; } }
EOF
```

### 9.2 Drive the LSP server

Construct a JSON-RPC sequence that opens `demo.nsl` and requests
`textDocument/formatting`. The relevant URI is
`file:///tmp/t5-toml-demo/demo.nsl`; the discovered TOML is
`file:///tmp/t5-toml-demo/.nsl-fmt.toml`.

```bash
# (envelope construction analogous to §3.2, omitted for brevity)
./build/bin/nsl-lsp < /tmp/t5-toml-demo.jsonl > /tmp/t5-toml-demo-out.txt
```

### 9.3 Inspect the response stream

Expected:

1. A `textDocument/publishDiagnostics` notification whose `uri`
   is `file:///tmp/t5-toml-demo/.nsl-fmt.toml` and whose
   `diagnostics` array carries two entries — one for `indent =
   "potato"` (string where enum expected) and one for
   `max_line_length = -1` (range violation). Each entry has
   `source: "nsl-fmt"`.
2. The `textDocument/formatting` response with a non-`null`
   `result` (the format request proceeded with
   `default_configuration()`, so the demo file is formatted to
   the default style).

The two emissions arrive in this order — notification first,
response second — per `config-resolution.contract.md` §7.

---

## 10. Performance check (SC-004)

For a fixture file of ≤1500 lines, the dispatch-to-response
latency should be under 300 ms. The `formatting_test` includes a
synthetic large fixture (constructed by replicating an audited
sample module) and asserts the latency budget.

```bash
docker run --rm -v "$PWD:/work" -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  ./build/bin/formatting_test --gtest_filter='*PerfBudget*'
```

Expected: 1 test passes; the median latency across 10 trials is
under 300 ms.

---

## 11. Cleanup

```bash
rm -rf /tmp/smoke.nsl /tmp/smoke.jsonl /tmp/smoke-out.txt /tmp/smoke-stderr.txt \
       /tmp/range.nsl /tmp/t5-toml-demo /tmp/t5-toml-demo-out.txt /tmp/t5-toml-demo.jsonl
```

---

## Troubleshooting

- **`formatting_test` reports `MethodNotFound` for every
  fixture**: the new dispatch-table entries in
  `NslLSPServer.cpp` are missing. Verify with `grep
  "textDocument/formatting" lib/LSP/NslLSPServer.cpp`.
- **`lifecycle_test::CapabilitiesExact` fails**: the canonical
  JSON in `../010-t3-lsp-skeleton/contracts/lsp-protocol.contract.md`
  §1.2 was not amended. Verify the diff includes both
  `documentFormattingProvider` and `documentRangeFormattingProvider`.
- **`SC-005 CLI_LSP_Parity` fails on one fixture but not others**:
  inspect the per-fixture stderr log records (`outcome=...`) to
  classify the divergence. If the LSP `outcome` differs from
  what the CLI produced (e.g., `RefusedParse` on one side,
  `Success` on the other), the fixture itself is borderline —
  reduce to the minimum failing input and add a unit test
  reproducing the boundary.
- **`format_cancellation_test` flakes**: the SC-010 budget of
  250 ms can be tight under load. The fixture should be
  artificially large (constructed to push the cancellation
  poll past the 250 ms threshold). If the test is flaky on a
  reasonable-spec runner, increase the fixture size; do NOT
  raise the SC-010 budget without a constitutional discussion
  (Principle V — the budget is part of the deterministic-
  pipeline contract).
- **TOML side-channel diagnostic does not appear in the editor's
  problems panel**: the editor must process the
  `publishDiagnostics` notification's `uri` field correctly. In
  some clients, opening the TOML file (or letting the client
  fetch it via `file://`) is required before the diagnostic is
  rendered. Confirm with `nvim --headless +'lua print(vim.json.encode(vim.diagnostic.get()))' +qa` (Neovim 0.10+) or the VS Code "Problems" panel.
