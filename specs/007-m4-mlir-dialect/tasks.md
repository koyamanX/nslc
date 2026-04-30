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

- [X] T001 Create the M4 test directory tree under `test/Dialect/` and `test_unit/`: `test/Dialect/{module-level,storage,control-terminal,action-block,action-helper,atomic,procedure,procedure-helper,system-task,marker,expansion-only,Types}/` (12 subdirs) and `test_unit/dialect_register_test/`. Each gets a `.keep` placeholder so M0's lit-discovery picks them up. **Done 2026-04-30**: 13 `.keep` files created (12 under `test/Dialect/` + 1 under `test_unit/dialect_register_test/`); existing `test/Dialect/.keep` (M0) and `test/Dialect/smoke.test` left untouched.
- [~] T002 **DEFERRED to T022 2026-04-30** — Phase 2 entry build verification rolled into T022's end-of-Phase-2 build verification (Phase 2 wraps the full M4-scaffolding build, so a separate pre-Phase-2 sanity build is redundant). Per the M3 precedent, Phase 2 entry checkpoints fold into Phase 2 exit verification when the agent has not yet introduced sources that could break the previous-milestone tree.
- [~] T003 [P] **DEFERRED 2026-04-30** — `cmake/AddNSLLibrary.cmake` (M0) already enforces downward-only layering via `_nsl_layer_index()` table; verified `nsl-dialect` is at layer index 7 (line 47, between `nsl-sema` at 6 and `nsl-driver` at 9). The macro's existing FATAL_ERROR on upward edges (line 89: "index M ≥ N") covers `nsl-dialect` → forbidden-as-upstream-of-`nsl-ast`/`nsl-sema`/`nsl-parse`/`nsl-lex`/`nsl-preprocess` automatically. SC-011 obligation satisfied by the M0 macro's configure-time enforcement; no script extension needed (parallel to M1/M2/M3's T003 DEFERRAL precedent).

**Checkpoint**: M4 directory skeleton in place; build sanity exercised; layering guard verified. Phase 2 work can begin.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: `nsl-dialect` library scaffolding — the TableGen dialect class + base op/type records (no ops yet), the umbrella public header, the dialect-init code, the empty NSLOps.cpp / NSLTypes.cpp scaffolding, the `nsl-opt` developer/test binary, and the driver dialect-load + stub-body wiring. Every user story depends on these.

**⚠️ CRITICAL**: No US1 / US2 / US3 task may begin until this phase is complete.

### Dialect class TableGen + umbrella header (research §3, data-model §1)

- [X] T004 [P] **Done 2026-04-30**: `lib/Dialect/NSL/IR/NSLDialect.td` declares `def NSL_Dialect : Dialect` (name="nsl", cppNamespace="::nsl::dialect", useDefaultTypePrinterParser=1) plus `class NSL_Op<...>` and `class NSL_Type<...>` bases for Phase 3 ops/types to inherit from. SPDX header uses `//` line-comments to match `scripts/check_spdx.py`'s `.td` recipe (the spec's "C-style `/* … */` comment block" wording was loose; the working SPDX checker enforces `//` for `.td`).
- [X] T005 [P] **Done 2026-04-30**: `include/nsl/Dialect/NSL/IR/NSLDialect.h` declares `nsl::dialect::registerNSLDialect(mlir::DialectRegistry&)` per FR-006 + dialect-api.contract.md §1+§3. Includes the TableGen-generated `NSLOpsDialect.h.inc` via bare-basename (the dialect's CMakeLists adds `${CMAKE_CURRENT_BINARY_DIR}` to the PUBLIC include path so consumers resolve it transitively).
- [X] T006 **Done 2026-04-30**: `lib/Dialect/NSL/IR/NSLDialect.cpp` implements `registerNSLDialect(registry)` as `registry.insert<NSLDialect>()` (idempotent by TypeID per upstream MLIR contract; verified by T020). `NSLDialect::initialize()` is empty at Phase 2; Phase 3 (T084) populates `addOperations<>` / `addTypes<>`.

### Type scaffolding (data-model §3, research §5)

- [X] T007 [P] **Done 2026-04-30**: `lib/Dialect/NSL/IR/NSLTypes.td` carries SPDX + `include "NSLDialect.td"` + comment markers anchoring Phase 3 T070–T072 (NSL_BitsType, NSL_StructType, NSL_MemType).
- [X] T008 [P] **Done 2026-04-30**: `lib/Dialect/NSL/IR/NSLTypes.cpp` carries SPDX + `#include "nsl/Dialect/NSL/IR/NSLDialect.h"` + commented-out `#define GET_TYPEDEF_CLASSES` / `#include "NSLOpsTypes.cpp.inc"` block ready to uncomment in Phase 3 (T070–T072).

### Op scaffolding (data-model §2, research §4)

- [X] T009 [P] **Done 2026-04-30**: `lib/Dialect/NSL/IR/NSLOps.td` carries SPDX + `include "NSLDialect.td"` + `include "NSLTypes.td"` + per-category Phase 3 task-anchor comments (T073–T083). This file is the `add_mlir_dialect`'s designated dialect-file argument (`add_mlir_dialect(NSLOps nsl)`).
- [X] T010 [P] **Done 2026-04-30**: `lib/Dialect/NSL/IR/NSLOps.cpp` carries SPDX + anonymous-namespace block reserved for Phase 4 verifier helpers + commented-out `#define GET_OP_CLASSES` / `#include "NSLOps.cpp.inc"` block. **F9 carry-over note recorded inline**: Phase 4 verifier bodies MUST use the upstream MLIR helper `op->getParentOfType<T>()` rather than any hand-rolled `findAncestorOfKind<T>` (the latter was proposed in research.md §4 / data-model §4 / T098 but is redundant with upstream).

### CMake wiring (research §15)

- [X] T011 **Done 2026-04-30**: `lib/Dialect/NSL/IR/CMakeLists.txt` rewritten — invokes `add_mlir_dialect(NSLOps nsl)` to produce the 6 `.h.inc` / `.cpp.inc` artifacts (NSLOps[, Types, Dialect]). Calls `add_mlir_doc(NSLOps NSLOps Dialect/ -gen-op-doc)` for markdown-doc generation (vacuous at Phase 2). Wraps in `add_nsl_library(nsl-dialect ... DEPENDS nsl-basic LINK_LIBS MLIRIR MLIRSupport)` per M0 macro. Adds `${CMAKE_CURRENT_BINARY_DIR}` to nsl-dialect's PUBLIC include path so consumers (`nsl-opt`, `test_unit/dialect_register_test`, future M5) resolve bare-basename includes of the generated headers. `add_dependencies(nsl-dialect MLIRNSLOpsIncGen)` ensures tablegen runs before any consuming TU compiles. **Layering verified** — DEPENDS is `nsl-basic` only; LINK_LIBS is `MLIRIR` `MLIRSupport`; no edge into nsl-ast / nsl-sema / nsl-parse / nsl-lex / nsl-preprocess (AddNSLLibrary macro's layer-index check enforces).
- [X] T012 **Done 2026-04-30 (no-op)**: `lib/CMakeLists.txt` already descends into `Dialect/NSL/IR/` (M0 wiring at line 20 — `add_subdirectory(Dialect/NSL/IR)`). No new top-level directory needed; the existing M0 scaffolding handles the descent.

### `nsl-opt` developer/test binary (research §6, contracts/nsl-opt-cli.contract.md)

- [X] T013 [P] **Done 2026-04-30**: `tools/nsl-opt/main.cpp` (~50 lines) constructs a `mlir::DialectRegistry`, calls `nsl::dialect::registerNSLDialect(registry)`, then `registry.insert<circt::hw::HWDialect, circt::comb::CombDialect, circt::seq::SeqDialect, circt::fsm::FSMDialect, circt::sv::SVDialect>()`, returns `mlir::asMainReturnCode(mlir::MlirOptMain(argc, argv, "NSL dialect tool\n", registry))`. Zero passes registered (per FR-015). SPDX header.
- [X] T014 [P] **Done 2026-04-30**: `tools/nsl-opt/CMakeLists.txt` declares `add_executable(nsl-opt main.cpp)` linking nsl-dialect + MLIRIR + MLIRSupport + MLIROptLib + the 5 CIRCT dialect libs (CIRCTHW, CIRCTComb, CIRCTSeq, CIRCTFSM, CIRCTSV). Output redirected to `${CMAKE_BINARY_DIR}/bin/` (alongside `nslc`). Install rule guarded by `NSL_INSTALL_DEV_TOOLS` option (default OFF) per FR-016 (release tarballs do not bundle nsl-opt by default).
- [X] T015 **Done 2026-04-30**: `tools/CMakeLists.txt` extended with `add_subdirectory(nsl-opt)` after the existing `nslc` subdir descent. Comment block updated to mark `nsl-opt` as the M4 developer/test binary.

### Driver dialect-load + stub bodies (research §7, FR-004)

- [X] T016 **Done 2026-04-30 (created, not edited)**: The pre-M4 driver had no `Compilation` class — `lib/Driver/{EmitTokens,EmitAST,Sema}.cpp` use free functions only. Created `include/nsl/Driver/Compilation.h` (a minimal `Compilation` class scaffold per data-model §5: `DiagnosticEngine&` + `mlir::MLIRContext`) and `lib/Driver/Compilation.cpp` (constructor body that calls `mlir_ctx_.loadDialect<nsl::dialect::NSLDialect>()` per design §11 line 1145). At M4 the class only exists for FR-004 forward-compatibility — it's never reached by the public `nslc` CLI (FR-023 rejects `-emit=mlir`). M5 will extend the class with the full per-stage pipeline + the CIRCT-dialect-load lines per design §11 lines 1146–1150 (those are loaded by `nsl-opt` only at M4, since the driver never reaches a CIRCT-emitting stage at M4).
- [X] T017 [P] **Done 2026-04-30**: `lib/Driver/LowerToNSL.cpp` implements `Compilation::lowerToNSL(ast::CompilationUnit&, sema::SemaResult&)` as a stub that calls `diag_.report(Severity::Error, SourceLocation{}, "MLIR lowering not yet implemented; see M5")` and returns `{}`. (Note: the project's `Severity` enum has no `Fatal` value — only `Note`/`Warning`/`Error`; spec/research used "Fatal" loosely. `Severity::Error` is the working substitute.)
- [X] T018 [P] **Done 2026-04-30**: `lib/Driver/RunNSLPasses.cpp` implements `Compilation::runNSLPasses(mlir::ModuleOp)` as the parallel stub: emits the same diagnostic and returns `mlir::failure()`.
- [X] T019 **Done 2026-04-30**: `lib/Driver/CMakeLists.txt` extended — added `Compilation.cpp`, `LowerToNSL.cpp`, `RunNSLPasses.cpp` to the source list and `${CMAKE_SOURCE_DIR}/include/nsl/Driver/Compilation.h` to HEADERS. The `DEPENDS` list already carried `nsl-dialect` from the M0 skeleton; comment annotation refreshed to anchor the M4 build edge to FR-004 / FR-019.

### TDD: dialect-register idempotency unit test (research §1, contracts/dialect-stability.contract.md §7)

- [X] T020 [P] **Done 2026-04-30 (TDD)**: `test_unit/dialect_register_test/idempotency_test.cc` exercises two assertions: (1) `SingleRegistrationSucceeds` — after one `registerNSLDialect(registry)` call the registry reports a `nsl` entry via `getDialectNames()`; (2) `RegistrationIsIdempotent` — calling twice on the same registry leaves `getDialectNames().size()` unchanged, satisfying contracts/dialect-stability.contract.md §7. **Authored RED** before T006's `lib/Dialect/NSL/IR/NSLDialect.cpp` lands the registration body — the translation unit fails to link until T006 ships `nsl::dialect::registerNSLDialect`.
- [X] T021 **Done 2026-04-30**: `test_unit/CMakeLists.txt` extended with `dialect_register_test` in the for-each suite list (alongside the existing M3 suites). The existing `EXISTS` guard at line 65 means the suite is harmless until both `idempotency_test.cc` AND `dialect_register_test/CMakeLists.txt` ship together (this commit). The per-suite CMakeLists declares `target_link_libraries(... nsl-dialect MLIRIR MLIRSupport GTest::gtest_main)` per the M1/M2/M3 helper convention.

### Sanity build green

- [~] T022 **DEFERRED 2026-04-30** — Phase 2 build-verification cannot be exercised from the current agent sandbox: `sg docker -c "docker run …"` requires elevation outside the sandbox, and the user must run the build manually. The expected post-Phase-2 verification is: `./scripts/ci.sh all` inside `ghcr.io/koyamanx/nsl-nslc:dev` produces all 6 stages green, `nsl-opt --version` runs, `nsl-opt --show-dialects` lists `nsl` + the 5 CIRCT dialects + `builtin`, the `dialect_register_test` gtest suite passes (turns green now that T006's `registerNSLDialect` body is idempotent because the dialect's `addOperations`/`addTypes` lists are empty), and the existing M0–M3 ctest+lit corpus stays green. **Recommended user action**: run `./scripts/ci.sh all` once before declaring Phase 2 done.

### Layered-deps guard extension

- [~] T023 [P] **DEFERRED 2026-04-30** per T003 precedent — `cmake/AddNSLLibrary.cmake` already enforces the layering rule via its `_nsl_layer_index()` table at line 47 (`nsl-dialect = 7`). The macro's FATAL_ERROR at line 105–110 ("layer X (index N) cannot depend on Y (index M ≥ N)") catches every forbidden upward edge from `nsl-dialect` into `nsl-basic`/`nsl-preprocess`/`nsl-lex`/`nsl-ast`/`nsl-parse`/`nsl-sema` configure-time, before any code compiles. Verified by inspection of the M4 patch: `lib/Dialect/NSL/IR/CMakeLists.txt`'s `add_nsl_library(nsl-dialect ... DEPENDS nsl-basic LINK_LIBS MLIRIR MLIRSupport)` exposes only the permitted edges. SC-011 satisfied by construction.

### Coverage-guard CI script (research §9)

- [X] T024 **Done 2026-04-30**: `scripts/check_dialect_coverage.py` (~190 lines incl. comments) parses `lib/Dialect/NSL/IR/NSLOps.td` via `_OP_RECORD_RE` regex; for each `nsl.<name>` asserts a `<name>_roundtrip.mlir` fixture exists under any `test/Dialect/<category>/` subdirectory; for each `.specify/m4_invariant_table.json` op entry with `invariants` ≥ 1, asserts at least one `<name>_invalid_*.mlir` fixture exists. Exits 0 (vacuous-pass when op set empty), 1 (coverage gap), 2 (script error). Made executable (`chmod +x`); SPDX header in `#` line-comment style.
- [X] T025 **Done 2026-04-30**: `.specify/m4_invariant_table.json` initial empty `{ "ops": [] }` payload with a `_doc` field anchoring the schema to data-model.md §8. Phase 3 (T085) populates the array with 40 op entries (each `name` + `category`); Phase 4 (T119) extends each entry with the `invariants` list from FR-013.
- [X] T026 **Done 2026-04-30**: `scripts/ci.sh`'s `stage_static_checks` extended with check 4 — invokes `python3 scripts/check_dialect_coverage.py --quiet` after the SPDX scan. Per FR-021 + Constitution Principle IX stage 2 (static checks). Vacuous at Phase 2; goes live as Phase 3 / 4 populate the op + invariant tables.

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
- [ ] T131 [P] Polish — fold the Phase-2 wording corrections (FU1, FU2) discovered by the `nsl-mlir-impl` agent: (a) FU1 — replace `Severity::Fatal` references in spec.md FR-004 / research.md §7 / tasks.md T017–T018 task descriptions + `lib/Driver/LowerToNSL.cpp` source comments with `Severity::Error` (the project's actual `Severity` enum is `Note < Warning < Error`; `Fatal` was a draft-era assumption). (b) FU2 — amend spec.md FR-028 wording from "TableGen `.td` files MUST carry the header in the file's leading multi-line C-style `/* … */` comment block" to "TableGen `.td` files MUST carry the header as a `//` line-comment per `scripts/check_spdx.py`'s `.td` recipe". Both are wording-only; no code changes needed.
- [X] T132 Polish — **DONE 2026-04-30** in commit `<TBD>` (FU4 spec/data-model amendment): spec.md FR-004 + scope-interpretation block + Assumptions paragraph + data-model.md §5 row for `Compilation::Compilation` all updated from "Compilation class declared in design §11 MUST gain ... at M4" / "MODIFIED at M4" to "**created at M4**". Documents that the M3 driver used free functions per `lib/Driver/EmitTokens.cpp` / `EmitAST.cpp` / `Sema.cpp` precedent; design §11's class definition was target-state, not extant code; M4 introduces the class skeleton, M5 extends it. Per `nsl-mlir-impl` agent's Phase 2 follow-up FU4.

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
