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

- [ ] T026 [P] [US1] Author `test/Lower/circt/round_trip/zero_nsl_ops.nsl` + `.test` — minimal NSL source that exercises representative ops; lit `RUN: nslc -emit=hw %s | FileCheck %s` with `CHECK-NOT: nsl\\.` asserting zero `nsl::*` ops in the output. Initially fails because no patterns registered.
- [ ] T027 [P] [US1] Author `test/Lower/circt/round_trip/loc_plumbing.nsl` + `.test` — minimal NSL source; lit `RUN: nslc -emit=hw %s | FileCheck %s` with `CHECK-NOT: loc(unknown)` asserting no `UnknownLoc` on any output op. Enforces FR-030 / SC-004.
- [ ] T028 [US1] Flesh out `test/Lower/circt/coverage_guard.cmake` — replace the empty skeleton with the bijection-checking logic per [`research.md`](./research.md) §14: walk `lib/Lower/CIRCTPatterns/*.cpp` for registered pattern types (regex-match `add<NSLToCIRCT_*_Pattern>`), walk `test/Lower/circt/<family>/*.nsl` for fixture names, normalise both to snake-case op names, assert bijection. On gap: `message(FATAL_ERROR ...)` with the offending list. Wire the script into the top-level `CMakeLists.txt` as a configure-time include.
- [ ] T029 [US1] Add `Compilation::emit` exit-code check via lit `not` directive — author `test/Lower/circt/round_trip/conversion_failure_exits_nonzero.test` exercising an `unknown_unsupported.nsl` source that intentionally fails conversion; assert `not nslc -emit=hw %s 2>&1 | FileCheck %s --check-prefix=ERR` matches `ERR: error: nsl→CIRCT conversion failed for op` (FR-028 enforcement)

**Checkpoint**: After T029, the US1 harness exists. T026 + T027 fail as expected (no patterns yet); T029 passes. The CI coverage guard fires when any pattern lands without a fixture (or vice versa). Implementation now proceeds in US2/US3/US4.

---

## Phase 4: User Story 2 — `nsl.module` + `nsl.declare` lower to `hw.module` with HW ports (Priority: P1)

**Goal**: The structural skeleton — every `nsl::ModuleOp` becomes an `hw::HWModuleOp` with the correct port list derived from the paired `nsl::DeclareOp`. Implicit `clk` + `rst_n` ports added for no-`interface` modules; explicit `interface` honoured per S20.

**Independent Test**: For every fixture under `test/Lower/circt/module/`, lit `RUN: nslc -emit=hw %s | FileCheck %s` succeeds. The `circt-opt` over the output reports verifier-clean. Independent of US3/US4/US5 (FSM, leaf-ops, round-trip).

### Tests for User Story 2 (MANDATORY per Constitution Principle VIII) ⚠️

- [ ] T030 [P] [US2] Author `test/Lower/circt/module/port_input_only.nsl` + `.expected.mlir` — `declare M { input a[8]; }` → `hw.module @M(%a: i8) -> ()` plus implicit `clk`, `rst_n`. FileCheck asserts the port list shape.
- [ ] T031 [P] [US2] Author `test/Lower/circt/module/port_output_only.nsl` + `.expected.mlir` — `declare M { output q[8]; }` → `hw.module @M() -> (q: i8)` plus implicit ports.
- [ ] T032 [P] [US2] Author `test/Lower/circt/module/port_mixed.nsl` + `.expected.mlir` — both input and output.
- [ ] T033 [P] [US2] Author `test/Lower/circt/module/interface_modifier.nsl` + `.expected.mlir` — `declare M { interface my_clk, my_rstn; ... }` → `hw.module @M(%my_clk: i1, %my_rstn: i1, ...)` per S20 user-named.
- [ ] T034 [P] [US2] Author `test/Lower/circt/module/implicit_clk_rstn.nsl` + `.expected.mlir` — no `interface` modifier → implicit `clk` + `rst_n` appended at end of input port list per [`contracts/firreg-convention.contract.md`](./contracts/firreg-convention.contract.md) §1
- [ ] T035 [P] [US2] Author `test/Lower/circt/module/empty_module.nsl` + `.expected.mlir` — port-less `module M {}` → `hw.module @M()` empty body
- [ ] T036 [P] [US2] Author `test/Lower/circt/module/submodule_singleton.nsl` + `.expected.mlir` — `module M { Sub sub; ... }` → `hw.instance "sub" @Sub(...)` inside `hw.module @M`
- [ ] T037 [P] [US2] Author `test/Lower/circt/module/submodule_with_param.nsl` + `.expected.mlir` — `param_int N = 8` consumed by submodule instantiation → `hw.instance` parameter wire (S16 + design line 1255)

### Implementation for User Story 2

- [ ] T038 [US2] Implement `OpConversionPattern<nsl::ModuleOp>` in `lib/Lower/CIRCTPatterns/ModulePatterns.cpp` — walks paired `nsl::DeclareOp` to build the port list, creates `hw::HWModuleOp`, recursively converts the body region. Per [`contracts/circt-lowering.contract.md`](./contracts/circt-lowering.contract.md) §3 port-list derivation rules. Implicit `clk` + `rst_n` appended for no-`interface` modules. (T030–T035 fixtures pass after this lands)
- [ ] T039 [US2] Implement `OpConversionPattern<nsl::SubmoduleOp>` in `lib/Lower/CIRCTPatterns/ParamPatterns.cpp` — singleton form (array form already exploded by M5's `NSLExplodeSubmodArrayPass`); creates `hw::InstanceOp` referencing target `hw.module` symbol; threads operand wires through. (T036 fixture passes)
- [ ] T040 [US2] Implement `OpConversionPattern<nsl::ParamIntOp>` in `lib/Lower/CIRCTPatterns/ParamPatterns.cpp` — produces `hw::InstanceOp` parameter-wire on consuming instances per S16 + design line 1255. (T037 fixture passes)
- [ ] T041 [P] [US2] Author `test/Lower/circt/module/submodule_with_strparam.nsl` + `.expected.mlir` — `param_str NAME = "foo"` consumed by submodule instantiation → `hw.instance` parameter wire (string-typed) per S16 + design line 1256
- [ ] T041b [US2] Implement `OpConversionPattern<nsl::ParamStrOp>` in `lib/Lower/CIRCTPatterns/ParamPatterns.cpp` — ditto for string-typed params (design line 1256). (T041 fixture passes after this lands)
- [ ] T042 [US2] Register `populateModulePatterns` to add the ModuleOp pattern from T038
- [ ] T043 [US2] Register `populateParamPatterns` to add the SubmoduleOp + ParamIntOp + ParamStrOp patterns from T039–T041
- [ ] T044 [US2] Run lit on `test/Lower/circt/module/` — confirm all 8 fixtures pass; CI coverage guard reports module + submodule + param patterns covered.

**Checkpoint**: After T044, US2's module skeleton works. `nslc -emit=hw` of any module-shape input produces a valid `hw::HWModuleOp` with the correct port list. US3 + US4 can now proceed in parallel (each operates on the body region inside the `hw.module`).

---

## Phase 5: User Story 3 — `nsl.proc` / `nsl.state` / `nsl.seq` lower to `fsm::MachineOp` (Priority: P1)

**Goal**: The README's named M6 pattern. Every `nsl::ProcOp` with its `nsl::StateOp` children becomes an `fsm::MachineOp` with `initial_state` from `nsl::FirstStateOp`. `nsl::SeqOp` inside a `nsl::FuncOp` becomes a `MachineOp` with auto-generated `seq_N` states. Goto, finish, and proc-call invocations route through `fsm::TransitionOp`.

**Independent Test**: For every fixture under `test/Lower/circt/fsm/`, lit `RUN: nslc -emit=hw %s | FileCheck %s` succeeds. Output contains zero `nsl.proc`/`nsl.state`/`nsl.seq`/`nsl.goto`/`nsl.first_state`/`nsl.finish` ops; `circt-opt` verifier-clean. Independent of US2 (modules are assumed at the parent level), US4, US5.

### Tests for User Story 3 (MANDATORY per Constitution Principle VIII) ⚠️

- [ ] T045 [P] [US3] Author `test/Lower/circt/fsm/single_state.nsl` + `.expected.mlir` — `proc P { state s0 { ... } }` + `first_state s0;` → `fsm.machine @P attributes { initial_state = @s0 } { fsm.state @s0 { ... } }`. Degenerate FSM.
- [ ] T046 [P] [US3] Author `test/Lower/circt/fsm/two_state_goto.nsl` + `.expected.mlir` — two states + `goto`; expect `fsm.transition` from source to target.
- [ ] T047 [P] [US3] Author `test/Lower/circt/fsm/first_state_not_first.nsl` + `.expected.mlir` — `first_state` declared after `state` defs; expect `initial_state` attr is correct regardless of source order. S28 case.
- [ ] T048 [P] [US3] Author `test/Lower/circt/fsm/finish_to_sink.nsl` + `.expected.mlir` — `nsl.finish` inside a state body; expect `fsm.transition @__sink__` with synthetic sink state in the machine.
- [ ] T049 [P] [US3] Author `test/Lower/circt/fsm/seq_inside_func.nsl` + `.expected.mlir` — `func F { seq { ... goto label1; ...; label1: ... } }`; expect `fsm.machine` with `seq_0`, `seq_1`, … states.
- [ ] T050 [P] [US3] Author `test/Lower/circt/fsm/proc_call_to_proc.nsl` + `.expected.mlir` — `nsl.call @Q` where `Q` is a proc; expect `fsm.transition @Q_initial_state`.

### Implementation for User Story 3

- [ ] T051 [US3] Implement `OpConversionPattern<nsl::ProcOp>` in `lib/Lower/CIRCTPatterns/FSMPatterns.cpp` — creates `fsm::MachineOp`, sets `initial_state` attribute from sibling `nsl::FirstStateOp`, walks `nsl::StateOp` children and converts each to `fsm::StateOp`. Synthetic `__sink__` state added if any `nsl::FinishOp` reachable in the proc body. (T045 + T047 + T048 fixtures pass)
- [ ] T052 [US3] Implement `OpConversionPattern<nsl::StateOp>` in `lib/Lower/CIRCTPatterns/FSMPatterns.cpp` — creates `fsm::StateOp` child of the enclosing `fsm::MachineOp`; converts the state body region; gathers `nsl::GotoOp` (state form) into the state's transitions sub-region. (T046 fixture passes)
- [ ] T053 [US3] Implement `OpConversionPattern<nsl::FirstStateOp>` in `lib/Lower/CIRCTPatterns/FSMPatterns.cpp` — consumed during `ProcOp` lowering; the standalone pattern is a stub that erases the op (its data was extracted by T051). Per [`contracts/circt-lowering.contract.md`](./contracts/circt-lowering.contract.md) §1 "consumed during" row.
- [ ] T054 [US3] Implement `OpConversionPattern<nsl::GotoOp>` (state form, S25) in `lib/Lower/CIRCTPatterns/FSMPatterns.cpp` — creates `fsm::TransitionOp` whose target is the state's symbol-ref. (T046 verified)
- [ ] T055 [US3] Implement `OpConversionPattern<nsl::SeqOp>` (inside `nsl::FuncOp`) in `lib/Lower/CIRCTPatterns/FSMPatterns.cpp` — creates `fsm::MachineOp` with auto-generated `seq_N` state names per design §10 line 1219. Maps each label-form `nsl::GotoOp` inside the seq to a `fsm::TransitionOp` to the matching `seq_N`. (T049 fixture passes)
- [ ] T056 [US3] Implement `OpConversionPattern<nsl::GotoOp>` (label form, inside seq) in `lib/Lower/CIRCTPatterns/FSMPatterns.cpp` — same shape as T054 but resolves the label to its `seq_N` state. (Verified by T049)
- [ ] T057 [US3] Implement `OpConversionPattern<nsl::FinishOp>` and `OpConversionPattern<nsl::FinishMethodOp>` in `lib/Lower/CIRCTPatterns/FSMPatterns.cpp` — create `fsm::TransitionOp` to the synthetic `__sink__` state. (T048 fixture verified)
- [ ] T058 [US3] Implement `OpConversionPattern<nsl::CallOp>` (proc-target variant) in `lib/Lower/CIRCTPatterns/FSMPatterns.cpp` — disambiguate by checking if the call target is a `proc_name` (vs `func_in`); for proc-target, creates `fsm::TransitionOp` to the proc's initial state. (T050 fixture passes; func-target variant is US4's T080)
- [ ] T059 [US3] Register `populateFSMPatterns` to add the 6 FSM patterns from T051–T058
- [ ] T060 [US3] Run lit on `test/Lower/circt/fsm/` — confirm all 6 fixtures pass; CI coverage guard reports FSM patterns covered.

**Checkpoint**: After T060, US3's FSM lowering works. Audited corpus's `cpu16` / `mips32_single_cycle` (M7) can structurally reach M6's output IR through their proc-heavy designs.

---

## Phase 6: User Story 4 — Combinational, state, and simulation `nsl::*` ops lower to `comb` / `seq` / `sv` (Priority: P2)

**Goal**: The bulk-volume conversion work — ~30 leaf-op patterns covering arithmetic, bit-ops, state elements, control flow, simulation, and the implicit `_init` block. Each pattern is mechanical (one-to-one with design §10's mapping table); the bulk of the engineering effort here is fixture authoring + ensuring the per-pattern reset / mux / ifdef conventions land correctly per Q2/Q3 + research §§4–5, 9.

**Independent Test**: For every fixture under `test/Lower/circt/{arith,state,control,sim}/`, lit `RUN: nslc -emit=hw %s | FileCheck %s` succeeds. Output contains zero `nsl.*` ops in the four covered families. Independent of US2 (modules), US3 (FSM), US5 (round-trip).

### Tests for User Story 4 — Arithmetic family (MANDATORY) ⚠️

- [ ] T061 [P] [US4] Author `test/Lower/circt/arith/add.nsl` + fixture for `nsl.add` → `comb.add`
- [ ] T062 [P] [US4] Author `test/Lower/circt/arith/sub.nsl` for `nsl.sub` → `comb.sub`
- [ ] T063 [P] [US4] Author `test/Lower/circt/arith/mul.nsl` for `nsl.mul` → `comb.mul`
- [ ] T064 [P] [US4] Author `test/Lower/circt/arith/eq.nsl` for `nsl.eq` → `comb.icmp eq`
- [ ] T065 [P] [US4] Author `test/Lower/circt/arith/ne.nsl` for `nsl.ne` → `comb.icmp ne`
- [ ] T066 [P] [US4] Author `test/Lower/circt/arith/lt_unsigned.nsl` for `nsl.lt` (unsigned operands) → `comb.icmp ult`
- [ ] T067 [P] [US4] Author `test/Lower/circt/arith/lt_signed.nsl` for `nsl.lt` (signed operands) → `comb.icmp slt`
- [ ] T068 [P] [US4] Author `test/Lower/circt/arith/le.nsl` for `nsl.le` → `comb.icmp sle`/`ule`
- [ ] T069 [P] [US4] Author `test/Lower/circt/arith/gt.nsl` for `nsl.gt` → `comb.icmp sgt`/`ugt`
- [ ] T070 [P] [US4] Author `test/Lower/circt/arith/ge.nsl` for `nsl.ge` → `comb.icmp sge`/`uge`

### Tests for User Story 4 — Bit-op family ⚠️

- [ ] T071 [P] [US4] Author `test/Lower/circt/arith/bit_ops.nsl` covering `nsl.and`/`or`/`xor` → `comb.and`/`or`/`xor`
- [ ] T072 [P] [US4] Author `test/Lower/circt/arith/shift.nsl` covering `nsl.shl`/`shr` → `comb.shl`/`shru`
- [ ] T073 [P] [US4] Author `test/Lower/circt/arith/logical.nsl` covering `nsl.land`/`lor`/`lnot` and `nsl.not`/`neg`
- [ ] T074 [P] [US4] Author `test/Lower/circt/arith/reductions.nsl` covering `nsl.reduce_and`/`or`/`xor` → `comb.icmp eq …, all-ones` / `comb.icmp ne …, 0` / `comb.parity`
- [ ] T075 [P] [US4] Author `test/Lower/circt/arith/sign_extend.nsl` for `nsl.sign_extend` → `comb.concat (replicate MSB, operand)` per Q1 → A
- [ ] T076 [P] [US4] Author `test/Lower/circt/arith/zero_extend.nsl` for `nsl.zero_extend` → `comb.concat (zeros, operand)`
- [ ] T077 [P] [US4] Author `test/Lower/circt/arith/concat.nsl` for `nsl.concat` (variadic) → `comb.concat`
- [ ] T078 [P] [US4] Author `test/Lower/circt/arith/extract_repeat.nsl` for `nsl.extract` → `comb.extract` and `nsl.repeat` → `comb.replicate`
- [ ] T079 [P] [US4] Author `test/Lower/circt/arith/mux_op.nsl` for `nsl.mux` (3-input) → `comb.mux`

### Tests for User Story 4 — State family ⚠️

- [ ] T080 [P] [US4] Author `test/Lower/circt/state/reg_basic.nsl` per [`contracts/firreg-convention.contract.md`](./contracts/firreg-convention.contract.md) §5 — bare `reg r[8];` + clocked transfer → `seq.firreg` with async-active-low reset wiring
- [ ] T081 [P] [US4] Author `test/Lower/circt/state/reg_with_init.nsl` for `reg r[8] = 42;` → `seq.firreg` with `reset_value 42`
- [ ] T082 [P] [US4] Author `test/Lower/circt/state/reg_with_interface.nsl` for the explicit-`interface` path → `seq.compreg` with user-named clock/reset operands
- [ ] T083 [P] [US4] Author `test/Lower/circt/state/wire_basic.nsl` for `wire w[8]; w = a + b;` → `hw.wire`
- [ ] T084 [P] [US4] Author `test/Lower/circt/state/mem_basic.nsl` for `mem m[8][256];` → `seq.firmem` with depth 256, width 8
- [ ] T085 [P] [US4] Author `test/Lower/circt/state/transfer_combinational.nsl` for `q = a + b;` (combinational `=` transfer) → direct value substitution

### Tests for User Story 4 — Control family ⚠️

- [ ] T086 [P] [US4] Author `test/Lower/circt/control/alt_priority.nsl` for `nsl.alt` → nested `comb.mux` chain (S13 priority semantics)
- [ ] T087 [P] [US4] Author `test/Lower/circt/control/any_parallel.nsl` for `nsl.any` → per-target `comb.or` of `comb.mux(cond, val, 0)` envelopes (S13 parallel)
- [ ] T088 [P] [US4] Author `test/Lower/circt/control/if_wire_lhs.nsl` for `nsl.if` over wire LHS → `comb.mux`
- [ ] T089 [P] [US4] Author `test/Lower/circt/control/if_reg_lhs.nsl` for `nsl.if` over reg LHS → `seq.firreg(data = comb.mux(cond, new, prev))` per Q3 → A
- [ ] T090 [P] [US4] Author `test/Lower/circt/control/chained_if_reg.nsl` — two nested `nsl.if`s over the same reg; expect nested `comb.mux`; one `seq.firreg` regardless of conditional depth
- [ ] T091 [P] [US4] Author `test/Lower/circt/control/call_func_in.nsl` for `nsl.call` to `func_in` → inline + `<func>_valid` `hw.wire`

### Tests for User Story 4 — Sim family ⚠️

- [ ] T092 [P] [US4] Author `test/Lower/circt/sim/sim_display.nsl` for `_display` → `sv.fwrite` inside `sv.ifdef "SIMULATION"`
- [ ] T093 [P] [US4] Author `test/Lower/circt/sim/sim_finish.nsl` for `_finish` → `sv.finish` inside ifdef
- [ ] T094 [P] [US4] Author `test/Lower/circt/sim/sim_init.nsl` for the `_init` system task variant
- [ ] T095 [P] [US4] Author `test/Lower/circt/sim/sim_delay.nsl` for `_delay`
- [ ] T096 [P] [US4] Author `test/Lower/circt/sim/s29_init_block.nsl` for the S29 module-level `_init { … }` block → single `sv.initial { … }` inside the SIMULATION ifdef per spec Q1-specify-time → B
- [ ] T097 [P] [US4] Author `test/Lower/circt/sim/multi_sim_per_module.nsl` exercising research §9 shared-ifdef rule — multiple sim ops in one module produce ONE `sv.ifdef` block

### Implementation for User Story 4 — Arithmetic family

- [ ] T098 [US4] Implement `OpConversionPattern<nsl::AddOp>`, `<nsl::SubOp>`, `<nsl::MulOp>` in `lib/Lower/CIRCTPatterns/ArithPatterns.cpp` — each maps to the matching `comb::*Op`. (T061 + T062 + T063 fixtures pass)
- [ ] T099 [US4] Implement `OpConversionPattern<nsl::EqOp>`, `<nsl::NeOp>`, `<nsl::LtOp>`, `<nsl::LeOp>`, `<nsl::GtOp>`, `<nsl::GeOp>` in `lib/Lower/CIRCTPatterns/ArithPatterns.cpp` — each maps to `comb::ICmpOp` with the appropriate predicate. Signedness disambiguated from M3 typed AST (the `nsl::*` op carries operand-type signedness as an attr per the M4 dialect contract). (T064–T070 fixtures pass)
- [ ] T100 [US4] Register `populateArithPatterns` to add the 9 arithmetic patterns

### Implementation for User Story 4 — Bit-op family

- [ ] T101 [US4] Implement `OpConversionPattern<nsl::AndOp>`, `<OrOp>`, `<XorOp>` in `lib/Lower/CIRCTPatterns/BitOpPatterns.cpp` (T071 fixture passes)
- [ ] T102 [US4] Implement `OpConversionPattern<nsl::ShlOp>`, `<ShrOp>` in BitOpPatterns.cpp — `comb::ShlOp` and `comb::ShrUOp` (logical/unsigned right shift) (T072 passes)
- [ ] T103 [US4] Implement `OpConversionPattern<nsl::LandOp>`, `<LorOp>`, `<LnotOp>`, `<NotOp>`, `<NegOp>` in BitOpPatterns.cpp — per design lines 1241–1245 mappings (T073 passes)
- [ ] T104 [US4] Implement `OpConversionPattern<nsl::ReduceAndOp>`, `<ReduceOrOp>`, `<ReduceXorOp>` in BitOpPatterns.cpp — per design lines 1246–1248 mappings (`comb.icmp eq` w/ all-ones, `comb.icmp ne` w/ 0, `comb.parity`) (T074 passes)
- [ ] T105 [US4] Implement `OpConversionPattern<nsl::SignExtendOp>`, `<ZeroExtendOp>` in BitOpPatterns.cpp per Q1 → A — `comb.concat (replicate MSB, operand)` and `comb.concat (zeros, operand)` (T075 + T076 pass)
- [ ] T106 [US4] Implement `OpConversionPattern<nsl::ConcatOp>`, `<ExtractOp>`, `<RepeatOp>`, `<MuxOp>` in BitOpPatterns.cpp — `comb.concat`, `comb.extract`, `comb.replicate`, `comb.mux` (T077 + T078 + T079 pass)
- [ ] T107 [US4] Register `populateBitOpPatterns` to add the ~13 bit-op patterns

### Implementation for User Story 4 — State family

- [ ] T108 [US4] Implement `OpConversionPattern<nsl::RegOp>` in `lib/Lower/CIRCTPatterns/StatePatterns.cpp` — branches on whether enclosing module has S20 `interface` modifier: no-`interface` → `seq::FirRegOp` with async-active-low reset per [`contracts/firreg-convention.contract.md`](./contracts/firreg-convention.contract.md) §1; with-`interface` → `seq::CompRegOp` per §2. (T080 + T081 + T082 pass)
- [ ] T109 [US4] Implement `OpConversionPattern<nsl::WireOp>` in StatePatterns.cpp — `hw::WireOp` (T083 passes)
- [ ] T110 [US4] Implement `OpConversionPattern<nsl::MemOp>` in StatePatterns.cpp — `seq::FirMemOp` with depth + width preserved (T084 passes)
- [ ] T111 [US4] Implement `OpConversionPattern<nsl::TransferOp>` (combinational `=`) in StatePatterns.cpp — direct value substitution (T085 passes)
- [ ] T112 [US4] Implement `OpConversionPattern<nsl::ClockedTransferOp>` (`:=`) in StatePatterns.cpp — feeds the FirRegOp's data operand (T080 verified through this pattern + T108)
- [ ] T113 [US4] Register `populateStatePatterns` to add the 5 state patterns

### Implementation for User Story 4 — Control family

- [ ] T114 [US4] Implement `OpConversionPattern<nsl::AltOp>` in `lib/Lower/CIRCTPatterns/ControlPatterns.cpp` — right-associative nested `comb.mux` chain per [`contracts/circt-lowering.contract.md`](./contracts/circt-lowering.contract.md) §4 (T086 passes)
- [ ] T115 [US4] Implement `OpConversionPattern<nsl::AnyOp>` in ControlPatterns.cpp — per-target `comb.or` of `comb.mux(cond, val, 0)` envelopes per §5 (T087 passes)
- [ ] T116 [US4] Implement `OpConversionPattern<nsl::IfOp>` in ControlPatterns.cpp — branches on LHS kind: wire LHS → `comb.mux`; reg LHS → mux-on-data per Q3 → A + [`contracts/firreg-convention.contract.md`](./contracts/firreg-convention.contract.md) §3. Chained `nsl.if`s nest the mux. (T088 + T089 + T090 pass)
- [ ] T117 [US4] Implement `OpConversionPattern<nsl::CallOp>` (func_in variant) in ControlPatterns.cpp — inline function body + materialise `<func>_valid` `hw.wire` per research §8. Disambiguate from proc-call variant (T058) via target-symbol lookup. (T091 passes)
- [ ] T118 [US4] Register `populateControlPatterns` to add the 4 control patterns

### Implementation for User Story 4 — Sim family

- [ ] T119 [US4] Implement the per-module SIMULATION ifdef materialiser helper in `lib/Lower/CIRCTPatterns/SimPatterns.cpp` — lazy-creates one `sv::IfDefOp` per `hw::HWModuleOp` body, returns its body region for op insertion. Used by all sim patterns and the S29 `_init` lowering.
- [ ] T120 [US4] Implement `OpConversionPattern<nsl::SimDisplayOp>` in SimPatterns.cpp — inserts `sv::FWriteOp` into the per-module ifdef body via the helper from T119 (T092 passes)
- [ ] T121 [US4] Implement `OpConversionPattern<nsl::SimFinishOp>` in SimPatterns.cpp — `sv::FinishOp` (T093 passes)
- [ ] T122 [US4] Implement `OpConversionPattern<nsl::SimInitOp>` in SimPatterns.cpp — contributes statements inside `sv::InitialOp` (T094 passes)
- [ ] T123 [US4] Implement `OpConversionPattern<nsl::SimDelayOp>` in SimPatterns.cpp — `sv::DelayOp` (T095 passes)
- [ ] T124 [US4] Implement S29 `_init` block lowering in SimPatterns.cpp — the body's converted statements wrap in a single `sv::InitialOp` inside the per-module SIMULATION ifdef per spec Q1-specify-time → B (T096 passes; T097 verifies single-ifdef sharing)
- [ ] T125 [US4] Register `populateSimPatterns` to add the 4 sim-op patterns + the S29 `_init` block lowering

### US4 Wrap-up

- [ ] T126 [US4] Run lit on `test/Lower/circt/{arith,state,control,sim}/` — confirm all ~37 fixtures pass; CI coverage guard reports all leaf-op family patterns covered.

**Checkpoint**: After T126, US4's bulk-volume work completes. Combined with US2 + US3, the M6 conversion is feature-complete; the only remaining work is US5's round-trip + determinism gate.

---

## Phase 7: User Story 5 — Lowered IR survives stock CIRCT passes (round-trip determinism gate) (Priority: P2)

**Goal**: The README's "round-trip through stock CIRCT passes" test gate. Author end-to-end fixtures that exercise full-module shapes; assert byte-identical double-emission and verifier-clean `circt-opt` round-trip externally.

**Independent Test**: Run `nslc -emit=hw <fixture> | circt-opt --convert-fsm-to-seq --lower-seq-to-sv --prepare-for-emission` for each `test/Lower/circt/round_trip/*.nsl`; assert exit zero, no diagnostics, no `unrealized_conversion_cast` ops in output. Re-run `nslc -emit=hw <fixture>` twice; assert byte-identical via `diff -q`.

- [ ] T127 [P] [US5] Author `test/Lower/circt/round_trip/small_cpu_subset.nsl` + recipe — minimal cpu-style fixture exercising proc + state + reg + wire + mem; lit recipe pipes through `circt-opt --convert-fsm-to-seq --lower-seq-to-sv --prepare-for-emission`; asserts zero diagnostics
- [ ] T128 [P] [US5] Author `test/Lower/circt/round_trip/handshake_pattern.nsl` + recipe — exercises func_in + call + valid signal
- [ ] T129 [P] [US5] Author `test/Lower/circt/round_trip/memory_array.nsl` + recipe — exercises `nsl.mem` round-trip through `seq.firmem` → `lowerSeqToSV`
- [ ] T130 [P] [US5] Author `test/Lower/circt/round_trip/sim_only.nsl` + recipe — module containing only sim ops; asserts the SIMULATION ifdef survives `prepareForEmission`
- [ ] T131 [P] [US5] Author `test/Lower/circt/round_trip/full_module_combination.nsl` + recipe — kitchen-sink fixture exercising all ~40 op patterns in one module
- [ ] T132 [US5] Wire the determinism gate into CI — extend M5's two-host-path `diff -q` script to also compare `nslc -emit=hw` outputs across two builds. Update `scripts/ci.sh` (or equivalent) to include `-emit=hw` in the determinism stage.
- [ ] T133 [US5] Run lit on `test/Lower/circt/round_trip/` — confirm all 5 fixtures pass.

**Checkpoint**: After T133, M6 acceptance gate (US1 — coverage guard reports zero gaps + US5 — round-trip + determinism) is fully green. M6 is feature-complete.

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Documentation alignment, M3-corpus extension review (does any M3 fixture now reach `-emit=hw` cleanly?), CodeRabbit findings disposition, post-merge XFAIL triage.

- [ ] T134 [P] Update [`docs/CLAUDE.md`](../../docs/CLAUDE.md) §3 quick-map "Adding an MLIR `nsl` dialect op" entry — extend with M6 cross-reference (the M6 contract files now exist; new ops require both M4 dialect contract amendment AND M6 conversion pattern)
- [ ] T135 [P] Update root [`CLAUDE.md`](../../CLAUDE.md) §1 language-feature roll-up table — flip the "Lower to CIRCT" column from "M6" forward-looking to "M6 ✓" for every row that now lands at M6
- [ ] T136 [P] Author one M3-corpus extension fixture under `test/Lower/m3_corpus/` per `Sn` whose lowering verdict can change post-CIRCT-conversion (none expected; document the empty set if so) — Sema verdicts are upstream, but a sanity-pass through `-emit=hw` confirms M5/M6 do not regress M3 cases
- [ ] T137 Author the post-implementation triage section at the bottom of this `tasks.md` after M6 merges — list any XFAILs introduced or closed during M6 work, with disposition (CLOSED / DEFERRED / WAI), per the M5 precedent

### Coverage-gap closures (added 2026-05-04 post-/speckit-analyze — close C1 + C2)

- [ ] T138 [P] [US1] Author `test/Lower/circt/round_trip/structural_generate_fail_fast.test` — hand-authored `.mlir` fixture containing a residual `nsl.structural_generate` op (an invariant violation that should never reach M6 from a clean M5 pipeline); lit recipe runs `not nsl-opt -nsl-to-circt %s 2>&1 | FileCheck %s --check-prefix=ERR` asserting `ERR: error: nsl→CIRCT conversion failed for op 'nsl.structural_generate'`. Closes FR-022 explicit fail-fast coverage (the implicit ConversionTarget illegal-op rule covered the case, but no positive regression existed). Belongs to US1's harness phase logically; appended at the tail per M5's T110 precedent.
- [ ] T139 [P] [US5] Author `test/Lower/circt/round_trip/stdin_pipe.test` — exercises `cat %S/zero_nsl_ops.nsl | nslc -emit=hw -` (matches the source already used by T026). Lit recipe asserts exit 0 AND output is byte-identical to the `-o`-form invocation. Closes FR-026 stdin-support explicit coverage.

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
