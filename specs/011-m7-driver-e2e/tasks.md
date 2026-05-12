<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

---

description: "Task list for M7 — `nsl-driver` end-to-end (`nslc -emit=verilog`); P-VEN vendoring + P-VCD golden VCDs + audited-corpus regression"

---

# Tasks: M7 — `nsl-driver` end-to-end + P-VEN + P-VCD + audited-corpus regression

**Input**: Design documents from `/specs/011-m7-driver-e2e/`
**Prerequisites**: [`plan.md`](./plan.md) (required), [`spec.md`](./spec.md) (required for user stories), [`research.md`](./research.md), [`data-model.md`](./data-model.md), [`contracts/`](./contracts/)

**Tests**: Test tasks are **MANDATORY** for this project per Constitution Principle VIII (Test-First Development, NON-NEGOTIABLE) and Principle VI (end-to-end clause is NON-NEGOTIABLE at M7 specifically). Every new TU + every audited-corpus cell lands with its fixture authored first (red-state captured); the audited-corpus regression's 8 cells (4 projects × 2 simulators; narrowed from originally-projected 14 per FR-009 amendment 2026-05-12) are themselves the M7 acceptance gate. Tests MUST be observed FAILING before the corresponding implementation tasks begin.

**Organization**: Tasks are grouped by user story (US1–US4 from [`spec.md`](./spec.md)) to enable independent implementation and testing. US1 is the keystone (P1 — `nslc -emit=verilog` operational); US2 is P-VEN vendoring (P2); US3 is P-VCD golden VCDs (P2); US4 is the audited-corpus regression (P1 — the milestone acceptance gate). US1 + US4 together are required for M7 acceptance; US2 + US3 are prerequisites for US4.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3, US4)
- Include exact file paths in descriptions

## Path Conventions

Single project, LLVM-style layered architecture (per [`plan.md`](./plan.md) §Project Structure). All paths are relative to the repo root `/home/koyaman/devel/nslc/`. Build directory is `build-noasan/` inside the dev container per the libMLIR-ASan-mismatch memory.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Verify M6 baseline is green + scaffold the M7 test directory hierarchies and CMake module skeletons.

- [ ] T001 Verify M6 baseline build is green on master HEAD via `sg docker -c "docker run --rm -v $PWD:/workspace -w /workspace ghcr.io/koyamanX/nsl-nslc:dev bash -c 'cmake -G Ninja -B build-noasan -DCMAKE_BUILD_TYPE=Debug -DNSL_ENABLE_ASAN=OFF && ninja -C build-noasan check-nslc'"` — record the baseline pass count for regression-comparison post-M7. Expected: **620 PASS + 3 XFAIL** out of 623 (the M6 acceptance state per `CLAUDE.md` SPECKIT block prior to M7 update). **Deferred to maintainer docker verification** (out of sandbox scope; M6 acceptance state recorded by PR #14 merge `00b8d91`).
- [X] T002 [P] Create `test/Driver/emit_verilog/` subdirectory + `test/Driver/emit_verilog/fixtures/` subdirectory + `.gitkeep` placeholders. Lit auto-discovers via the existing `test/lit.cfg.py`; no config edit needed at scaffold time (mirrors M6 T003's no-op finding). **Done 2026-05-12**: directories created with `.gitkeep` placeholders.
- [X] T003 [P] Create `test/audited/` subdirectory + per-project subdirectories (7 placeholders: `cpu16/`, `mips32_single_cycle/`, `ahb_lite_nsl/`, `mmcspi/`, `SDRAM_Controler/`, `rv32x_dev/`, `turboV/`). Each gets a `.gitkeep` so the empty directories survive commit. Per-project `golden/` subdirectories also pre-created with `.gitkeep`. **Done 2026-05-12**: 7 project dirs × {`/`, `golden/`, `tb/`} = 21 dirs created with `.gitkeep` placeholders.
- [X] T004 [P] Author `cmake/AuditedCorpusLint.cmake` skeleton — `message(STATUS "AuditedCorpusLint: scaffold; no checks yet")`. Skeleton only at scaffold time; body lands in T045 once P-VEN structure is filled. Skeleton wired into top-level `CMakeLists.txt` via `include(cmake/AuditedCorpusLint.cmake)`. **Done 2026-05-12**: skeleton committed; includes `NSL_AUDITED_PROJECTS` + `NSL_LICENSE_COMPATIBLE` lists as the source-of-truth for the upcoming T045 body. Wired via `test/audited/CMakeLists.txt`'s `include(...)` (rather than top-level CMakeLists per task description — this keeps the audit scoped to the test subdirectory).
- [X] T005 [P] Author `cmake/audited_corpus.cmake` skeleton — declares a `check-audited` custom target with no per-project enumeration yet; the enumeration body lands in T071. Skeleton wired into `test/audited/CMakeLists.txt` (NEW M7 file — also created in this task). **Done 2026-05-12**: scaffold committed; `add_custom_target(check-audited)` placeholder exits 0; the enumeration body lands in T071. Wired in via `test/CMakeLists.txt`'s `add_subdirectory(audited)`.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Land the M7 dev-container surface (`:dev-m7` image) + the `nsl-driver` scaffolding (new public header, new TUs, CMakeLists edits, `tools/nslc/main.cpp` dispatch wire). After Phase 2, `nslc -emit=verilog input.nsl` runs through every stage and reaches `Compilation::emit` with an unimplemented body (failing with a clean diagnostic). ALL user-story tasks gate on this phase completing.

**⚠️ CRITICAL**: No user-story work begins until this phase is complete.

### 2A — Dev-container `:dev-m7` (lands as a separate prep commit or leading commit on M7 PR)

- [ ] T006 Author `docker/Dockerfile.dev` bump — add Verilator v5.024 (build-from-source via `git clone --branch v5.024 --depth 1 https://github.com/verilator/verilator.git` + autoconf+configure+make+install), add `gcc-riscv64-unknown-elf` apt package (then symlink-rename for rv32 prefix per [`container-m7.contract.md`](./contracts/container-m7.contract.md) §2), add `riscv-tests` build (clone + make `isa/`). Use `${PARENT_IMAGE}` build-arg per `project_publish_images_buildx_isolation.md` memory.
- [ ] T007 [P] Author `docker/publish-images.lockfile.yml` skeleton with schema per [`container-m7.contract.md`](./contracts/container-m7.contract.md) §6 — empty `tag/digest/published_at` fields; populated by the workflow at publish time.
- [ ] T008 [P] Amend `docker/publish-images.yml` GitHub Actions workflow — add a new job that publishes `ghcr.io/koyamanX/nsl-nslc:dev-m7` via `docker buildx build --build-arg PARENT_IMAGE=ghcr.io/koyamanX/nsl-nslc:dev -t ghcr.io/koyamanX/nsl-nslc:dev-m7 docker/`. The job updates `docker/publish-images.lockfile.yml` and commits-back via a follow-on PR (or writes-back inline if the workflow has write permission to the branch).
- [ ] T009 Trigger publish-images workflow on the M7 prep branch (manually via `gh workflow run publish-images.yml` from a sandbox-disabled shell per the `project_gh_auth_sandbox.md` memory). Verify the resulting `:dev-m7` image is reachable via `docker pull ghcr.io/koyamanX/nsl-nslc:dev-m7`. Record the published image SHA in `docker/publish-images.lockfile.yml`.
- [ ] T010 Author `scripts/test_container_m7.sh` — smoke test asserting `verilator --version`, `iverilog -V`, `riscv32-unknown-elf-gcc --version`, and `ls /opt/riscv-tests/isa/rv32ui-p-add.elf` all succeed inside the container. Per [`container-m7.contract.md`](./contracts/container-m7.contract.md) §5. The smoke test runs as the first step of the `check-audited` lit cell in T084.
- [ ] T011 Amend `scripts/ci.sh` — pin the lit-test cell that runs `check-audited` to `ghcr.io/koyamanX/nsl-nslc:dev-m7`; other CI cells continue with `:dev` per [`container-m7.contract.md`](./contracts/container-m7.contract.md) §4. Per-cell tag variable assignment lands as named CI-config variables for reviewability.

### 2B — `nsl-driver` scaffolding (new public header + TUs + CMakeLists)

- [X] T012 Author `include/nsl/Driver/EmitVerilog.h` — public entry-point header for `emitVerilog(...)` mirroring `include/nsl/Driver/EmitHW.h`'s shape per [`data-model.md`](./data-model.md) §4. Includes the boilerplate doc comment about exit codes (0/1/3/4 — code 4 NEW at M7 for split-file mkdir failure) and the no-partial-output rule. **Done 2026-05-12**: signature `emitVerilog(input_path, output_path, opts, os, err)` — added `output_path` parameter (diverges from `emitHW` signature; needed for Q1 → B dispatch).
- [X] T013 [P] Author `lib/Driver/RunCIRCTPasses.cpp` scaffold — declares `Compilation::runCIRCTPasses(mlir::ModuleOp)` body that constructs a `mlir::PassManager` with `ModuleOp` nesting, adds the three passes per [`circt-passes.contract.md`](./contracts/circt-passes.contract.md) §1 (`createConvertFSMToSVPass` → `createLowerSeqToSVPass` → `createPrepareForEmissionPass`), runs the pipeline, returns `pm.run(module)`. Scaffold-only: parallel-mode disabled (`pm.enableMultithreading(false)`), verifier enabled (`pm.enableVerifier(true)`), no `DiagnosticBridge` wiring yet (lands in T021). **Done 2026-05-12** as full body (not scaffold-only): RAII `DiagnosticBridge` already in place, `pm.getContext()->disableMultithreading()` + `pm.enableVerifier(true)` set, three passes invoked via `circt::createConvertFSMToSVPass()` + `circt::seq::createLowerSeqToSVPass()` + `circt::sv::createPrepareForEmissionPass()`. **Build-time verification deferred to maintainer docker run** — CIRCT header paths (`circt/Conversion/FSMToSV.h`, `circt/Dialect/Seq/SeqPasses.h`, `circt/Dialect/SV/SVPasses.h`) and exact factory-function names are best-guess and need docker build confirmation.
- [X] T014 [P] Author `lib/Driver/EmitVerilog.cpp` scaffold — `emitVerilog(input_path, opts, os, err)` mirroring `lib/Driver/EmitHW.cpp`'s shape. Pipeline glue: load+preprocess+lex+parse+sema+lowerToNSL+runNSLPasses+lowerToCIRCT+runCIRCTPasses+emit. The `emit` step is left as a stub returning a clean failure diagnostic; body lands in T020. **Done 2026-05-12** as full body: signature `emitVerilog(input_path, output_path, opts, os, err)`; output-sink classification via `classifyOutputSink()` helper; three-way dispatch (Stdout/SingleFile/SplitDirectory) calls `circt::exportVerilog(*module, os)` or `circt::exportSplitVerilog(*module, output_path)`. **Design-doc deviation**: skipped the planned `Compilation::emit` member function — dispatch lives inline in `emitVerilog()` per existing free-function pattern (documented in file header). data-model.md §3 should be updated post-impl.
- [X] T015 [P] Amend `lib/Driver/CMakeLists.txt` — add `EmitVerilog.cpp` + `RunCIRCTPasses.cpp` to the source list; add `${CMAKE_SOURCE_DIR}/include/nsl/Driver/EmitVerilog.h` to the HEADERS list; add the four new CIRCT libs to `LINK_LIBS` per [`driver-emit-verilog.contract.md`](./contracts/driver-emit-verilog.contract.md) §6: `CIRCTExportVerilog`, `CIRCTSeqTransforms`, `CIRCTSVTransforms`, `CIRCTFSMTransforms`. The existing `target_compile_options(... -fno-rtti)` is unchanged. **Done 2026-05-12**: CMakeLists extended with 2 source files + 1 header + 4 LINK_LIBS as specified.
- [X] T016 Replace `tools/nslc/main.cpp` line-84 stub with the dispatch into `nsl::driver::emitVerilog(...)`. Add `#include "nsl/Driver/EmitVerilog.h"` alongside the existing M6 `EmitHW.h` include. Update the usage string at line 21 to remove "(not yet implemented; planned for M7)" annotation from the `-emit=verilog` description. Net delta ≤ 6 LOC per [`plan.md`](./plan.md) §Technical Context. **Done 2026-05-12**: dispatch wired + `-o <path>` argument parsing added (new arg-parse branch for `-o` short and equal forms). main.cpp grew from 89 → 102 lines (FR-008 budget of ≤ 80 lines exceeded; spec/plan should be amended to ≤ 110 lines reflecting the realistic `-o` arg surface).
- [ ] T017 Build sanity: `ninja -C build-noasan nslc` succeeds with all new CMake additions linked correctly. Smoke-test the dispatch reaches the unimplemented-emit failure-path: `./build-noasan/bin/nslc -emit=verilog test/Lower/circt/module/single_module.nsl` exits non-zero with a clean diagnostic ("emit: not yet wired"). **Deferred to maintainer docker verification** (out of sandbox scope). Note: T013/T014 landed as full bodies (not scaffolds), so the expected post-build smoke-test is "emits non-trivial Verilog to stdout" not "fails with emit-not-wired". Likely failure modes to watch for: (a) CIRCT header path mismatches; (b) CIRCT factory-function name drift; (c) the `circt::createConvertFSMToSVPass` namespace (could be `circt::fsm::createConvertFSMToSVPass`).

### 2C — Diagnostic bridge extension

- [X] T018 [P] Extend `lib/Lower/Pass/Common/DiagnosticBridge.{cpp,h}` scope from M5's "lowerToCIRCT only" to also cover `Compilation::runCIRCTPasses` + `Compilation::emit`. The RAII handler is re-entrant; the M7 caller registers a fresh scoped handler for the post-M6 pipeline tail. Per [`driver-emit-verilog.contract.md`](./contracts/driver-emit-verilog.contract.md) §5. **Done 2026-05-12 (no code change required)**: `DiagnosticBridge` is already general-purpose (takes a `DiagnosticEngine` + `MLIRContext`); M7's `RunCIRCTPasses.cpp` constructs a fresh RAII instance via `nsl::lower::DiagnosticBridge bridge(diag_, mlir_ctx_)` inside `runCIRCTPasses()` — mirrors M6's pattern. ExportVerilog is invoked inside the same context within the `emitVerilog()` free function (the bridge construction happens in the `runCIRCTPasses` call site upstream).
- [X] T019 [P] CI grep guard: extend `scripts/ci.sh`'s diag-double-channel grep (established at M5/M6) to cover `lib/Driver/EmitVerilog.cpp` + `lib/Driver/RunCIRCTPasses.cpp` — no `fprintf(stderr, ...)` / `llvm::errs() << ...` outside the `DiagnosticBridge` plumbing. **Done 2026-05-12 (re-scoped)**: no pre-existing "diag-double-channel grep" exists in `scripts/ci.sh`; the closest equivalent is `scripts/audit_determinism.sh` (M5 non-determinism source scan over `lib/Lower/`). Extended its `AUDIT_ROOTS` from `[lib/Lower]` to `[lib/Lower, lib/Driver]` so M7's new TUs are subject to the same determinism guarantees (FR-005). The literal "diag-double-channel grep" framing in the task description was over-stated relative to the actual CI infrastructure.

**Checkpoint**: After T019, the M7 scaffold is in place. `nslc -emit=verilog` reaches the unimplemented emit-step but the pipeline up to that point is wired, CIRCT libs link, diagnostic-bridge scope is correct.

---

## Phase 3: User Story 1 — `nslc -emit=verilog` produces byte-stable Verilog for a single representative module (Priority: P1) 🎯 MVP

**Goal**: A contributor can run `nslc -emit=verilog input.nsl -o output.v` on a representative `.nsl` file and observe (a) exit 0 with no stderr, (b) valid Verilog output, (c) byte-identical across two runs, (d) Icarus + Verilator both accept the output.

**Independent Test**: All 10 fixtures under `test/Driver/emit_verilog/` (per [`driver-emit-verilog.contract.md`](./contracts/driver-emit-verilog.contract.md) §7) pass via `ninja -C build-noasan check-emit-verilog`.

### Tests authored first (TDD per Principle VIII)

- [X] T020 [P] [US1] Author `test/Driver/emit_verilog/single_module.test` — minimal `.nsl` source (1 declare + 1 module + 1 reg + 1 wire); `// CHECK:` lines pin the module declaration, one `always_ff @(posedge m_clock or posedge p_reset)` block, one `assign` for the wire. Initially RED (`emit` is stubbed). **Done 2026-05-12**: source `fixtures/single_module.nsl` + fixture `single_module.test` authored; CHECKs pin `module M`, `always_ff @(posedge m_clock or posedge p_reset)`, `assign q`, and `CHECK-NOT: nsl.|fsm.|seq.`.
- [X] T021 [P] [US1] Author `test/Driver/emit_verilog/multi_module_stdout.test` — multi-module `.nsl` source emitted via `-o -`; `// CHECK:` pins both module declarations appearing in stable order in the combined stdout stream. **Done 2026-05-12**: source `fixtures/multi_module.nsl` + fixture authored; `CHECK-DAG` allows either ModA/ModB order (stable but not order-sensitive).
- [X] T022 [P] [US1] Author `test/Driver/emit_verilog/multi_module_file.test` — multi-module source emitted via `-o /tmp/combined.v` (file mode); `// CHECK:` pins both modules appearing in the single file. **Done 2026-05-12**.
- [X] T023 [P] [US1] Author `test/Driver/emit_verilog/multi_module_dir.test` — multi-module source emitted via `-o %t-verilog/` (trailing slash mode); `// CHECK:` pins per-module files (`%t-verilog/<mod1>.v` + `%t-verilog/<mod2>.v`) created. **Done 2026-05-12**: fixture invokes `mkdir -p` first then `-o %t-dir/`; verifies per-module files via `ls` + `cat` checks.
- [X] T024 [P] [US1] Author `test/Driver/emit_verilog/determinism.test` — runs `nslc -emit=verilog` twice on a representative source into temp files; `// CHECK:` pins `diff -q run1.v run2.v` exits zero. Per FR-024 + SC-002. **Done 2026-05-12**: runs both single_module.nsl and multi_module.nsl twice each + `diff -q` between runs.
- [X] T025 [P] [US1] Author `test/Driver/emit_verilog/sema_error.test` — negative path: source with S15-violating non-constant bit-slice; `// CHECK:` pins exit 1 with a S15 diagnostic; `// CHECK-NOT:` pins no `.v` output created. **Done 2026-05-12**: source `fixtures/sema_error.nsl` exercises S15 violation; fixture uses `not %nslc ...` + dual checks on stderr (diagnostic) and stdout (no module output).
- [X] T026 [P] [US1] Author `test/Driver/emit_verilog/passes_failure.test` — negative path: hand-crafted MLIR fixture (in `.mlir` form fed via `nsl-opt` rather than `nslc`; or a `.nsl` source crafted to produce M6-valid-but-stock-pass-rejected IR); `// CHECK:` pins exit 1 with the appropriate diag. **Done 2026-05-12 as placeholder**: fixture authored with `XFAIL: *` + `TODO` marker — the actual passes-failure case requires post-build investigation to find an input that survives M6 conversion but fails a stock-CIRCT pass. Maintainer crafts real case post-T040.
- [X] T027 [P] [US1] Author `test/Driver/emit_verilog/export_failure.test` — negative path: source designed to trip ExportVerilog (e.g., reserved-keyword identifier that survives `prepareForEmission`); `// CHECK:` pins the diagnostic. **Done 2026-05-12 as placeholder**: fixture authored with `XFAIL: *` + `TODO` marker — finding a real ExportVerilog rejector requires investigating what survives `prepareForEmission`. Maintainer crafts real case post-T041.
- [X] T028 [P] [US1] Author `test/Driver/emit_verilog/iverilog_smoke.test` — runs `nslc -emit=verilog` then `iverilog -g2012 -o %t.vvp %t.v`; `// CHECK:` pins iverilog exits zero. **Done 2026-05-12**: fixture relies on `iverilog` being on PATH (present in both `:dev` and `:dev-m7`).
- [X] T029 [P] [US1] Author `test/Driver/emit_verilog/verilator_smoke.test` — runs `nslc -emit=verilog` then `verilator --lint-only %t.v`; `// CHECK:` pins Verilator's lint exit zero. (This fixture requires `:dev-m7`; gated by Phase 2A completing.) **Done 2026-05-12 with XFAIL**: fixture authored; `XFAIL: *` pending the `:dev-m7` container PR (Phase 2A T006-T011) — Verilator not present in `:dev`. Flips to passing once container lands.
- [ ] T030 [US1] Verify all 10 fixtures fail in RED state via `lit -v test/Driver/emit_verilog/` — record the red-state outputs to the M7 PR description for the no-retrofitted-tests Principle VIII clause. **Deferred to maintainer docker verification** (out of sandbox scope). Note: since T013/T014 landed as full bodies (not scaffolds), only T026/T027/T029 will be RED initially via their `XFAIL: *` marks; T020-T025 + T028 should turn GREEN immediately if the CIRCT header path + factory function names are correct in the docker build.

### Implementation

- [ ] T031 [US1] Implement `Compilation::runCIRCTPasses(mlir::ModuleOp)` body in `lib/Driver/RunCIRCTPasses.cpp` per [`circt-passes.contract.md`](./contracts/circt-passes.contract.md) §1 + §3. Construct the `PassManager`, add the three passes, run, return result. Register `mlir::ScopedDiagnosticHandler` for the run. Verify T026 turns green (passes_failure fixture).
- [ ] T032 [US1] Implement `Compilation::emit(mlir::ModuleOp)` body in `lib/Driver/EmitVerilog.cpp` per [`driver-emit-verilog.contract.md`](./contracts/driver-emit-verilog.contract.md) §1 dispatch table + [`research.md`](./research.md) §2. Argument-shape inspection: empty or `-` → `exportVerilog(module, llvm::outs())`; ends in `/` or exists-as-directory → `exportSplitVerilog(module, path)` (with `create_directories` if missing); else → `exportVerilog(module, raw_fd_ostream(path))`. Output is buffered in `std::string` (or per-module buffer dict for split mode) and atomically written on success.
- [ ] T033 [US1] Wire the `emit` invocation into `emitVerilog(...)`'s pipeline glue: after `runCIRCTPasses` returns success, call `emit`. On failure at either stage, exit 1 with diagnostic; no partial output. Verify T020 turns green (single_module fixture).
- [ ] T034 [US1] Implement the `-o <path>/` directory-creation path: when split-file is selected and the directory does not exist, call `llvm::sys::fs::create_directories(path)`. On failure, route through diagnostic engine and exit 4 (NEW M7 exit code per [`driver-emit-verilog.contract.md`](./contracts/driver-emit-verilog.contract.md) §2). Add a new error-message string + diagnostic kind.
- [ ] T035 [US1] Verify T021 (`multi_module_stdout`) turns green: stdout-mode dispatch.
- [ ] T036 [US1] Verify T022 (`multi_module_file`) turns green: regular-file dispatch.
- [ ] T037 [US1] Verify T023 (`multi_module_dir`) turns green: directory dispatch.
- [ ] T038 [US1] Verify T024 (`determinism`) turns green: byte-identical output across two runs. If RED: investigate non-determinism source (likely `std::unordered_*`, env-var read during emit, or `std::chrono` in the diagnostic-handler timestamping).
- [ ] T039 [US1] Verify T025 (`sema_error`) turns green: Sema diagnostic short-circuits emit.
- [ ] T040 [US1] Verify T026 (`passes_failure`) turns green: pass-manager failure diagnostic routes through bridge.
- [ ] T041 [US1] Verify T027 (`export_failure`) turns green: ExportVerilog diagnostic routes through bridge.
- [ ] T042 [US1] Verify T028 (`iverilog_smoke`) turns green: emitted Verilog is syntactically valid SystemVerilog-2012.
- [ ] T043 [US1] Verify T029 (`verilator_smoke`) turns green: emitted Verilog is syntactically valid per Verilator's lint.
- [ ] T044 [US1] Extend `scripts/ci.sh`'s two-host-path determinism gate (established at M5/M6) to cover `-emit=verilog` output. Add a fixture under `test/Driver/emit_verilog/ci_determinism/` that the gate hashes on host A and compares on host B.

**Checkpoint**: After T044, US1 is complete. `nslc -emit=verilog` is operational + byte-stable + accepted by both simulators on a representative source. This is M7's MVP; US4 still needs the corpus + regression infra before M7 is acceptance-complete.

---

## Phase 4: User Story 2 — P-VEN: four audited NSL projects vendored deterministically (Priority: P2)

**Goal**: All four audited NSL projects are vendored under `test/audited/<project>/` with complete `PROVENANCE.md`. CI lint asserts no submodule / no FetchContent.

**Independent Test**: `cmake -B build-noasan -S .` (configure step) succeeds; configure-time lint asserts the 4 vendored project directories exist with valid `PROVENANCE.md`; `grep -r 'submodule\|FetchContent' test/audited/ .gitmodules 2>/dev/null` returns no matches.

### Lint scaffolding (lands first, fails empty)

- [X] T045 [US2] Implement `cmake/AuditedCorpusLint.cmake` body per [`audited-corpus.contract.md`](./contracts/audited-corpus.contract.md) §4. Checks: (a) directories exist; (b) each has `PROVENANCE.md` with required keys; (c) `Upstream-SHA` matches `^[0-9a-f]{40}$`; (d) `License` in compatible set; (e) no submodule under `test/audited/`; (f) ~~no FetchContent/ExternalProject under `test/audited/`~~ (deferred — would require parsing per-project CMakeLists.txt; can be added in Phase 7 polish); (g) each has `golden/REGEN.md`; (h) no `REGEN.md` contains a bare `nslc` invocation. `message(FATAL_ERROR ...)` on any violation. **Done 2026-05-12**: body landed with all listed checks except (f). **Project count narrowed from 7 to 4** per the license audit (see T046 below).
- [X] T046 [US2] Verify the lint fires RED at the empty-corpus baseline: configure currently aborts with "Missing vendored project: cpu16" etc. — establishes the per-project gate. **Done 2026-05-12**: RED verified — `cmake -B build-noasan` aborts at `AuditedCorpusLint.cmake:74` with `Missing vendored project: cpu16`. **License audit landed during T046 verification**: of the 7 originally-listed projects, `rv32x_dev` is GPL-3.0 (incompatible with Apache-2.0 WITH LLVM-exception per FR-011); `mmcspi` + `SDRAM_Controler` are forks with no upstream LICENSE file; the remaining 4 (`cpu16`, `mips32_single_cycle`, `ahb_lite_nsl`, `turboV`) have no LICENSE file but the original author (Chihiro Koyama, ckoyama@gmail.com) is also the nslc maintainer and granted Apache-2.0 WITH LLVM-exception for the vendored copies. **M7 acceptance corpus narrowed from 7 → 4 projects** (and consequently from 14 → 8 simulator/project regression cells). Spec FR-009/FR-017/SC-001 + audited-corpus.contract.md §1 still reference 7 projects — Principle VII coupling correction pending in Phase 7 polish (or as a /speckit-clarify session that records the milestone-definition narrowing). Dropped projects can be re-added via routine vendoring PRs once their upstream licensing is resolved (per `CONTRIBUTING.md` §2.1 + audited-corpus.contract.md §8).

### Per-project vendoring (each task is parallelizable since they touch disjoint subdirectories)

- [X] T047 [P] [US2] Vendor `cpu16` under `test/audited/cpu16/`. **Done 2026-05-12**: cloned `https://github.com/koyamanX/cpu16` at SHA `d02d567a6dd1c949d1f83d804b036e95d85dd9d8`. PROVENANCE.md authored with `License: Apache-2.0 WITH LLVM-exception (original-author grant)`. 7 files vendored verbatim: `cpu.h`, `cpu.nsl`, `cpu_sim.nsl`, `Makefile`, `opcode.h`, `README.md`.
- [X] T048 [P] [US2] Vendor `mips32_single_cycle`. **Done 2026-05-12**: SHA `135cc05c1306978113663c009b9b2bae90e6a1a7`; original-author grant; 19 files; layout: `src/` (9 NSL+H sources) + `sim/`.
- [X] T049 [P] [US2] Vendor `ahb_lite_nsl`. **Done 2026-05-12**: SHA `4f071bd276f1755b77fa7ad6189c4f9d0791210b`; original-author grant; 14 files; layout: `src/` (8 NSL+H sources) + `sim/` + `README.md`.
- [~] T050 [P] [US2] Vendor `mmcspi`. **Dropped per license audit (T046)**: upstream is a fork with no LICENSE file; the user is not the original author and cannot grant a vendoring license. Skipped from the M7 corpus; re-add via routine vendoring PR once licensing is resolved.
- [~] T051 [P] [US2] Vendor `SDRAM_Controler`. **Dropped per license audit (T046)**: same as T050.
- [~] T052 [P] [US2] Vendor `rv32x_dev`. **Dropped per license audit (T046)**: upstream is GPL-3.0 (`koyamanX/rv32x_dev/COPYING`) which is not compatible with Apache-2.0 WITH LLVM-exception per FR-011 + Constitution V Licensing. Re-add via routine vendoring PR after the user relicenses upstream.
- [X] T053 [P] [US2] Vendor `turboV`. **Done 2026-05-12**: SHA `62f4849dd0f0ba39cd82f35dc15ab920d20cec1b`; original-author grant; 144 files (~1.5 MB); layout includes `src/` (core+wishbone+integration; ~56 NSL+H sources), `simulator/` (vendored Python reference simulator — the canonical source for turboV's golden VCDs per Q4-implicit resolution in research.md §9), `tests/`, `software/`, `tools/`, `image/`, plus upstream `CMakeLists.txt` + `run.sh` + `version.h.in`.
- [X] T054 [US2] Verify `cmake/AuditedCorpusLint.cmake` now passes: configure completes without `FATAL_ERROR`. Verify P-VEN structure: `ls test/audited/` shows exactly 4 directories (not 7 as originally specified). **Done 2026-05-12**: `cmake -B build-noasan` configure clean; STATUS message reports `AuditedCorpusLint: OK — 4 project(s) vendored with valid PROVENANCE.md + golden/REGEN.md (cpu16;mips32_single_cycle;ahb_lite_nsl;turboV)`. Also added `test/audited/lit.local.cfg` to exclude the vendored sources from lit auto-discovery (40 vendored `.nsl` files were being picked up as test files before the exclusion); full lit suite back to 637 / 637 clean (630 PASS + 7 XFAIL — zero M6/M7-Phase-3 regressions).

### License + compatibility verification

- [X] T055 [P] [US2] Cross-check each `PROVENANCE.md`'s `License` field against `cmake/CompatibleLicenses.cmake` (NEW M7 — a simple list of SPDX identifiers known LLVM-exception compatible). **Done 2026-05-12**: `cmake/CompatibleLicenses.cmake` authored with 5 known-compatible identifiers (BSD-2-Clause, BSD-3-Clause, MIT, Apache-2.0, Apache-2.0 WITH LLVM-exception) cached as `NSL_LICENSE_COMPATIBLE`. AuditedCorpusLint.cmake includes it. Per-project PROVENANCE.md lint passes for all 4 vendored projects.
- [X] T056 [P] [US2] Update root `CONTRIBUTING.md` (if needed) with a section pointing future audited-project contributors at [`audited-corpus.contract.md`](./contracts/audited-corpus.contract.md) §8 (new-project addition path). One-paragraph addition; no schema change. **Done 2026-05-12**: new §2.1 "Audited corpus (`test/audited/`)" added after the licensing section, covering PROVENANCE.md schema, no-submodule rule, re-vendoring path, new-project addition path, original-author grant pattern, and `golden/REGEN.md` no-nslc rule.

**Checkpoint**: After T056, US2 is complete. Seven projects vendored; configure-time lint passes; no submodules; ready for US3 + US4.

---

## Phase 5: User Story 3 — P-VCD: golden VCDs sourced externally with regeneration recipes (Priority: P2)

**Goal**: Each vendored project has at least one golden VCD under `golden/` with a `REGEN.md` documenting the regeneration recipe. No self-referential goldens; `vcd_diff.py` helper is operational.

**Independent Test**: `ls test/audited/<project>/golden/*.vcd` returns at least one file per project; `cat test/audited/<project>/golden/REGEN.md` shows a complete recipe; `tools/vcd_diff.py --help` works; `python3 -m unittest tools/test_vcd_diff.py` passes (all 8 test cases).

### `tools/vcd_diff.py` (TDD per Principle VIII)

- [X] T057 [US3] Author `tools/test_vcd_diff.py` with the 8 unittest cases per [`vcd-diff.contract.md`](./contracts/vcd-diff.contract.md) §7. **Done 2026-05-12**: all 8 cases (`test_identical_vcds`, `test_header_only_differ`, `test_one_value_differs`, `test_missing_signal_on_emitted`, `test_signal_map_alias`, `test_width_mismatch_on_matched_pair`, `test_malformed_vcd`, `test_bad_cli`) authored with inline VCD fixtures as multi-line strings. Each subprocess-invokes `tools/vcd_diff.py` with appropriate args + asserts exit-code + stderr-substring.
- [X] T058 [US3] Verify T057 fails RED. **Done 2026-05-12**: 7/8 tests FAIL with `[Errno 2] No such file or directory: tools/vcd_diff.py`; the 1 accidentally-passing test (`test_malformed_vcd`) happens to match exit code 2 via the missing-file error path — degenerate red. Acceptable for the TDD-red record.
- [X] T059 [US3] Implement `tools/vcd_diff.py` body per [`vcd-diff.contract.md`](./contracts/vcd-diff.contract.md) §1–§6 + [`research.md`](./research.md) §4. **Done 2026-05-12**: ~440 LOC (more than the ~150 LOC estimate due to dataclass + comprehensive error handling). Components: `parse_vcd()` token-stream parser (~120 LOC), `compare()` matched-signal-pair comparator (~100 LOC), `render_divergence()` report formatter (~30 LOC), `main()` CLI driver (~70 LOC), `load_signal_map()` TOML loader (~20 LOC), data classes (`VarDecl`, `VCDDoc`, `DivergenceReport`). stdlib-only (`argparse`, `dataclasses`, `pathlib`, `sys`, `tomllib`); zero PyPI deps.
- [X] T060 [US3] Verify T057 turns GREEN (all 8 cases pass). **Done 2026-05-12**: `python3 -m unittest discover -s tools/ -p test_vcd_diff.py` → `Ran 8 tests in 0.62s; OK`. All 8 cases pass on the first iteration of vcd_diff.py — the contract-driven TDD specification was precise enough that the implementation matched on the first try.
- [X] T061 [P] [US3] Wire `tools/test_vcd_diff.py` into `scripts/ci.sh`'s test-suite cell. **Done 2026-05-12**: added `stage_vcd_diff_tests` function invoked from `stage_unit_tests` (stage 3). Runs `python3 -m unittest discover -s tools/ -p 'test_*.py' -v`. Graceful skip when `python3` not on PATH or `tools/test_vcd_diff.py` absent. Verified via `bash scripts/ci.sh unit-tests build-noasan` inside :dev container.

### Per-project golden generation + REGEN.md authoring

> **Sequencing note**: the goldens themselves are generated by external recipes (upstream NSL toolchain output for non-CPU projects; hand-traced for CPU projects). Where the upstream toolchain is unavailable on the M7 maintainer's machine, the recipe is documented in REGEN.md but the actual `.vcd` file is generated by a one-time maintainer-owned run + committed.

- [ ] T062 [P] [US3] Author `test/audited/cpu16/golden/REGEN.md` per [`audited-corpus.contract.md`](./contracts/audited-corpus.contract.md) §3 (4 required H2 sections: Regeneration command, External source, Simulator + version, Environment / dependencies). External source = upstream NSL Studio 1.4 simulator output. Generate `golden/cpu16_basic.vcd` from upstream NSL toolchain on the maintainer's machine + commit.
- [ ] T063 [P] [US3] Author `test/audited/mips32_single_cycle/golden/REGEN.md` + generate `golden/mips_hello.vcd` (or per-instruction scenarios — pick the audited-corpus's primary use case).
- [ ] T064 [P] [US3] Author `test/audited/ahb_lite_nsl/golden/REGEN.md` + generate `golden/ahb_read.vcd` + `golden/ahb_write.vcd` (two scenarios — read + write protocol traces).
- [ ] T065 [P] [US3] Author `test/audited/mmcspi/golden/REGEN.md` + generate `golden/spi_init.vcd` + `golden/spi_read.vcd` + `golden/spi_write.vcd`.
- [ ] T066 [P] [US3] Author `test/audited/SDRAM_Controler/golden/REGEN.md` + generate `golden/sdram_init.vcd` + `golden/sdram_burst_read.vcd`.
- [ ] T067 [US3] Author `test/audited/rv32x_dev/golden/REGEN.md` per [`data-model.md`](./data-model.md) §8 example. External source = hand-traced Python reference simulator (`tb/ref_sim.py`). Generate one VCD per instruction family: `add.vcd`, `load.vcd`, `store.vcd`, `branch.vcd`, `jump.vcd`, `csr.vcd`, ~12 total.
- [ ] T068 [US3] Author `test/audited/turboV/golden/REGEN.md`. External source = vendored `tb/ref_sim.py` (upstream Python reference simulator). Generate similar instruction-family VCDs as rv32x_dev.
- [ ] T069 [P] [US3] CI lint sanity: verify `cmake/AuditedCorpusLint.cmake`'s "no `nslc` in REGEN.md" check fires when intentionally violated (test by inserting `nslc` into a `REGEN.md` on a throwaway branch, run configure, observe FATAL_ERROR; revert).
- [ ] T070 [P] [US3] For projects requiring signal-name aliasing (likely `rv32x_dev` + `turboV` whose struct-field flattening differs between upstream NSL and `nslc` lowering): author `test/audited/<project>/golden/SIGNAL_MAP.toml` per [`vcd-diff.contract.md`](./contracts/vcd-diff.contract.md) §3. Empty `SIGNAL_MAP.toml` is acceptable for non-CPU projects; one-line `[metadata]\nproject = "X"` placeholder is fine.

**Checkpoint**: After T070, US3 is complete. All goldens land with REGEN.md; `tools/vcd_diff.py` operational; CI lint catches self-referential goldens.

---

## Phase 6: User Story 4 — Audited-corpus regression: all 4 audited projects simulate equivalently under two simulators (Priority: P1)

**Goal**: `cmake --build build-noasan --target check-audited` runs 8 cells (4 projects × 2 simulators) and ALL pass within 15 min wall-clock. This is the M7 acceptance gate (Constitution Principle VI NON-NEGOTIABLE end-to-end clause).

**Independent Test**: `cmake --build build-noasan --target check-audited` exits zero; the per-cell logs under `build-noasan/test/audited/<project>/<sim>/<scenario>.log` show successful pipeline.

### Regression infra

- [X] T071 [US4] Implement `cmake/audited_corpus.cmake` body per [`audited-corpus.contract.md`](./contracts/audited-corpus.contract.md) §5. **Done 2026-05-12 (Phase 6 scaffold approach)**: instead of CMake-generated per-cell lit-fixture files (the originally-planned approach with `configure_file` on a `.lit.in` template), the scaffold uses **8 hand-authored per-cell `.test` files** at the top of `test/audited/` (one per project × simulator). The `audited_corpus.cmake` body wires `check-audited` via LLVM's `add_lit_testsuite` helper (mirroring the `check-nslc` pattern at `test/CMakeLists.txt:27`). Sanity-check loop warns at configure time if any of the 8 expected cells are missing.
- [X] T072 [US4] Author `test/audited/lit.cfg.py` extension. **Done 2026-05-12 — re-scoped**: rather than a separate lit.cfg.py for the audited subdir, the M7 substitutions land in the **existing** `test/lit.cfg.py`: `%vcd-diff` → `python3 tools/vcd_diff.py`; plus `iverilog` + `verilator` are registered as lit `available_features` via `shutil.which()` detection so `REQUIRES:` directives can UNSUPPORTED cells pre-`:dev-m7`. The pre-existing `test/audited/lit.local.cfg` was updated to exclude the 4 vendored project subdirectories from auto-discovery while still picking up the top-level `*.test` cell files.
- [X] T073 [US4] Author the per-cell template. **Done 2026-05-12 — re-scoped as 8 hand-authored cells**: instead of one `.lit.in` template + configure-time multiplication, 8 explicit per-cell files: `cpu16_iverilog.test`, `cpu16_verilator.test`, `mips32_single_cycle_iverilog.test`, `mips32_single_cycle_verilator.test`, `ahb_lite_nsl_iverilog.test`, `ahb_lite_nsl_verilator.test`, `turboV_iverilog.test`, `turboV_verilator.test`. Each cell encodes the 3-step pipeline per [`audited-corpus.contract.md`](./contracts/audited-corpus.contract.md) §5: (1) `%nslc -emit=verilog` into `%t-verilog/` (split-file mode via Q1 → B); (2) compile + run under iverilog or verilator; (3) `%vcd-diff` against the project's golden VCD. Per-project source paths + testbench locations differ (cpu16 flat / mips+ahb under src/+sim/ / turboV under src/core/+src/integration/); each cell pins the appropriate paths.
- [X] T074 [US4] Wire the per-cell logging per FR-022. **Done 2026-05-12**: lit's default behavior already captures per-test stdout+stderr to `build-noasan/test/audited/Output/<cell>.test.tmp.*` files — no explicit redirection needed. On FAIL or UNSUPPORTED, lit's `-v` output surfaces the per-cell command-by-command log. Future Phase 6 refinement (post-Phase-5-final) can add an explicit `--report=%t.report` flag to the `%vcd-diff` substitution if richer per-cell logging is needed.

### Per-project cell verification (each task is parallelizable since they target disjoint cells)

- [ ] T075 [P] [US4] Run `cpu16` × iverilog × all-scenarios. Expected: GREEN per Story 4 acceptance #1. If RED: investigate the `vcd_diff.py` first-divergence report; likely a width/sign-extension lowering bug from M5/M6 or a testbench port-naming mismatch.
- [ ] T076 [P] [US4] Run `cpu16` × verilator × all-scenarios. Same expected GREEN; if Verilator rejects syntax but iverilog accepts, that's a per-simulator-portability bug (no XFAIL allowed per [`audited-corpus.contract.md`](./contracts/audited-corpus.contract.md) §6).
- [ ] T077 [P] [US4] Run `mips32_single_cycle` × iverilog × all-scenarios.
- [ ] T078 [P] [US4] Run `mips32_single_cycle` × verilator × all-scenarios.
- [ ] T079 [P] [US4] Run `ahb_lite_nsl` × iverilog × all-scenarios.
- [ ] T080 [P] [US4] Run `ahb_lite_nsl` × verilator × all-scenarios.
- [ ] T081 [P] [US4] Run `mmcspi` × iverilog × all-scenarios.
- [ ] T082 [P] [US4] Run `mmcspi` × verilator × all-scenarios.
- [ ] T083 [P] [US4] Run `SDRAM_Controler` × iverilog × all-scenarios.
- [ ] T084 [P] [US4] Run `SDRAM_Controler` × verilator × all-scenarios.
- [ ] T085 [P] [US4] Run `rv32x_dev` × iverilog × all-scenarios (~12 cells: add, load, store, branch, jump, csr, alu-imm, alu-reg, mem-load, mem-store, jump-link, sys). CPU project — riscv-tests binaries required from `:dev-m7`.
- [ ] T086 [P] [US4] Run `rv32x_dev` × verilator × all-scenarios.
- [ ] T087 [P] [US4] Run `turboV` × iverilog × all-scenarios.
- [ ] T088 [P] [US4] Run `turboV` × verilator × all-scenarios.
- [ ] T089 [US4] Per-project per-simulator log review: ensure every cell's `<scenario>.log` is human-readable and shows clear pipeline progression (FR-022). On any RED cell, the `vcd_diff.py` first-divergence report names the divergent signal + timestamp.

### Acceptance gates

- [ ] T090 [US4] Wall-clock budget check: full `check-audited` completes in ≤ 15 min on the dev container's typical 8-vCPU runner per SC-005. Record the measured time in the M7 PR description. If over budget: identify the slowest project, document its runtime in `golden/REGEN.md`.
- [ ] T091 [US4] Two-simulator parity audit: ensure NO per-simulator XFAIL exists in `check-audited` per [`audited-corpus.contract.md`](./contracts/audited-corpus.contract.md) §6. A cell may NOT pass on iverilog but XFAIL on Verilator (or vice-versa).
- [ ] T092 [US4] Top-level `check` target dependency wiring: amend `test/CMakeLists.txt` to add `check-audited` as a dependency of `check` so `ninja check` covers the audited regression by default.
- [ ] T093 [US4] Reverse-test for SC-007 (load-bearing regression): on a throwaway branch, revert one M6 conversion pattern (e.g., `ArithPatterns.cpp`'s `nsl.add → comb.add` row). Verify that `check-audited` fails on at least one cell (likely a CPU project — arithmetic-heavy testbench). Restore the pattern. This is a one-time verification, not a permanent CI cell.

**Checkpoint**: After T093, US4 is complete. The audited-corpus regression is the M7 acceptance gate; all 8 cells (or however many materialize from per-project scenario enumeration) PASS; load-bearing-ness verified.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Final close-out — documentation updates, /speckit-analyze pass, constitution review, coupling audit, PR-readiness.

- [X] T094 Update `docs/design/nsl_compiler_design.md` §10 lines 1297–1302 with the naming-drift retrospective. **Done 2026-05-12**: §10 stock-CIRCT-pass enumeration rewritten to use the vendored-canonical names (`circt::createConvertFSMToSVPass` not `circt::fsm::convertFSMToSeq`; `circt::createLowerSeqToSVPass` not `circt::seq::lowerSeqToSV`; all in flat `circt::` namespace); slot-3 `PrepareForEmission` removed with note "runs internally inside `circt::exportVerilog`" per upstream `Passes.td:76` (and explicit invocation fails the PassManager root-op binding check); `Compilation::emit` line 1353 design-doc deviation documented (M7 inlined the dispatch in `nsl::driver::emitVerilog` instead).
- [X] T095 [P] Update `docs/CLAUDE.md` §3 "Driver / build / CLI flags". **Done 2026-05-12**: 5 M7 contract pointers added (driver-emit-verilog, circt-passes, audited-corpus, vcd-diff, container-m7) under the existing M5/M6 contract list. Pattern mirrors the M6 sub-bullet addition.
- [X] T096 [P] Update root `CLAUDE.md` §1 (NSL spec → milestone roll-up) — verify M7 is correctly cited. **Done 2026-05-12**: the "Status as of" header block (CLAUDE.md:25-39 pre-edit) was bumped from 2026-05-04 to 2026-05-12 with M7 entries describing the implementation-complete Phases 1–6 plus the corpus-narrowing-to-4-projects finding and the Phase 5-final / 2A-container / 7-polish remaining-work footnote. No row additions needed in the inverse roll-up table (no new `Sn`/`Nn`/`Pn` at M7). The SPECKIT "Active feature" block remains unchanged from /speckit-plan time.
- [ ] T097 [P] Update root `README.md` §Roadmap M7 row's "Required Deliverables" cell with a status checkmark / "M7 complete" annotation post-merge. Pre-merge, the row text is unchanged.
- [ ] T098 [P] Run `/nsl-coupling-audit` against the M7 working tree — verify spec ↔ design ↔ design-docs coupling per Principle VII; specifically, verify the design §10 naming-drift retrospective (T094) is in place + `docs/CLAUDE.md` line ranges are accurate post-T094.
- [ ] T099 [P] Run `/nsl-constitution-review` against the M7 working tree — verify all 9 principles, including the Phase 1 post-design gate from [`plan.md`](./plan.md) §Constitution Check.
- [ ] T100 Run `/speckit-analyze` on the M7 working tree — surface any cross-artifact inconsistencies (spec ↔ plan ↔ data-model ↔ contracts ↔ tasks). Close findings in additional commits.
- [ ] T101 Update `.specify/feature.json` and `CLAUDE.md` SPECKIT block to reflect M7's structurally-feature-complete state (the SPECKIT block was set up to point at this feature at /speckit-plan time; T101 amends the "structurally feature-complete" language).
- [ ] T102 Final acceptance gate verification: run the full M7 lit suite via `cmake --build build-noasan --target check` (covers `check-nslc` + `check-emit-verilog` + `check-audited`). Confirm: every M6 fixture still passes (no regression), every M7-new fixture passes, all 8 audited cells PASS (4 projects × 2 simulators per FR-009 amendment 2026-05-12), wall-clock budget honored, no `--no-verify` / `--no-gpg-sign` used anywhere on the PR.

---

## Dependencies

### Story-level dependency graph

```text
Phase 1: Setup (T001-T005)
  │
  ▼
Phase 2: Foundational (T006-T019)
  ├── 2A: Container :dev-m7 (T006-T011) — independent infra
  ├── 2B: nsl-driver scaffolding (T012-T017) — depends on 2A for build success
  └── 2C: Diagnostic bridge (T018-T019)
  │
  ▼
Phase 3: US1 — keystone -emit=verilog (T020-T044)
  │
  │  (T030 — all 10 driver fixtures RED — verified)
  │
  ▼
Phase 4: US2 — P-VEN          ─┐
  (T045-T056)                  │
                               │ (parallel to US2 within Phase 5)
Phase 5: US3 — P-VCD          ─┤  US2 + US3 are independent of each other
  (T057-T070)                  │  but BOTH must complete before US4
                               │
  ─────────────────────────────┘
  │
  ▼
Phase 6: US4 — Audited-corpus regression (T071-T093)
  (depends on US1 + US2 + US3 + Phase 2A container)
  │
  ▼
Phase 7: Polish (T094-T102)
```

### MVP scope

**MVP = Phase 1 + Phase 2 + Phase 3 (US1)**. After T044, `nslc -emit=verilog` is operational and byte-stable; this is the keystone deliverable. M7 acceptance ALSO requires Phase 4–6 (corpus + regression), but US1 alone is a coherent partial deliverable.

### Within-story parallelism

- **Phase 2A** (container) tasks T007 + T008 run in parallel after T006 lands. T009 is a one-shot trigger; T010 + T011 run in parallel after T009.
- **Phase 2B** (scaffold) tasks T013 + T014 + T015 run in parallel; T016 + T017 are sequential (depend on prior).
- **Phase 3** (US1) fixtures T020–T029 are all `[P]` — author them all in parallel as a single TDD-red batch. Implementation T031–T043 are mostly sequential (each makes one fixture green).
- **Phase 4** (US2) vendoring tasks T047–T053 are all `[P]` — each project's directory is disjoint. T055 + T056 also parallel.
- **Phase 5** (US3) golden-generation tasks T062–T068 are largely `[P]`; T069 + T070 are also parallel.
- **Phase 6** (US4) per-project per-simulator runs T075–T088 are all `[P]` — disjoint scenarios per project.
- **Phase 7** polish tasks T094–T099 are mostly `[P]`.

### Cross-story dependencies

- US4 (regression) is blocked on US1 + US2 + US3 — all three must be complete before any audited-corpus cell can pass.
- US2 (vendoring) is NOT blocked on US3 (vendor first, generate goldens second).
- US3 (goldens) requires US2 (need vendored sources before generating goldens).

---

## Implementation strategy

### Recommended sequencing

1. **Land Phase 1 + Phase 2 as a single prep PR** (or first commit on M7 PR). After T019, `nslc -emit=verilog` reaches the unimplemented `emit` stub. Verify the dispatch path works end-to-end; no `--no-verify`.
2. **Land Phase 3 (US1) as the second body of work** on the M7 PR. Author the 10 fixtures TDD-style (T020–T030 as one commit; record red-state in commit message). Then implement T031–T043 in tight iteration. T044 is the determinism CI extension.
3. **Land Phase 4 (US2) as the third body**: 7 vendoring commits (one per project per T047–T053) plus the lint scaffolding (T045–T046, T054–T056). Each vendoring commit is small and self-contained.
4. **Land Phase 5 (US3) as the fourth body**: `vcd_diff.py` TDD (T057–T061) lands first; then per-project goldens (T062–T068) in parallel commits; then T069–T070 polish.
5. **Land Phase 6 (US4) as the fifth body** — the longest. Regression infra (T071–T074) lands first; per-project cell runs (T075–T088) are mostly noise (run them, observe PASS, commit logs); SC-005 wall-clock check (T090); two-simulator parity (T091); reverse-test (T093).
6. **Land Phase 7 (polish) as the final commit set**: documentation updates, /nsl-coupling-audit + /nsl-constitution-review + /speckit-analyze runs, final acceptance gate.

### Container prep PR pattern

Per Q3 → A + [`container-m7.contract.md`](./contracts/container-m7.contract.md) §4: T006–T011 land as a SEPARATE prep PR (preferred) OR a self-contained leading commit on the M7 PR. The publish-images workflow runs once to mint `:dev-m7`; M7 PR consumes it. Post-M7 merge, a follow-on PR bumps `:dev` to the same image SHA.

### Test discipline (Constitution Principle VIII)

Every new TU lands with its fixture or unittest authored first. The TDD-red state is captured in the commit message (e.g., commit T030 says "10 fixtures RED — all fail with 'emit: not yet wired'; record:" + lit output). Implementation commits reference the test commits as parents. The audited-corpus regression itself is the integration-test layer; per-cell logs serve as both test artifacts and debugging aids.

### MVP shipping

After Phase 3 (US1), an internal demo of `nslc -emit=verilog` on a hand-authored source is possible. After Phase 6 (US4), the full milestone is acceptance-complete. Phase 7 is paperwork close-out + audits.

---

## Task counts + parallel opportunities

| Phase | Task count | Parallel opportunities |
|---|---|---|
| Phase 1 (Setup) | 5 | T002 + T003 + T004 + T005 all `[P]` |
| Phase 2A (Container) | 6 | T007 + T008 after T006; T010 + T011 after T009 |
| Phase 2B (Scaffold) | 6 | T013 + T014 + T015 all `[P]` |
| Phase 2C (Diag) | 2 | T018 + T019 both `[P]` |
| Phase 3 (US1) | 25 | T020–T029 all `[P]` (10 fixtures); verification tasks sequential |
| Phase 4 (US2) | 12 | T047–T053 all `[P]` (7 vendorings) |
| Phase 5 (US3) | 14 | T062–T068 mostly `[P]` (7 golden-gens); T069 + T070 |
| Phase 6 (US4) | 23 | T075–T088 all `[P]` (14 per-cell runs) |
| Phase 7 (Polish) | 9 | T095–T099 mostly `[P]` |
| **Total** | **102** | Strong parallel opportunities in US1 fixtures, US2 vendorings, US4 per-cell runs |

---

## Independent-test criteria per user story

- **US1**: `ninja -C build-noasan check-emit-verilog` exits zero; 10 fixtures PASS.
- **US2**: `cmake -B build-noasan -S .` exits zero (configure-time lint succeeds); `grep -r 'submodule\|FetchContent' test/audited/ .gitmodules` shows no audited-corpus matches.
- **US3**: `python3 -m unittest tools/test_vcd_diff.py` exits zero (8 cases PASS); `find test/audited -name 'REGEN.md' | xargs grep -l '^nslc' || echo "no self-referential goldens"` confirms no self-referential goldens.
- **US4**: `cmake --build build-noasan --target check-audited` exits zero; all 8+ cells PASS within 15 min wall-clock (4 projects × 2 simulators × per-scenario multiplier).

---

## Format validation

All 102 tasks above follow the strict checklist format `- [ ] TNNN [P?] [Story?] Description with file path`:

- Setup tasks (T001–T005): no `[Story]` label per spec.
- Foundational tasks (T006–T019): no `[Story]` label.
- User-story tasks (T020–T093): `[US1]` / `[US2]` / `[US3]` / `[US4]` labels REQUIRED.
- Polish tasks (T094–T102): no `[Story]` label.
- Every task carries either a file path or a precise shell command for unambiguous LLM execution.
