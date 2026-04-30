<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->
---
description: "Tasks for M4 — `nsl` MLIR Dialect (`nsl-dialect`: TableGen ops + types + verifiers + nsl-opt round-trip)"
---

# Tasks: M4 — `nsl` MLIR Dialect (`nsl-dialect`: TableGen ops + types + verifiers + nsl-opt round-trip)

**Input**: Design documents from `/specs/007-m4-mlir-dialect/`
**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md, contracts/

**Tests**: Test tasks are **MANDATORY** for this project per Constitution Principle VIII (Test-First Development, NON-NEGOTIABLE) and Principle VI ("Dialect tests use `nsl-opt` for round-trip verification of `.mlir`", NON-NEGOTIABLE). Every user story includes test tasks at the appropriate layer. Tests MUST be written and observed FAILING before the corresponding implementation tasks begin. Per Clarifications session 2026-04-30 Q1 → Option A, the dialect verifier checks **structural invariants only** — Sema-equivalent re-checks of `S1`–`S29` are out of scope. Per Q2 → Option B, parent-op invariants split between standard `HasParent<X>` TableGen trait (immediate parent) and hand-written ancestor-walk via `findAncestorOfKind<T>` helper (transitive parent; affects ~5 ops). Verifier diagnostic message text uses **substring match** (FileCheck `// expected-error{{<substring>}}`), not literal-string assertion — see `contracts/verifier-diagnostic.contract.md` for the carve-out rationale.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story?] Description with file path`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: User-story label (US1, US2, US3) — required for user-story phase tasks; absent for Setup / Foundational / Polish
- Every task description includes the exact file path

## Path Conventions

- Compiler middle-end layout (M0/M1/M2/M3 baseline; matches LLVM/CIRCT convention):
  - Public headers: `include/nsl/Dialect/NSL/IR/`
  - TableGen sources: `lib/Dialect/NSL/IR/NSL{Dialect,Ops,Types}.td`
  - Implementations: `lib/Dialect/NSL/IR/NSL{Dialect,Ops,Types}.cpp`
  - Driver glue: `lib/Driver/Compilation.cpp` (modified at M4); `lib/Driver/{LowerToNSL,RunNSLPasses}.cpp` (M4 new — stubs)
  - Developer/test binary: `tools/nsl-opt/main.cpp`
  - lit + FileCheck tests: `test/Dialect/{module-level,storage,control-terminal,action-block,action-helper,atomic,procedure,procedure-helper,system-task,marker,expansion-only,Types}/`
  - GoogleTest unit tests: `test_unit/dialect_register_test/`

> **Op count note.** Spec FR-010 describes **40 named ops + auto-generated terminators** (the draft-era "Total: 35" miscount was corrected pre-implementation in commit `e47484a` after `/speckit-analyze` flagged it; finding F2 in the analyze report). 40 ops + 3 types is the authoritative count enumerated in the FR-010 table and `data-model.md` §2.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project skeleton for M4; M0 stood up the build / CI / SPDX scan / empty layer skeleton, and M1/M2/M3 filled the first six layers (`nsl-basic` through `nsl-sema`) — Phase 1 here is small.

- [ ] T001 Create the M4 test directory tree under `test/Dialect/` and `test_unit/`: `test/Dialect/{module-level,storage,control-terminal,action-block,action-helper,atomic,procedure,procedure-helper,system-task,marker,expansion-only,Types}/` (12 subdirs) and `test_unit/dialect_register_test/`. Each gets a `.keep` placeholder so M0's lit-discovery picks them up.
- [ ] T002 Sanity-verify the M0+M1+M2+M3 build is still green on branch `007-m4-mlir-dialect` by running `./scripts/ci.sh all` inside `ghcr.io/koyamanx/nsl-nslc:dev` against the unchanged-since-M3 source tree (checkpoint before adding M4 sources). Expects: 6 stages green; the unchanged M0–M3 fixture corpus passes.
- [ ] T003 [P] **DEFERRED** if `cmake/AddNSLLibrary.cmake` (M0) already enforces downward-only layering via `_nsl_layer_index()` table — verify the macro covers `nsl-dialect` (7) → forbidden upstream of `nsl-ast` (5), `nsl-sema` (6), `nsl-parse` (4), `nsl-lex` (3), `nsl-preprocess` (2). If the layer table includes `nsl-dialect`, document the deferral here (parallel to M3's T003 deferral). Otherwise, extend the table to include `nsl-dialect` at index 7 and add the forbidden-as-upstream entries; SC-011 obligation satisfied by the macro's configure-time enforcement.

**Checkpoint**: M4 directory skeleton in place; build sanity exercised; layering guard verified. Phase 2 work can begin.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: `nsl-dialect` library scaffolding — the TableGen dialect class + base op/type records (no ops yet), the umbrella public header, the dialect-init code, the empty NSLOps.cpp / NSLTypes.cpp scaffolding, the `nsl-opt` developer/test binary, and the driver dialect-load + stub-body wiring. Every user story depends on these.

**⚠️ CRITICAL**: No US1 / US2 / US3 task may begin until this phase is complete.

### Dialect class TableGen + umbrella header (research §3, data-model §1)

- [ ] T004 [P] Author `lib/Dialect/NSL/IR/NSLDialect.td` — declares `def NSL_Dialect : Dialect { let name = "nsl"; let cppNamespace = "::nsl::dialect"; let useDefaultTypePrinterParser = 1; ... }` per design §7 lines 943–947. Includes the `class NSL_Op<string mnemonic, list<Trait> traits = []> : Op<NSL_Dialect, mnemonic, traits>` base for ops to inherit from. SPDX header in `/* … */` C-style comment block.
- [ ] T005 [P] Author `include/nsl/Dialect/NSL/IR/NSLDialect.h` — single public umbrella header per `contracts/dialect-api.contract.md` §1. `#include`s the TableGen-generated `NSLDialect.h.inc`, `NSLOps.h.inc`, `NSLTypes.h.inc` (private build artifacts — but the umbrella IS the only consumer surface). Declares `namespace nsl::dialect { void registerNSLDialect(mlir::DialectRegistry &); }` per FR-006. SPDX header.
- [ ] T006 Author `lib/Dialect/NSL/IR/NSLDialect.cpp` — `nsl::dialect::registerNSLDialect(registry)` implementation: calls `registry.insert<NSLDialect>()`. The `NSLDialect` class's `initialize()` method calls `addOperations<...>()` and `addTypes<...>()` over the eventual op + type sets (initially empty; populated as Phase 3 / 4 add records). SPDX header.

### Type scaffolding (data-model §3, research §5)

- [ ] T007 [P] Author `lib/Dialect/NSL/IR/NSLTypes.td` — empty file with SPDX header + `include "NSLDialect.td"`. The 3 type records (`NSL_BitsType`, `NSL_StructType`, `NSL_MemType`) land in Phase 3 US1 per the TDD ordering (round-trip fixtures gate the type definitions).
- [ ] T008 [P] Author `lib/Dialect/NSL/IR/NSLTypes.cpp` — empty scaffolding: `#include "NSLTypes.cpp.inc"` (autogen by tablegen; will be empty until Phase 3 adds type records). SPDX header.

### Op scaffolding (data-model §2, research §4)

- [ ] T009 [P] Author `lib/Dialect/NSL/IR/NSLOps.td` — empty file with SPDX header + `include "NSLDialect.td"` + `include "NSLTypes.td"`. The 40 op records land in Phase 3 US1 (TableGen records) and Phase 4 US2 (hand-written verifier bodies in `NSLOps.cpp`).
- [ ] T010 [P] Author `lib/Dialect/NSL/IR/NSLOps.cpp` — empty scaffolding: `#include "NSLOps.cpp.inc"` (autogen by tablegen; will be empty until Phase 3). Adds an anonymous-namespace block reserved for verifier helpers (`findAncestorOfKind<T>`, `emitParentMismatch` per data-model §4) — empty at Phase 2; populated at Phase 4. SPDX header.

### CMake wiring (research §15)

- [ ] T011 Author `lib/Dialect/NSL/IR/CMakeLists.txt` — invokes `add_mlir_dialect(NSL nsl)` to generate the `.h.inc` / `.cpp.inc` artifacts; invokes `add_nsl_library(nsl-dialect SOURCES NSLDialect.cpp NSLOps.cpp NSLTypes.cpp HEADERS ${NSL_INCLUDE_DIR}/nsl/Dialect/NSL/IR/NSLDialect.h DEPENDS nsl-basic MLIRIR MLIRSupport)` per M0's macro convention. **Verify** the generated CMake target has NO link edge to `nsl-ast`, `nsl-sema`, `nsl-parse`, `nsl-lex`, or `nsl-preprocess` (M0 macro layering check + spec FR-001/FR-003 + `contracts/dialect-stability.contract.md` §8).
- [ ] T012 Edit `lib/Dialect/CMakeLists.txt` (or `lib/CMakeLists.txt` as appropriate) to descend into `Dialect/NSL/IR/` so the new sub-directory builds. Edit root `CMakeLists.txt` if a new top-level `Dialect/` directory needs adding.

### `nsl-opt` developer/test binary (research §6, contracts/nsl-opt-cli.contract.md)

- [ ] T013 [P] Author `tools/nsl-opt/main.cpp` — vanilla `MlirOptMain` style per research §6. Calls `nsl::dialect::registerNSLDialect(registry)` then `registry.insert<HW, Comb, Seq, FSM, SV>()` to load the CIRCT dialects per design §11 lines 1146–1150. Returns `mlir::asMainReturnCode(mlir::MlirOptMain(argc, argv, "NSL dialect tool\n", registry))`. ~50 lines of C++. SPDX header.
- [ ] T014 [P] Author `tools/nsl-opt/CMakeLists.txt` — adds `add_llvm_executable(nsl-opt main.cpp)` linking against `nsl-dialect`, `MLIRIR`, `MLIRSupport`, `MLIROptLib`, and the CIRCT dialect libraries (`CIRCTHW`, `CIRCTComb`, `CIRCTSeq`, `CIRCTFSM`, `CIRCTSV`). Marks the target as a developer/test tool — install rule omitted or guarded by an `NSL_INSTALL_DEV_TOOLS` cmake option (default OFF) per FR-016.
- [ ] T015 Edit `tools/CMakeLists.txt` to descend into `nsl-opt/`.

### Driver dialect-load + stub bodies (research §7, FR-004)

- [ ] T016 Edit `lib/Driver/Compilation.cpp` — add the line `mlirCtx_.loadDialect<nsl::dialect::NSLDialect>();` to the `Compilation` constructor's body, after the existing CIRCT-dialect-load lines per design §11 line 1145. **Verify** `Compilation`'s constructor remains the only place where `mlirCtx_` is mutated at construction time.
- [ ] T017 [P] Author `lib/Driver/LowerToNSL.cpp` — implements `Compilation::lowerToNSL(ast::CompilationUnit&, sema::SemaResult&) -> mlir::OwningOpRef<mlir::ModuleOp>` as a trivial diagnostic stub: emits `Severity::Fatal` "MLIR lowering not yet implemented; see M5" via `diag_.emit(...)` and returns `{}`. SPDX header. Per FR-004 + research §7.
- [ ] T018 [P] Author `lib/Driver/RunNSLPasses.cpp` — implements `Compilation::runNSLPasses(mlir::ModuleOp) -> mlir::LogicalResult` as a parallel stub: emits the same diagnostic and returns `mlir::failure()`. SPDX header. Per FR-004 + research §7.
- [ ] T019 Edit `lib/Driver/CMakeLists.txt` — add `LowerToNSL.cpp` and `RunNSLPasses.cpp` to the `nsl-driver` source list; add `nsl-dialect` to its `DEPENDS`.

### TDD: dialect-register idempotency unit test (research §1, contracts/dialect-stability.contract.md §7)

- [ ] T020 [P] TDD — author `test_unit/dialect_register_test/idempotency_test.cc`: GoogleTest fixture that constructs an `mlir::DialectRegistry`, calls `nsl::dialect::registerNSLDialect(registry)` twice, and asserts the second call is a no-op (the registry's dialect set has size 1). Run; observe FAILING (the entry-point doesn't exist or isn't idempotent yet).
- [ ] T021 Edit `test_unit/CMakeLists.txt` to register the `dialect_register_test` suite per the M1/M2/M3 helper convention.

### Sanity build green

- [ ] T022 Run `./scripts/ci.sh all` inside the dev container — expect: M4 library + binary build clean; T020 turns green (the empty registration entry-point is idempotent because the dialect's `addOperations` / `addTypes` lists are empty); existing M0+M1+M2+M3 ctest + lit corpus stays green.

### Layered-deps guard extension

- [ ] T023 [P] **DEFERRED** if `cmake/AddNSLLibrary.cmake` (M0) macro covers `nsl-dialect` per T003 — same precedent as M1 / M2 / M3 deferrals. Otherwise, edit `scripts/check_layering.py` (introduced at M2) to forbid `nsl-dialect → {nsl-ast, nsl-sema, nsl-parse, nsl-lex, nsl-preprocess}` link edges; permit `nsl-driver → nsl-dialect`.

### Coverage-guard CI script (research §9)

- [ ] T024 Author `scripts/check_dialect_coverage.py` — parses `lib/Dialect/NSL/IR/NSLOps.td` (greps `def NSL_*Op : NSL_Op<"<name>", ...>` records); for each `nsl.<name>`, asserts a `<name>_roundtrip.mlir` fixture exists under `test/Dialect/<some-category>/`; for each cell in `.specify/m4_invariant_table.json` with ≥ 1 invariant, asserts a `<name>_invalid_*.mlir` fixture exists. ~80 lines of Python 3.8+. At Phase 2 the script runs against an empty op set and passes vacuously; Phase 3 / 4 populate the op + invariant tables and the script exercises real coverage.
- [ ] T025 Author `.specify/m4_invariant_table.json` — initial empty `{ "ops": [] }`. Populated incrementally as Phase 3 / 4 add ops + invariants. The CI guard reads this; the spec FR-013 table is the human-authored ground-truth, this JSON is the machine-readable mirror updated in lock-step.
- [ ] T026 Edit `scripts/ci.sh` (or the static-checks stage) to invoke `python3 scripts/check_dialect_coverage.py` on every CI run. Per FR-021 + Constitution Principle IX stage 2 (static checks).

**Checkpoint** (target): nsl-dialect library scaffolding builds; `nsl-opt --version` runs and lists the `nsl` + 5 CIRCT dialects via `--show-dialects`; `nslc -emit=tokens` and `nslc -emit=ast` outputs are byte-identical pre/post (regression-tested by T022's `./scripts/ci.sh all`); 1 new gtest suite green; CI guard scripts wired but vacuous. Phase 3 (US1: round-trip every op via nsl-opt) work can begin.

---

## Phase 3: User Story 1 — Round-trip every `nsl.*` op via `nsl-opt` (Priority: P1) 🎯 MVP

**Goal**: Every `nsl.*` op listed in spec FR-010 (40 named ops) and every `!nsl.*` type (3 types) parses, verifies clean (with the trivial-success verifier from Phase 2 — real verifier bodies land at US2), and prints to byte-identical output through `nsl-opt`. Fixed-point round-trip property holds: `nsl-opt %s | nsl-opt - | FileCheck %s` succeeds on every fixture.

**Independent Test**: After this phase, `nsl-opt fixture.mlir` round-trips every op + every type without emitting diagnostics; `scripts/check_dialect_coverage.py` confirms a `<op>_roundtrip.mlir` fixture exists for each registered op. Independent of US2 (verifier rejection — those fixtures don't exist yet at the end of US1) and US3 (driver invariant — that's a regression check).

### Tests for User Story 1 (MANDATORY per Constitution Principle VIII) ⚠️

> Write these tests FIRST. They MUST be observed FAILING against the unchanged tree before any implementation task in this story begins. (Failing-state: TableGen records for ops + types don't exist yet; round-trip fixtures fail to parse — `nsl-opt: error: unknown operation 'nsl.module'`.)

#### Type round-trip fixtures (3 fixtures; all [P])

- [ ] T027 [P] [US1] Author `test/Dialect/Types/bits_roundtrip.mlir` — exercises `!nsl.bits<N>` for N ∈ {1, 8, 16, 64} on representative ops (e.g., `nsl.wire "w" : !nsl.bits<8>`). lit run-line: `// RUN: nsl-opt %s | FileCheck %s` + `// RUN: nsl-opt %s | nsl-opt - | FileCheck %s` per FR-017's two-pass clause. CHECK lines assert the type form round-trips byte-identically.
- [ ] T028 [P] [US1] Author `test/Dialect/Types/struct_roundtrip.mlir` — exercises `!nsl.struct<@T>` referring to a sibling `nsl.struct @T { ... }`. Two-pass round-trip.
- [ ] T029 [P] [US1] Author `test/Dialect/Types/mem_roundtrip.mlir` — exercises `!nsl.mem<[D x T]>` for representative shapes (e.g., `!nsl.mem<[256 x !nsl.bits<8>]>`, `!nsl.mem<[16 x !nsl.struct<@MyStruct>]>`). Two-pass round-trip.

#### Op round-trip fixtures (40 fixtures; all [P]; bulk authoring)

- [ ] T030 [P] [US1] Author `test/Dialect/module-level/module_roundtrip.mlir` — `nsl.module @M { ... }` empty + populated forms.
- [ ] T031 [P] [US1] Author `test/Dialect/module-level/struct_roundtrip.mlir` — `nsl.struct @MyStruct { ... }` with multiple fields.
- [ ] T032 [P] [US1] Author `test/Dialect/module-level/submodule_roundtrip.mlir` — `nsl.submodule @Inst : @Template`.
- [ ] T033 [P] [US1] Author `test/Dialect/module-level/connect_roundtrip.mlir` — `nsl.connect %sub.port, %sig`.
- [ ] T034 [P] [US1] Author `test/Dialect/storage/reg_roundtrip.mlir` — `nsl.reg "q" : !nsl.bits<8> = 0` with init attribute.
- [ ] T035 [P] [US1] Author `test/Dialect/storage/wire_roundtrip.mlir` — `nsl.wire "w" : !nsl.bits<8>`.
- [ ] T036 [P] [US1] Author `test/Dialect/storage/variable_roundtrip.mlir` — `nsl.variable "v" : !nsl.bits<8>`.
- [ ] T037 [P] [US1] Author `test/Dialect/storage/mem_roundtrip.mlir` — `nsl.mem "m" : !nsl.mem<[256 x !nsl.bits<8>]>`.
- [ ] T038 [P] [US1] Author `test/Dialect/control-terminal/func_in_roundtrip.mlir` — `nsl.func_in "do"(...) : !nsl.bits<8>`.
- [ ] T039 [P] [US1] Author `test/Dialect/control-terminal/func_out_roundtrip.mlir` — `nsl.func_out "done"(...)`.
- [ ] T040 [P] [US1] Author `test/Dialect/control-terminal/func_self_roundtrip.mlir` — `nsl.func_self "fire"(...)`.
- [ ] T041 [P] [US1] Author `test/Dialect/action-block/alt_roundtrip.mlir` — `nsl.alt { nsl.case %c1 { ... } nsl.default { ... } }`.
- [ ] T042 [P] [US1] Author `test/Dialect/action-block/any_roundtrip.mlir` — `nsl.any { nsl.case %c1 { ... } nsl.default { ... } }`.
- [ ] T043 [P] [US1] Author `test/Dialect/action-block/if_roundtrip.mlir` — `nsl.if %c { ... } else { ... }`.
- [ ] T044 [P] [US1] Author `test/Dialect/action-block/parallel_roundtrip.mlir` — `nsl.parallel { ... }`.
- [ ] T045 [P] [US1] Author `test/Dialect/action-block/seq_roundtrip.mlir` — `nsl.seq { ... }` inside `nsl.func`.
- [ ] T046 [P] [US1] Author `test/Dialect/action-block/while_roundtrip.mlir` — `nsl.while %c { ... }` inside `nsl.seq`.
- [ ] T047 [P] [US1] Author `test/Dialect/action-block/for_roundtrip.mlir` — `nsl.for %init, %cond, %step { ... }` inside `nsl.seq`.
- [ ] T048 [P] [US1] Author `test/Dialect/action-helper/case_roundtrip.mlir` — `nsl.case %c { ... }` inside `nsl.alt` and `nsl.any` (two variant fixtures or one with both).
- [ ] T049 [P] [US1] Author `test/Dialect/action-helper/default_roundtrip.mlir` — `nsl.default { ... }`.
- [ ] T050 [P] [US1] Author `test/Dialect/atomic/transfer_roundtrip.mlir` — `nsl.transfer %dst, %src`.
- [ ] T051 [P] [US1] Author `test/Dialect/atomic/clocked_transfer_roundtrip.mlir` — `nsl.clocked_transfer %reg, %src`.
- [ ] T052 [P] [US1] Author `test/Dialect/atomic/incdec_roundtrip.mlir` — `nsl.incdec %reg { kind = pre_inc }` for the kind-enum variants.
- [ ] T053 [P] [US1] Author `test/Dialect/atomic/call_roundtrip.mlir` — `nsl.call @target(%a, %b)`.
- [ ] T054 [P] [US1] Author `test/Dialect/atomic/finish_roundtrip.mlir` — `nsl.finish` inside `nsl.proc`.
- [ ] T055 [P] [US1] Author `test/Dialect/atomic/finish_method_roundtrip.mlir` — `nsl.finish_method @procInst`.
- [ ] T056 [P] [US1] Author `test/Dialect/atomic/invoke_method_roundtrip.mlir` — `nsl.invoke_method @procInst(%a)`.
- [ ] T057 [P] [US1] Author `test/Dialect/procedure/proc_roundtrip.mlir` — `nsl.proc @P { ... }` with first_state + states.
- [ ] T058 [P] [US1] Author `test/Dialect/procedure/first_state_roundtrip.mlir` — `nsl.first_state @s0` inside `nsl.proc`.
- [ ] T059 [P] [US1] Author `test/Dialect/procedure/state_roundtrip.mlir` — `nsl.state @s0 { nsl.goto @s1 }`.
- [ ] T060 [P] [US1] Author `test/Dialect/procedure/func_roundtrip.mlir` — `nsl.func @scopedName { ... }`; cover both bare and dotted-name (`@ic.ready`) forms.
- [ ] T061 [P] [US1] Author `test/Dialect/procedure-helper/goto_roundtrip.mlir` — `nsl.goto @target` inside `nsl.seq` (label form) and inside `nsl.state` (state form).
- [ ] T062 [P] [US1] Author `test/Dialect/system-task/sim_display_roundtrip.mlir` — `nsl.sim_display "fmt", %args`.
- [ ] T063 [P] [US1] Author `test/Dialect/system-task/sim_finish_roundtrip.mlir` — `nsl.sim_finish "fmt", %args`.
- [ ] T064 [P] [US1] Author `test/Dialect/system-task/sim_init_roundtrip.mlir` — `nsl.sim_init { nsl.sim_delay 10 }`.
- [ ] T065 [P] [US1] Author `test/Dialect/system-task/sim_delay_roundtrip.mlir` — standalone `nsl.sim_delay 10`.
- [ ] T066 [P] [US1] Author `test/Dialect/marker/fire_probe_roundtrip.mlir` — `nsl.fire_probe @controlTerminal`.
- [ ] T067 [P] [US1] Author `test/Dialect/marker/struct_cast_roundtrip.mlir` — `nsl.struct_cast %v : @T`.
- [ ] T068 [P] [US1] Author `test/Dialect/marker/field_roundtrip.mlir` — `nsl.field @member`.
- [ ] T069 [P] [US1] Author `test/Dialect/expansion-only/structural_generate_roundtrip.mlir` — `nsl.structural_generate { ... }` with loop-bound attrs.

### Implementation — Type records (3 records; all [P])

- [ ] T070 [P] [US1] Add `def NSL_BitsType : NSL_Type<"BitsType", "bits"> { ... }` to `lib/Dialect/NSL/IR/NSLTypes.td` with `unsigned width` parameter per data-model §3. T027 turns green.
- [ ] T071 [P] [US1] Add `def NSL_StructType : NSL_Type<"StructType", "struct"> { ... }` with `mlir::SymbolRefAttr name` parameter. T028 turns green.
- [ ] T072 [P] [US1] Add `def NSL_MemType : NSL_Type<"MemType", "mem"> { ... }` with `unsigned depth` + `mlir::Type elementType` parameters. T029 turns green.

### Implementation — Op records (40 records; bulk authoring; all [P] within a category, sequentially across categories)

- [ ] T073 [P] [US1] Add module-level op records (`NSL_ModuleOp`, `NSL_StructOp`, `NSL_SubmoduleOp`, `NSL_ConnectOp`) to `lib/Dialect/NSL/IR/NSLOps.td` with the trait sets per data-model §2.1. Empty `verify()` bodies (return success at this phase). T030–T033 turn green.
- [ ] T074 [P] [US1] Add storage op records (`NSL_RegOp`, `NSL_WireOp`, `NSL_VariableOp`, `NSL_MemOp`) per data-model §2.2. Empty `verify()` bodies. T034–T037 turn green.
- [ ] T075 [P] [US1] Add control-terminal op records (`NSL_FuncInOp`, `NSL_FuncOutOp`, `NSL_FuncSelfOp`) per data-model §2.3. T038–T040 turn green.
- [ ] T076 [P] [US1] Add action-block op records (`NSL_AltOp`, `NSL_AnyOp`, `NSL_IfOp`, `NSL_ParallelOp`, `NSL_SeqOp`, `NSL_WhileOp`, `NSL_ForOp`) per data-model §2.4. Empty `verify()` bodies. T041–T047 turn green.
- [ ] T077 [P] [US1] Add action-helper op records (`NSL_CaseOp`, `NSL_DefaultOp`) per data-model §2.5. T048–T049 turn green.
- [ ] T078 [P] [US1] Add atomic op records (`NSL_TransferOp`, `NSL_ClockedTransferOp`, `NSL_IncDecOp`, `NSL_CallOp`, `NSL_FinishOp`, `NSL_FinishMethodOp`, `NSL_InvokeMethodOp`) per data-model §2.6. T050–T056 turn green.
- [ ] T079 [P] [US1] Add procedure op records (`NSL_ProcOp`, `NSL_FirstStateOp`, `NSL_StateOp`, `NSL_FuncOp`) per data-model §2.7. T057–T060 turn green.
- [ ] T080 [P] [US1] Add procedure-helper op record (`NSL_GotoOp`) per data-model §2.8. T061 turns green.
- [ ] T081 [P] [US1] Add system-task op records (`NSL_SimDisplayOp`, `NSL_SimFinishOp`, `NSL_SimInitOp`, `NSL_SimDelayOp`) per data-model §2.9. T062–T065 turn green.
- [ ] T082 [P] [US1] Add marker op records (`NSL_FireProbeOp`, `NSL_StructCastOp`, `NSL_FieldOp`) per data-model §2.10. T066–T068 turn green.
- [ ] T083 [P] [US1] Add expansion-only op record (`NSL_StructuralGenerateOp`) per data-model §2.11. T069 turns green.

### Dialect registration update

- [ ] T084 [US1] Edit `lib/Dialect/NSL/IR/NSLDialect.cpp` — extend `NSLDialect::initialize()` body to call `addOperations<...>()` over all 40 ops (plus auto-generated terminators) and `addTypes<...>()` over all 3 types via the TableGen-generated `#define GET_OP_LIST` / `#define GET_TYPEDEF_LIST` macros. After this lands, `nsl-opt --show-dialects --dialect=nsl` lists the full op + type set.

### Coverage-guard data update

- [ ] T085 [US1] Update `.specify/m4_invariant_table.json` — populate the `ops` array with the 40 op entries (each with `name` + `category`; `invariants` arrays remain empty until US2). Sync with FR-010's table mechanically (see `scripts/check_dialect_coverage.py` syntax). The CI guard now exercises real op-coverage assertions.

### Verification

- [ ] T086 [US1] Run `./scripts/ci.sh all` — expect: 40 round-trip op fixtures pass + 3 type round-trip fixtures pass; `scripts/check_dialect_coverage.py` confirms paired `<op>_roundtrip.mlir` exists for every registered op (43 lit fixtures total under `test/Dialect/`); the dialect-register idempotency unit test stays green; existing M0–M3 corpus stays green. Per FR-017, FR-018, FR-021, SC-001, SC-003, SC-004.

**Checkpoint** (target): User Story 1 fully functional. `nsl-opt fixture.mlir` round-trips every op and every type. The dialect surface is structurally complete (40 ops + 3 types + 2 auto-terminators); only the verifier bodies (US2) and driver-invariant verification (US3) remain. **MVP deliverable**: contributors and M5 implementors can author and inspect `.mlir` IR using the full dialect at this point.

---

## Phase 4: User Story 2 — Verifier rejects malformed `nsl.*` ops with source-locating diagnostics (Priority: P1)

**Goal**: Every structural invariant enumerated in spec FR-013 is enforced by a verifier body — TableGen-trait-only for the bulk (~25 ops) or hand-written `verify()` body for the ~15 ops with non-trivial structural rules. Per Q2 Option B, the ~5 transitive-parent ops use a hand-written ancestor-walk via `findAncestorOfKind<T>`; the rest use TableGen `HasParent<X>` directly. Diagnostic message text follows MLIR's `op->emitOpError(...)` convention (substring-matched in fixtures per Q1 carve-out).

**Independent Test**: After this phase, every cell in FR-013 with ≥ 1 invariant has at least one `<op>_invalid_<reason>.mlir` fixture under `test/Dialect/<category>/` (~50 fixtures total). Each fixture's `// expected-error{{<substring>}}` matches the verifier's emitted diagnostic. `scripts/check_dialect_coverage.py` confirms paired-fixture existence per FR-021. US1's round-trip fixtures stay green (the new verifier bodies don't reject well-formed input).

### Tests for User Story 2 (MANDATORY per Constitution Principle VIII) ⚠️

> Write these tests FIRST. They MUST be observed FAILING against the unchanged tree before any implementation task in this story begins. (Failing-state: verifier bodies are empty stubs from US1; malformed input parses + verifies clean — no diagnostic emitted; `// expected-error` annotations are unmet.)

#### Bulk invalid-fixture authoring (~50 fixtures; all [P])

- [ ] T087 [P] [US2] Author module-level invalid fixtures: `test/Dialect/module-level/{module_invalid_nested.mlir, module_invalid_no_symname.mlir, struct_invalid_no_symname.mlir, struct_invalid_circular_field.mlir, submodule_invalid_wrong_parent.mlir, connect_invalid_type_mismatch.mlir}`. (`submodule_invalid_wrong_parent.mlir` was added per `/speckit-analyze` finding F3 — the FR-013 row for `nsl.submodule` carries `parent = nsl.module` as a structural invariant per FR-019.) Each uses `// RUN: nsl-opt --verify-diagnostics %s` + `// expected-error@+1 {{<op-name> + invariant-shape substring}}`.
- [ ] T088 [P] [US2] Author storage invalid fixtures: `test/Dialect/storage/{reg_invalid_wrong_parent.mlir, reg_invalid_bad_result_type.mlir, wire_invalid_wrong_parent.mlir, wire_invalid_bad_result_type.mlir, variable_invalid_wrong_parent.mlir, variable_invalid_bad_result_type.mlir, mem_invalid_wrong_parent.mlir, mem_invalid_bad_result_type.mlir}`.
- [ ] T089 [P] [US2] Author control-terminal invalid fixtures: `test/Dialect/control-terminal/{func_in_invalid_wrong_parent.mlir, func_out_invalid_wrong_parent.mlir, func_self_invalid_wrong_parent.mlir}`.
- [ ] T090 [P] [US2] Author action-block invalid fixtures: `test/Dialect/action-block/{alt_invalid_empty.mlir, any_invalid_empty.mlir, if_invalid_wrong_region_count.mlir, parallel_invalid_wrong_region_count.mlir, seq_invalid_wrong_parent.mlir, while_invalid_not_inside_seq.mlir, for_invalid_not_inside_seq.mlir, for_invalid_bad_loop_attrs.mlir}` (covering Q2 Option B transitive-parent ops via the `must be enclosed by 'nsl.seq'` substring; `if_*` and `parallel_*` were added per `/speckit-analyze` findings F6 and F7 — FR-013 specifies "two regions" for `nsl.if` and "one region" for `nsl.parallel` as structural invariants per FR-019).
- [ ] T091 [P] [US2] Author action-helper invalid fixtures: `test/Dialect/action-helper/{case_invalid_wrong_parent.mlir, default_invalid_wrong_parent.mlir}`.
- [ ] T092 [P] [US2] Author atomic invalid fixtures: `test/Dialect/atomic/{transfer_invalid_type_mismatch.mlir, clocked_transfer_invalid_dst_not_reg.mlir, clocked_transfer_invalid_type_mismatch.mlir, incdec_invalid_dst_not_reg.mlir, incdec_invalid_bad_kind.mlir, call_invalid_arg_count.mlir, finish_invalid_outside_proc.mlir, finish_method_invalid_no_symref.mlir, invoke_method_invalid_no_symref.mlir}` — covering Q2 Option B's `nsl.finish` transitive-parent rule.
- [ ] T093 [P] [US2] Author procedure invalid fixtures: `test/Dialect/procedure/{proc_invalid_no_symname.mlir, proc_invalid_two_first_states.mlir, first_state_invalid_outside_proc.mlir, first_state_invalid_no_target.mlir, state_invalid_outside_proc.mlir, state_invalid_no_symname.mlir, func_invalid_no_symname.mlir}`.
- [ ] T094 [P] [US2] Author procedure-helper invalid fixtures: `test/Dialect/procedure-helper/{goto_invalid_not_in_seq_or_state.mlir, goto_invalid_bad_target.mlir}` — covering Q2 Option B's transitive-parent rule with two-kind targets.
- [ ] T095 [P] [US2] Author system-task invalid fixtures: `test/Dialect/system-task/{sim_display_invalid_wrong_parent.mlir, sim_finish_invalid_wrong_parent.mlir, sim_init_invalid_wrong_parent.mlir, sim_delay_invalid_wrong_parent.mlir}`.
- [ ] T096 [P] [US2] Author marker invalid fixtures: `test/Dialect/marker/{fire_probe_invalid_bad_target.mlir, struct_cast_invalid_type_mismatch.mlir, field_invalid_bad_index.mlir, field_invalid_type_mismatch.mlir}`.
- [ ] T097 [P] [US2] Author expansion-only invalid fixture: `test/Dialect/expansion-only/structural_generate_invalid_bad_loop_attrs.mlir`.

### Implementation — Verifier helpers (research §4, data-model §4)

- [ ] T098 [US2] Add to `lib/Dialect/NSL/IR/NSLOps.cpp` (anonymous namespace at top of file): `template<typename T> T findAncestorOfKind(mlir::Operation *op);` — walks `op->getParentOp()` upward; returns first `T*` ancestor or `nullptr`. Also `mlir::LogicalResult emitParentMismatch(mlir::Operation *op, llvm::StringRef expectedKind);` — emits `op->emitOpError("must be enclosed by 'nsl." + expectedKind + "'")` and returns `failure()`. Also `bool isRegLikeValue(mlir::Value v);` — returns true if `v` is the result of an `nsl.reg` op or an `nsl.field` of a reg-typed struct.

### Implementation — TableGen-trait-only verifiers (~25 ops; bulk update; all [P])

- [ ] T099 [P] [US2] Update op records in `lib/Dialect/NSL/IR/NSLOps.td` to add `HasParent<X>` traits and `SameOperandsElementType` / `SameOperandsShape` traits per data-model §2's "Verifier style" column for ops marked "TableGen-trait only" (~25 ops: `NSL_SubmoduleOp`, `NSL_FuncInOp`/`FuncOutOp`/`FuncSelfOp`, `NSL_IfOp`, `NSL_ParallelOp`, `NSL_SeqOp`, `NSL_CaseOp`, `NSL_DefaultOp`, `NSL_TransferOp`, `NSL_FinishMethodOp`, `NSL_InvokeMethodOp`, `NSL_StateOp`, `NSL_FuncOp`, `NSL_SimDisplayOp`/`SimFinishOp`/`SimInitOp`/`SimDelayOp`, etc.). The fixture-author's `// expected-error` substring matches whatever MLIR's standard trait diagnostic emits. T087–T097 fixtures matching trait-only ops turn green.

### Implementation — Hand-written verifier bodies (~15 ops; bulk update; all [P])

- [ ] T100 [P] [US2] Implement `LogicalResult ModuleOp::verify();` in `lib/Dialect/NSL/IR/NSLOps.cpp` — checks `sym_name` presence + struct-field non-circularity. Module-level invalid fixtures matching turn green.
- [ ] T101 [P] [US2] Implement `LogicalResult StructOp::verify();` — checks `sym_name` + field-list non-circular.
- [ ] T102 [P] [US2] Implement `LogicalResult ConnectOp::verify();` — checks operand types match.
- [ ] T103 [P] [US2] Implement `LogicalResult RegOp::verify();` — checks result type is `BitsType` or `StructType`.
- [ ] T104 [P] [US2] Implement `LogicalResult WireOp::verify();` — checks result type is `BitsType`.
- [ ] T105 [P] [US2] Implement `LogicalResult VariableOp::verify();` — checks result type is `BitsType`.
- [ ] T106 [P] [US2] Implement `LogicalResult MemOp::verify();` — checks result type is `MemType`.
- [ ] T107 [P] [US2] Implement `LogicalResult AltOp::verify();` and `AnyOp::verify();` — check ≥ 1 case-or-default child; child kinds ∈ {Case, Default}.
- [ ] T108 [P] [US2] Implement `LogicalResult WhileOp::verify();` and `ForOp::verify();` — use `findAncestorOfKind<SeqOp>` per Q2 Option B; emit `must be enclosed by 'nsl.seq'` on failure. `ForOp` also checks loop-bound-attrs shape.
- [ ] T109 [P] [US2] Implement `LogicalResult ClockedTransferOp::verify();` — checks first operand `isRegLikeValue` + type-match.
- [ ] T110 [P] [US2] Implement `LogicalResult IncDecOp::verify();` — checks first operand `isRegLikeValue` + kind-enum valid.
- [ ] T111 [P] [US2] Implement `LogicalResult CallOp::verify();` — checks symbol ref present + arg count matches resolved control-terminal.
- [ ] T112 [P] [US2] Implement `LogicalResult FinishOp::verify();` — uses `findAncestorOfKind<ProcOp>` per Q2 Option B.
- [ ] T113 [P] [US2] Implement `LogicalResult ProcOp::verify();` — checks `sym_name` + at most one `FirstStateOp` child.
- [ ] T114 [P] [US2] Implement `LogicalResult FirstStateOp::verify();` — checks symbol ref resolves to a sibling `StateOp`.
- [ ] T115 [P] [US2] Implement `LogicalResult GotoOp::verify();` — uses `findAncestorOfKind<SeqOp>` (label form) OR `findAncestorOfKind<StateOp>` (state form) per Q2 Option B; checks symbol ref resolves to a sibling label op or `StateOp`.
- [ ] T116 [P] [US2] Implement `LogicalResult FireProbeOp::verify();` — checks symbol ref resolves to a sibling `FuncInOp` / `FuncOutOp` / `FuncSelfOp`.
- [ ] T117 [P] [US2] Implement `LogicalResult StructCastOp::verify();` and `FieldOp::verify();` — checks operand-result type match; `FieldOp` checks integer attr is in field-index range and result type matches struct field type.
- [ ] T118 [P] [US2] Implement `LogicalResult StructuralGenerateOp::verify();` — checks loop-bound-attr shape.

### Coverage-guard data update

- [ ] T119 [US2] Update `.specify/m4_invariant_table.json` — populate each op entry's `invariants` array with the structural invariants from FR-013. The CI guard now exercises real per-invariant fixture-existence assertions.

### Verification

- [ ] T120 [US2] Run `./scripts/ci.sh all` — expect: 40 round-trip op fixtures + 3 type round-trip fixtures + ~50 invalid fixtures all pass; `scripts/check_dialect_coverage.py` confirms paired round-trip + invalid fixtures per FR-021; verifier diagnostic format matches `^[^:]+:\d+:\d+: error: 'nsl\.[a-z_]+' op .+$` regex per SC-005; existing M0–M3 corpus stays green.

**Checkpoint** (target): User Story 2 fully functional. Every structural invariant in FR-013 is enforced; ~50 invalid fixtures exercise the full verifier surface; the architectural seam from Sema (semantic) to dialect (structural) holds. The dialect is now ready for M5's AST→MLIR builder to consume — the AST-built MLIR will arrive Sema-clean by construction and pass through the dialect's structural verifiers without diagnostic.

---

## Phase 5: User Story 3 — Driver and `-emit=*` surface from M0–M3 unchanged (Priority: P2)

**Goal**: `nslc -emit=tokens` and `nslc -emit=ast` outputs are byte-identical pre/post-M4 across the M1+M2+M3 fixture corpus; `nslc -emit=mlir` is rejected at the CLI parser; the layered-deps invariant holds (`nsl-dialect` does not depend on lower layers); the `--version` string MAY change to reflect MLIR/CIRCT pins but no other CLI behavior changes.

**Independent Test**: Run `nslc -emit=tokens` and `nslc -emit=ast` on the M1+M2+M3 lit corpus; diff stdout/stderr against pre-M4 captures (or against the in-tree golden fixtures, which haven't changed since M3). All match. The `nslc --help` output lists `tokens` and `ast` (NOT `mlir`/`hw`/`verilog`); per FR-024.

### Tests for User Story 3 (MANDATORY per Constitution Principle VIII) ⚠️

- [ ] T121 [P] [US3] Author `test/driver/m4_no_regression.test` — runs `nslc -emit=tokens` and `nslc -emit=ast` on representative inputs from `test/lex/`, `test/parse/`, `test/sema/` and asserts the M3-era goldens still match. (Most of this is implicit in the existing M0–M3 lit corpus passing on every CI run; this fixture is a focused smoke test that the M4 patch hasn't perturbed driver behavior.)
- [ ] T122 [P] [US3] Author `test/driver/m4_emit_help.test` — runs `nslc --help` and asserts the `-emit=*` choices listed are exactly `tokens` or `ast`. The `// CHECK-NOT: mlir` / `CHECK-NOT: hw` / `CHECK-NOT: verilog` lines lock down the FR-024 rule.
- [ ] T123 [P] [US3] Author `test/driver/m4_emit_mlir_rejected.test` — runs `nslc -emit=mlir foo.nsl` and asserts the CLI parser exits non-zero with a clear "invalid -emit= choice 'mlir'" message; per FR-023.

### Verification

- [ ] T124 [US3] Run `./scripts/ci.sh all` — expect: T121–T123 pass; existing M0–M3 corpus stays green (no regression in `nslc -emit=tokens` / `-emit=ast` outputs). The layered-deps guard (T023 / M0 macro) confirms `nsl-dialect`'s dependency direction. SC-007 + FR-022 satisfied.

**Checkpoint** (target): User Story 3 verified. Driver behavior unchanged at M4; the `-emit=*` surface is exactly `{tokens, ast}`; the dialect is loaded into `mlir::MLIRContext` but unreachable from the public CLI; `nsl-dialect` does not depend on lower layers. Forward-compatibility surface (`Compilation::lowerToNSL` / `runNSLPasses` declarations) is in place for M5 to fill in.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Documentation, design-doc consolidation, and small post-implementation cleanup. Not gating; can land in the same PR as US3 or as a follow-up.

- [ ] T125 [P] Update `docs/design/nsl_compiler_design.md` §7 — add the consolidation note flagged by plan.md Constitution-VII (b): the marker / lowering-helper ops introduced in §§8–10 (`nsl.fire_probe`, `nsl.struct_cast`, `nsl.field`, `nsl.case`, `nsl.default`, `nsl.goto`, `nsl.structural_generate`) belong in §7's op summary as well; document them inline so the dialect's full op list is in one section. Per Principle VII (spec/design coupling) + spec SC-009.
- [ ] T126 [P] Update `docs/CLAUDE.md` §6 (compiler-design TOC) — refresh §7 line ranges if T125's consolidation note shifts boundaries. Per Principle VII line-range rule.
- [ ] T127 [P] Update `README.md` Building/Status — add a 5-line `nsl-opt` round-trip example matching the M3-era `nslc -emit=ast` example pattern. Reference the M4 quickstart at `specs/007-m4-mlir-dialect/quickstart.md` §3.
- [X] T128 Polish — **DONE pre-implementation 2026-04-30** in commit `e47484a` (post-`/speckit-analyze` remediation): spec.md FR-010 footer + SC-001 + SC-009 + SC-012, plan.md (5 occurrences), research.md, data-model.md (4 occurrences), quickstart.md, contracts/dialect-api.contract.md (2 occurrences) all corrected from "35 named ops"/"36th op"/"42 public types" to "40 named ops"/"41st op"/"47 public types". Per `/speckit-analyze` finding F2.
- [ ] T129 [P] Run `scripts/check_spdx.py` against `git ls-files` — expect: 100% of new files under `lib/Dialect/`, `include/nsl/Dialect/`, `tools/nsl-opt/`, `test/Dialect/`, `test_unit/dialect_register_test/`, plus the new driver source files carry the SPDX header. Per SC-010.
- [ ] T130 Final M4 acceptance: run `./scripts/ci.sh all` once more end-to-end inside the dev container — expect: all 6 stages green; ~88 dialect fixtures pass; M0–M3 corpus stays green; `--version` is unchanged or shows the MLIR/CIRCT pin update only. SC-001 through SC-012 all measured.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately on the `007-m4-mlir-dialect` branch.
- **Foundational (Phase 2)**: Depends on Setup completion — BLOCKS all user stories.
- **User Stories (Phases 3–5)**: All depend on Foundational completion.
  - US1 (P1) is the MVP and must complete before US2 (TDD red phase for verifiers requires the op records from US1).
  - US2 (P1) and US3 (P2) are independent of each other and can run in parallel.
- **Polish (Phase 6)**: Depends on all user stories complete.

### User Story Dependencies

- **US1 (Round-trip)**: Depends on Foundational. Independent of US2, US3.
- **US2 (Verifier rejection)**: Depends on Foundational + US1 (verifiers extend op records that US1 created). NOT independent of US1.
- **US3 (Driver invariant)**: Depends on Foundational. Independent of US1 and US2.

### Within Each User Story

- Tests MUST be written and observed FAILING before implementation (Constitution Principle VIII).
- TableGen records before verifier bodies (US1 lands ops, US2 lands verifier bodies on the same ops).
- Type records can land before or after op records (independent).
- Bulk-authored fixtures within a story: heavily parallel ([P]).

### Parallel Opportunities

- All tasks marked [P] within Phase 1 / Phase 2 can run in parallel.
- Within US1: T027–T029 (3 type fixtures) and T030–T069 (40 op fixtures) are all [P]; the corresponding implementation tasks T070–T083 are also [P] within their respective categories.
- Within US2: T087–T097 (~50 invalid fixtures across 11 categories) are all [P]; T100–T118 (verifier bodies) are all [P].
- Across stories (post-Foundational): US2 and US3 can run in parallel by separate developers.
- Polish phase tasks T125–T127 + T129 are all [P].

---

## Parallel Example: User Story 1

```bash
# Launch fixture authoring for all 40 op round-trip fixtures + 3 type fixtures together:
Task: "Author test/Dialect/Types/bits_roundtrip.mlir"
Task: "Author test/Dialect/Types/struct_roundtrip.mlir"
Task: "Author test/Dialect/Types/mem_roundtrip.mlir"
Task: "Author test/Dialect/module-level/module_roundtrip.mlir"
Task: "Author test/Dialect/module-level/struct_roundtrip.mlir"
# ... etc — all 43 fixtures parallel

# Once fixtures are committed (failing-state observed), author TableGen records by category in parallel:
Task: "Add module-level op records to NSLOps.td"
Task: "Add storage op records to NSLOps.td"
Task: "Add control-terminal op records to NSLOps.td"
# ... etc — 11 op categories + 3 types parallel
```

## Parallel Example: User Story 2

```bash
# Launch ~50 invalid fixture authoring across 11 categories in parallel:
Task: "Author module-level invalid fixtures (5)"
Task: "Author storage invalid fixtures (8)"
Task: "Author control-terminal invalid fixtures (3)"
# ... etc — 11 category-level batches parallel

# Once fixtures committed (failing-state observed), implement verifier bodies in parallel:
Task: "Implement ModuleOp::verify() in NSLOps.cpp"
Task: "Implement StructOp::verify() in NSLOps.cpp"
# ... etc — ~15 hand-written verifier bodies + ~25 TableGen-trait updates parallel
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational (CRITICAL — blocks all stories)
3. Complete Phase 3: User Story 1 (round-trip every op)
4. **STOP and VALIDATE**: `nsl-opt fixture.mlir` round-trips every op + every type
5. Demo if ready — M5 implementors can begin authoring AST→MLIR lowering against the round-trip-stable dialect surface

### Incremental Delivery

1. Complete Setup + Foundational → Foundation ready
2. Add User Story 1 → Test independently → Demo (MVP — dialect surface stable)
3. Add User Story 2 → Test independently → Demo (verifier hardened; fixture corpus complete)
4. Add User Story 3 → Test independently → Demo (driver-invariant verification)
5. Polish → Final M4 acceptance

### Parallel Team Strategy

With multiple developers post-Foundational:

1. Team completes Setup + Foundational together (small phase, ~1 dev-day)
2. Once Foundational is done:
   - Developer A: US1 (40 op round-trip fixtures + 40 op TableGen records + 3 types)
   - Developer B: US2 (~50 invalid fixtures + ~15 hand-written verifier bodies + ~25 trait updates) — starts after US1's TableGen records land
   - Developer C: US3 + Polish (driver-invariant verification + design-doc consolidation)
3. Stories integrate independently; Polish phase wraps the bundle.

---

## Notes

- [P] tasks = different files, no dependencies on incomplete tasks.
- [Story] label maps task to specific user story for traceability.
- Each user story should be independently completable and testable except where noted (US2 depends on US1's TableGen records being present).
- Verify tests fail before implementing (Principle VIII TDD).
- Commit after each task or logical group.
- Stop at any checkpoint to validate independently.
- Avoid: vague tasks, same-file conflicts within a [P] batch, cross-story dependencies that break independence.
- The op-count correction (35 → 40) noted in this preamble + T128 is a known spec-internal inconsistency to address in Polish; it does NOT block any user story.
