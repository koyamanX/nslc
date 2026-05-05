<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Implementation Plan: T3 — `nsl-lsp` Skeleton

**Branch**: `010-t3-lsp-skeleton` | **Date**: 2026-05-05 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/010-t3-lsp-skeleton/spec.md`

## Summary

Deliver tooling-track milestone **T3** — the first user-visible LSP
deliverable and the architectural seam every later LSP-track
milestone (T4, T5, T9, T10) extends with new LSP methods. T11
(editor packaging across Neovim / Emacs / Sublime; not a new
LSP method) consumes the same `nsl-lsp` binary and gates on T3 +
T8 per [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
§4.4. T3 ships:

1. **`tools/nsl-lsp/`** — a thin entry-point binary
   (`main.cpp` ≤ 70 lines) that delegates to the library. Wired into
   the existing `add_nsl_library` / target convention used by
   `tools/nslc/` and `tools/nsl-opt/`.
2. **`lib/LSP/`** — the implementation library `libNSLLSP.a`. Houses
   four classes per
   [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
   §3.1 / §3.4: `JSONTransport` (stdin/stdout JSON-RPC framing),
   `NslLSPServer` (LSP protocol layer), `NslServer` (language-logic
   layer), `TUScheduler` + `NslTU` (per-document threading + cache).
3. **`include/nsl/LSP/Server.h`** — single public header exposing
   only the `runStdioServer(int argc, char** argv) -> int` entry
   point. All implementation classes stay private to `lib/LSP/`.
4. **`test/lsp/`** — a new test layer. Three integration-test
   binaries built on a small in-tree gtest harness that spawns
   `nsl-lsp` as a subprocess: `lifecycle_test`, `diagnostics_test`,
   `folding_test`, plus a `cancellation_test` for FR-020h–j /
   SC-010.

**Technical approach (single sentence)**: Reuse `libNSLFrontend.a`
through narrow seams (Sema → diagnostics, AST → folding ranges),
add a thin LSP protocol veneer on top, and gate every wire-visible
behavior with a frozen contract (initialize-response shape,
diagnostic-mapping seam, folding-range seam, cancellation seam) so
that T4 / T5 / T9 / T10 extend the seams rather than reshape them.

**Architecture decisions** (full rationale in [research.md](./research.md)):

- **JSON-RPC**: hand-rolled `JSONTransport` using `llvm::json::Value`
  for parsing/serialization. clangd's `JSONTransport` is the
  precedent shape, but its source is too entangled with `clangd`'s
  internals to vendor cleanly; T3 implements only the framing and
  request-parsing primitives it needs (~200 LOC).
- **Concurrency**: `llvm::ThreadPool` for the worker pool (already
  a transitive dep via LLVM); `std::mutex` + `std::condition_variable`
  for per-`NslTU` synchronization, matching the design-doc
  skeleton verbatim.
- **Test harness**: in-tree gtest, subprocess-based, no external
  Python dep. Each test spawns `nsl-lsp` over a pair of pipes,
  sends a deterministic JSON-RPC envelope sequence, and asserts on
  the captured response stream.
- **Folding-range computation**: an `ASTVisitor` walk emitting
  `FoldingRange` per block-opener AST node whose start and end
  span ≥ 2 source lines. The visitor lives in `lib/LSP/` and
  reuses `nsl::ast::ASTVisitor` from `libNSLFrontend.a`.
- **Diagnostic mapping**: a free function
  `toLspDiagnostic(const nsl::Diagnostic&, const SourceManager&)
  -> llvm::json::Value`. Reads include-from-notes via
  `Diagnostic::is_include_from_note` and surfaces them through
  LSP `Diagnostic.relatedInformation`.
- **Position encoding**: UTF-16 unconditionally per FR-004 (LSP 3.16
  pin). A single utility — `byteOffsetToLspPosition(StringRef line,
  size_t byteOffset)` — performs the byte-offset → UTF-16 code-unit
  conversion at the protocol boundary; the conversion is a no-op
  when the line is pure ASCII.
- **Library boundary**: `libNSLLSP.a` is a new T-track library; it
  lives under `lib/LSP/` parallel to the M-track libraries
  (`lib/Basic/`, `lib/Sema/`, etc.). Constitution Principle II's
  "single public header" rule applies and is satisfied by
  `include/nsl/LSP/Server.h` (one umbrella header). T-track
  libraries are not part of the nine-library layered table from
  `docs/design/nsl_compiler_design.md` §3 — they consume that
  table's output via `libNSLFrontend.a` and add no compiler-track
  layers.

Principle II's no-duplication rule is satisfied by linker-map
inspection (SC-005). Principle V (determinism) is satisfied by:
worker-count-independent diagnostic ordering (sort by
`(SourceLocation, Severity)` per existing `DiagnosticEngine`
behavior), serialized JSON output (`llvm::json::Value` is
deterministic), and explicit document-version tracking that drops
stale diagnostics. Principle VIII (TDD) is satisfied by the
fixture-first ordering recorded in `tasks.md`.

## Technical Context

**Language/Version**: C++17 across all new code (Constitution
Build/Code/Licensing Standards). No C++20 features. Generator
scripts in Python 3 if needed (none anticipated for T3).

**Primary Dependencies**:
- **`libNSLFrontend.a`** — existing aggregate library, transitively
  links `nsl-basic` / `nsl-preprocess` / `nsl-lex` / `nsl-parse` /
  `nsl-ast` / `nsl-sema`. Consumed by `nsl-lsp` via the existing
  `add_nsl_library` CMake convention. (Constitution Principle II.)
- **`llvm::json`** — LLVM's JSON library, already pulled in via
  `nsl-driver`. Provides `llvm::json::Value`, `llvm::json::Object`,
  `llvm::json::Array`, parsing, and pretty-printing.
- **`llvm::Support`** — `llvm::ThreadPool`, `llvm::raw_ostream`,
  `llvm::sys::*`. All already transitively present.
- **No new external dependencies** — neither `pytest-lsp` nor any
  other Python test driver is added; the harness is in-tree gtest
  with subprocess management via `llvm::sys::Process` /
  `llvm::sys::ExecuteAndWait`.

**Storage**: N/A — `nsl-lsp` is a stateless server in the sense
that it persists no state between sessions. Per-session document
state (open URIs, parsed ASTs, diagnostics) lives in
`TUScheduler` memory only.

**Testing**:
- **Layer**: a new "LSP integration" test layer at `test/lsp/`.
  Per Constitution Principle VI's per-layer accepted-driver list,
  the canonical drivers (lit + FileCheck for lowering / E2E;
  gtest / nsl-opt for unit / dialect) cover the compiler track;
  tooling-track LSP testing is out of that list's enumerated scope
  by construction (parallel to T1's TextMate scope-test layer).
  gtest-driving-subprocess is the convention.
- **Format**: each gtest fixture sets up an `LspSession` helper that
  spawns `nsl-lsp`, sends a vector of JSON-RPC envelopes, and
  collects responses + stderr. Assertions match expected JSON
  shapes and stderr patterns.
- **Coverage** per FR-021/FR-022 + SC-001/SC-002/SC-007/SC-010:
  one fixture per S-constraint with a locked diagnostic string
  (~23 fixtures); one fixture per block-opener (~14 productions);
  one cancellation fixture; one large-file performance fixture
  (≤ 1500 lines per SC-004).

**Target Platform**: Linux x86_64 (matches Principle IX build
matrix). The dev container `ghcr.io/koyamanx/nsl-nslc:dev`
provides clang/gcc/cmake/ninja/lit/gtest. No new platform.

**Project Type**: tooling library + binary; reuses the existing
`add_nsl_library` CMake convention.

**Performance Goals**:
- SC-004: didOpen→publishDiagnostics under 250 ms for files
  ≤ 1500 lines on a standard CI runner.
- SC-007: combined integration tests under 30 s on CI.
- SC-010: cancellation acknowledgment under 200 ms.

**Constraints**:
- Constitution Principle II: zero duplication of lexer / parser /
  sema; verifiable via linker map (SC-005).
- Constitution Principle V: byte-identical `publishDiagnostics`
  for byte-identical input across two runs (SC-003).
- Constitution Principle VII: capability advertisement is exact;
  any later T-milestone that adds a capability updates the
  `lsp-protocol.contract.md` assertion in the same PR (SC-008).
- LSP 3.16 floor (per Clarifications session 2026-05-05 Q3); UTF-16
  unconditionally; no `positionEncodings` capability advertisement.
- `Full` text sync only on `didChange` (per Q1).
- `NSL_INCLUDE` env-var read once at server startup (per Q2).
- stderr-only logging with `NSL_LSP_LOG_LEVEL` knob (per Q4).
- Real cancellation for `foldingRange` only (per Q5; the only
  cancellable T3 request).

**Scale/Scope**: 7 LSP wire messages — 4 sync/lifecycle methods
(`initialize`, `textDocument/didOpen`, `textDocument/didChange`,
`textDocument/didClose`) plus 1 server→client notification
(`textDocument/publishDiagnostics`) plus 1 client→server request
(`textDocument/foldingRange`) plus 1 cancellation notification
(`$/cancelRequest`). Lifecycle scaffolding (`initialized` /
`shutdown` / `exit`) is base-protocol overhead, not a counted
method. LSP framing layer (~200 LOC); protocol-handler dispatch
(~150 LOC); language-logic layer (~100 LOC); TUScheduler +
NslTU (~250 LOC); in-tree test harness (~250 LOC); ~750 LOC C++
headers + sources total in `lib/LSP/` + ~70 LOC in
`tools/nsl-lsp/main.cpp`. Tests: ~30 fixtures × ~25 LOC each ≈
750 LOC test code. Total in-tree footprint ≤ 1.5 KLOC.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

Per the project Constitution (`.specify/memory/constitution.md`
v1.7.0), the nine principles apply as follows. Status legend:
**PASS** — satisfied by construction or by an explicit plan
artefact; **N/A** — principle does not apply to this layer.

| # | Principle | Status | Notes |
|---|---|---|---|
| I | Spec Is Authoritative | **PASS** | T3 surfaces existing M3 Sema diagnostics through an LSP wire format; no spec edit. The "no silent AST drops" sub-clause does not apply (T3 is downstream of the parser). |
| II | Layered Library Architecture | **PASS** | `nsl-lsp` is a user-facing tooling binary per Principle II §3 ("`nsl-lsp`, `nsl-fmt`, `nsl-lint` are user-facing tooling binaries"). It links `libNSLFrontend.a` and reuses `Lexer`/`Preprocessor`/`Parser`/`Sema`/`SymbolTable`/`TypeSystem` without re-implementation. SC-005 enforces this structurally via linker-map inspection. The new `libNSLLSP.a` is a tooling library (not a compiler-track layer) and obeys the single-public-header rule with `include/nsl/LSP/Server.h`. |
| III | Stock CIRCT Below `nsl` Dialect | **N/A** | T3 is upstream of MLIR/CIRCT; no dialect or CIRCT involvement. |
| IV | Source-Locating Diagnostics | **PASS** | The LSP `Diagnostic.range` is computed from the existing `nsl::SourceLocation` plumbing; `#line` round-trip per the M1 invariant flows through unchanged. The diagnostic-mapping contract preserves the include-from-notes semantic via LSP `relatedInformation`. |
| V | Inspectable, Deterministic Pipeline | **PASS** | SC-003 enforces byte-identical `publishDiagnostics` across two runs over the same input. Diagnostics are sorted by `(loc, severity)` (existing `DiagnosticEngine` behavior); JSON serialization via `llvm::json::Value` is deterministic. The TUScheduler's worker count is configurable but does not affect output ordering. The `-emit=` flag does not extend to `nsl-lsp` (`nsl-lsp` is a server, not a stage); the inspectability requirement applies via the integration-test fixtures, which are themselves reproducible CLI artefacts. |
| VI | Layered Test Discipline | **PASS** | T3 introduces a new test layer at `test/lsp/`. Per the per-layer accepted-driver list, the compiler-track drivers are enumerated; tooling-track LSP testing follows the same TDD principle (fixture-first, observed-failing) using gtest + subprocess as the conventional driver. The audited-projects list is unchanged (T3 does not add audited projects; it consumes whatever is vendored at the time). |
| VII | Spec ↔ Design Coupling | **PASS** | T3 lands two coupling updates in the same PR: (a) `CLAUDE.md` §2.1 LSP-method roll-up rows for `publishDiagnostics` and `foldingRange` change from "T3" projection to "delivered at T3"; (b) `docs/design/nsl_tooling_design.md` §3 gains a "T3 status" annotation linking back to the contract under `specs/010-t3-lsp-skeleton/contracts/`. Forward extension points (T4/T5/T9/T10) remain projections in the same tables. |
| VIII | Test-First Development | **PASS** | `tasks.md` will require all four integration-test binaries to land first, observed failing against the unchanged tree (which has no `nsl-lsp` binary at all), then the implementation lands and all assertions pass. The XFAIL→green progression is recorded in commit history (or in PR description for squash-merge) per the no-retrofitted-tests clause. |
| IX | Continuous Integration & Delivery | **PASS** | The LSP integration tests integrate into `./scripts/ci.sh` stage 3 (unit & layer tests). The harness exits non-zero on any assertion failure. SPDX headers on every new file. No bypass. The new `nsl-lsp` binary is not a release artifact at T3 (no tagged release until M9); it ships as a built-from-source target consumed by the test harness only. |

**No Constitution violations.** No `Complexity Tracking` entries
needed.

**Re-check after Phase 1 design**: pending; perform after generating
data-model, contracts, and quickstart.

## Project Structure

### Documentation (this feature)

```text
specs/010-t3-lsp-skeleton/
├── spec.md                                    # /speckit-specify output
├── plan.md                                    # this file (/speckit-plan output)
├── research.md                                # Phase 0 (/speckit-plan)
├── data-model.md                              # Phase 1 (/speckit-plan)
├── quickstart.md                              # Phase 1 (/speckit-plan)
├── contracts/
│   ├── lsp-protocol.contract.md               # Phase 1 — initialize/lifecycle/sync/encoding/cancellation/logging
│   ├── diagnostic-mapping.contract.md         # Phase 1 — DiagnosticEngine → LSP Diagnostic seam
│   ├── folding-range.contract.md              # Phase 1 — AST → FoldingRange seam
│   └── lsp-test-harness.contract.md           # Phase 1 — integration-test driver shape
├── checklists/
│   └── requirements.md                        # /speckit-specify quality gate
└── tasks.md                                   # /speckit-tasks output (NOT this command)
```

### Source code (repository root)

```text
include/nsl/LSP/                                # NEW — single public header per Principle II
└── Server.h                                   # exposes runStdioServer(int argc, char** argv)

lib/LSP/                                        # NEW — libNSLLSP.a implementation
├── CMakeLists.txt                              # add_nsl_library(NSLLSP …)
├── JSONTransport.cpp                           # stdin/stdout JSON-RPC framing
├── JSONTransport.h                             # private header
├── NslLSPServer.cpp                            # LSP-protocol layer (dispatch, capabilities, lifecycle)
├── NslLSPServer.h                              # private header
├── NslServer.cpp                               # language-logic layer (diagnostics, folding)
├── NslServer.h                                 # private header
├── TUScheduler.cpp                             # threading + per-document cache
├── TUScheduler.h                               # private header
├── DiagnosticMapper.cpp                        # nsl::Diagnostic → LSP Diagnostic adapter (free fns)
├── DiagnosticMapper.h                          # private header
├── FoldingRangeBuilder.cpp                     # AST visitor → LSP FoldingRange[]
├── FoldingRangeBuilder.h                       # private header
├── PositionEncoding.cpp                        # byte-offset ↔ UTF-16 code-unit conversion
├── PositionEncoding.h                          # private header
└── Logger.cpp                                  # stderr logger w/ NSL_LSP_LOG_LEVEL parsing

tools/nsl-lsp/                                  # NEW — entry-point binary
├── CMakeLists.txt                              # add_executable(nsl-lsp main.cpp)
└── main.cpp                                    # ≤ 70 lines; calls nsl::lsp::runStdioServer

test/lsp/                                       # NEW — LSP integration test layer
├── CMakeLists.txt                              # gtest + lit-discovered targets
├── lit.local.cfg.py                            # disables lit suffix discovery on .json fixture files
├── LspSession.cpp                              # in-tree gtest harness — spawn nsl-lsp, send/recv JSON-RPC
├── LspSession.h                                # public to test_unit code
├── lifecycle_test.cpp                          # FR-001/002/003/021 lifecycle scenarios
├── diagnostics_test.cpp                        # FR-005/006/007/010/011/012/013 diagnostic seam
├── folding_test.cpp                            # FR-014/015/016/017/022 folding seam
├── cancellation_test.cpp                       # FR-020h/i/j + SC-010 cancellation seam
└── fixtures/
    ├── s01_double_underscore.nsl               # one fixture per S-constraint with locked-string assertion
    ├── s02_wire_with_init.nsl
    ├── … (one per Sn with diagnostic; ~23 total)
    ├── parse_error_recovery.nsl                # parse-error fixture for FR-017 / US3 acceptance 4
    ├── empty.nsl                               # empty-document edge case
    ├── module_with_blocks.nsl                  # folding-range coverage of all block-openers
    ├── multiline_block_comment.nsl             # FR-015 comment-fold coverage
    ├── large_file.nsl                          # ≥ 1500-line fixture for SC-004 budget
    └── cancellation_target.nsl                 # large fixture for SC-010 cancellation budget

scripts/ci.sh                                   # AMEND — stage 3 (unit-tests) gains lsp-integration sub-step

CMakeLists.txt                                  # AMEND — add_subdirectory(lib/LSP) + tools/nsl-lsp + test/lsp
```

**Structure Decision**:
- `nsl-lsp` follows the existing `nslc` / `nsl-opt` pattern: thin
  binary at `tools/<name>/main.cpp`, implementation library under
  `lib/<Name>/`, single public header at `include/nsl/<Name>/`. The
  reasoning is consistency with the Principle II library layout —
  the tooling binaries enjoy the same internal/external split the
  compiler libraries enjoy.
- The `test/lsp/` tree is parallel to existing `test/sema/`,
  `test/parse/`, etc. Lit does not drive the LSP tests directly —
  ctest does (gtest registration). However, since some fixtures
  are `.nsl` files with embedded JSON-RPC expected-output blocks,
  lit's suffix-based discovery would otherwise pick them up; the
  `lit.local.cfg.py` at `test/lsp/` opts out (matches the
  precedent set by `test/tooling/` for T1).
- `include/nsl/LSP/Server.h` is the **single** public header
  exposing only `runStdioServer`. Internals (`NslLSPServer`,
  `NslServer`, `TUScheduler`, `NslTU`, mapper / builder / logger
  utilities) are private to `lib/LSP/`. Tests that need internal
  access include the private headers via project-internal include
  paths (the same convention `test_unit/` uses for the M3 Sema
  internals).

## Complexity Tracking

> Empty — Constitution Check passes on all 9 principles with no
> deviations.

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| _none_    | _none_     | _none_                              |
