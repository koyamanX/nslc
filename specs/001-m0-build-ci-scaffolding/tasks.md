<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Tasks: M0 — Build & CI Scaffolding (with P-CI)

**Branch**: `001-m0-build-ci-scaffolding` | **Date**: 2026-04-26
**Input**: Design documents from `/specs/001-m0-build-ci-scaffolding/`
**Prerequisites**: [plan.md](./plan.md), [spec.md](./spec.md), [research.md](./research.md), [data-model.md](./data-model.md), [contracts/](./contracts/), [quickstart.md](./quickstart.md)

**Tests**: Test tasks are **MANDATORY** for this project per Constitution Principle VIII (Test-First Development, NON-NEGOTIABLE). Every user story includes test tasks at the appropriate layer (CMake macro fixtures, lit smoke fixtures, pytest fixtures, CTest determinism gate). Tests MUST be written and observed FAILING against the unchanged tree before the corresponding implementation tasks begin.

**Organization**: Tasks are grouped by user story (US1 P1, US2 P1, US3 P2) to enable independent implementation and testing. The three user stories of this feature are:
- **US1**: Contributor builds the project from a fresh clone (9 libs + `nslc --version` + per-layer lit smokes)
- **US2**: Every PR is automatically gated by CI (six-stage pipeline + branch protection + local reproducibility + determinism)
- **US3**: License/provenance enforced on every new file (SPDX-header check)

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: User story label (US1, US2, US3) — REQUIRED for tasks inside a user-story phase
- File paths are absolute under repo root unless noted

## Path Conventions

Single-project compiler layout per `plan.md` §Project Structure:
- Headers: `include/nsl/<Layer>/`
- Sources: `lib/<Layer>/`
- Driver: `tools/nslc/`
- Lit tests: `test/<Layer>/`
- Unit tests: `test_unit/<suite>/`
- CMake helpers: `cmake/`
- Scripts: `scripts/`
- CI workflow: `.github/workflows/ci.yml`

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization and the empty directory skeleton every later phase drops files into.

- [x] T001 [P] Create root `.gitignore` excluding `build/`, `build1/`, `build2/`, `*.bak`, `*.swp`, `__pycache__/`, `.lit_test_times.txt`
- [x] T002 [P] Create `cmake/deps.lock` pinning the LLVM/MLIR/CIRCT prebuilt tarball URL + SHA-256 (research §2; one tarball checked into the project's `nslc-deps` GitHub release)
- [x] T003 Create top-level `CMakeLists.txt`: `cmake_minimum_required(VERSION 3.22)`, `project(nslc LANGUAGES CXX)`, `set(CMAKE_CXX_STANDARD 17)`, `set(CMAKE_CXX_STANDARD_REQUIRED ON)`, `set(CMAKE_CXX_EXTENSIONS OFF)`, default `CMAKE_BUILD_TYPE=Release`, option `NSL_BUILD_TESTS` (default ON)
- [x] T004 [P] Create per-layer header dirs with `.keep` files: `include/nsl/{Basic,Preprocess,Lex,Parse,AST,Sema,Dialect/NSL/IR,Lower,Driver}/.keep` (9 dirs)
- [x] T005 [P] Create per-layer source dirs (placeholder, will be populated with `CMakeLists.txt` in US1): `lib/{Basic,Preprocess,Lex,Parse,AST,Sema,Dialect/NSL/IR,Lower,Driver}/.keep`
- [x] T006 [P] Create per-layer test dirs (placeholder for `.lit-smoke.test`): `test/{Basic,Preprocess,Lex,Parse,AST,Sema,Dialect,Lower,Driver}/.keep`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: CMake plumbing, determinism toolchain, and lit/test wiring that every user story consumes.

**⚠️ CRITICAL**: No user-story work can begin until this phase is complete.

- [x] T007 Create `cmake/NSLDeterminism.cmake` per research §4: set `CMAKE_C_ARCHIVE_CREATE` / `CMAKE_CXX_ARCHIVE_CREATE` to use `ar Drcs`, append linker flag `-Wl,--build-id=none`, append compile flags `-ffile-prefix-map=${CMAKE_SOURCE_DIR}=. -fmacro-prefix-map=${CMAKE_SOURCE_DIR}=. -frandom-seed=$<TARGET_OBJECTS:>` (FR-018, SC-005, Principle V)
- [x] T008 [P] Create `cmake/modules/` (initially empty `CMakeLists.txt` placeholder; `FindCIRCT.cmake` only if CIRCT's bundled config is insufficient — see research §2)
- [x] T009 Add to top-level `CMakeLists.txt`: `find_package(MLIR REQUIRED CONFIG)`, `find_package(CIRCT REQUIRED CONFIG)`, include `cmake/NSLDeterminism.cmake`, include `cmake/AddNSLLibrary.cmake` (research §2; depends T007)
- [x] T010 Create `lib/CMakeLists.txt` listing `add_subdirectory()` for all 9 layers in §3 order (Basic → Preprocess → Lex → AST → Parse → Sema → Dialect/NSL/IR → Lower → Driver)
- [x] T011 [P] Create `test/CMakeLists.txt` invoking `add_lit_testsuite(check-nslc "Running the NSLC regression tests" ${CMAKE_CURRENT_SOURCE_DIR})` per research §9
- [x] T012 [P] Create `test/lit.cfg.py`: define substitutions `%nslc`, `%FileCheck`, `%spdx_check`; add suffix list `['.test', '.nsl']`; root config only (per-layer `lit.local.cfg` deferred per research §9)
- [x] T013 [P] Create `test/lit.site.cfg.py.in` template with `@NSLC_BINARY_DIR@`, `@LLVM_TOOLS_BINARY_DIR@`, `@PYTHON_EXECUTABLE@` substitution slots
- [x] T014 [P] Create `test_unit/CMakeLists.txt`: `enable_testing()`, `find_package(GTest REQUIRED)`, `find_package(Python3 REQUIRED COMPONENTS Interpreter)`, `add_subdirectory()` placeholders for the three suite dirs added in US1/US2/US3
- [x] T015 Wire `add_subdirectory(lib)` + `add_subdirectory(tools)` + `add_subdirectory(test)` + `add_subdirectory(test_unit)` into top-level `CMakeLists.txt` (depends T010–T014)

**Checkpoint**: `cmake -S . -B build -G Ninja` configures cleanly with zero compile targets registered. The skeleton is ready for US1 to add the 9 libraries and the `nslc` driver.

---

## Phase 3: User Story 1 — Contributor builds from a fresh clone (Priority: P1) 🎯 MVP

**Goal**: From a clean clone on Linux x86_64 with documented prerequisites, a single configure-and-build invocation produces all nine static-library archives, the `nslc` driver binary, and a green `lit` run with one passing smoke test per existing layer.

**Independent Test**: `cmake -S . -B build -G Ninja && cmake --build build` produces 9 `.a` archives under `build/lib/nsl/<Layer>/` and `build/bin/nslc`. Then `./build/bin/nslc --version` exits 0 with stdout matching `^nslc [0-9A-Za-z._+-]+$`. Then `cd build && lit -v ../test` runs ≥10 fixtures (9 layer smokes + driver version test), all green.

### Tests for User Story 1 (MANDATORY per Constitution Principle VIII) ⚠️

> **NOTE: Write these tests FIRST. They MUST be observed FAILING against the unchanged tree before any implementation task in this story begins.**

- [ ] T016 [P] [US1] Create CMake test fixture `test_unit/add_nsl_library_test/valid_layer_name/CMakeLists.txt`: invokes `add_nsl_library(nsl-basic)`, asserts configure succeeds (contract `add_nsl_library.contract.md` §Test row 1)
- [ ] T017 [P] [US1] Create CMake test fixture `test_unit/add_nsl_library_test/unknown_layer_name/CMakeLists.txt`: invokes `add_nsl_library(nsl-bogus)`; expected configure-time `FATAL_ERROR` containing the literal "§3 layer table" (contract row 2; CTest `WILL_FAIL`)
- [ ] T018 [P] [US1] Create CMake test fixture `test_unit/add_nsl_library_test/downward_dep/CMakeLists.txt`: `add_nsl_library(nsl-lex DEPENDS nsl-basic)` succeeds (contract row 3)
- [ ] T019 [P] [US1] Create CMake test fixture `test_unit/add_nsl_library_test/upward_dep/CMakeLists.txt`: `add_nsl_library(nsl-basic DEPENDS nsl-lex)` configure-FAILS with `index M ≥ N` diagnostic (contract row 4; `WILL_FAIL`)
- [ ] T020 [P] [US1] Create CMake test fixture `test_unit/add_nsl_library_test/sibling_bypass/CMakeLists.txt`: `add_nsl_library(nsl-lex DEPENDS nsl-parse)` configure-FAILS (cyclic; contract row 5; `WILL_FAIL`)
- [ ] T021 [P] [US1] Create CMake test fixture `test_unit/add_nsl_library_test/multi_header_basic/CMakeLists.txt`: `nsl-basic` declares both `SourceLocation.h` and `Diagnostic.h` in `HEADERS` (contract row 6)
- [ ] T022 [P] [US1] Create CMake test fixture `test_unit/add_nsl_library_test/per_node_headers_ast/CMakeLists.txt`: `nsl-ast` declares `Decl.h`, `Stmt.h`, `Expr.h` in one `HEADERS` (contract row 7)
- [ ] T023 [US1] Create `test_unit/add_nsl_library_test/CMakeLists.txt` registering all 7 fixtures via `add_test`, with CTest `WILL_FAIL TRUE` set on the four expected-failure cases (T017, T019, T020 — and T021/T022 are positive) (depends T016–T022)
- [ ] T024 [P] [US1] Create lit fixture `test/Driver/version.test`: `RUN: %nslc --version | FileCheck %s` + `CHECK: {{^nslc [0-9A-Za-z._+-]+$}}` (contract `nslc-version.contract.md` §Test contract; FR-006; spec Q5)
- [ ] T025 [P] [US1] Create per-layer smoke lit fixtures (9 files): `test/Basic/.lit-smoke.test`, `test/Preprocess/.lit-smoke.test`, `test/Lex/.lit-smoke.test`, `test/Parse/.lit-smoke.test`, `test/AST/.lit-smoke.test`, `test/Sema/.lit-smoke.test`, `test/Dialect/.lit-smoke.test`, `test/Lower/.lit-smoke.test`, `test/Driver/.lit-smoke.test` — each contains `RUN: echo "smoke for <layer>" | FileCheck %s` + `CHECK: smoke for <layer>` (FR-007)

### Implementation for User Story 1

- [ ] T026 [US1] Implement `cmake/AddNSLLibrary.cmake` per contract `add_nsl_library.contract.md` §Behavior: layer-table validation (citing `nsl_compiler_design.md §3` in error string), `add_library STATIC`, C++17 + PIC pin, `HEADERS` install via `FILE_SET`, `DEPENDS` dependency-direction guard with index check, `LINK_LIBS` external linkage, `EXCLUDE_FROM_LIBNSLFRONTEND` flag, aggregate-property `NSL_FRONTEND_LIBS` registration (depends T016–T023; observe T016/T018/T021/T022 PASS and T017/T019/T020 fail-as-expected)
- [ ] T027 [P] [US1] Implement `cmake/NSLVersion.cmake`: `find_package(Git)`, `execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty OUTPUT_VARIABLE NSLC_GIT_DESCRIBE OUTPUT_STRIP_TRAILING_WHITESPACE)` with `Git_FOUND` fallback to literal `unknown` (research §5; FR-006)
- [ ] T028 [P] [US1] Create template `include/nsl/Driver/Version.h.in`: `#define NSLC_VERSION_STRING "@NSLC_GIT_DESCRIBE@"` (consumed by `configure_file` from T027)
- [ ] T029 [P] [US1] Implement `tools/nslc/main.cpp` (≤60 lines) per contract `nslc-version.contract.md` §Implementation skeleton: include `nsl/Driver/Version.h`; `--version` / `-v` / `-V` print `nslc <NSLC_VERSION_STRING>\n` to stdout, exit 0; any other argv → exit 2 + usage line on stderr (FR-005, FR-006, SC-002)
- [ ] T030 [P] [US1] Create `tools/nslc/CMakeLists.txt`: `configure_file(${CMAKE_SOURCE_DIR}/include/nsl/Driver/Version.h.in ${CMAKE_BINARY_DIR}/include/nsl/Driver/Version.h @ONLY)`; `add_executable(nslc main.cpp)`; `target_link_libraries(nslc PRIVATE nsl-driver)`; `set_target_properties(nslc PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)`
- [ ] T031 [P] [US1] Create `tools/CMakeLists.txt`: `add_subdirectory(nslc)`
- [ ] T032 [P] [US1] Create `lib/Basic/CMakeLists.txt`: `add_nsl_library(nsl-basic)` — no sources, no DEPENDS at M0 (FR-001 row 1)
- [ ] T033 [P] [US1] Create `lib/Preprocess/CMakeLists.txt`: `add_nsl_library(nsl-preprocess DEPENDS nsl-basic)` (FR-001 row 2)
- [ ] T034 [P] [US1] Create `lib/Lex/CMakeLists.txt`: `add_nsl_library(nsl-lex DEPENDS nsl-basic)` (FR-001 row 3)
- [ ] T035 [P] [US1] Create `lib/AST/CMakeLists.txt`: `add_nsl_library(nsl-ast DEPENDS nsl-basic)` (FR-001 row 5; ordered before nsl-parse per data-model.md §entity 1)
- [ ] T036 [P] [US1] Create `lib/Parse/CMakeLists.txt`: `add_nsl_library(nsl-parse DEPENDS nsl-lex nsl-ast)` (FR-001 row 4)
- [ ] T037 [P] [US1] Create `lib/Sema/CMakeLists.txt`: `add_nsl_library(nsl-sema DEPENDS nsl-ast)` (FR-001 row 6)
- [ ] T038 [P] [US1] Create `lib/Dialect/NSL/IR/CMakeLists.txt`: `add_nsl_library(nsl-dialect LINK_LIBS MLIRIR CIRCTHWDialect)` — no intra-NSL DEPENDS at M0 (FR-001 row 7; Principle III stock-CIRCT consumption)
- [ ] T039 [P] [US1] Create `lib/Lower/CMakeLists.txt`: `add_nsl_library(nsl-lower DEPENDS nsl-sema nsl-dialect LINK_LIBS CIRCTHWDialect CIRCTComb CIRCTSeq CIRCTSV)` (FR-001 row 8)
- [ ] T040 [P] [US1] Create `lib/Driver/CMakeLists.txt`: `add_nsl_library(nsl-driver HEADERS Version.h DEPENDS nsl-basic nsl-preprocess nsl-lex nsl-parse nsl-ast nsl-sema nsl-dialect nsl-lower)` and includes the configure-time-generated `Version.h` (FR-001 row 9)

**Checkpoint**: `cmake -S . -B build -G Ninja && cmake --build build && ./build/bin/nslc --version && lit -v test/ && ctest --test-dir build` all green. **MVP complete — US1 deliverable demoable.**

---

## Phase 4: User Story 2 — Every PR is automatically gated by CI (Priority: P1)

**Goal**: Six-stage CI pipeline (Build matrix → Static checks → Unit/layer → Lowering → End-to-end → Formal) runs on every PR and push to `main`. `scripts/ci.sh` is the authoritative local-reproduction entry point and is mirrored byte-for-byte by `.github/workflows/ci.yml`. Branch protection enforces required-checks for everyone including admins. Two CI runs on the same SHA produce byte-identical artifacts.

**Independent Test**: Open a PR with deliberate breakage (e.g., remove a `CHECK:` from a `.lit-smoke.test`). Observe CI marking the run red at the right stage with a path-resolved diagnostic. Run `./scripts/ci.sh <stage>` locally, observe the same failure. Restore. Observe CI green and the merge gate releases. Compare artifact bytes between two CI runs on the same SHA — `diff -r build1/lib build2/lib && diff -r build1/bin build2/bin` exits 0.

### Tests for User Story 2 (MANDATORY per Constitution Principle VIII) ⚠️

> **NOTE: Write these tests FIRST. They MUST be observed FAILING against the unchanged tree before any implementation task in this story begins.**

- [ ] T041 [P] [US2] Create CTest fixture `test_unit/determinism_test/CMakeLists.txt`: invokes `scripts/check_determinism.sh build build2`, asserts exit 0; this is the SC-005 / FR-018 acceptance gate (US2 acceptance scenario 3 + scenario 5)
- [ ] T042 [P] [US2] Create pytest `test_unit/ci_sh_test/test_dispatch.py`: invoke `scripts/ci.sh nonexistent-stage` → exit ≠ 0 with `unknown stage` message; invoke `scripts/ci.sh e2e` → exit 0 with stdout containing `wired but empty` (FR-015); invoke `scripts/ci.sh formal` → same wired-but-empty contract; invoke `scripts/ci.sh all` on a clean tree at the same git ref where remote CI is green → exit 0 (FR-017, SC-006)
- [ ] T043 [P] [US2] Create pytest `test_unit/ci_yml_test/test_ci_yml.py`: parse `.github/workflows/ci.yml` as YAML; assert presence of jobs `build-matrix`, `static-checks`, `unit-and-layer-tests`, `lowering-tests`, `end-to-end`, `formal`; assert `build-matrix.runs-on == 'ubuntu-22.04'` (research §7); assert `build-matrix.strategy.matrix` cross-product is exactly `Debug × Release × {gcc, clang}` (4 cells; spec Q2); assert `end-to-end` and `formal` jobs have `if: false`
- [ ] T044 [P] [US2] Create pytest `test_unit/branch_protection_test/test_protection_json.py`: parse `.github/branch-protection.json`; assert `enforce_admins == true`, `allow_force_pushes == false`, `allow_deletions == false`; assert `required_status_checks.contexts` contains exactly the 7 names listed in `ci-pipeline.contract.md` §Required-checks list (FR-016, spec Q3)
- [ ] T045 [P] [US2] Wire pytest+CTest registrations: `test_unit/ci_sh_test/CMakeLists.txt`, `test_unit/ci_yml_test/CMakeLists.txt`, `test_unit/branch_protection_test/CMakeLists.txt`, and add `add_subdirectory()`s to `test_unit/CMakeLists.txt`

### Implementation for User Story 2

- [ ] T046 [US2] Implement `scripts/ci.sh` per contract `ci-pipeline.contract.md` §Local-CI shape: `set -euo pipefail`, dispatcher `case $1 in build-matrix|static-checks|unit-tests|lowering-tests|e2e|formal|all)`; `--matrix` flag for stage 1 to fan out 4 cells; stage `e2e` and `formal` echo "Stage N: wired but empty until M{7,8} — see roadmap" and exit 0 (FR-015); `all` runs 1→2→3→4 sequentially and stops at first failure (FR-017, FR-021; depends T041–T044)
- [ ] T047 [P] [US2] Create `scripts/check_determinism.sh`: `set -euo pipefail`, takes two build-dir args, runs `diff -r ${1}/lib ${2}/lib && diff -r ${1}/bin ${2}/bin`, exits non-zero on byte divergence (research §4 last step; called by stage 1 on the `Release × gcc` cell)
- [ ] T048 [P] [US2] Create `.clang-format` (LLVM base): `BasedOnStyle: LLVM` + project tweaks per LLVM/CIRCT convention (FR-009)
- [ ] T049 [P] [US2] Create `.clang-tidy` (LLVM-derived profile): inherit from LLVM base, enable `bugprone-*`, `cert-*`, `cppcoreguidelines-*` selectively per `nsl_compiler_design.md` §13; include a guard against `__DATE__`/`__TIME__`/`__TIMESTAMP__` macros (research §4 determinism) (FR-008)
- [ ] T050 [US2] Create `.github/workflows/ci.yml` per contract `ci-pipeline.contract.md` §Jobs: `build-matrix` 4-cell matrix on `ubuntu-22.04` with `fail-fast: false`; `static-checks` `needs: build-matrix` calling `./scripts/ci.sh static-checks`; `unit-and-layer-tests` `needs: build-matrix` calling `./scripts/ci.sh unit-tests`; `lowering-tests` `needs: build-matrix` calling `./scripts/ci.sh lowering-tests`; `end-to-end` and `formal` jobs with `if: false` plus a separate `sticky-comment` job emitting/updating a PR comment per research §8 (FR-013, FR-014, FR-015; depends T046)
- [ ] T051 [P] [US2] Create `.github/branch-protection.json` per contract `ci-pipeline.contract.md` §Required-checks list: 7 contexts (`build-matrix (Debug, gcc)`, `build-matrix (Debug, clang)`, `build-matrix (Release, gcc)`, `build-matrix (Release, clang)`, `static-checks`, `unit-and-layer-tests`, `lowering-tests`); `enforce_admins: true`; `allow_force_pushes: false`; `allow_deletions: false`; stages 5/6 deliberately excluded until M7/M8 land (FR-016, spec Q3)
- [ ] T052 [P] [US2] Create `.github/branch-protection.md` documenting the canonical settings, the `enforce_admins: true` rationale, and the **only permitted bypass** (GitHub repo-admin "merge without waiting for required checks" override + named reason recorded in PR description per spec Q3)
- [ ] T053 [P] [US2] Create `scripts/apply_branch_protection.sh`: invokes `gh api repos/${OWNER}/${REPO}/branches/main/protection --method PUT --input .github/branch-protection.json`; idempotent (re-running on an already-applied config surfaces "no changes")

**Checkpoint**: `./scripts/ci.sh all` exits 0 locally; `gh workflow run ci.yml` (or PR open) produces 6 jobs — 4 stage-1 cells + 3 blocking stages + 2 wired-but-empty (skipped + sticky-comment); branch protection on `main` blocks merge until the 7 required contexts are green; two CI runs on the same SHA produce byte-identical artifacts.

---

## Phase 5: User Story 3 — License/provenance enforced on every new file (Priority: P2)

**Goal**: When any file is added, the static-checks stage verifies the file carries `SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception` in the comment syntax appropriate for the file's extension, with a path-resolved diagnostic on failure. Files for which no comment-syntax recipe is registered FAIL loudly.

**Independent Test**: Open a PR adding a single `.cpp` file without an SPDX header. Observe CI's static-checks stage failing with `<path>:1: SPDX header missing or malformed` plus expected/observed lines. Add the header. Observe CI green. Try a wrong identifier (bare `Apache-2.0`) — diagnostic names both expected and observed. Try a `.xyz` extension with no recipe — diagnostic says "no recipe registered". A path on the version-controlled exception list passes silently.

### Tests for User Story 3 (MANDATORY per Constitution Principle VIII) ⚠️

> **NOTE: Write these tests FIRST. They MUST be observed FAILING against the unchanged tree before any implementation task in this story begins.**

- [ ] T054 [P] [US3] Create pytest fixture file `test_unit/spdx_check_test/fixtures/valid_md.md` (HTML-comment SPDX) and test function `test_valid_md_passes` in `scripts/check_spdx_test.py` per contract `spdx-check.contract.md` row 1
- [ ] T055 [P] [US3] Create fixture `test_unit/spdx_check_test/fixtures/valid_cpp.cpp` (C++ `//` SPDX) and test `test_valid_cpp_passes` (contract row 2)
- [ ] T056 [P] [US3] Create fixture `test_unit/spdx_check_test/fixtures/valid_py_with_shebang.py` (`#!` line 1, SPDX line 2) and test `test_valid_py_with_shebang_passes` (contract row 3)
- [ ] T057 [P] [US3] Create fixture `test_unit/spdx_check_test/fixtures/missing_header.cpp` (no SPDX line) and test `test_missing_header_fails`: exit 1, diagnostic names file:1 (contract row 4)
- [ ] T058 [P] [US3] Create fixture `test_unit/spdx_check_test/fixtures/wrong_identifier.cpp` (bare `Apache-2.0` without LLVM-exception) and test `test_wrong_identifier_fails`: diagnostic names BOTH expected AND observed lines (contract row 5)
- [ ] T059 [P] [US3] Create fixture `test_unit/spdx_check_test/fixtures/unknown.xyz` (no recipe) and test `test_unknown_extension_fails_loudly`: exit 1 with `no recipe registered for extension '.xyz'` diagnostic — silent skip is forbidden (contract row 6; FR-010)
- [ ] T060 [P] [US3] Test `test_exempt_path_skipped` in `scripts/check_spdx_test.py`: temp `spdx_exceptions.txt` listing the fixture path → recorded EXEMPT, exit 0 even though no header exists (contract row 7; FR-012)
- [ ] T061 [P] [US3] Test `test_stale_exception_fails`: exception list names `/tmp/nonexistent_path` → exit 1 with `stale exception list entry` message — prevents the exception list from silently rotting (contract row 8)
- [ ] T062 [P] [US3] Test `test_mixed_results_summary_correct`: invocation with 3 valid + 2 missing + 1 exempt → summary line `spdx-check: 3 passed, 2 failed, 1 exempt (out of 6 files)` exactly (contract row 9)
- [ ] T063 [P] [US3] Test `test_git_ls_files_mode_works`: `--all` flag invokes `git ls-files` and produces the same result as passing the equivalent file list (contract row 10; FR-010 / spec Q4 full-repo scope)
- [ ] T064 [US3] Wire pytest into CMake: `test_unit/spdx_check_test/CMakeLists.txt` adds `add_test(NAME spdx-check-tests COMMAND ${Python3_EXECUTABLE} -m pytest ${CMAKE_SOURCE_DIR}/scripts/check_spdx_test.py)` and is included from `test_unit/CMakeLists.txt` (depends T054–T063)

### Implementation for User Story 3

- [ ] T065 [US3] Implement `scripts/check_spdx.py` per contract `spdx-check.contract.md` §Per-file algorithm: per-extension recipe table per data-model.md §entity 4 (`.md` HTML / `.cpp` `//` / `.cmake` `#` / `.py`+`.sh` `#` with shebang skip / `.ebnf` `(* *)` / `.nsl` `//` / `.yml`+`.yaml`+`.toml` `#` / `.json` no-recipe-fail-loud); CLI args `--exceptions <path>` (default `scripts/spdx_exceptions.txt`), `--all` (= `git ls-files`), `<file>...`; output schema (per-failure block + summary line); exit codes 0/1/2 stable per contract §Exit codes; stale-exception detection (depends T054–T064)
- [ ] T066 [P] [US3] Create `scripts/spdx_exceptions.txt`: empty stub with `# format: one path per line; '#'-prefixed lines ignored` header (per data-model §entity 4b — empty at M0; LICENSE itself is the only file conventionally exempt and is not in repo as new)
- [ ] T067 [US3] Wire SPDX check into `scripts/ci.sh static-checks`: append `python3 scripts/check_spdx.py --all` as a sub-step alongside the `clang-format --dry-run --Werror` and `clang-tidy -p build` sub-steps; stage exit code is the max of the three sub-step exit codes (contract `ci-pipeline.contract.md` §Stage 2; FR-010, FR-011, spec Q4; depends T065 + T046)

**Checkpoint**: `python3 scripts/check_spdx.py --all` passes on the clean tree; deliberate header removal triggers exit-1 with path-resolved diagnostic; CI stage 2 runs the SPDX check on every PR + push to `main` against the full `git ls-files` output (spec Q4).

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Documentation hooks, governance gates, and final acceptance validation.

- [ ] T068 [P] Append "Building" section to `README.md` linking to [`specs/001-m0-build-ci-scaffolding/quickstart.md`](./quickstart.md) and naming `./scripts/ci.sh` as the local-reproduction entry point (FR-021)
- [ ] T069 [P] Append "Local CI reproduction" sub-section to `CONTRIBUTING.md` documenting the `./scripts/ci.sh` stage-name dispatch, the `--matrix` flag, the wired-but-empty stage status, and the spec-Q3 named-reason bypass requirement (FR-017, FR-021)
- [ ] T070 [P] Update SPECKIT START/END markers in root `CLAUDE.md` to point at [`specs/001-m0-build-ci-scaffolding/plan.md`](./plan.md) (already partial — verify and finalize)
- [ ] T071 Run `./scripts/ci.sh all` end-to-end on a clean clone per [`quickstart.md`](./quickstart.md); capture stdout + per-stage exit codes; verify SC-001..SC-008 are observable (build < documented time; `nslc --version` < 100 ms per SC-002; determinism gate green per SC-005; reviewer-locatable failure per SC-007)
- [ ] T072 [P] Run the `nsl-coupling-audit` agent against the working tree to verify Principle VII spec ↔ design coupling — the M0 row in [`docs/CLAUDE.md`](../../docs/CLAUDE.md) and the language-feature roll-up in [`CLAUDE.md`](../../CLAUDE.md) §1 must reflect the new build artifacts; resolve any findings before merge
- [ ] T073 Run the `nsl-constitution-review` agent against the branch (Principle V/VI/VII/VIII/IX gate per merge protocol); resolve any blocking findings before merge

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: No dependencies — can start immediately.
- **Phase 2 (Foundational)**: Depends on Phase 1. **BLOCKS all user stories.**
- **Phase 3 (US1 — P1)**: Depends on Phase 2.
- **Phase 4 (US2 — P1)**: Depends on Phase 2 and **at least the US1 implementation tasks T026–T040** because the determinism gate, stage-1 smoke (`nslc --version`), and stages 3/4 all consume the build outputs US1 produces. (US2 *test authoring* T041–T045 may proceed in parallel with US1 implementation — only the US2 *passing* state requires US1 done.)
- **Phase 5 (US3 — P2)**: Depends on Phase 2. The SPDX script and its tests are independent of US1/US2; the **CI wiring step T067** depends on US2's `scripts/ci.sh` (T046).
- **Phase 6 (Polish)**: Depends on US1 + US2 + US3 complete.

### User Story Dependencies

- **US1 (P1)**: No cross-story deps — pure build skeleton.
- **US2 (P1)**: Tests independent; passing-state requires US1's build outputs.
- **US3 (P2)**: Tests + impl independent of US1/US2; CI integration (T067) requires US2's `ci.sh`.

### Within Each User Story (Principle VIII)

- All `[Tests for US…]` tasks MUST be authored AND observed failing before the corresponding implementation task lands.
- US1: T016–T025 → T026 (macro impl) → T027–T040 (per-layer + driver impl, all parallel after T026 + T027).
- US2: T041–T045 → T046 (`ci.sh`) → T047–T053 (parallel after T046).
- US3: T054–T063 → T064 (CMake wiring) → T065 (`check_spdx.py`) → T066 + T067.

### Parallel Opportunities

- All [P] tasks in Phase 1 can run in parallel.
- All [P] tasks in Phase 2 can run in parallel after T007/T009 land the foundational pieces they branch from.
- US1 test-authoring tasks T016–T025 are all [P] — different files / different fixture dirs.
- US1 per-layer `lib/<Layer>/CMakeLists.txt` tasks T032–T040 are all [P] — different files, all gated only on T026.
- US2 test-authoring tasks T041–T044 are all [P]; implementation tasks T047–T053 are all [P] after T046 lands.
- US3 test-authoring tasks T054–T063 are all [P] — different fixture files.
- Different user stories can be staffed to different developers in parallel after Phase 2 lands.

---

## Parallel Examples

### US1 — Author all 7 macro fixtures + 1 driver-version fixture + 9 layer smokes simultaneously

```bash
# Once Phase 2 is green, fire all US1 test-authoring tasks together:
Task T016: test_unit/add_nsl_library_test/valid_layer_name/CMakeLists.txt
Task T017: test_unit/add_nsl_library_test/unknown_layer_name/CMakeLists.txt
Task T018: test_unit/add_nsl_library_test/downward_dep/CMakeLists.txt
Task T019: test_unit/add_nsl_library_test/upward_dep/CMakeLists.txt
Task T020: test_unit/add_nsl_library_test/sibling_bypass/CMakeLists.txt
Task T021: test_unit/add_nsl_library_test/multi_header_basic/CMakeLists.txt
Task T022: test_unit/add_nsl_library_test/per_node_headers_ast/CMakeLists.txt
Task T024: test/Driver/version.test
Task T025: test/{Basic,Preprocess,Lex,Parse,AST,Sema,Dialect,Lower,Driver}/.lit-smoke.test
```

### US1 — All 9 per-layer CMakeLists files after T026 lands

```bash
Task T032: lib/Basic/CMakeLists.txt
Task T033: lib/Preprocess/CMakeLists.txt
Task T034: lib/Lex/CMakeLists.txt
Task T035: lib/AST/CMakeLists.txt
Task T036: lib/Parse/CMakeLists.txt
Task T037: lib/Sema/CMakeLists.txt
Task T038: lib/Dialect/NSL/IR/CMakeLists.txt
Task T039: lib/Lower/CMakeLists.txt
Task T040: lib/Driver/CMakeLists.txt
```

### US3 — All 10 SPDX test fixtures + functions in parallel

```bash
Task T054..T063: 10 pytest fixtures and test functions
```

---

## Implementation Strategy

### MVP First (US1 only)

1. Complete Phase 1 (T001–T006) — directory skeleton.
2. Complete Phase 2 (T007–T015) — CMake foundation, lit wiring, find_package(MLIR/CIRCT).
3. Complete Phase 3 (T016–T040) — author US1 tests first (observe failing), then `add_nsl_library` macro, then per-layer CMakeLists + `nslc` driver.
4. **STOP and VALIDATE**: from a clean clone, `cmake --build build && ./build/bin/nslc --version && lit -v test/ && ctest --test-dir build` all green. Demoable.

US1 alone establishes the buildable substrate that every subsequent milestone (M1 lexer, M2 parser, …) drops files into.

### Incremental Delivery

1. **MVP (US1 only)** → buildable repo + smoke binary + lit test tree → demoable.
2. **+ US2** → six-stage CI online + branch protection enforcing required-checks + local `scripts/ci.sh` mirrors remote 1:1 + double-build determinism gate green → first PR can be opened against `main` with full Principle IX coverage.
3. **+ US3** → SPDX-header check enforced on every PR + push → license discipline machine-enforced from M0 onward.
4. **Polish + governance** → docs hooks, coupling-audit + constitution-review agents both green → merge gate releases.

### Parallel Team Strategy

With multiple developers:

1. **Developer A**: Phase 1 + Phase 2 + US1 (the critical path through MVP).
2. **Developer B**: US2 test-authoring (T041–T045) and US2 implementation files (`.clang-format`, `.clang-tidy`, branch-protection JSON/MD, `apply_branch_protection.sh`, `check_determinism.sh`) in parallel with Developer A — these are independent of US1 outputs.
3. **Developer C**: US3 entirely (script + tests + exception-list stub) — self-contained.
4. Once US1 lands, Developer B integrates US2's `ci.sh` and `ci.yml` against the now-buildable tree; Developer C's `check_spdx.py` is wired into US2's stage 2.

---

## Notes

- **Principle VIII (Test-First)** is non-negotiable. Every test task MUST land in a separate commit from its corresponding implementation task, and the test commit MUST be observed failing on the unchanged tree (CI red on the test commit + green on the impl commit demonstrates this; alternatively local `ctest` / `lit` / `pytest` red→green sequence is auditable in `git log`).
- **Principle VI (Test discipline)** mandates per-layer smoke fixtures (T025) using lit + FileCheck — no substitutes accepted. The 9 `.lit-smoke.test` fixtures land at M0 so all later milestones drop layer-specific tests in by file placement (FR-007).
- **Principle V (Determinism)** is enforced by T007 (NSLDeterminism.cmake), T041 (CTest gate), T047 (`check_determinism.sh`), and T050 (CI stage 1 last step). Any regression in any of these four is a Principle V violation and a merge blocker.
- **Principle IX wired-but-empty rule**: stages 5/6 (`end-to-end`, `formal`) ship `if: false` + sticky-comment per research §8 (T050) and MUST NOT appear in `branch-protection.json` `contexts` until M7/M8 (T051) — otherwise GitHub blocks every PR on a never-firing check.
- **Spec Q3 named-reason bypass**: the only legal bypass of the merge gate is GitHub's repo-admin "merge without waiting for required checks" override, recorded with a named reason in the PR description. T052 documents this; T051 wires `enforce_admins: true` to make it the *only* available bypass surface.
- **Spec Q4 SPDX scope**: full-repo `git ls-files` scan on every PR + push (not PR-changed-only) — caches latent violations from rebases or force-pushes. T067 wires this; T065 implements `--all`.
- **Spec Q5 version format**: `nslc <git-describe>` — pre-tag `nslc 0.0.0-dev+g<sha>`, tagged `nslc <tag>`, dirty `…-dirty`. T024 + T029 enforce.
- **AI-assist trailers**: per `CONTRIBUTING.md` §5, commits authored with AI assistance MUST carry `Assisted-by:` trailers. The `/speckit-implement` flow handles this automatically.
- **Linear / GitHub Issues**: per `MEMORY.md`, this work is tracked in Linear under team/project `nslc` with branch prefix `NSLC-<N>`. The /speckit-taskstoissues skill can convert these tasks to GitHub Issues if a per-task issue trail is desired.
