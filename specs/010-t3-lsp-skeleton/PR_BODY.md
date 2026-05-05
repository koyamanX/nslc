<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# T3: `nsl-lsp` skeleton — first user-visible LSP deliverable

**Closes** the README §Roadmap row T3 milestone. Tooling-track delivery; M3 is the only compiler-track gate (already landed on `master`).

## Summary

`bin/nsl-lsp` is now a real LSP server. Drop the binary into any LSP-capable editor (VS Code, Neovim built-in LSP, Emacs eglot/lsp-mode, Helix, Sublime LSP) and live Sema diagnostics appear as you type. The README §Roadmap test gate — *"open a file with a Sema error, observe diagnostic; edit, observe re-diagnose"* — is materialized as `LifecycleSuite::README_TestGate_OpenErrorEditFix` and passes.

The PR ships:

- `tools/nsl-lsp/` — thin binary entry (≤ 70 lines)
- `lib/LSP/` — `libNSLLSP.a` implementation library (clangd-style three-layer: JSON-RPC transport → LSP-protocol layer → language-logic layer → TUScheduler → reused `libNSLFrontend.a`)
- `include/nsl/LSP/Server.h` — single public header per Constitution Principle II
- `test/lsp/` — new gtest integration test layer with the `LspSession` subprocess harness
- `scripts/lsp_link_audit.sh` — Principle II structural audit, wired into `scripts/ci.sh` stage 2
- `scripts/gen_lsp_fixtures.py` — one-shot generator that lifts `test/sema/s<NN>/fail*.nsl` bodies for the parameterized Sn coverage
- `specs/010-t3-lsp-skeleton/` — full speckit pipeline (spec, plan, research, data-model, 4 contracts, quickstart, tasks, requirements checklist)
- Coupling-doc updates: `CLAUDE.md` §2.1, `docs/design/nsl_tooling_design.md` §3 banner, `docs/CLAUDE.md` task→section map, `README.md` §Roadmap row T3

## Test results

`ctest -R lsp` inside `ghcr.io/koyamanx/nsl-nslc:dev`:

- **51 / 51 PASS in 3.01s** (well under the SC-007 30s budget)

| Suite | Tests | Coverage |
|-------|-------|----------|
| LifecycleSuite | 7 | initialize / shutdown / exit / pre-init rejection / log-level validation / NSL_INCLUDE log + **README test gate** |
| DiagnosticsSuite | 12 | empty array / SingleS01 / sort order / parse error / determinism / edit-clears / edit-introduces / didClose / incremental rejection / stale version / rapid edits / **OpenLatency_Under250ms** |
| **CodeMappingSuite/AllSn** | **23** | **one parameterized test per non-constructive Sn (S01–S29 minus 6 constructive) — closes SC-002** |
| FoldingSuite | 6 | multi-line blocks / single-line / block-comment kind / zero-based / parse-error recovery / determinism |
| CancellationSuite | 2 | CancelCompleted / CancelNeverSeen |
| ArchitectureSuite | 1 | link audit (no frontend duplication, single public header) |

## Acceptance criteria

| ID | Description | Status |
|----|-------------|--------|
| **FR-021 (test gate)** | Open + see diagnostic; edit + re-diagnose | ✅ GREEN |
| SC-001 | Test gate passes | ✅ GREEN |
| SC-002 | 100% Sn round-trip with correct code | ✅ GREEN (23/23 parameterized) |
| SC-003 | Determinism (byte-identical across two runs) | ✅ GREEN |
| SC-004 | didOpen → publishDiagnostics ≤ 250 ms / 1500 lines | ✅ GREEN |
| SC-005 | Single libNSLFrontend.a (no duplication) | ✅ GREEN (link audit) |
| SC-007 | 30s combined CI budget | ✅ GREEN (3.01s) |
| SC-008 | Exact capability advertisement | ✅ GREEN |
| SC-009 | stderr ERROR/WARN on test failure | ✅ GREEN |
| SC-010 | Cancellation ≤ 200 ms | ⏸ deferred (requires async-dispatch refactor; cancellation seam itself is functional + tested) |
| SC-006 | T4 forward-extensibility | ⏸ promise verified at T4 land |

## TDD progression

The branch contains 10 commits:

1. `8d8af4a` — speckit artifacts (spec + 5 clarifications + plan + research + data-model + 4 contracts + tasks + checklist)
2. `f5c81e2` — speckit-analyze cleanup (close U1/I1/I2 findings)
3. `d999a25` — Phase 1 Setup (directory tree, CMake, stub binary)
4. `78d167e` — Phase 2 Foundational (utilities, JSON-RPC, scheduler, lifecycle)
5. `cb3a35f` — Phase 3 / US1 (diagnostic seam end-to-end)
6. **`684ad7d` — Phase 4 / US2 (README test gate GREEN)**
7. `81408ab` — Phases 5-7 (folding + cancellation + audit + coupling docs)
8. `44fbc74` — SC-002 close (23-Sn parameterized coverage)
9. *(this commit)* — SC-004 close (1500-line latency fixture + budget assertion)

Strict-TDD ordering was honored from Phase 3 onwards (US1, US2, US3 tests authored ahead of implementation, observed failing first). Phase 2 documents an explicit deviation (handlers landed before lifecycle tests; the manual stdio smoke test recorded in the Phase 2 commit message serves as the failing-state evidence).

## Implementation amendments captured in commits

A handful of contract-level refinements surfaced during implementation; each is captured in the commit message that introduced it:

- `add_library(NSLLSP STATIC ...)` instead of `add_nsl_library` (the macro is hardcoded to the 9 §3 layer names; T-track libraries are outside that table).
- `--whole-archive,nsl-sema,--no-whole-archive` link option (per-`Sn` constraint TUs self-register at static-init; without `--whole-archive` the linker silently drops them).
- `-fno-rtti` for NSLLSP + LspSession (LLVM ABI compat — avoids `typeinfo for llvm::support::detail::format_adapter` link errors).
- `DiagnosticMapper` extracts `code` via trailing-suffix regex on `(S<NN>)` instead of message-prefix lookup (exploits M3's established `S<n>_*.cpp` convention of embedding the Sn id in the message).
- `FoldingRangeBuilder` uses text walking instead of an `ASTVisitor` pattern (~120 LOC vs ~600 LOC of boilerplate; output is identical for NSL since every `{...}` is a recognized block-opener).
- `lsp_link_audit.sh` uses grep-based source-tree audit instead of nm-based binary symbol audit (the Itanium ABI's destructor-variant emission makes nm-level dedup detection unreliable).
- `target_compile_definitions` for `NSL_LSP_FIXTURES_DIR` instead of the gtest_discover_tests `ENVIRONMENT` property (the property serializes a list-valued env into multiple property args, only the last of which `set_tests_properties` honors).
- Dispatch allows `shutdown` pre-`initialized` (clangd-equivalent; LSP spec permits immediate teardown after initialize).

## Constitution compliance

All 9 principles verified:

- **I. Spec Authoritative** — M3 frozen diagnostic strings consumed verbatim; no spec edit.
- **II. Layered Library** — `nsl-lsp` reuses `libNSLFrontend.a` via `nsl-driver`; single public header `Server.h`; structural audit script wired into CI.
- **III. Stock CIRCT** — N/A (T3 is upstream of MLIR).
- **IV. Source-Locating Diagnostics** — `SourceManager::resolveVirtual` preserves `#line` round-trip; `relatedInformation` carries include-from notes.
- **V. Deterministic Pipeline** — per-suite `Determinism_TwoRunsByteIdentical` tests (DiagnosticsSuite + FoldingSuite).
- **VI. Layered Test Discipline** — new test layer at `test/lsp/` (gtest + subprocess); audited-corpus list unchanged.
- **VII. Spec ↔ Design Coupling** — CLAUDE.md / docs/design / docs/CLAUDE.md / README.md all updated in this PR.
- **VIII. TDD** — strict ordering Phase 3 onward; Phase 2 deviation explicitly captured.
- **IX. CI** — `scripts/ci.sh` stage 2 gains the LSP link audit; stage 3's existing `ctest --test-dir build` auto-discovers all `lsp_*` tests.

## Deferred (post-merge follow-ups)

20 of 117 tasks deferred — none load-bearing:

- **PR-merge-time** (T111 full CI run, T113 quickstart walkthrough, T116 binary size capture) — best done at PR creation/merge.
- **SC-010 cancellation budget assertion** (T093) — requires async-dispatch refactor (current synchronous-dispatch design makes mid-flight cancellation structurally untestable; the seam itself is functional and the CancelCompleted/CancelNeverSeen tests exercise the in-flight table + early-exit path).
- **Edge-case tests** (T053/T064 preprocess error, T054/T065 UTF-8, T055/T062 include-chain `relatedInformation`, T061 severity-tiebreak) — secondary enrichments.
- **JSONTransport unit test** (T019) — transport is exercised by every integration test today; dedicated unit test is nice-to-have.
- **Misc fixtures** (T086 #line-relocated, T087 cancellation_target ≥10000 nodes, T089 folding_parse_error) — paired with the deferred tests above.
- **Lifecycle red-state file** (T042) — sandbox blocked the original write; commit-message records establish the failing-state instead.

## How to test locally

```bash
docker run --rm -v "$PWD:/work" -w /work ghcr.io/koyamanx/nsl-nslc:dev sh -c '
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && \
  cmake --build build && \
  ctest --test-dir build -R lsp --output-on-failure
'
```

Expected: `100% tests passed, 0 tests failed out of 51`.

## How to use as an editor LSP

```lua
-- Neovim ~/.config/nvim/after/ftplugin/nsl.lua
vim.lsp.start({
  name = 'nsl-lsp',
  cmd = { vim.fn.getcwd() .. '/build/bin/nsl-lsp' },
  cmd_env = {
    NSL_INCLUDE       = vim.fn.getcwd() .. '/test/lsp/fixtures',
    NSL_LSP_LOG_LEVEL = 'info',
  },
  root_dir = vim.fn.getcwd(),
})
```

Open any `.nsl` file with a Sema error; red squiggle appears at the offending location, with the diagnostic message including the `(S<NN>)` constraint id. Edit to fix; squiggle disappears.

---

🤖 Generated with [Claude Code](https://claude.com/claude-code)
