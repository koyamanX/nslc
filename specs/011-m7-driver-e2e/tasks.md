<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

---

description: "Task list for M7 — `nsl-driver` end-to-end (`nslc -emit=verilog`); P-VEN vendoring + P-VCD golden VCDs + audited-corpus regression"

---

# Tasks: M7 — `nsl-driver` end-to-end + P-VEN + P-VCD + audited-corpus regression

**Input**: Design documents from `/specs/011-m7-driver-e2e/`
**Prerequisites**: [`plan.md`](./plan.md) (required), [`spec.md`](./spec.md) (required for user stories), [`research.md`](./research.md), [`data-model.md`](./data-model.md), [`contracts/`](./contracts/)

**Tests**: Test tasks are **MANDATORY** for this project per Constitution Principle VIII (Test-First Development, NON-NEGOTIABLE) and Principle VI (end-to-end clause is NON-NEGOTIABLE at M7 specifically). Every new TU + every audited-corpus cell lands with its fixture authored first (red-state captured); the audited-corpus regression's 14 cells are themselves the M7 acceptance gate. Tests MUST be observed FAILING before the corresponding implementation tasks begin.

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

- [ ] T001 Verify M6 baseline build is green on master HEAD via `sg docker -c "docker run --rm -v $PWD:/workspace -w /workspace ghcr.io/koyamanX/nsl-nslc:dev bash -c 'cmake -G Ninja -B build-noasan -DCMAKE_BUILD_TYPE=Debug -DNSL_ENABLE_ASAN=OFF && ninja -C build-noasan check-nslc'"` — record the baseline pass count for regression-comparison post-M7. Expected: **620 PASS + 3 XFAIL** out of 623 (the M6 acceptance state per `CLAUDE.md` SPECKIT block prior to M7 update).
- [ ] T002 [P] Create `test/Driver/emit_verilog/` subdirectory + `test/Driver/emit_verilog/fixtures/` subdirectory + `.gitkeep` placeholders. Lit auto-discovers via the existing `test/lit.cfg.py`; no config edit needed at scaffold time (mirrors M6 T003's no-op finding).
- [ ] T003 [P] Create `test/audited/` subdirectory + per-project subdirectories (7 placeholders: `cpu16/`, `mips32_single_cycle/`, `ahb_lite_nsl/`, `mmcspi/`, `SDRAM_Controler/`, `rv32x_dev/`, `turboV/`). Each gets a `.gitkeep` so the empty directories survive commit. Per-project `golden/` subdirectories also pre-created with `.gitkeep`.
- [ ] T004 [P] Author `cmake/AuditedCorpusLint.cmake` skeleton — `message(STATUS "AuditedCorpusLint: scaffold; no checks yet")`. Skeleton only at scaffold time; body lands in T065 once P-VEN structure is filled. Skeleton wired into top-level `CMakeLists.txt` via `include(cmake/AuditedCorpusLint.cmake)`.
- [ ] T005 [P] Author `cmake/audited_corpus.cmake` skeleton — declares a `check-audited` custom target with no per-project enumeration yet; the enumeration body lands in T085. Skeleton wired into `test/audited/CMakeLists.txt` (NEW M7 file — also created in this task).

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

- [ ] T012 Author `include/nsl/Driver/EmitVerilog.h` — public entry-point header for `emitVerilog(...)` mirroring `include/nsl/Driver/EmitHW.h`'s shape per [`data-model.md`](./data-model.md) §4. Includes the boilerplate doc comment about exit codes (0/1/3/4 — code 4 NEW at M7 for split-file mkdir failure) and the no-partial-output rule.
- [ ] T013 [P] Author `lib/Driver/RunCIRCTPasses.cpp` scaffold — declares `Compilation::runCIRCTPasses(mlir::ModuleOp)` body that constructs a `mlir::PassManager` with `ModuleOp` nesting, adds the three passes per [`circt-passes.contract.md`](./contracts/circt-passes.contract.md) §1 (`createConvertFSMToSVPass` → `createLowerSeqToSVPass` → `createPrepareForEmissionPass`), runs the pipeline, returns `pm.run(module)`. Scaffold-only: parallel-mode disabled (`pm.enableMultithreading(false)`), verifier enabled (`pm.enableVerifier(true)`), no `DiagnosticBridge` wiring yet (lands in T021).
- [ ] T014 [P] Author `lib/Driver/EmitVerilog.cpp` scaffold — `emitVerilog(input_path, opts, os, err)` mirroring `lib/Driver/EmitHW.cpp`'s shape. Pipeline glue: load+preprocess+lex+parse+sema+lowerToNSL+runNSLPasses+lowerToCIRCT+runCIRCTPasses+emit. The `emit` step is left as a stub returning a clean failure diagnostic; body lands in T020.
- [ ] T015 [P] Amend `lib/Driver/CMakeLists.txt` — add `EmitVerilog.cpp` + `RunCIRCTPasses.cpp` to the source list; add `${CMAKE_SOURCE_DIR}/include/nsl/Driver/EmitVerilog.h` to the HEADERS list; add the four new CIRCT libs to `LINK_LIBS` per [`driver-emit-verilog.contract.md`](./contracts/driver-emit-verilog.contract.md) §6: `CIRCTExportVerilog`, `CIRCTSeqTransforms`, `CIRCTSVTransforms`, `CIRCTFSMTransforms`. The existing `target_compile_options(... -fno-rtti)` is unchanged.
- [ ] T016 Replace `tools/nslc/main.cpp` line-84 stub with the dispatch into `nsl::driver::emitVerilog(...)`. Add `#include "nsl/Driver/EmitVerilog.h"` alongside the existing M6 `EmitHW.h` include. Update the usage string at line 21 to remove "(not yet implemented; planned for M7)" annotation from the `-emit=verilog` description. Net delta ≤ 6 LOC per [`plan.md`](./plan.md) §Technical Context.
- [ ] T017 Build sanity: `ninja -C build-noasan nslc` succeeds with all new CMake additions linked correctly. Smoke-test the dispatch reaches the unimplemented-emit failure-path: `./build-noasan/bin/nslc -emit=verilog test/Lower/circt/module/single_module.nsl` exits non-zero with a clean diagnostic ("emit: not yet wired").

### 2C — Diagnostic bridge extension

- [ ] T018 [P] Extend `lib/Lower/Pass/Common/DiagnosticBridge.{cpp,h}` scope from M5's "lowerToCIRCT only" to also cover `Compilation::runCIRCTPasses` + `Compilation::emit`. The RAII handler is re-entrant; the M7 caller registers a fresh scoped handler for the post-M6 pipeline tail. Per [`driver-emit-verilog.contract.md`](./contracts/driver-emit-verilog.contract.md) §5.
- [ ] T019 [P] CI grep guard: extend `scripts/ci.sh`'s diag-double-channel grep (established at M5/M6) to cover `lib/Driver/EmitVerilog.cpp` + `lib/Driver/RunCIRCTPasses.cpp` — no `fprintf(stderr, ...)` / `llvm::errs() << ...` outside the `DiagnosticBridge` plumbing.

**Checkpoint**: After T019, the M7 scaffold is in place. `nslc -emit=verilog` reaches the unimplemented emit-step but the pipeline up to that point is wired, CIRCT libs link, diagnostic-bridge scope is correct.

---

## Phase 3: User Story 1 — `nslc -emit=verilog` produces byte-stable Verilog for a single representative module (Priority: P1) 🎯 MVP

**Goal**: A contributor can run `nslc -emit=verilog input.nsl -o output.v` on a representative `.nsl` file and observe (a) exit 0 with no stderr, (b) valid Verilog output, (c) byte-identical across two runs, (d) Icarus + Verilator both accept the output.

**Independent Test**: All 10 fixtures under `test/Driver/emit_verilog/` (per [`driver-emit-verilog.contract.md`](./contracts/driver-emit-verilog.contract.md) §7) pass via `ninja -C build-noasan check-emit-verilog`.

### Tests authored first (TDD per Principle VIII)

- [ ] T020 [P] [US1] Author `test/Driver/emit_verilog/single_module.test` — minimal `.nsl` source (1 declare + 1 module + 1 reg + 1 wire); `// CHECK:` lines pin the module declaration, one `always_ff @(posedge m_clock or posedge p_reset)` block, one `assign` for the wire. Initially RED (`emit` is stubbed).
- [ ] T021 [P] [US1] Author `test/Driver/emit_verilog/multi_module_stdout.test` — multi-module `.nsl` source emitted via `-o -`; `// CHECK:` pins both module declarations appearing in stable order in the combined stdout stream.
- [ ] T022 [P] [US1] Author `test/Driver/emit_verilog/multi_module_file.test` — multi-module source emitted via `-o /tmp/combined.v` (file mode); `// CHECK:` pins both modules appearing in the single file.
- [ ] T023 [P] [US1] Author `test/Driver/emit_verilog/multi_module_dir.test` — multi-module source emitted via `-o %t-verilog/` (trailing slash mode); `// CHECK:` pins per-module files (`%t-verilog/<mod1>.v` + `%t-verilog/<mod2>.v`) created.
- [ ] T024 [P] [US1] Author `test/Driver/emit_verilog/determinism.test` — runs `nslc -emit=verilog` twice on a representative source into temp files; `// CHECK:` pins `diff -q run1.v run2.v` exits zero. Per FR-024 + SC-002.
- [ ] T025 [P] [US1] Author `test/Driver/emit_verilog/sema_error.test` — negative path: source with S15-violating non-constant bit-slice; `// CHECK:` pins exit 1 with a S15 diagnostic; `// CHECK-NOT:` pins no `.v` output created.
- [ ] T026 [P] [US1] Author `test/Driver/emit_verilog/passes_failure.test` — negative path: hand-crafted MLIR fixture (in `.mlir` form fed via `nsl-opt` rather than `nslc`; or a `.nsl` source crafted to produce M6-valid-but-stock-pass-rejected IR); `// CHECK:` pins exit 1 with the appropriate diag.
- [ ] T027 [P] [US1] Author `test/Driver/emit_verilog/export_failure.test` — negative path: source designed to trip ExportVerilog (e.g., reserved-keyword identifier that survives `prepareForEmission`); `// CHECK:` pins the diagnostic.
- [ ] T028 [P] [US1] Author `test/Driver/emit_verilog/iverilog_smoke.test` — runs `nslc -emit=verilog` then `iverilog -g2012 -o %t.vvp %t.v`; `// CHECK:` pins iverilog exits zero.
- [ ] T029 [P] [US1] Author `test/Driver/emit_verilog/verilator_smoke.test` — runs `nslc -emit=verilog` then `verilator --lint-only %t.v`; `// CHECK:` pins Verilator's lint exit zero. (This fixture requires `:dev-m7`; gated by Phase 2A completing.)
- [ ] T030 [US1] Verify all 10 fixtures fail in RED state via `lit -v test/Driver/emit_verilog/` — record the red-state outputs to the M7 PR description for the no-retrofitted-tests Principle VIII clause.

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

## Phase 4: User Story 2 — P-VEN: seven audited NSL projects vendored deterministically (Priority: P2)

**Goal**: All seven audited NSL projects are vendored under `test/audited/<project>/` with complete `PROVENANCE.md`. CI lint asserts no submodule / no FetchContent.

**Independent Test**: `cmake -B build-noasan -S .` (configure step) succeeds; configure-time lint asserts the seven directories exist with valid `PROVENANCE.md`; `grep -r 'submodule\|FetchContent' test/audited/ .gitmodules 2>/dev/null` returns no matches.

### Lint scaffolding (lands first, fails empty)

- [ ] T045 [US2] Implement `cmake/AuditedCorpusLint.cmake` body per [`audited-corpus.contract.md`](./contracts/audited-corpus.contract.md) §4. Checks: (a) 7 directories exist; (b) each has `PROVENANCE.md` with required keys; (c) `Upstream-SHA` matches `^[0-9a-f]{40}$`; (d) `License` in compatible set; (e) no submodule under `test/audited/`; (f) no FetchContent/ExternalProject under `test/audited/`; (g) each has `golden/REGEN.md`; (h) no `REGEN.md` invokes `nslc`. `message(FATAL_ERROR ...)` on any violation.
- [ ] T046 [US2] Verify the lint fires RED at the empty-corpus baseline: configure currently aborts with "Missing vendored project: cpu16" etc. — establishes the per-project gate.

### Per-project vendoring (each task is parallelizable since they touch disjoint subdirectories)

- [ ] T047 [P] [US2] Vendor `cpu16` under `test/audited/cpu16/`: `git clone https://github.com/overtone-osc/cpu16 /tmp/cpu16 && cd /tmp/cpu16 && SHA=$(git rev-parse HEAD)` then copy all `.nsl`/`.v`/`tb/`/`LICENSE`/`README*` files. Authoring `test/audited/cpu16/PROVENANCE.md` per [`audited-corpus.contract.md`](./contracts/audited-corpus.contract.md) §2: `Upstream-URL`, `Upstream-SHA: ${SHA}`, `License: BSD-2-Clause`, `Vendored-At: 2026-05-12`, `Vendored-By: ckoyama`, `## Notes: none` (or describe any vendor-time modifications).
- [ ] T048 [P] [US2] Vendor `mips32_single_cycle` under `test/audited/mips32_single_cycle/`: same shape as T047 (BSD-2-Clause).
- [ ] T049 [P] [US2] Vendor `ahb_lite_nsl` under `test/audited/ahb_lite_nsl/`: same shape (BSD-2-Clause).
- [ ] T050 [P] [US2] Vendor `mmcspi` under `test/audited/mmcspi/`: same shape (MIT).
- [ ] T051 [P] [US2] Vendor `SDRAM_Controler` under `test/audited/SDRAM_Controler/`: same shape (MIT). Note: keeping the upstream-misspelled name "Controler" verbatim per the verbatim-copy rule.
- [ ] T052 [P] [US2] Vendor `rv32x_dev` under `test/audited/rv32x_dev/`: same shape (Apache-2.0). Note: this project ships RISC-V test code; vendor `tests/` subdirectory as well.
- [ ] T053 [P] [US2] Vendor `turboV` under `test/audited/turboV/`: same shape (Apache-2.0). Vendor the upstream Python reference simulator under `tb/ref_sim.py` (cross-referenced from `golden/REGEN.md` in T076).
- [ ] T054 [US2] Verify `cmake/AuditedCorpusLint.cmake` now passes: configure completes without `FATAL_ERROR`. Verify P-VEN structure: `ls test/audited/` shows exactly 7 directories.

### License + compatibility verification

- [ ] T055 [P] [US2] Cross-check each `PROVENANCE.md`'s `License` field against `cmake/CompatibleLicenses.cmake` (NEW M7 — a simple list of SPDX identifiers known LLVM-exception compatible: `BSD-2-Clause`, `BSD-3-Clause`, `MIT`, `Apache-2.0`, `Apache-2.0 WITH LLVM-exception`). The audit-lint reads this list. Author the file with the 5 known-compatible identifiers.
- [ ] T056 [P] [US2] Update root `CONTRIBUTING.md` (if needed) with a section pointing future audited-project contributors at [`audited-corpus.contract.md`](./contracts/audited-corpus.contract.md) §8 (new-project addition path). One-paragraph addition; no schema change.

**Checkpoint**: After T056, US2 is complete. Seven projects vendored; configure-time lint passes; no submodules; ready for US3 + US4.

---

## Phase 5: User Story 3 — P-VCD: golden VCDs sourced externally with regeneration recipes (Priority: P2)

**Goal**: Each vendored project has at least one golden VCD under `golden/` with a `REGEN.md` documenting the regeneration recipe. No self-referential goldens; `vcd_diff.py` helper is operational.

**Independent Test**: `ls test/audited/<project>/golden/*.vcd` returns at least one file per project; `cat test/audited/<project>/golden/REGEN.md` shows a complete recipe; `tools/vcd_diff.py --help` works; `python3 -m unittest tools/test_vcd_diff.py` passes (all 8 test cases).

### `tools/vcd_diff.py` (TDD per Principle VIII)

- [ ] T057 [US3] Author `tools/test_vcd_diff.py` with the 8 unittest cases per [`vcd-diff.contract.md`](./contracts/vcd-diff.contract.md) §7: `test_identical_vcds`, `test_header_only_differ`, `test_one_value_differs`, `test_missing_signal_on_emitted`, `test_signal_map_alias`, `test_width_mismatch_on_matched_pair`, `test_malformed_vcd`, `test_bad_cli`. Each case authors small VCD fixtures inline as multi-line strings. Initially RED (`vcd_diff.py` does not exist).
- [ ] T058 [US3] Verify T057 fails RED (`python3 -m unittest tools/test_vcd_diff.py` exits non-zero with "No module named 'vcd_diff'"). Record red-state.
- [ ] T059 [US3] Implement `tools/vcd_diff.py` body per [`vcd-diff.contract.md`](./contracts/vcd-diff.contract.md) §1–§6 + [`research.md`](./research.md) §4. Components: CLI driver (~30 LOC), parser (~70 LOC), comparator (~50 LOC), signal-map loader using stdlib `tomllib` (~10 LOC). Iterate until all 8 unittest cases pass.
- [ ] T060 [US3] Verify T057 turns GREEN (all 8 cases pass). Record per-case test output to the M7 PR description.
- [ ] T061 [P] [US3] Wire `tools/test_vcd_diff.py` into `scripts/ci.sh`'s test-suite cell: add a step `python3 -m unittest discover tools/ -p 'test_*.py'` that runs in CI alongside the lit suite.

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

## Phase 6: User Story 4 — Audited-corpus regression: all seven projects simulate equivalently under two simulators (Priority: P1)

**Goal**: `cmake --build build-noasan --target check-audited` runs 14 cells (7 projects × 2 simulators) and ALL pass within 15 min wall-clock. This is the M7 acceptance gate (Constitution Principle VI NON-NEGOTIABLE end-to-end clause).

**Independent Test**: `cmake --build build-noasan --target check-audited` exits zero; the per-cell logs under `build-noasan/test/audited/<project>/<sim>/<scenario>.log` show successful pipeline.

### Regression infra

- [ ] T071 [US4] Implement `cmake/audited_corpus.cmake` body per [`audited-corpus.contract.md`](./contracts/audited-corpus.contract.md) §5. Enumeration: `file(GLOB AUDITED_PROJECTS RELATIVE ${CMAKE_SOURCE_DIR}/test/audited ${CMAKE_SOURCE_DIR}/test/audited/*)`. For each project, enumerate `golden/*.vcd` scenarios. For each (project × simulator × scenario) tuple, generate a per-cell lit-test instance under `${CMAKE_BINARY_DIR}/test/audited/<project>/<sim>/<scenario>.lit`.
- [ ] T072 [US4] Author `test/audited/lit.cfg.py` extension. Imports the top-level `test/lit.cfg.py` config + adds substitutions: `%nsl-driver-verilog` → `${NSLC_BINARY} -emit=verilog`; `%audited-simulate-iverilog <verilog-dir> <tb-dir> <vcd-out>` → shell snippet that compiles + runs iverilog; `%audited-simulate-verilator <...>` → equivalent for Verilator; `%vcd-diff <golden> <emitted> [--signal-map=<path>]` → invokes `tools/vcd_diff.py`.
- [ ] T073 [US4] Author the per-cell `.lit` template (lives under `test/audited/cell_template.lit.in`). Steps per [`audited-corpus.contract.md`](./contracts/audited-corpus.contract.md) §5: (1) `%nsl-driver-verilog %<project>/*.nsl -o %t-verilog/`; (2) iverilog OR verilator compile-and-run; (3) `%vcd-diff %<project>/golden/%<scenario>.vcd %t-vcd/%<scenario>.vcd [--signal-map=...]`. The template substitutes `<project>`, `<scenario>`, `<simulator>` at CMake-configure time.
- [ ] T074 [US4] Wire the per-cell logging per FR-022: each lit instance redirects its stdout+stderr to `build-noasan/test/audited/<project>/<sim>/<scenario>.log`. lit's `%t` substitution + a tail-after-failure rule capture the log on failure for inspection.

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

**Checkpoint**: After T093, US4 is complete. The audited-corpus regression is the M7 acceptance gate; all 14 cells (or however many materialize from per-project scenario enumeration) PASS; load-bearing-ness verified.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Final close-out — documentation updates, /speckit-analyze pass, constitution review, coupling audit, PR-readiness.

- [ ] T094 Update `docs/design/nsl_compiler_design.md` §10 lines 1297–1302 with the naming-drift retrospective per [`research.md`](./research.md) §1 + [`circt-passes.contract.md`](./contracts/circt-passes.contract.md) §2: rename `circt::fsm::convertFSMToSeq` → `circt::fsm::convertFSMToSV` (upstream-canonical) + the four `LINK_LIBS` names. Documentation-only change; does NOT amend any contract.
- [ ] T095 [P] Update `docs/CLAUDE.md` §3 "Driver / build / CLI flags" task-→-section map: add a sub-bullet for M7's contracts (`driver-emit-verilog.contract.md`, `circt-passes.contract.md`, `audited-corpus.contract.md`, `vcd-diff.contract.md`, `container-m7.contract.md`). Mirrors the M6 "M6 contract `-emit=hw`" line addition.
- [ ] T096 [P] Update root `CLAUDE.md` §1 (NSL spec → milestone roll-up) — verify M7 is correctly cited as "End-to-end NSL → Verilog via `-emit=verilog`" pointing at this feature. No row addition needed (no new `Sn`/`Nn`/`Pn` at M7); the existing M6 column entries' commentary in the "**Status as of 2026-05-04**" block is updated to reflect M7 close-out.
- [ ] T097 [P] Update root `README.md` §Roadmap M7 row's "Required Deliverables" cell with a status checkmark / "M7 complete" annotation post-merge. Pre-merge, the row text is unchanged.
- [ ] T098 [P] Run `/nsl-coupling-audit` against the M7 working tree — verify spec ↔ design ↔ design-docs coupling per Principle VII; specifically, verify the design §10 naming-drift retrospective (T094) is in place + `docs/CLAUDE.md` line ranges are accurate post-T094.
- [ ] T099 [P] Run `/nsl-constitution-review` against the M7 working tree — verify all 9 principles, including the Phase 1 post-design gate from [`plan.md`](./plan.md) §Constitution Check.
- [ ] T100 Run `/speckit-analyze` on the M7 working tree — surface any cross-artifact inconsistencies (spec ↔ plan ↔ data-model ↔ contracts ↔ tasks). Close findings in additional commits.
- [ ] T101 Update `.specify/feature.json` and `CLAUDE.md` SPECKIT block to reflect M7's structurally-feature-complete state (the SPECKIT block was set up to point at this feature at /speckit-plan time; T101 amends the "structurally feature-complete" language).
- [ ] T102 Final acceptance gate verification: run the full M7 lit suite via `cmake --build build-noasan --target check` (covers `check-nslc` + `check-emit-verilog` + `check-audited`). Confirm: every M6 fixture still passes (no regression), every M7-new fixture passes, all 14 audited cells PASS, wall-clock budget honored, no `--no-verify` / `--no-gpg-sign` used anywhere on the PR.

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
- **US4**: `cmake --build build-noasan --target check-audited` exits zero; all 14+ cells PASS within 15 min wall-clock.

---

## Format validation

All 102 tasks above follow the strict checklist format `- [ ] TNNN [P?] [Story?] Description with file path`:

- Setup tasks (T001–T005): no `[Story]` label per spec.
- Foundational tasks (T006–T019): no `[Story]` label.
- User-story tasks (T020–T093): `[US1]` / `[US2]` / `[US3]` / `[US4]` labels REQUIRED.
- Polish tasks (T094–T102): no `[Story]` label.
- Every task carries either a file path or a precise shell command for unambiguous LLM execution.
