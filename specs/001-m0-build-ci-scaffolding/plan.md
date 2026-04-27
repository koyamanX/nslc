<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Implementation Plan: M0 — Build & CI Scaffolding (with P-CI)

**Branch**: `001-m0-build-ci-scaffolding` | **Date**: 2026-04-26 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/001-m0-build-ci-scaffolding/spec.md`

## Summary

Bootstrap the `nslc` project's compile-and-CI floor. Deliverables, all
mandated by the spec:

- **Nine empty C++17 static-library skeletons** (per
  `docs/design/nsl_compiler_design.md` §3) wired through a single
  `add_nsl_library` CMake macro that enforces Constitution Principle
  II's downward-only dependency direction.
- **lit + FileCheck** test scaffolding, with one passing smoke test
  per layer so subsequent milestones drop tests in by file placement.
- **Project-wide `.clang-tidy` and `.clang-format`** in LLVM/CIRCT
  conventions.
- **SPDX-header presence script** running against `git ls-files` on
  every CI run (spec Q4); script written first with fixture tests
  (Principle VIII).
- **Smoke `nslc --version` binary** that prints `nslc <git-describe>`
  to stdout, exit 0 (spec Q5); driver is ≤ ~60 lines per Principle II.
- **Six-stage GitHub Actions CI pipeline** per Principle IX: Build
  matrix (`Debug × Release × {GCC, Clang}` on Linux x86_64 — spec
  Q2) → Static checks (clang-format + clang-tidy + SPDX) →
  Unit/layer → Lowering (lit+FileCheck) → End-to-end (wired-but-empty
  pre-M7) → Formal (wired-but-empty pre-M8).
- **Branch-protection** enforcing required-checks for everyone
  including admins; the only bypass is GitHub's repo-admin "merge
  without waiting for required checks" override, requiring a named
  reason in the PR description (spec Q3).
- **`scripts/ci.sh`** as the single authoritative local-reproduction
  entry point (FR-017, FR-021).

Every artifact above is a constitutional invariant or spec FR;
implementation latitude is in *how* (CMake patterns, GitHub Actions
YAML shape, script language) rather than *what*.

## Technical Context

**Language/Version**: C++17 (Constitution Build/Code/Licensing — C++20 is forbidden until a constitutional amendment lifts the freeze). Helper scripts in **Python 3.8+** (lit's host language; LLVM standard) and **Bash** for thin shell glue.
**Primary Dependencies**: **LLVM + MLIR** at the CIRCT-pinned commit; **CIRCT** main; **GoogleTest** for unit tests (research §1); **lit + FileCheck** from the LLVM source tree (Principle VI mandate). All consumed from a vendored prebuilt LLVM/MLIR/CIRCT install (research §2) — no source build of LLVM in CI.
**Storage**: N/A (build-system feature).
**Testing**: **lit + FileCheck** for the lowering and end-to-end stages (Principle VI mandate, no substitutes). **GoogleTest** for unit-level fixture tests of the SPDX script and the `add_nsl_library` macro (research §1). At M0 each of the 9 layer test directories contains exactly one smoke fixture; each layer subsequently grows tests via file-drop placement.
**Target Platform**: Linux x86_64. macOS / additional architectures deferred (spec Assumptions; Principle IX permits adding later, never dropping).
**Project Type**: Compiler — single project with the LLVM/CIRCT-style `include/` + `lib/` + `tools/` + `test/` + `cmake/` + `scripts/` + `.github/workflows/` layout (see Project Structure below).
**Performance Goals**: `nslc --version` < 100 ms on the reference host (SC-002). No broader CI runtime SLOs at M0 — deliberately deferred until a measurement basis exists. (Recorded in spec coverage summary as "Performance: Clear; deeper SLOs Deferred".)
**Constraints**: byte-deterministic library archives + `nslc` binary across two CI runs of the same git ref (FR-018, SC-005, Principle V). Determinism strategy in research §4. SPDX scan covers the **full repo** on every CI run (FR-010, spec Q4). Branch-protection enforces required-checks for everyone including repo admins (FR-016, spec Q3); admin-override bypass requires a named reason in the PR description.
**Scale/Scope**: 9 library skeletons + 1 driver binary (`tools/nslc/main.cpp` ≤ 60 lines) + 1 GitHub Actions workflow file + 1 SPDX script + 1 local-CI script + 4 CMake helper modules. Fixture-test count: 1 smoke test per layer (9) + 1 driver smoke (`nslc --version`) + ~3–6 SPDX fixture tests + ~3 macro fixture tests = ~16–19 tests at M0. Intentionally small: M0 establishes the substrate that later milestones consume.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

Evaluation against Constitution v1.4.0 (the version in
`.specify/memory/constitution.md`):

| Principle | Applies to M0? | Status | Notes |
|---|---|---|---|
| **I. Spec Is Authoritative** | Indirectly | ✅ | M0 introduces no language features; no edits to `docs/spec/*.ebnf` are required. The `Sn`/`Nn`/`Pn` numbering authority is unaffected. |
| **II. Layered Library Architecture** | **Yes — load-bearing** | ✅ | The build's reason for being. `add_nsl_library` MUST refuse to wire a dependency that violates `nsl_compiler_design.md` §3's downward-only direction. The 9 layer names are fixed by the table; the `nsl-ast` per-node-headers exception is preserved by the macro accepting multiple HEADERS args. Driver `main.cpp` ≤ ~60 lines (FR-005). Sole macro is the only library-declaration mechanism (FR-002). |
| **III. Stock CIRCT Below `nsl` Dialect** | Forward-looking | ✅ | M0 ships no dialect content. Research §2 (LLVM/CIRCT consumption) MUST yield a path that lets M4+ wire `hw`/`comb`/`seq`/`fsm`/`sv` directly without forking; the vendored-prebuilt approach satisfies this. |
| **IV. Source-Locating Diagnostics** | Forward-looking | ✅ | M0 has no diagnostic-emitting code. The `add_nsl_library` macro enforces the per-layer `include/nsl/<Layer>/` header convention, which the future `SourceLocation`/`Diagnostic` headers in `nsl-basic` will inhabit. |
| **V. Inspectable, Deterministic Pipeline** | **Yes — gating** | ✅ | At M0, the only "stage output" is `nslc --version`, which is built from a configure-time-substituted git-describe string and is constant for a given ref. **Library `.a` archives and the `nslc` binary MUST be byte-identical between two builds at the same ref** — research §4 nails the determinism toolchain (`ar D` deterministic mode, `--build-id=none` linker flag, no `__DATE__`/`__TIME__` macros, mtime-stripped install rules). FR-018 + SC-005 enforce. |
| **VI. Layered Test Discipline** | **Yes — load-bearing** | ✅ | Each of the 9 layer test directories ships with one passing smoke fixture (FR-007). Lowering and end-to-end tests use lit + FileCheck (mandatory; no substitutes). Stages 5 and 6 are wired-but-empty until M7 / M8 (FR-015) — the wiring is committed at M0 so the pipeline shape never moves. |
| **VII. Spec ↔ Design Coupling** | **Yes** | ✅ | M0 implements `docs/design/nsl_compiler_design.md` §3 (layer table) and §13 (build system) verbatim. **No edits to `docs/spec/*.ebnf` and no edits to `docs/design/`** are required by this plan. The §13 layout (`include/nsl/<Layer>/`, `lib/<Layer>/`, `tools/nslc/`, `test/<Layer>/`) is the project structure below. The plan triggers `nsl-coupling-audit` on PR open, but the audit will find nothing to update (deliberate). |
| **VIII. Test-First Development** | **Yes — gating** | ✅ | The non-trivial pieces (SPDX script, `add_nsl_library` macro, the smoke `nslc --version`) MUST land their tests first. Tasks plan will sequence each with a "test commit (observed failing) → implementation commit (passes)" pair. The fixture for `nslc --version` is a lit `RUN: %nslc --version \| FileCheck %s` test that shipping the binary makes pass. |
| **IX. Continuous Integration & Delivery** | **Yes — IS the deliverable** | ✅ | The entire P-CI half of this feature is the operationalization of Principle IX. All 6 stages, the local-reproduction entry point, the no-bypass clause, the named-reason override (spec Q3) — all flow through P-CI. M0 is the trigger to flip from the "transitional clause" (PR submitter runs local-equivalent and links output) to the steady-state ("CI runs automatically"). |
| **Build/Code/Licensing Standards** | Yes | ✅ | C++17 enforced via `target_compile_features(<lib> PUBLIC cxx_std_17)` + `set(CMAKE_CXX_EXTENSIONS OFF)`. LLVM/CIRCT coding conventions enforced via `.clang-format` (LLVM base) + `.clang-tidy` (LLVM-derived). Apache-2.0 WITH LLVM-exception SPDX header on every new file (FR-010..012). `add_nsl_library` matches LLVM-style helper-macro convention. |
| **Development Workflow** | Yes | ✅ | This plan was drafted via `/speckit-specify` → `/speckit-clarify` → `/speckit-plan` per the Spec Kit pipeline mandate. Tasks will route through `/speckit-tasks` → `/speckit-implement`. AI-assisted commits will carry `Assisted-by:` trailers per `CONTRIBUTING.md` §5. |
| **External Integrations** (Linear / GitHub Issues / CodeRabbit) | Yes | ✅ | M0's PR(s) will be tracked via Linear (`NSLC-<N>` per memory) — feature-track work. CodeRabbit gate applies. The plan does NOT introduce any project-level integration changes (no MCP-server work, no Linear workflow edits). |
| **Governance — Milestone Plan** | Yes | ✅ | M0 + P-CI are the canonical first compiler-track deliverable per `README.md` §Roadmap. No milestone renumbering, no plan amendment. |

**Gate result: PASSES** on first evaluation. No violations to record in the Complexity Tracking section.

## Project Structure

### Documentation (this feature)

```text
specs/001-m0-build-ci-scaffolding/
├── plan.md                                  # this file
├── spec.md                                  # /speckit-specify + /speckit-clarify output (421 lines)
├── research.md                              # Phase 0 output — every Technical Context decision justified
├── data-model.md                            # Phase 1 output — the 5 entities expressed structurally
├── quickstart.md                            # Phase 1 output — clone → build → smoke verification
├── contracts/                               # Phase 1 output — interface contracts
│   ├── add_nsl_library.contract.md          # CMake macro signature + dependency-validation rules
│   ├── ci-pipeline.contract.md              # GitHub Actions workflow shape + required-check names
│   ├── nslc-version.contract.md             # `nslc --version` CLI: stdout schema, exit code, perf
│   └── spdx-check.contract.md               # `scripts/check_spdx.py` CLI + exit codes + diagnostics
├── checklists/
│   └── requirements.md                      # /speckit-specify validation (already exists)
└── tasks.md                                 # /speckit-tasks output (NOT created by /speckit-plan)
```

### Source Code (repository root)

```text
nslc/
├── CMakeLists.txt                           # NEW — top-level: project(), C++17, options, deps
├── cmake/                                   # NEW
│   ├── AddNSLLibrary.cmake                  # NEW — `add_nsl_library` macro (FR-002, FR-004)
│   ├── NSLVersion.cmake                     # NEW — git-describe → version string for `nslc --version` (FR-006, spec Q5)
│   ├── NSLDeterminism.cmake                 # NEW — toolchain flags for byte-stable archives (FR-018, Principle V)
│   └── modules/                             # NEW — find scripts (FindCIRCT.cmake if needed; otherwise empty at M0)
├── include/nsl/                             # NEW — public headers, one dir per layer
│   ├── Basic/                               # M1 layer dir (M0: empty + .keep)
│   ├── Preprocess/                          # M1
│   ├── Lex/                                 # M1
│   ├── Parse/                               # M2
│   ├── AST/                                 # M2 (per-node-kind headers; macro accepts multiple HEADERS — Principle II exception)
│   ├── Sema/                                # M3
│   ├── Dialect/NSL/IR/                      # M4
│   ├── Lower/                               # M5/M6
│   └── Driver/                              # M7
├── lib/                                     # NEW — implementation
│   ├── CMakeLists.txt                       # NEW — add_subdirectory()s the 9 layers
│   ├── Basic/CMakeLists.txt                 # NEW — `add_nsl_library(nsl-basic ...)` skeleton
│   ├── Preprocess/CMakeLists.txt            # NEW
│   ├── Lex/CMakeLists.txt                   # NEW
│   ├── Parse/CMakeLists.txt                 # NEW
│   ├── AST/CMakeLists.txt                   # NEW
│   ├── Sema/CMakeLists.txt                  # NEW
│   ├── Dialect/NSL/IR/CMakeLists.txt        # NEW
│   ├── Lower/CMakeLists.txt                 # NEW
│   └── Driver/CMakeLists.txt                # NEW
├── tools/nslc/                              # NEW
│   ├── CMakeLists.txt                       # NEW
│   └── main.cpp                             # NEW — ≤ ~60 lines; smoke `--version` (FR-005, FR-006)
├── test/                                    # NEW — lit-rooted tree
│   ├── CMakeLists.txt                       # NEW — wires `add_lit_testsuite` (CIRCT/LLVM convention)
│   ├── lit.cfg.py                           # NEW — root lit config (substitutions: %nslc, %FileCheck, %spdx_check)
│   ├── lit.site.cfg.py.in                   # NEW — configure-time substitution (NSLC_BINARY_DIR, etc.)
│   ├── Driver/version.test                  # NEW — `RUN: %nslc --version \| FileCheck %s` (TDD seed for FR-006)
│   ├── Basic/smoke.test                # NEW — per-layer smoke (the conventional "this layer's lit dir is wired" probe)
│   ├── Preprocess/smoke.test           # NEW
│   ├── Lex/smoke.test                  # NEW
│   ├── Parse/smoke.test                # NEW
│   ├── AST/smoke.test                  # NEW
│   ├── Sema/smoke.test                 # NEW
│   ├── Dialect/smoke.test              # NEW
│   └── Lower/smoke.test                # NEW
├── test_unit/                               # NEW — GoogleTest unit tests (separate from lit tree)
│   ├── CMakeLists.txt                       # NEW
│   ├── spdx_check_test/                     # NEW — fixture-driven tests for scripts/check_spdx.py (TDD seed for FR-010..012)
│   └── add_nsl_library_test/                # NEW — CMake-time tests for the dependency-direction guard (TDD seed for FR-002, FR-004)
├── scripts/                                 # NEW
│   ├── ci.sh                                # NEW — local-reproduction entry point (FR-017, FR-021)
│   └── check_spdx.py                        # NEW — SPDX presence checker (FR-010..012)
├── .github/                                 # NEW
│   ├── workflows/
│   │   └── ci.yml                           # NEW — 6-stage matrix workflow (FR-013..020)
│   └── branch-protection.md                 # NEW — documented config (manual apply via `gh api` or UI; FR-016, spec Q3)
├── .clang-tidy                              # NEW — LLVM/CIRCT-derived profile (FR-008)
├── .clang-format                            # NEW — LLVM base (FR-009)
├── docs/                                    # EXISTING — unchanged by this feature
│   ├── CLAUDE.md
│   ├── design/
│   └── spec/
├── examples/                                # EXISTING — 20 .nsl files, all SPDX-headered, untouched by M0
├── README.md                                # MODIFIED — add a "Building" section pointing to quickstart.md (small)
├── CONTRIBUTING.md                          # POSSIBLY MODIFIED — append a "Local CI reproduction" sub-section pointing to scripts/ci.sh
├── CLAUDE.md                                # MODIFIED — SPECKIT START/END marker updated to point to this plan.md
└── LICENSE                                  # EXISTING — Apache-2.0 WITH LLVM-exception
```

**Structure Decision**: Single-project compiler layout matching CIRCT
and LLVM convention. Source split between `include/nsl/<Layer>/`
(public headers, per Principle II) and `lib/<Layer>/` (private
implementations). The driver lives in `tools/nslc/`. Two test trees:
**`test/`** for lit + FileCheck (mandatory for lowering and
end-to-end per Principle VI; convention-aligned with LLVM/CIRCT) and
**`test_unit/`** for GoogleTest unit fixtures (research §1). The
`cmake/` directory holds the `add_nsl_library` macro plus version-
derivation and determinism helpers. `scripts/` holds the SPDX
checker and the local-CI entry point. `.github/workflows/ci.yml` is
the six-stage pipeline. This layout is mandated by Constitution
Principle II + `nsl_compiler_design.md` §13 + the 9-layer table in
§3; no alternate structures are viable without an amendment.

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

No violations to track. Constitution Check (above) passes on first
evaluation and re-evaluation post-design (see Phase 1 closing
re-check at the end of `research.md`).
