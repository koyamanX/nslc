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

`ctest -R "^lsp_"` inside `ghcr.io/koyamanx/nsl-nslc:dev`:

- **57 / 57 PASS in 3.12s** (well under the SC-007 30s budget)

| Suite | Tests | Coverage |
|-------|-------|----------|
| LifecycleSuite | 8 | initialize / shutdown / exit / pre-init rejection / log-level validation / NSL_INCLUDE log + **README test gate** + JSONTransport |
| DiagnosticsSuite | 15 | empty / SingleS01 / sort-line-then-column + **severity tiebreak** / parse error / preprocess error / UTF-8 / determinism / edit-clears / edit-introduces / didClose / incremental rejection / stale version / rapid edits / **OpenLatency_Under250ms** / **IncludeFromNotes** |
| **CodeMappingSuite/AllSn** | **23** | **one parameterized test per non-constructive Sn (S01–S29 minus 6 constructive) — closes SC-002** |
| FoldingSuite | 8 | multi-line blocks / single-line / block-comment kind / zero-based / parse-error recovery / include-adjusts-lines / **Cancellation_Under200ms** / determinism |
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
| SC-010 | Cancellation ≤ 200 ms | ✅ GREEN (0.17 s elapsed in `FoldingSuite::Cancellation_Under200ms`; foldingRange offloaded to a worker thread so the cancel signal can flip mid-walk) |
| SC-006 | T4 forward-extensibility | ⏸ promise verified at T4 land |

## TDD progression

The branch contains 15 commits:

1. `8d8af4a` — speckit artifacts (spec + 5 clarifications + plan + research + data-model + 4 contracts + tasks + checklist)
2. `f5c81e2` — speckit-analyze cleanup (close U1/I1/I2 findings)
3. `d999a25` — Phase 1 Setup (directory tree, CMake, stub binary)
4. `78d167e` — Phase 2 Foundational (utilities, JSON-RPC, scheduler, lifecycle)
5. `cb3a35f` — Phase 3 / US1 (diagnostic seam end-to-end)
6. **`684ad7d` — Phase 4 / US2 (README test gate GREEN)**
7. `81408ab` — Phases 5-7 (folding + cancellation + audit + coupling docs)
8. `44fbc74` — SC-002 close (23-Sn parameterized coverage)
9. `ed496b8` — SC-004 close (1500-line latency fixture + budget assertion)
10. `5c3f379` — JSONTransport unit test + UTF-8 + binary-size capture
11. `bd4e9f2` — preprocess error path (T053 + T064) + flake stabilization
12. `9b87f92` — include-chain fixtures + stale-drop fix + .nslh SPDX recipe
13. `fa747e7` — T086 IncludeAdjustsLines + T042 TDD-evidence document
14. `7f06c5b` — **T062 IncludeFromNotes — FR-026 post-preprocessing include chain**
15. *(this commit)* — **T061 + T087 + T089 + T093 close the remaining test gates**

Strict-TDD ordering was honored from Phase 3 onwards (US1, US2, US3 tests authored ahead of implementation, observed failing first). Phase 2 documents an explicit deviation (handlers landed before lifecycle tests; the manual stdio smoke test recorded in the Phase 2 commit message serves as the failing-state evidence). Per-phase red→green hashes are recorded in `specs/010-t3-lsp-skeleton/tdd-evidence.md`.

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
- `SourceManager` gained `permanent_include_sites[256]` + `getOriginalIncludeStackFor(FileID)` + `findFileIDByPath(StringRef)` (commit `7f06c5b`) so post-preprocessing diagnostics — Sema, MLIR passes, the LSP server — can still query the include chain after `popIncludeFrame`. `Diagnostic.cpp::report` bridges synthetic preprocessed-buffer FileIDs back to physical via `resolveVirtual`'s `#line`-preserved path; `NslTU::reparse` replays `#line` directives onto the synth buffer so the bridge resolves correctly. Without this, FR-026's "diagnostics in `#include`d files carry the chain in `relatedInformation`" was structurally unreachable from the LSP path.
- `NslLSPServer::onFoldingRange` runs the walk on a worker thread (commit `9b9979d`); without offloading, the dispatch thread blocks until the walk completes and the cancellation flag never flips mid-walk. The run loop joins all workers on exit (after cancelling any still-in-flight tokens so they unwind quickly) so process teardown is clean.

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

All 117 task-list items closed. The only non-task work explicitly deferred to a later milestone:

- **SC-006 T4 forward-extensibility** — by design, verified at the T4 hover/definition/documentSymbol/semanticTokens/signatureHelp land. The current public-API surface (`include/nsl/LSP/Server.h` + the in-`lib/` extension points in `NslLSPServer.cpp`'s dispatch table) is built for it.

The previously-deferred tasks all closed during the follow-up commits 10–15 above:
- T019 (JSONTransport unit test) → closed in `5c3f379`
- T042 (lifecycle red-state record) → closed via `specs/010-t3-lsp-skeleton/tdd-evidence.md` in `fa747e7`
- T053 / T064 (preprocess error path) → closed in `bd4e9f2`
- T054 / T065 (UTF-8 fixture/test) → closed in `5c3f379`
- T055 / T062 (include-chain + `relatedInformation`) → fixtures in `9b87f92`; relatedInformation seam in `7f06c5b`
- T061 (severity-tiebreak) → closed in this commit
- T086 (`#line`-relocated folds) → closed in `fa747e7`
- T087 / T093 (cancellation ≥ 10k node fixture + ≤ 200 ms budget) → closed in this commit (foldingRange offloaded to a worker thread so the cancel signal can flip mid-walk)
- T089 (`folding_parse_error.nsl` fixture) → closed in this commit

## How to test locally

```bash
docker run --rm -v "$PWD:/work" -w /work ghcr.io/koyamanx/nsl-nslc:dev sh -c '
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && \
  cmake --build build && \
  ctest --test-dir build -R lsp --output-on-failure
'
```

Expected: `100% tests passed, 0 tests failed out of 57`.

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
