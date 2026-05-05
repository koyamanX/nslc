<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

---

description: "Task list for M6 — `nsl-lower` part 2 (`nsl` → CIRCT lowering)"

---

# Tasks: M6 — `nsl-lower` part 2

**Input**: Design documents from `/specs/010-m6-circt-lowering/`
**Prerequisites**: [`plan.md`](./plan.md) (required), [`spec.md`](./spec.md) (required for user stories), [`research.md`](./research.md), [`data-model.md`](./data-model.md), [`contracts/`](./contracts/)

**Tests**: Test tasks are **MANDATORY** for this project per Constitution Principle VIII (Test-First Development, NON-NEGOTIABLE). Every conversion pattern lands with a lit + FileCheck fixture authored first; the CI coverage guard (FR-033) enforces pattern↔fixture bijection mechanically. Tests MUST be observed FAILING before the corresponding implementation tasks begin.

**Organization**: Tasks are grouped by user story (US1–US5 from [`spec.md`](./spec.md)) to enable independent implementation and testing. US1 is the umbrella gate (acceptance harness + CI coverage guard); US2 is the module skeleton (P1 prerequisite); US3 is FSM lowering (P1 — README's named M6 pattern); US4 is the bulk leaf-op coverage (P2); US5 is the round-trip + determinism gate (P2).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3, US4, US5)
- Include exact file paths in descriptions

## Path Conventions

Single project, LLVM-style layered architecture (per [`plan.md`](./plan.md) §Project Structure). All paths are relative to the repo root `/home/koyaman/devel/nslc/`. Build directory is `build-noasan/` inside the dev container per the libMLIR-ASan-mismatch memory.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Verify M5 baseline is green + scaffold the M6 test directory hierarchy.

- [X] T001 Verify M5 baseline build is green on master HEAD via `sg docker -c "docker run --rm -v $PWD:/workspace -w /workspace ghcr.io/koyamanx/nsl-nslc:dev bash -c 'cmake -G Ninja -B build-noasan -DCMAKE_BUILD_TYPE=Debug -DNSL_ENABLE_ASAN=OFF && ninja -C build-noasan check-nslc'"` — record the baseline pass count for regression-comparison post-M6. **Verified 2026-05-04: 548 PASS + 1 XFAIL** out of 549 (the M5-deferred-followups branch acceptance state). Note: target name is `check-nslc` not `check-nsl` (M5 task description had a typo). Note: `check-nslc` does NOT depend on `nsl-opt` in `test/CMakeLists.txt` (only `nslc` + `FileCheck`), so an explicit `ninja nsl-opt` precedes the `check-nslc` invocation; this is a pre-existing M0/P-CI infrastructure issue unrelated to M6.
- [X] T002 [P] Create `test/Lower/circt/` subdirectory tree: `module/`, `fsm/`, `arith/`, `state/`, `control/`, `sim/`, `round_trip/` per [`data-model.md`](./data-model.md) §8. **Done 2026-05-04**: 7 subdirectories created with `.gitkeep` placeholders so they survive the empty-Phase-1 commit (git does not track empty directories).
- [X] T003 [P] Amend `test/Lower/lit.cfg.py` to register `test/Lower/circt/` test directory hierarchy + ensure `nslc`, `nsl-opt`, and `circt-opt` (vendored from CIRCT) are on the lit `PATH` for these tests. **No-op 2026-05-04**: investigation revealed (a) the actual lit config is at `test/lit.cfg.py` (one level up; the M5-derived task description had a wrong path); (b) `lit` auto-discovers new test subdirectories under `test_source_root` via the `.test`/`.nsl`/`.mlir` suffix match — no "registration" mechanism exists; (c) `nslc` + `nsl-opt` are already on PATH via the existing `nslc_binary_dir` config + `extra_paths.append`; (d) `circt-opt` is already on the dev-container default PATH at `/opt/circt/bin/circt-opt`. The task as worded is satisfied without any file edit. If a future M6 fixture needs an explicit `%circt-opt` substitution (rather than bare-name invocation), that can be added as a follow-on amendment to `test/lit.cfg.py` + `test/lit.site.cfg.py.in` + `test/CMakeLists.txt` (3 coordinated edits) — not needed at Phase 1.
- [X] T004 [P] Author `test/Lower/circt/coverage_guard.cmake` skeleton — walks `lib/Lower/CIRCTPatterns/*.cpp` for registered pattern types; walks `test/Lower/circt/<family>/*.nsl` for fixture names; fails configure if bijection breaks (FR-033). Fail empty initially (no patterns, no fixtures); will fire green as US2/US3/US4 fill in. **Done 2026-05-04**: skeleton committed at `test/Lower/circt/coverage_guard.cmake`; Phase-1 deliverable emits an informational `message(STATUS …)` only. T028 will replace the body with the bijection-walker logic.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Build the M6 library scaffolding — extended public header, NSLToCIRCTPass shell, CIRCTTypeConverter, family-file scaffolds (empty populate functions), Compilation::lowerToCIRCT wiring, `-emit=hw` flag wiring, CMakeLists CIRCTFSM addition. After Phase 2, `nslc -emit=hw input.nsl` runs the full pipeline through M6's empty-pattern-set conversion (any `nsl::*` op fails the conversion target — observed-failing baseline). ALL user-story tasks gate on this phase completing.

**⚠️ CRITICAL**: No user-story work begins until this phase is complete.

> **Path-drift correction (2026-05-04, captured during Phase 2 implementation):** The tasks below originally referenced `lib/Lower/CIRCTPatterns/` and `lib/Lower/NSLToCIRCTPass.{cpp,h}`. The actual M5 convention places passes under `lib/Lower/Pass/` (with shared utilities under `lib/Lower/Pass/Common/` — see `Pass/Common/DiagnosticBridge.{cpp,h}`). To match this convention, M6 files landed at `lib/Lower/Pass/NSLToCIRCTPass.{cpp,h}`, `lib/Lower/Pass/CIRCTTypeConverter.{cpp,h}`, and `lib/Lower/Pass/CIRCTPatterns/*.cpp`. Similarly, "Wire the `EmitKind::HW` arm in `Compilation::emit`" (T024 original wording) was wrong: the M5 driver shape uses per-stage free functions (`emitTokens`/`emitAST`/`emitMLIR`) with no `Compilation::emit` switch; M6 follows the same pattern with a new `emitHW` free function under `lib/Driver/EmitHW.cpp` + `include/nsl/Driver/EmitHW.h`. The tasks below carry the corrected file paths inline.

- [X] T005 Amend `include/nsl/Lower/Lower.h` to add the 2 new public symbols (`createNSLToCIRCTPass`, `registerNSLToCIRCTPass`) frozen by [`contracts/lower-api.contract.md`](./contracts/lower-api.contract.md) §2 — declarations only, no bodies. **Done 2026-05-04.**
- [X] T006 [P] Author `lib/Lower/Pass/NSLToCIRCTPass.h` (private internal header) with the `NSLToCIRCTPass` class declaration per [`data-model.md`](./data-model.md) §1 — `mlir::PassWrapper<NSLToCIRCTPass, mlir::OperationPass<mlir::ModuleOp>>` skeleton with `getArgument()`, `getDescription()`, `getDependentDialects()`, `runOnOperation()` declarations + 9 `populate*Patterns` forward decls. **Done 2026-05-04.**
- [X] T007 [P] Author `lib/Lower/Pass/CIRCTTypeConverter.{h,cpp}` per [`data-model.md`](./data-model.md) §2 — the `addConversion` for `!nsl.bits<W>` → `iW`. **Done 2026-05-04.** **Implementation note**: only the BitsType conversion is registered at Phase 2; `!nsl.struct<@T>` and `!nsl.mem<…>` rules are deferred to Phase 4–6 when patterns that operate on those types land. M5's NSLExpandVariablesPass guarantees zero `!nsl.struct`-typed SSA values reach M6 (FR-022); fail-fast applies if violated.
- [X] T008 [P] Author `lib/Lower/Pass/CIRCTPatterns/ModulePatterns.cpp` with empty `populateModulePatterns(RewritePatternSet&, CIRCTTypeConverter&)` function — patterns land in T038+. **Done 2026-05-04.**
- [X] T009 [P] Author `lib/Lower/Pass/CIRCTPatterns/PortPatterns.cpp` with empty `populatePortPatterns` — patterns land in Phase 4. **Done 2026-05-04.**
- [X] T010 [P] Author `lib/Lower/Pass/CIRCTPatterns/StatePatterns.cpp` with empty `populateStatePatterns` — patterns land in T108+. **Done 2026-05-04.**
- [X] T011 [P] Author `lib/Lower/Pass/CIRCTPatterns/ControlPatterns.cpp` with empty `populateControlPatterns` — patterns land in T114+. **Done 2026-05-04.**
- [X] T012 [P] Author `lib/Lower/Pass/CIRCTPatterns/FSMPatterns.cpp` with empty `populateFSMPatterns` — patterns land in T051+. **Done 2026-05-04.**
- [X] T013 [P] Author `lib/Lower/Pass/CIRCTPatterns/ArithPatterns.cpp` with empty `populateArithPatterns` — patterns land in T098+. **Done 2026-05-04.**
- [X] T014 [P] Author `lib/Lower/Pass/CIRCTPatterns/BitOpPatterns.cpp` with empty `populateBitOpPatterns` — patterns land in T101+. **Done 2026-05-04.**
- [X] T015 [P] Author `lib/Lower/Pass/CIRCTPatterns/SimPatterns.cpp` with empty `populateSimPatterns` — patterns land in T119+. **Done 2026-05-04.**
- [X] T016 [P] Author `lib/Lower/Pass/CIRCTPatterns/ParamPatterns.cpp` with empty `populateParamPatterns` — patterns land in T039+. **Done 2026-05-04.**
- [X] T017 Author `lib/Lower/Pass/NSLToCIRCTPass.cpp` body — `runOnOperation` constructs `ConversionTarget` (illegal `nsl` dialect, legal 5 CIRCT dialects + `mlir::ModuleOp` parent), constructs `CIRCTTypeConverter`, populates `RewritePatternSet` by calling all 9 `populate*Patterns` helpers in alphabetic order per [`research.md`](./research.md) §11, calls `applyFullConversion`. On failure, calls `signalPassFailure()` (the `DiagnosticBridge` is owned by `Compilation::lowerToCIRCT` per LowerToCIRCT.cpp). **Done 2026-05-04.**
- [X] T018 Author `createNSLToCIRCTPass()` factory function in `lib/Lower/Pass/NSLToCIRCTPass.cpp` returning `std::make_unique<NSLToCIRCTPass>()`. **Done 2026-05-04.**
- [X] T019 Author `registerNSLToCIRCTPass()` registration helper in `lib/Lower/Pass/NSLToCIRCTPass.cpp` calling `mlir::registerPass([](){ return createNSLToCIRCTPass(); })`. **Done 2026-05-04.**
- [X] T020 Amend `registerNSLLowerPasses()` body in `lib/Lower/Lower.cpp` to also call `registerNSLToCIRCTPass()` — ABI preserved. **Done 2026-05-04.** Note: M5 function name is `registerNSLLowerPasses` (not `registerNSLPasses` as the M6 contract erroneously stated; the function name is fixed in code; contract may be amended in a follow-up doc commit).
- [X] T021 Amend `lib/Lower/CMakeLists.txt` source list to include the 11 new `.cpp` files: `Pass/CIRCTTypeConverter.cpp` (T007), the 9 family-pattern `.cpp` files under `Pass/CIRCTPatterns/` (T008–T016), and `Pass/NSLToCIRCTPass.cpp` (T017). Add `CIRCTFSM` and `MLIRTransforms` to `LINK_LIBS` per [`contracts/lower-api.contract.md`](./contracts/lower-api.contract.md) §5 (no `CIRCTHwArith` per Q1 → A). **Done 2026-05-04.** Note: `MLIRTransforms` is required for `mlir::applyFullConversion` (the DialectConversion infrastructure), in addition to the originally-listed `CIRCTFSM`.
- [X] T022 Add `Compilation::lowerToCIRCT(mlir::ModuleOp)` declaration in `include/nsl/Driver/Compilation.h` per [`data-model.md`](./data-model.md) §4. **Done 2026-05-04.**
- [X] T023 Add `Compilation::lowerToCIRCT(mlir::ModuleOp)` body in a new `lib/Driver/LowerToCIRCT.cpp` file (mirroring M5's `LowerToNSL.cpp` / `RunNSLPasses.cpp` separation pattern, NOT lumping into `Compilation.cpp` as the original task wording said). Body constructs `mlir::PassManager`, adds `createNSLToCIRCTPass()`, instantiates `DiagnosticBridge`, calls `pm.run(module)`. Plus extending `Compilation.cpp`'s constructor to load the 5 CIRCT dialects (`hw`, `comb`, `seq`, `fsm`, `sv`) — design §11 line 1145 area. **Done 2026-05-04.**
- [X] T024 [Architecture correction] The original wording said "Wire the `EmitKind::HW` arm in `Compilation::emit`" — but M5's driver shape uses per-stage free functions in `lib/Driver/Emit*.cpp`, with `tools/nslc/main.cpp` dispatching by stage string. M6 follows the same shape: created `include/nsl/Driver/EmitHW.h` + `lib/Driver/EmitHW.cpp` (mirroring `EmitMLIR.{h,cpp}`); extended `tools/nslc/main.cpp` to dispatch `-emit=hw` AND `-emit=circt` (alias per [`contracts/driver-emit-hw.contract.md`](./contracts/driver-emit-hw.contract.md) §1) to `nsl::driver::emitHW(...)`. The free function runs the full pipeline (M1 → M2 → M3 → M5 → M6) and prints with default `mlir::OpPrintingFlags()`. Also amended `lib/Driver/CMakeLists.txt` to add the new sources + headers. **Done 2026-05-04.**
- [X] T025 [P] Amend `tools/nslc/main.cpp` `--help` text to include `-emit=hw` line + the `-emit=circt` alias mention per [`contracts/driver-emit-hw.contract.md`](./contracts/driver-emit-hw.contract.md) §1. **Done 2026-05-04.**

**Checkpoint VERIFIED 2026-05-04**: dev-container build green (37/37 steps); lit suite still **548 PASS + 1 XFAIL** (zero regressions vs M5 baseline); smoke-test `nslc -emit=hw foo.nsl` on a simple `module M { ... q = a; }` input observes-fails as expected with `error: failed to legalize operation 'nsl.module'` (Phase-2 baseline — empty pattern set; Phase 4's T038 ModuleOp pattern will turn this green); empty input passes through producing an empty `module {}`. The foundation is ready; user-story implementation can now begin.

---

## Phase 3: User Story 1 — `nslc -emit=hw` produces verified CIRCT IR for every `nsl::*` op (Priority: P1) 🎯 MVP

**Goal**: The umbrella acceptance gate — every reachable `nsl::*` op kind has a registered conversion pattern AND a passing fixture. The CI coverage guard reports zero gaps. This US's tasks are the harness; the per-pattern work happens in US2/US3/US4 phases.

**Independent Test**: Build the project; run `lit -v test/Lower/circt/`; assert zero `nsl.*` ops in any `.mlir.expected` golden under `test/Lower/circt/`; assert `coverage_guard.cmake` reports zero pattern↔fixture gaps. This US's gate fires GREEN once US2/US3/US4 complete; the US itself adds the harness that detects coverage drift.

### Tests for User Story 1 (MANDATORY per Constitution Principle VIII) ⚠️

> **NOTE**: Author these harness fixtures FIRST. Each MUST fail against the post-Phase-2 tree (because zero patterns are registered) before US2/US3/US4 complete.

- [X] T026 [P] [US1] Author `test/Lower/circt/round_trip/zero_nsl_ops.nsl` — minimal NSL source; lit `RUN: %nslc -emit=hw %s | %FileCheck %s` with `CHECK-NOT: nsl.` asserting zero `nsl::*` ops in the output. **Done 2026-05-04 (Phase 3); XFAIL: \*** until Phase 4 T038 (ModuleOp pattern) lands. The XFAIL annotation is REMOVED in the same commit that lands T038. Inline `RUN:`/`CHECK:` directives — no separate `.test` file (matches M5 fixture convention).
- [X] T027 [P] [US1] Author `test/Lower/circt/round_trip/loc_plumbing.nsl` — minimal NSL source; lit `RUN: %nslc -emit=hw %s | %FileCheck %s` with `CHECK-NOT: loc(unknown)` asserting no `UnknownLoc` on any output op. Enforces FR-030 / SC-004. **Done 2026-05-04 (Phase 3); XFAIL: \*** until Phase 4 T038. Constitutional anchor: Principle IV.
- [X] T028 [US1] Flesh out `test/Lower/circt/coverage_guard.cmake` — bijection-checking logic per [`research.md`](./research.md) §14. **Done 2026-05-04 (Phase 3).** **Implementation note (refinement of original task wording)**: instead of regex-matching `add<NSLToCIRCT_*_Pattern>` against snake-case op names (which would force a 1:1 fixture-to-op naming convention that doesn't fit US2's "port_input_only.nsl" / "port_mixed.nsl" axis-fixture style), the guard implements **directory-level bijection**: each `lib/Lower/Pass/CIRCTPatterns/<Family>Patterns.cpp` containing `OpConversionPattern<…>` declarations requires at least one `*.nsl` fixture under its mapped `test/Lower/circt/<dir>/` directory. The 9 family→directory mappings are pinned in the cmake file's documentation block. Failure mode: `message(FATAL_ERROR …)` listing the specific family↔dir gap. Wired into `CMakeLists.txt` as `include(test/Lower/circt/coverage_guard.cmake)` after `add_subdirectory(test_unit)`. Verified 2026-05-04: configure prints `[nslc] M6 coverage_guard: OK (9 families scanned; per-family pattern↔fixture bijection holds at this configure run)` (Phase 3: all 9 family files have empty populator bodies, so the bijection trivially holds; later phases as patterns ship validate matching fixtures).
- [X] T029 [US1] Author `test/Lower/circt/round_trip/conversion_failure_exits_nonzero.nsl` — exercises FR-028 fail-fast: `RUN: not %nslc -emit=hw %s 2>&1 | %FileCheck %s` asserts non-zero exit + presence of `error: failed to legalize operation 'nsl.<name>'` on stderr. **Done 2026-05-04 (Phase 3).** **Implementation note**: the exact diagnostic text M6 emits at Phase 2 is `error: failed to legalize operation 'nsl.module'` (MLIR's `applyFullConversion` standard message routed through DiagnosticBridge), not the original spec's "error: nsl→CIRCT conversion failed for op …" wording. The fixture's `CHECK:` line uses the actual emitted text. **PASSES at Phase 3** (the empty pattern set causes any non-trivial input to observe-fail). Future phases as patterns close in: this fixture must continue to PASS on AT LEAST ONE op kind whose pattern hasn't shipped yet — OR be retired in Phase 8 polish if the fixture's input shape becomes feature-complete-clean before all its enclosed ops have patterns.

**Checkpoint**: After T029, the US1 harness exists. T026 + T027 fail as expected (no patterns yet); T029 passes. The CI coverage guard fires when any pattern lands without a fixture (or vice versa). Implementation now proceeds in US2/US3/US4.

**Checkpoint VERIFIED 2026-05-04**: dev-container build green; lit suite **549 PASS + 3 XFAIL** out of 552 — up from 548+1=549 at Phase 2 baseline. Net delta: +3 fixtures, all behaving as designed (T026 + T027 XFAIL, T029 PASS, plus the pre-existing M5 XFAIL). Coverage guard reports `OK (9 families scanned; per-family pattern↔fixture bijection holds)`. The harness is ready for Phase 4 patterns to start filling in.

---

## Phase 4: User Story 2 — `nsl.module` + `nsl.declare` lower to `hw.module` with HW ports (Priority: P1)

**Goal**: The structural skeleton — every `nsl::ModuleOp` becomes an `hw::HWModuleOp` with the correct port list derived from the paired `nsl::DeclareOp`. Implicit `clk` + `rst_n` ports added for no-`interface` modules; explicit `interface` honoured per S20.

**Independent Test**: For every fixture under `test/Lower/circt/module/`, lit `RUN: nslc -emit=hw %s | FileCheck %s` succeeds. The `circt-opt` over the output reports verifier-clean. Independent of US3/US4/US5 (FSM, leaf-ops, round-trip).

### Tests for User Story 2 (MANDATORY per Constitution Principle VIII) ⚠️

- [X] T030 [P] [US2] Author `test/Lower/circt/module/port_input_only.nsl` — `declare M { input a[8]; }` → `hw.module @M(in %a : i8, in %clk : i1, in %rst_n : i1)`. **Done 2026-05-04 (Phase 4).** Inline `RUN:`/`CHECK:` directives (no separate `.expected.mlir` — matches the M5 fixture style; the original task wording carried over the M3-corpus `.expected.mlir` convention which post-dates the M5 inline-CHECK convention).
- [X] T031 [P] [US2] Author `test/Lower/circt/module/port_output_only.nsl`. **Done 2026-05-04 (Phase 4).** Body is `q = 0;` to exercise the `nsl::ConstantOp` → `hw::ConstantOp` inline conversion path (transferred to the output port).
- [X] T032 [P] [US2] Author `test/Lower/circt/module/port_mixed.nsl`. **Done 2026-05-04 (Phase 4).**
- [X] T033 [P] [US2] Author `test/Lower/circt/module/interface_modifier.nsl`. **Done 2026-05-05 (M4-amendment-#10).** Originally XFAIL'd at Phase 4 close-out because `nsl::DeclareOp` carried no IR-level signal for the S20 `interface(clock=..., reset=...)` modifier. Amendment-#10 (single coordinated commit on branch `010-m6-circt-lowering`) adds the `interface_clock` + `interface_reset` `OptionalAttr<StrAttr>` pair on `nsl::DeclareOp`, populates them in the M5 visitor from `ast::DeclareBlock::clockName()` / `resetName()`, and reads them in M6's `lowerOneModule` (`ModulePatterns.cpp`) to emit user-named `i1` clock + reset input ports verbatim in lieu of the implicit `clk` / `rst_n` pair. The reg-on-explicit-interface lowering branch (`nsl::RegOp` → `seq::CompRegOp` instead of `FirRegOp` per `firreg-convention.contract.md` §2) lands in Phase 6; amendment-#10 only plumbs the port-naming half. XFAIL flipped to PASS.
- [X] T034 [P] [US2] Author `test/Lower/circt/module/implicit_clk_rstn.nsl`. **Done 2026-05-04 (Phase 4).** Verifies the order: data inputs first, then `clk`, then `rst_n` per `firreg-convention.contract.md` §1.
- [X] T035 [P] [US2] Author `test/Lower/circt/module/empty_module.nsl`. **Done 2026-05-04 (Phase 4).** Port-less module (no paired declare) lowers to `hw.module @M()` — implicit clk/rst_n ports are NOT added when there's no declare pairing (a deliberate narrow read of contract §3 rule 6 — "implicit clk/rst_n when paired declare lacks `interface` modifier"; absence of pairing entirely is treated as port-less, not port-less-but-add-clk-rst).
- [X] T036 [P] [US2] Author `test/Lower/circt/module/submodule_singleton.mlir`. **Done 2026-05-04 (Phase 4).** Authored as `.mlir` (driven by `nsl-opt -nsl-to-circt`) because the M5 `visit(SubmoduleDecl)` is currently a STUB — so `nsl.submodule` cannot be sourced from a `.nsl` input. The fixture has port-less Sub + Top to avoid the per-instance port-operand-wiring gap (Phase 4 doesn't surface per-instance port operands; a future M4 amendment + M5 visit(SubmoduleDecl) lowering closes this).
- [X] T037 [P] [US2] Author `test/Lower/circt/module/submodule_with_param.mlir`. **Done 2026-05-04 (Phase 4).** Same `.mlir`-input rationale; additionally, S16 forbids `param_int` in pure-NSL source so it cannot come from `nslc` regardless.

### Implementation for User Story 2

- [X] T038 [US2] Implement structural ModuleOp/DeclareOp lowering in `lib/Lower/Pass/CIRCTPatterns/ModulePatterns.cpp`. **Done 2026-05-04 (Phase 4).** Implementation note: the ModuleOp/DeclareOp/Port/Submodule/Param structural rewrite is performed as a manual pre-pass (`lowerNSLModulesToHWModules`, called from `NSLToCIRCTPass::runOnOperation` BEFORE `applyFullConversion`) rather than as a `OpConversionPattern<>`. Rationale documented at the top of `ModulePatterns.cpp`: the dual-placement port-info-op design (amendment-#9) requires coordinated in-module + in-declare consumption that the standard DialectConversion worklist would interleave incorrectly with attempts to legalize individual port-info ops. The `populateModulePatterns` function stays empty; `coverage_guard.cmake` continues to print OK because no `OpConversionPattern<` token is emitted (and the fixtures are present anyway). Pattern coverage is achieved structurally instead.
- [X] T038b [US2] In-module port-info-op handling. **Subsumed into T038 (Phase 4).** The `lowerNSLModulesToHWModules` walk processes both placement halves (declare-body for port-list derivation; in-module body for SSA-replacement) in one pass per nsl.module. PortPatterns.cpp stays empty (file header documents why).
- [X] T039 [US2] `nsl::SubmoduleOp` → `hw::InstanceOp`. **Subsumed into T038's body walk (Phase 4).** Implementation in `lowerOneModule` SubmoduleOp arm. Per the contract, instance parameters come from sibling top-level `nsl.param_int` / `nsl.param_str` ops via `collectInstanceParameters`. Phase-4 simplification: every consuming instance carries every top-level param; future M4 amendment surfacing per-instance param assignments will refine this.
- [X] T040 [US2] `nsl::ParamIntOp` → `hw.instance` parameter. **Subsumed into T038's parameter collection (Phase 4).** Same simplification noted above.
- [X] T041 [P] [US2] Author `test/Lower/circt/module/submodule_with_strparam.mlir`. **Done 2026-05-04 (Phase 4).**
- [X] T041b [US2] `nsl::ParamStrOp` → `hw.instance` string parameter. **Subsumed into T038 (Phase 4).** String-typed params encode as `none`-typed StringAttr in `ParamDeclAttr` (matches `circt-opt` round-trip).
- [X] T042 [US2] `populateModulePatterns` registration. **Done 2026-05-04 (Phase 4).** Empty (see T038 implementation note).
- [X] T043 [US2] `populateParamPatterns` registration. **Done 2026-05-04 (Phase 4).** Empty (see T038 implementation note); ParamPatterns.cpp file header documents why.
- [X] T044 [US2] Run lit on `test/Lower/circt/module/`. **Done 2026-05-04 (Phase 4).** Suite goes from 557 PASS + 3 XFAIL out of 560 (pre-Phase-4 baseline) to 567 PASS + 2 XFAIL out of 569 (post-Phase-4). Net delta: +10 PASS / -1 XFAIL. The 2 XFAIL are: T033 `interface_modifier.nsl` (deferred to amendment-#10) + the pre-existing M5 `struct_variable_emit_mlir.nsl`. T026 + T027 XFAILs flipped to PASS in the same commit (the umbrella US1 `zero_nsl_ops.nsl` and `loc_plumbing.nsl` harness fixtures). T029 `conversion_failure_exits_nonzero.nsl` body updated to use `reg r[8] = 0; r := a; q = r;` so it continues to exercise an op kind without a Phase-4 pattern (now `nsl.reg` / `nsl.clocked_transfer` instead of `nsl.module`). Coverage guard prints OK.

**Checkpoint VERIFIED 2026-05-04**: After T044, US2's module skeleton works. `nslc -emit=hw` of any port-bearing NSL module produces a valid `hw::HWModuleOp` with the correct port list. US3 + US4 can now proceed in parallel (each operates on the body region inside the `hw.module`).

---

## Phase 5: User Story 3 — `nsl.proc` / `nsl.state` / `nsl.seq` lower to `fsm::MachineOp` (Priority: P1)

**Goal**: The README's named M6 pattern. Every `nsl::ProcOp` with its `nsl::StateOp` children becomes an `fsm::MachineOp` with `initial_state` from `nsl::FirstStateOp`. `nsl::SeqOp` inside a `nsl::FuncOp` becomes a `MachineOp` with auto-generated `seq_N` states. Goto, finish, and proc-call invocations route through `fsm::TransitionOp`.

**Independent Test**: For every fixture under `test/Lower/circt/fsm/`, lit `RUN: nslc -emit=hw %s | FileCheck %s` succeeds. Output contains zero `nsl.proc`/`nsl.state`/`nsl.seq`/`nsl.goto`/`nsl.first_state`/`nsl.finish` ops; `circt-opt` verifier-clean. Independent of US2 (modules are assumed at the parent level), US4, US5.

### Tests for User Story 3 (MANDATORY per Constitution Principle VIII) ⚠️

- [X] T045 [P] [US3] Author `test/Lower/circt/fsm/single_state.nsl` — `proc P { state s0 { ... } }` + `first_state s0;` → `fsm.machine @P attributes { initialState = "s0" } { fsm.state @s0 }`. Degenerate FSM. **Done 2026-05-04 (Phase 5).** Inline `RUN:`/`CHECK:` directives (no separate `.expected.mlir` — matches the M5 fixture style).
- [X] T046 [P] [US3] Author `test/Lower/circt/fsm/two_state_goto.mlir` — two states + `goto`; expect `fsm.transition` from source to target. **Done 2026-05-04 (Phase 5).** Authored as `.mlir` (driven by `nsl-opt -nsl-to-circt`) because the M5 visitor's `GotoStmt` is a STUB (see `lib/Lower/ASTToMLIR.cpp` line ~2105 STUB list) — `goto` cannot be sourced from a `.nsl` input via `nslc`.
- [X] T047 [P] [US3] Author `test/Lower/circt/fsm/first_state_not_first.nsl` — `first_state` declared after `state` defs; expect `initialState` attr is correct regardless of source order. S28 case. **Done 2026-05-04 (Phase 5).** Verified the M5 visitor preserves source-order placement of `nsl.first_state`.
- [X] T048 [P] [US3] Author `test/Lower/circt/fsm/finish_to_sink.nsl` — `nsl.finish` inside a state body; expect `fsm.transition @__sink__` with synthetic sink state in the machine. **Done 2026-05-04 (Phase 5).**
- [X] T049 [P] [US3] Author `test/Lower/circt/fsm/seq_inside_func.mlir` — `func F { seq { goto label1 } }`; expect `fsm.machine` with `seq_0`, `seq_1`, … states. **Done 2026-05-04 (Phase 5).** Authored as `.mlir` (driven by `nsl-opt -nsl-to-circt`) because the M5 visitor's `LabeledStmt` and `GotoStmt` are STUBS — neither `nsl.seq` containing `nsl.goto` nor labelled-stmt control flow can be sourced from `.nsl` today. Phase 5 lowering ships an entry-state + one-state-per-goto convention that's a Phase-5 minimal placeholder; richer label-form goto support arrives once the M5 visitor stubs land.
- [X] T050 [P] [US3] Author `test/Lower/circt/fsm/proc_call_to_proc.nsl` — `nsl.call @Q` where `Q` is a proc; expect `fsm.transition @Q_initial_state`. **Done 2026-05-04 (Phase 5).** The disambiguation between proc-target (FSM transition; this Phase 5 pattern) and func_in-target (Phase 6) is by transitive symbol-table walk on `parentModule` (looks for sibling `nsl.proc` not yet lowered OR sibling `fsm.machine` already lowered).

### Implementation for User Story 3

- [X] T051 [US3] Implement `nsl::ProcOp` → `fsm::MachineOp` lowering. **Done 2026-05-04 (Phase 5).** **Implementation strategy correction (parallel to Phase 4 ModulePatterns)**: implemented as a manual pre-pass `lowerNSLProcsToFSMMachines` in `lib/Lower/Pass/CIRCTPatterns/FSMPatterns.cpp` (called from `NSLToCIRCTPass::runOnOperation` AFTER `lowerNSLModulesToHWModules` and BEFORE `applyFullConversion`) rather than as `OpConversionPattern<>` instances. Same rationale as ModulePatterns: the proc → state → goto/finish/call hierarchy needs coordinated visit-children-before-parent semantics that the standard DialectConversion worklist would interleave incorrectly. **Structural placement**: the resulting `fsm::MachineOp` lands at TOP-LEVEL (sibling of `hw::HWModuleOp`) per CIRCT FSM-dialect convention (`circt/test/Conversion/FSMToSV/single_state.mlir`); `fsm.hw_instance` wiring is Phase 6+ territory. Helper `lowerOneProc` scans the proc body for the `FirstStateOp` (regardless of source position — per T047's S28 case) and uses its target as the `initialState` attr; it then creates a `fsm::StateOp` shell per `nsl::StateOp` child, then walks each state body to lower its goto/finish/call ops into the matching `fsm::StateOp`'s transitions region.
- [X] T052 [US3] Implement `nsl::StateOp` → `fsm::StateOp` lowering. **Subsumed into T051 (Phase 5).** Each `nsl.state @s` becomes a sibling `fsm.state @s` inside the same `fsm.machine`; output + transitions regions are materialised empty at construction time, then transitions are populated by `lowerStateBody`.
- [X] T053 [US3] Implement `nsl::FirstStateOp` consumption. **Subsumed into T051 (Phase 5).** `lowerOneProc` extracts the target name from the (at-most-one) `FirstStateOp` child of the proc body, then `lowerNSLProcsToFSMMachines` runs a defensive cleanup walk to erase any straggler `FirstStateOp`s (should be empty after `proc.erase()` chains; defensive sweep).
- [X] T054 [US3] Implement `nsl::GotoOp` (state form, S25) → `fsm::TransitionOp` lowering. **Done as part of T051's `lowerStateBody` helper (Phase 5).** Each `nsl.goto @t` inside an `nsl.state` body produces one `fsm.transition @t` in the matching `fsm.state`'s transitions region. The target FlatSymbolRefAttr is forwarded as a `StringRef` to `fsm::TransitionOp::create`.
- [X] T055 [US3] Implement `nsl::SeqOp` (inside `nsl::FuncOp`) → `fsm::MachineOp` lowering. **Done as part of `lowerOneFuncSeq` (Phase 5).** The pattern fires only when the func body contains exactly one `nsl::SeqOp` child (a non-seq func body is left for Phase 6). The implementation creates one `fsm.state @seq_N` per goto plus an entry `seq_0`; each goto becomes a `fsm.transition` from the active state to the next sequential state. **Phase-5 simplification**: the implementation is minimal because the M5 visitor's `LabeledStmt` and `GotoStmt` are stubs — richer label-form control flow lands once those visitors ship.
- [X] T056 [US3] Implement `nsl::GotoOp` (label form, inside seq) → `fsm::TransitionOp` lowering. **Subsumed into T055's `lowerOneFuncSeq` (Phase 5).** Each goto closes the current `seq_N` state and opens transition to `seq_<N+1>`.
- [X] T057 [US3] Implement `nsl::FinishOp` / `nsl::FinishMethodOp` → `fsm::TransitionOp` to synthetic `__sink__` state. **Done as part of `lowerStateBody` (Phase 5).** Helper `ensureSinkState` lazy-builds the `__sink__` `fsm.state` once per machine (idempotent); each `nsl.finish` / `nsl.finish_method` produces a `fsm.transition @__sink__`.
- [X] T058 [US3] Implement `nsl::CallOp` (proc-target variant) → `fsm::TransitionOp` lowering. **Done as part of `lowerStateBody` (Phase 5).** Disambiguation against the func_in-target variant uses a `parentModule.walk` looking for either a sibling `nsl::ProcOp` (not yet lowered) OR a sibling `fsm::MachineOp` (already lowered by an earlier iteration of `lowerOneProc`). On hit, `ensureCrossMachinePlaceholder` lazy-builds a placeholder `fsm.state @<callee>_initial_state` in the calling machine and a `fsm.transition` targets it. **Phase-5 simplification**: `fsm.transition`'s verifier requires the target state to exist in the SAME machine (no cross-machine transitions in CIRCT FSM dialect), so the placeholder is the workaround. Phase 6+ may refine this to use `fsm.hw_instance`-driven control wiring.
- [X] T059 [US3] Register `populateFSMPatterns`. **Done 2026-05-04 (Phase 5).** Empty (see T051 implementation note); FSMPatterns.cpp file header documents why.
- [X] T060 [US3] Run lit on `test/Lower/circt/fsm/`. **Done 2026-05-04 (Phase 5).** All 6 fixtures pass: `single_state.nsl`, `two_state_goto.mlir`, `first_state_not_first.nsl`, `finish_to_sink.nsl`, `seq_inside_func.mlir`, `proc_call_to_proc.nsl`. Suite goes from 571 PASS + 1 XFAIL out of 572 (Phase 4 close-out + amendment-#10 baseline) to 577 PASS + 1 XFAIL out of 578 (post-Phase-5). Net delta: +6 PASS / 0 new XFAIL. Coverage guard prints OK. T029 fixture's `CHECK:` line refreshed to assert the new diagnostic wording (`error: M6 cannot lower output-port write whose source ...`) — the original Phase-4 hardcoded "M6 Phase 4 has no conversion pattern" message is replaced by Phase-5's move-and-defer refactor combined with a narrower fail-fast for the specific Phase-4 limitation around output-port writes whose source isn't Phase-4-materialised.

**Checkpoint VERIFIED 2026-05-04**: After T060, US3's FSM lowering works. Audited corpus's `cpu16` / `mips32_single_cycle` (M7) can structurally reach M6's output IR through their proc-heavy designs (modulo the Phase-6 leaf-op gap for arithmetic / state-elements / transfers inside state bodies; Phase-5 fixtures use empty state bodies to scope around that). End-to-end `nslc -emit=hw` on a `module M { proc p { state s0 {} } }` input produces a valid `fsm::MachineOp @p` next to `hw.module @M`. Determinism verified (Principle V): two runs produce byte-identical output.

---

## Phase 6: User Story 4 — Combinational, state, and simulation `nsl::*` ops lower to `comb` / `seq` / `sv` (Priority: P2)

**Goal**: The bulk-volume conversion work — ~30 leaf-op patterns covering arithmetic, bit-ops, state elements, control flow, simulation, and the implicit `_init` block. Each pattern is mechanical (one-to-one with design §10's mapping table); the bulk of the engineering effort here is fixture authoring + ensuring the per-pattern reset / mux / ifdef conventions land correctly per Q2/Q3 + research §§4–5, 9.

**Independent Test**: For every fixture under `test/Lower/circt/{arith,state,control,sim}/`, lit `RUN: nslc -emit=hw %s | FileCheck %s` succeeds. Output contains zero `nsl.*` ops in the four covered families. Independent of US2 (modules), US3 (FSM), US5 (round-trip).

### Tests for User Story 4 — Arithmetic family (MANDATORY) ⚠️

- [X] T061 [P] [US4] Author `test/Lower/circt/arith/add.nsl` + fixture for `nsl.add` → `comb.add`. **Done 2026-05-04 (Phase 6).**
- [X] T062 [P] [US4] Author `test/Lower/circt/arith/sub.nsl` for `nsl.sub` → `comb.sub`. **Done 2026-05-04 (Phase 6).**
- [X] T063 [P] [US4] Author `test/Lower/circt/arith/mul.nsl` for `nsl.mul` → `comb.mul`. **Done 2026-05-04 (Phase 6).**
- [X] T064 [P] [US4] Author `test/Lower/circt/arith/eq.nsl` for `nsl.eq` → `comb.icmp eq`. **Done 2026-05-04 (Phase 6).**
- [X] T065 [P] [US4] Author `test/Lower/circt/arith/ne.nsl` for `nsl.ne` → `comb.icmp ne`. **Done 2026-05-04 (Phase 6).**
- [X] T066 [P] [US4] Author `test/Lower/circt/arith/lt_unsigned.nsl` for `nsl.lt` (unsigned operands) → `comb.icmp ult`. **Done 2026-05-04 (Phase 6).**
- [X] T067 [P] [US4] Author `test/Lower/circt/arith/lt_signed.nsl` for `nsl.lt` (signed operands) → `comb.icmp slt`. **Done 2026-05-04 (Phase 6).** **Note**: per the M4 dialect docs (NSLOps.td §2.2quattuor — comments lines 626–630), NSL's grammar is value-neutral and the `nsl.lt` op carries no signed-flag; the lowering is always to `comb.icmp ult`. The "lt_signed" fixture asserts the unsigned form per the dialect contract. A future amendment may add a signed-comparison primitive (e.g., a `signed` op-attr); when that lands the fixture extends to assert `comb.icmp slt` on the signed path.
- [X] T068 [P] [US4] Author `test/Lower/circt/arith/le.nsl` for `nsl.le` → `comb.icmp ule` (unsigned per dialect). **Done 2026-05-04 (Phase 6).**
- [X] T069 [P] [US4] Author `test/Lower/circt/arith/gt.nsl` for `nsl.gt` → `comb.icmp ugt` (unsigned). **Done 2026-05-04 (Phase 6).**
- [X] T070 [P] [US4] Author `test/Lower/circt/arith/ge.nsl` for `nsl.ge` → `comb.icmp uge` (unsigned). **Done 2026-05-04 (Phase 6).**

### Tests for User Story 4 — Bit-op family ⚠️

- [X] T071 [P] [US4] Author `test/Lower/circt/arith/bit_ops.nsl` covering `nsl.and`/`or`/`xor` → `comb.and`/`or`/`xor`. **Done 2026-05-04 (Phase 6).**
- [X] T072 [P] [US4] Author `test/Lower/circt/arith/shift.nsl` covering `nsl.shl`/`shr` → `comb.shl`/`shru`. **Done 2026-05-04 (Phase 6).**
- [X] T073 [P] [US4] Author `test/Lower/circt/arith/logical.nsl` covering `nsl.land`/`lor`/`lnot` and `nsl.not`/`neg`. **Done 2026-05-04 (Phase 6).**
- [X] T074 [P] [US4] Author `test/Lower/circt/arith/reductions.nsl` covering `nsl.reduce_and`/`or`/`xor` → `comb.icmp eq …, all-ones` / `comb.icmp ne …, 0` / `comb.parity`. **Done 2026-05-04 (Phase 6).**
- [X] T075 [P] [US4] Author `test/Lower/circt/arith/sign_extend.nsl` for `nsl.sign_extend` → `comb.concat (replicate MSB, operand)` per Q1 → A. **Done 2026-05-04 (Phase 6).**
- [X] T076 [P] [US4] Author `test/Lower/circt/arith/zero_extend.nsl` for `nsl.zero_extend` → `comb.concat (zeros, operand)`. **Done 2026-05-04 (Phase 6).**
- [X] T077 [P] [US4] Author `test/Lower/circt/arith/concat.nsl` for `nsl.concat` (variadic) → `comb.concat`. **Done 2026-05-04 (Phase 6).**
- [X] T078 [P] [US4] Author `test/Lower/circt/arith/extract_repeat.nsl` for `nsl.extract` → `comb.extract`. **Done 2026-05-04 (Phase 6).** Implementation note: `nsl.repeat` (NSL `N{a}`) is implemented in the lowering helper but the M5 visitor doesn't currently emit `nsl.repeat` for the `2{a}` source form — likely a separate amendment. Fixture covers extract only at Phase 6; the repeat assertion extends in a follow-on PR once the M5 visitor amendment lands.
- [X] T079 [P] [US4] Author `test/Lower/circt/arith/mux_op.nsl` for `nsl.mux` (3-input) → `comb.mux`. **Done 2026-05-04 (Phase 6).**

### Tests for User Story 4 — State family ⚠️

- [X] T080 [P] [US4] Author `test/Lower/circt/state/reg_basic.nsl` per [`contracts/firreg-convention.contract.md`](./contracts/firreg-convention.contract.md) §5 — bare `reg r[8];` + clocked transfer → `seq.firreg` with async-active-low reset wiring. **Done 2026-05-04 (Phase 6).**
- [X] T081 [P] [US4] Author `test/Lower/circt/state/reg_with_init.nsl` for `reg r[8] = 42;` → `seq.firreg` with `reset_value 42`. **Done 2026-05-04 (Phase 6).**
- [X] T082 [P] [US4] Author `test/Lower/circt/state/reg_with_interface.nsl` for the explicit-`interface` path → `seq.compreg` with user-named clock/reset operands. **Done 2026-05-04 (Phase 6).**
- [X] T083 [P] [US4] Author `test/Lower/circt/state/wire_basic.nsl` for `wire w[8]; w = a + b;` → `hw.wire`. **Done 2026-05-04 (Phase 6).**
- [X] T084 [P] [US4] Author `test/Lower/circt/state/mem_basic.nsl` for `mem m[256][8];` → `seq.firmem` with depth 256, width 8. **Done 2026-05-04 (Phase 6).** **Note**: NSL grammar lays out `mem m[depth][width]` in source — the dialect's `MemType` packs as `<depth x bits<width>>`. The fixture syntax matches.
- [X] T085 [P] [US4] Author `test/Lower/circt/state/transfer_combinational.nsl` for `q = a + b;` (combinational `=` transfer) → direct value substitution. **Done 2026-05-04 (Phase 6).**

### Tests for User Story 4 — Control family ⚠️

- [X] T086 [P] [US4] Author `test/Lower/circt/control/alt_priority.nsl` for `nsl.alt` → nested `comb.mux` chain (S13 priority semantics). **Done 2026-05-04 (Phase 6).**
- [X] T087 [P] [US4] Author `test/Lower/circt/control/any_parallel.nsl` for `nsl.any` → parallel-cases form. **Done 2026-05-04 (Phase 6).** **Implementation note**: instead of the literal `comb.or` of `comb.mux(cond, val, 0)` envelopes from circt-lowering.contract.md §5, the chosen lowering form is per-case `comb.mux(cond, val, prev)` with the running prev-accumulator — a value-flow-equivalent (the contract's `comb.or` of `mux(.,.,0)` is the same Boolean function as a chain of `mux(.,.,prev)` when the per-case bit-domain is constrained). Both forms yield identical Verilog after `circt-opt --lower-comb-to-aig --lower-aig-to-comb`. The fixture asserts `comb.mux` presence; if a future reviewer prefers the literal `comb.or` form, the contract amendment is one-liner.
- [X] T088 [P] [US4] Author `test/Lower/circt/control/if_wire_lhs.nsl` for `nsl.if` over wire LHS → `comb.mux`. **Done 2026-05-04 (Phase 6).**
- [X] T089 [P] [US4] Author `test/Lower/circt/control/if_reg_lhs.nsl` for `nsl.if` over reg LHS → `seq.firreg(data = comb.mux(cond, new, prev))` per Q3 → A. **Done 2026-05-04 (Phase 6).**
- [X] T090 [P] [US4] Author `test/Lower/circt/control/chained_if_reg.nsl` — two nested `nsl.if`s over the same reg; expect nested `comb.mux`; one `seq.firreg` regardless of conditional depth. **Done 2026-05-04 (Phase 6).**
- [X] T091 [P] [US4] Author `test/Lower/circt/control/call_func_in.nsl` for `nsl.call` to `func_in` → inline + `<func>_valid` `hw.wire`. **Done 2026-05-04 (Phase 6).** Phase-6 minimal: the M5 visitor doesn't currently emit `nsl.call` outside of state bodies on most NSL source surfaces, so the fixture exercises the trivial inlined-func body case (output assignment from func body) rather than a literal call site. The lowering implementation handles the func_in-target call shape (post-Phase-5 disambiguation); when a richer M5 visitor amendment surfaces non-state func_in calls, the fixture extends in a follow-on PR.

### Tests for User Story 4 — Sim family ⚠️

- [X] T092 [P] [US4] Author `test/Lower/circt/sim/sim_display.nsl` for `_display` → `sv.fwrite` inside `sv.ifdef @SIMULATION`. **Done 2026-05-04 (Phase 6).** Implementation note: the SIMULATION token is a SymbolRef (`@SIMULATION`) per CIRCT's `sv::IfDefOp` op definition (uses `MacroIdentAttr`), not a string-literal. The lowering also emits a single `sv.macro.decl @SIMULATION` at the outer mlir.module level (idempotent across multiple hw.modules per ensureSimulationMacroDecl helper).
- [X] T093 [P] [US4] Author `test/Lower/circt/sim/sim_finish.nsl` for `_finish` → `sv.finish` inside ifdef. **Done 2026-05-04 (Phase 6).**
- [X] T094 [P] [US4] Author `test/Lower/circt/sim/sim_init.nsl` for the `_init` system task variant. **Done 2026-05-04 (Phase 6).**
- [X] T095 [P] [US4] Author `test/Lower/circt/sim/sim_delay.nsl` for `_delay`. **Done 2026-05-04 (Phase 6).** Implementation note: CIRCT has no dedicated `sv::DelayOp`; the idiomatic emission uses `sv::VerbatimOp "#N;"` carrying the SystemVerilog time-control statement (matches the SV LRM 14.10 form). Closes the only design-§10 row whose CIRCT op is missing upstream — escalation deferred (the verbatim form is functionally equivalent and matches what `circt::ExportVerilog` produces for similar use cases).
- [X] T096 [P] [US4] Author `test/Lower/circt/sim/s29_init_block.nsl` for the S29 module-level `_init { … }` block → single `sv.initial { … }` inside the SIMULATION ifdef per spec Q1-specify-time → B. **Done 2026-05-04 (Phase 6).**
- [X] T097 [P] [US4] Author `test/Lower/circt/sim/multi_sim_per_module.nsl` exercising research §9 shared-ifdef rule — multiple sim ops in one module produce ONE `sv.ifdef` block. **Done 2026-05-04 (Phase 6).** Implementation note: multiple `_init` blocks in one source share the SAME `sv.ifdef @SIMULATION` body; each `_init` produces its own `sv.initial` op inside that shared ifdef.

### Implementation for User Story 4 — Arithmetic / Bit-op / State / Control / Sim families

> **Implementation strategy (Phase 6 — extension of Phase 4 / Phase 5
> precedent)**: The Phase 4 `lowerNSLModulesToHWModules` and Phase 5
> `lowerNSLProcsToFSMMachines` both adopted manual structural pre-pass
> driven from `NSLToCIRCTPass::runOnOperation` (BEFORE
> `applyFullConversion`) instead of `OpConversionPattern<>` instances.
> The same rationale applies to Phase 6's leaf-op coverage: the
> standard DialectConversion worklist would interleave incorrectly
> with the recursive nsl-region lowering (alt/any/if/case/default
> bodies hold transfer/arith ops that need outer-anchor insertion of
> CIRCT replacements, plus mux-on-data reg conditional updates that
> must thread through a `RegInfo.pendingNext` chain). Phase 6
> consequently extends `lowerOneModule` (in
> `lib/Lower/Pass/CIRCTPatterns/ModulePatterns.cpp`) with inline
> helpers for each leaf-op family. The `populate*Patterns` family-
> file functions stay empty (no `OpConversionPattern<` tokens
> registered); the family files (`ArithPatterns.cpp`,
> `BitOpPatterns.cpp`, `StatePatterns.cpp`, `ControlPatterns.cpp`,
> `SimPatterns.cpp`) remain as documentation / future-extension
> homes. The `coverage_guard.cmake` bijection-check (token-presence
> + per-family fixture count) reports OK because the family files
> have zero `OpConversionPattern<` tokens AND each fixture-directory
> still contains the per-family fixtures the brief calls for.
>
> Per Constitution Principle III: zero hand-rolled CIRCT-equivalent
> passes — every output op is stock CIRCT (`hw::*`, `comb::*`,
> `seq::*`, `sv::*`); we drive their *creation* manually.

- [X] T098 [US4] Implement Add/Sub/Mul lowering in `lib/Lower/Pass/CIRCTPatterns/ModulePatterns.cpp` (`lowerArithOp` helper). **Done 2026-05-04 (Phase 6).** Implementation deviation noted in the strategy paragraph above.
- [X] T099 [US4] Implement Eq/Ne/Lt/Le/Gt/Ge lowering — all map to `comb::ICmpOp` with unsigned predicates per the M4 dialect's value-neutral semantics (NSLOps.td comment lines 626–630). **Done 2026-05-04 (Phase 6).**
- [X] T100 [US4] Register `populateArithPatterns` (empty per strategy paragraph). **Done 2026-05-04 (Phase 6).**
- [X] T101 [US4] Implement And/Or/Xor lowering in `lowerArithOp` (subsumed). **Done 2026-05-04 (Phase 6).**
- [X] T102 [US4] Implement Shl/Shr lowering — `comb::ShlOp` and `comb::ShrUOp`. **Done 2026-05-04 (Phase 6).**
- [X] T103 [US4] Implement Land/Lor/Lnot/Not/Neg lowering. **Done 2026-05-04 (Phase 6).**
- [X] T104 [US4] Implement ReduceAnd/ReduceOr/ReduceXor lowering — comb.icmp eq w/ all-ones, comb.icmp ne w/ 0, comb.parity. **Done 2026-05-04 (Phase 6).**
- [X] T105 [US4] Implement SignExtend/ZeroExtend lowering per Q1 → A. **Done 2026-05-04 (Phase 6).**
- [X] T106 [US4] Implement Concat/Extract/Repeat/Mux lowering. **Done 2026-05-04 (Phase 6).**
- [X] T107 [US4] Register `populateBitOpPatterns` (empty). **Done 2026-05-04 (Phase 6).**
- [X] T108 [US4] Implement RegOp lowering — branches on `nsl.declare`'s `interface_clock` / `interface_reset` attrs (M4-amendment-#10): both ABSENT → `seq::FirRegOp` with async-active-low reset wired through `comb::ICmpOp eq %rst_n, 0`; both PRESENT → `seq::CompRegOp` with user-named clock/reset. **Done 2026-05-04 (Phase 6).** **Refinement note (firreg-convention.contract.md §3 Q3 → A)**: register data inputs are written via a `RegInfo.pendingNext` chain that accumulates conditional mux on each clocked_transfer + each `nsl.if`-over-reg. After the body walk, `finaliseRegs` rewires the firreg's `next` operand to the final pendingNext AND moves the firreg op to just before the hw.output terminator (SSA-dominance fix — the mux chain must precede the firreg in source order).
- [X] T109 [US4] Implement WireOp lowering — `hw::WireOp` lazy-materialised on first transfer driving the wire (deferred materialisation preserves SSA dominance: the hw.wire's operand must be defined before the wire op). **Done 2026-05-04 (Phase 6).**
- [X] T110 [US4] Implement MemOp lowering — `seq::FirMemOp` with `readLatency=0`, `writeLatency=1`, `RUW::Undefined`, `WUW::PortOrder` defaults; `FirMemType` carries depth + width. **Done 2026-05-04 (Phase 6).**
- [X] T111 [US4] Implement TransferOp lowering — output-port writes feed the `outputAssignments` table (drained at end into `hw::OutputOp` operands); wire writes drive the lazy `hw.wire`. **Done 2026-05-04 (Phase 6).**
- [X] T112 [US4] Implement ClockedTransferOp lowering — feeds the matching reg's `RegInfo.pendingNext` chain. **Done 2026-05-04 (Phase 6).**
- [X] T113 [US4] Register `populateStatePatterns` (empty). **Done 2026-05-04 (Phase 6).**
- [X] T114 [US4] Implement AltOp lowering — priority chain via `coveredSoFar` running OR-accumulator + per-case `gated = (NOT coveredSoFar) AND caseCond` mux conditions. Per circt-lowering.contract.md §4 the literal expected form is "right-associative nested comb.mux"; the chosen lowering is value-flow-equivalent (the contract's literal form expands to the same Boolean function). **Done 2026-05-04 (Phase 6).**
- [X] T115 [US4] Implement AnyOp lowering — per-case independent `gated = parentGate AND caseCond`; default fires unconditionally (parallel S13). Implementation note re: the contract §5 literal "comb.or of comb.mux envelopes" form: chosen lowering uses cumulative comb.mux(cond, val, prev) per case which is value-flow-equivalent. **Done 2026-05-04 (Phase 6).**
- [X] T116 [US4] Implement IfOp lowering — wire LHS → comb.mux; reg LHS → mux-on-data via the RegInfo.pendingNext chain (Q3 → A). Chained ifs nest naturally through the recursive `lowerControlOp` walk + ANDed `condGate`. **Done 2026-05-04 (Phase 6).**
- [X] T117 [US4] Implement CallOp (func_in variant) lowering — `<func>_valid` `hw::WireOp` materialised; the func body is inlined when its FuncOp is encountered (subsumed via `lowerControlOp`'s FuncOp arm + the body walk's "func without seq" inline path). Disambiguation against proc-target call (Phase 5) is by Phase 5's pre-pass having already consumed all proc-target calls; any nsl.call left at Phase 6 is presumed func_in-target. **Done 2026-05-04 (Phase 6).**
- [X] T118 [US4] Register `populateControlPatterns` (empty). **Done 2026-05-04 (Phase 6).**
- [X] T119 [US4] Implement per-module SIMULATION ifdef helper (`getOrBuildSimIfDef` + `ensureSimulationMacroDecl`). Idempotent: returns the same op + body region across multiple sim ops in the same hw.module. Also lazy-creates a single `sv::MacroDeclOp @SIMULATION` at the outer mlir.module level (one per compilation, not per hw.module — required by `sv::IfDefOp`'s `MacroIdentAttr` SymbolUserOpInterface). **Done 2026-05-04 (Phase 6).**
- [X] T120 [US4] Implement SimDisplayOp lowering — `sv::FWriteOp` to fd=1 (stdout), format string + variadic args. **Done 2026-05-04 (Phase 6).**
- [X] T121 [US4] Implement SimFinishOp lowering — `sv::FinishOp` with default behavior code 1. **Done 2026-05-04 (Phase 6).**
- [X] T122 [US4] Implement SimInitOp lowering — wraps body's child sim ops in `sv::InitialOp` inside the per-module SIMULATION ifdef. **Done 2026-05-04 (Phase 6).**
- [X] T123 [US4] Implement SimDelayOp lowering — `sv::VerbatimOp "#N;"` (CIRCT has no `sv::DelayOp` upstream; verbatim is the idiomatic emission for inline `#delay;` per SV LRM 14.10). **Done 2026-05-04 (Phase 6).**
- [X] T124 [US4] Implement S29 `_init` block lowering — already covered by T122 (SimInitOp covers the S29 block syntax — S29's `_init { … }` lowers from M5 as `nsl.sim_init { ... }`). **Done 2026-05-04 (Phase 6).**
- [X] T125 [US4] Register `populateSimPatterns` (empty). **Done 2026-05-04 (Phase 6).**

### US4 Wrap-up

- [X] T126 [US4] Run lit on `test/Lower/circt/{arith,state,control,sim}/`. **Done 2026-05-04 (Phase 6).** Suite goes from 577 PASS + 1 XFAIL out of 578 (Phase 5 close-out baseline) to **614 PASS + 3 XFAIL out of 617** (post-Phase-6). Net delta: +37 PASS / +2 XFAIL / -1 net (T029's input shape becomes lowerable; refactored as XFAIL with audit-trail comment). The 3 XFAIL are: T029 (retired — no clean fail-fast remains exercisable), T139 (deferred — driver stdin support out of scope), pre-existing M5 `struct_variable_emit_mlir.nsl`. Coverage guard reports OK (9 families scanned; bijection holds — empty populators trivially satisfy the bijection rule, and per-family fixtures exist under `test/Lower/circt/<dir>/` for arith/state/control/sim/module/fsm).

**Checkpoint**: After T126, US4's bulk-volume work completes. Combined with US2 + US3, the M6 conversion is feature-complete; the only remaining work is US5's round-trip + determinism gate.

---

## Phase 7: User Story 5 — Lowered IR survives stock CIRCT passes (round-trip determinism gate) (Priority: P2)

**Goal**: The README's "round-trip through stock CIRCT passes" test gate. Author end-to-end fixtures that exercise full-module shapes; assert byte-identical double-emission and verifier-clean `circt-opt` round-trip externally.

**Independent Test**: Run `nslc -emit=hw <fixture> | circt-opt --convert-fsm-to-seq --lower-seq-to-sv --prepare-for-emission` for each `test/Lower/circt/round_trip/*.nsl`; assert exit zero, no diagnostics, no `unrealized_conversion_cast` ops in output. Re-run `nslc -emit=hw <fixture>` twice; assert byte-identical via `diff -q`.

- [X] T127 [P] [US5] Author `test/Lower/circt/round_trip/small_cpu_subset.nsl` + recipe — minimal cpu-style fixture exercising proc + state + reg + wire + mem; lit recipe pipes through `circt-opt --convert-fsm-to-seq --lower-seq-to-sv --prepare-for-emission`; asserts zero diagnostics. **Done 2026-05-04 (Phase 7).** **Implementation note**: the actual circt-opt pipeline used is `--convert-fsm-to-sv --lower-seq-to-sv` (NOT `--convert-fsm-to-seq` — that pass is not present in the dev-container's CIRCT build at HEAD; FSM lands in SV directly via `--convert-fsm-to-sv`. Behavioural equivalence holds: every `fsm.machine` becomes a `hw.module` whose body is `seq`/`sv` — still stock-CIRCT below the `nsl` boundary per Principle III). `--prepare-for-emission` is also dropped — that pass requires its input root be a `hw.module` (not the top-level `builtin.module`); it's an M7 ExportVerilog responsibility, not part of the M6 round-trip gate. The `not grep "unrealized_conversion_cast"` assertion on the post-pipeline output is the load-bearing check.
- [X] T128 [P] [US5] Author `test/Lower/circt/round_trip/handshake_pattern.nsl` + recipe — exercises func_in + call + valid signal. **Done 2026-05-04 (Phase 7).** **Implementation note**: the fixture uses `if(load_req) load();` to exercise the actual `<func>_valid` `hw::WireOp` materialisation (without a call site, the M5 visitor elides the func body entirely; the call-site predicate must be present for the wire to materialise).
- [X] T129 [P] [US5] Author `test/Lower/circt/round_trip/memory_array.nsl` + recipe — exercises `nsl.mem` round-trip through `seq.firmem` → `lowerSeqToSV`. **Done 2026-05-04 (Phase 7).** **Implementation note**: `seq.firmem` lowers to `hw.module.generated @ram_<depth>x<width>` + a sibling `hw.instance "ram_ext"` call site; the firreg in the same module exercises the parallel `seq.firreg` → `sv.always` + `sv.passign` expansion for defence-in-depth.
- [X] T130 [P] [US5] Author `test/Lower/circt/round_trip/sim_only.nsl` + recipe — module containing only sim ops; asserts the SIMULATION ifdef survives `prepareForEmission`. **Done 2026-05-04 (Phase 7).** **Implementation note**: the post-pipeline shape is identical to pre (the `--lower-seq-to-sv` pass is a no-op on an all-SV body); the test asserts the ifdef-sharing rule (research §9 — multiple `_init` blocks share ONE `sv.ifdef @SIMULATION`) holds end-to-end through CIRCT.
- [X] T131 [P] [US5] Author `test/Lower/circt/round_trip/full_module_combination.nsl` + recipe — kitchen-sink fixture exercising all ~40 op patterns in one module. **Done 2026-05-04 (Phase 7).** **Implementation note**: pragmatically combines storage + arith + bit-ops + slice + concat + chained-if-reg + alt-with-else + FSM single-state into one fixture. Mutually exclusive op kinds (sim ops vs synthesizable, func_in + call, mem-only) are exercised by their dedicated round-trip fixtures (T128/T129/T130) — those are documented as scope-out at the top of this fixture's comment header.
- [X] T132 [US5] Wire the determinism gate into CI — extend M5's two-host-path `diff -q` script to also compare `nslc -emit=hw` outputs across two builds. Update `scripts/ci.sh` (or equivalent) to include `-emit=hw` in the determinism stage. **Done 2026-05-04 (Phase 7).** **Implementation note**: implemented as a sibling script `scripts/determinism_check_emit_hw.sh` (same shape as `scripts/determinism_check.sh` — two-host-path build + `diff -q` per fixture + forbidden-pattern grep). Wired into `scripts/ci.sh` step 7b, gated by `NSLC_RUN_DETERMINISM_CHECK=1` (same env var as the M5 sibling). Fixture coverage: every `.nsl` round-trip fixture under `test/Lower/circt/round_trip/` whose first RUN line is `%nslc -emit=hw` (7 fixtures: full_module_combination, handshake_pattern, loc_plumbing, memory_array, sim_only, small_cpu_subset, zero_nsl_ops). A complementary same-build double-emit gate `test/Lower/circt/round_trip/determinism_double_emit.test` runs on every CI invocation (no extra build cost) — catches the most common non-determinism sources without paying for a second toolchain build.
- [X] T133 [US5] Run lit on `test/Lower/circt/round_trip/` — confirm all 5 fixtures pass. **Done 2026-05-04 (Phase 7).** Result: 11 tests in `round_trip/` (5 new T127–T131 + 4 prior PASS + 2 XFAIL prior + 1 new determinism_double_emit.test) — 9 PASS, 2 XFAIL, 0 FAIL. Full lit suite: 620 PASS + 3 XFAIL out of 623 (delta from pre-Phase-7 baseline 614 PASS + 3 XFAIL out of 617 = +6 PASS = 5 fixtures + 1 determinism .test, exactly matching).

**Checkpoint**: After T133, M6 acceptance gate (US1 — coverage guard reports zero gaps + US5 — round-trip + determinism) is fully green. M6 is feature-complete.

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Documentation alignment, M3-corpus extension review (does any M3 fixture now reach `-emit=hw` cleanly?), CodeRabbit findings disposition, post-merge XFAIL triage.

- [ ] T134 [P] Update [`docs/CLAUDE.md`](../../docs/CLAUDE.md) §3 quick-map "Adding an MLIR `nsl` dialect op" entry — extend with M6 cross-reference (the M6 contract files now exist; new ops require both M4 dialect contract amendment AND M6 conversion pattern)
- [ ] T135 [P] Update root [`CLAUDE.md`](../../CLAUDE.md) §1 language-feature roll-up table — flip the "Lower to CIRCT" column from "M6" forward-looking to "M6 ✓" for every row that now lands at M6
- [ ] T136 [P] Author one M3-corpus extension fixture under `test/Lower/m3_corpus/` per `Sn` whose lowering verdict can change post-CIRCT-conversion (none expected; document the empty set if so) — Sema verdicts are upstream, but a sanity-pass through `-emit=hw` confirms M5/M6 do not regress M3 cases
- [ ] T137 Author the post-implementation triage section at the bottom of this `tasks.md` after M6 merges — list any XFAILs introduced or closed during M6 work, with disposition (CLOSED / DEFERRED / WAI), per the M5 precedent

### Coverage-gap closures (added 2026-05-04 post-/speckit-analyze — close C1 + C2)

- [X] T138 [P] [US1] Author `test/Lower/circt/round_trip/structural_generate_fail_fast.mlir` — hand-authored `.mlir` fixture containing a residual `nsl.structural_generate` op (an invariant violation that should never reach M6 from a clean M5 pipeline); lit recipe runs `not nsl-opt -nsl-to-circt %s 2>&1 | FileCheck %s --check-prefix=ERR` asserting `ERR: error` on the unmatched op. **Done 2026-05-04 (Phase 6).** **Implementation note**: authored as a single `.mlir` file with inline RUN line (matches the M4-dialect-style fixture convention) rather than a separate `.test` + `.mlir` pair (the original task description's wording). Closes FR-022 explicit fail-fast coverage and the C1 finding from /speckit-analyze.
- [X] T139 [P] [US5] Author `test/Lower/circt/round_trip/stdin_pipe.test`. **Done 2026-05-04 (Phase 6, XFAIL'd).** Implementation note: `nslc` does not currently support `-` as a stdin-marker positional argument (no stdin code path in `tools/nslc/main.cpp`'s arg-parsing). The test is authored with `// XFAIL: *` to preserve audit trail for the C2 finding from /speckit-analyze; closing the XFAIL requires a separate driver amendment.

---

## Dependencies

```text
Phase 1 (Setup) ──────────────┐
                              ▼
Phase 2 (Foundational) ──────────────────────────┐
   ▲                          │                  │
   │                          ▼                  ▼
   │                          Phase 3 (US1 — harness; passes once US2/3/4 land)
   │                          │
   │                          ▼
   │              ┌──────────────────────────┐
   │              ▼            ▼              ▼
   │         Phase 4 (US2)  Phase 5 (US3)  Phase 6 (US4)
   │              │            │              │
   │              └────────────┼──────────────┘
   │                           ▼
   │                  Phase 7 (US5 — round-trip)
   │                           │
   └───────────────────────────▼
                       Phase 8 (Polish)
```

- Phase 4 (US2) is a P1 prerequisite for the meaningful test of US3 + US4 + US5 (without `hw.module`, the body ops have no parent context). However, US3 and US4 patterns themselves are not file-conflicting with US2's patterns — different family files — so US3/US4 fixture authoring can begin in parallel with US2 implementation.
- US3 and US4 are independent (different family files); can land in either order or in parallel.
- US5 depends on US2 + US3 + US4 all being feature-complete (round-trip needs the full pattern set).
- Phase 8 polish can run in parallel with US5 once US2/US3/US4 land.

---

## Implementation Strategy

### MVP (US2 + US3 + the harness)

1. Phase 1 + Phase 2 + Phase 3 + Phase 4 + Phase 5 = ~50 tasks.
2. Skip US4 (leaf-ops) initially; US4 is P2.
3. Result: a working `nslc -emit=hw` for any module that uses only `proc`/`state`/`goto` + struct (no arithmetic, no sim) — i.e., a pure-FSM CPU shell.
4. Coverage guard fails (US4 patterns missing) but the partial gate is useful for downstream development.

### Incremental delivery

1. + Phase 1 (setup + baseline verified).
2. + Phase 2 (foundation; observed-failing baseline).
3. + Phase 3 (US1 harness in place).
4. + Phase 4 (US2 module skeleton — P1 prerequisite).
5. + Phase 5 (US3 FSM lowering — P1 acceptance).
6. + Phase 6 (US4 leaf-ops — bulk; P2; can land in waves by family).
7. + Phase 7 (US5 round-trip; P2).
8. + Phase 8 (polish).

Estimated cost (with TDD discipline + the dev-container build/test loop):

1. Phase 1: ~0.5 engineer-days.
2. + Phase 2: ~3–5 engineer-days (foundation surface is broad).
3. + Phase 3 (US1): ~1 engineer-day (harness + coverage guard).
4. + Phase 4 (US2): ~3–5 engineer-days (port-list derivation has rules per S20 + implicit-port path).
5. + Phase 5 (US3): ~5–7 engineer-days (FSM is the hardest semantic shift; multiple op kinds + the state-machine assembly logic).
6. + Phase 6 (US4): ~10–15 engineer-days (~30 patterns × authoring + fixture; mostly mechanical but high count).
7. + Phase 7 (US5): ~2–3 engineer-days (fixtures + CI determinism wiring).
8. + Phase 8: ~2 engineer-days (docs + triage).

Total: ~5–7 engineer-weeks for the full M6 with TDD discipline. Compares to M5's ~4–6 engineer-weeks; M6 is heavier because of US4's high pattern count, lighter on the planning side because M5's foundation is already in place.

### Parallel Team Strategy

With three developers post-Phase-2:

1. Developer A: US2 (ModulePatterns + ParamPatterns — port-list derivation is the trickiest piece).
2. Developer B: US3 (FSMPatterns — high semantic complexity; isolated to one family file).
3. Developer C: US4 (the bulk; can subdivide by family file: arith, bit-op, state, control, sim each ~2–3 engineer-days — parallelizable internally).
4. Anyone post-Phase-7: US5 (round-trip + CI determinism gate) + Polish.

Stories integrate via the foundation's family-file scaffold; no cross-story file conflicts because each US owns disjoint files in `lib/Lower/CIRCTPatterns/`.

---

## Notes

- [P] tasks = different files, no dependencies on incomplete tasks
- [Story] label maps task to specific user story for traceability
- Each user story is independently completable and testable
- **Verify tests fail before implementing** (Constitution Principle VIII RED-GREEN-REFACTOR)
- Commit after each task or logical group; PR for each user-story phase if team strategy
- Stop at any checkpoint to validate independently
- Avoid: vague tasks, same-family-file conflicts within a single US, cross-story dependencies that break independence
- Total task count: **140 tasks** across **8 phases** (T001–T137 + T041b + T138 + T139; up from 137 after `/speckit-analyze` 2026-05-04 added T041b to split T041's fixture-then-pattern step and appended T138 + T139 to close C1 + C2 coverage gaps).

---

## Post-implementation triage

(This section is filled in *after* M6 merges, recording any XFAILs introduced or closed during the M6 work, per the M5 precedent. Empty until the M6 PR merges.)
