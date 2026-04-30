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

> **Op count note.** Spec FR-010 describes **41 named ops + auto-generated terminators** (post-Q6 of Session 2026-04-30 — `nsl.field_decl` added per Option B's two-op split for the `nsl.field` overload). The earlier draft-era "Total: 35" miscount was corrected pre-implementation in commit `e47484a` after `/speckit-analyze` Pass 1 flagged it as F2; the bump from 40 to 41 followed Q6's resolution. 41 ops + 3 types is the authoritative count enumerated in the FR-010 table and `data-model.md` §2.

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
- [X] T025 **Done 2026-04-30**: `.specify/m4_invariant_table.json` initial empty `{ "ops": [] }` payload with a `_doc` field anchoring the schema to data-model.md §8. Phase 3 (T085) populates the array with 41 op entries (each `name` + `category`; post-Q6 — includes `nsl.field_decl`); Phase 4 (T119) extends each entry with the `invariants` list from FR-013.
- [X] T026 **Done 2026-04-30**: `scripts/ci.sh`'s `stage_static_checks` extended with check 4 — invokes `python3 scripts/check_dialect_coverage.py --quiet` after the SPDX scan. Per FR-021 + Constitution Principle IX stage 2 (static checks). Vacuous at Phase 2; goes live as Phase 3 / 4 populate the op + invariant tables.

**Checkpoint** (target): nsl-dialect library scaffolding builds; `nsl-opt --version` runs and lists the `nsl` + 5 CIRCT dialects via `--show-dialects`; `nslc -emit=tokens` and `nslc -emit=ast` outputs are byte-identical pre/post (regression-tested by T022's `./scripts/ci.sh all`); 1 new gtest suite green; CI guard scripts wired but vacuous. Phase 3 (US1: round-trip every op via nsl-opt) work can begin.

---

## Phase 3: User Story 1 — Round-trip every `nsl.*` op via `nsl-opt` (Priority: P1) 🎯 MVP

**Goal**: Every `nsl.*` op listed in spec FR-010 (41 named ops, post-Q6) and every `!nsl.*` type (3 types) parses, verifies clean (with the trivial-success verifier from Phase 2 — real verifier bodies land at US2), and prints to byte-identical output through `nsl-opt`. Fixed-point round-trip property holds: `nsl-opt %s | nsl-opt - | FileCheck %s` succeeds on every fixture.

**Independent Test**: After this phase, `nsl-opt fixture.mlir` round-trips every op + every type without emitting diagnostics; `scripts/check_dialect_coverage.py` confirms a `<op>_roundtrip.mlir` fixture exists for each registered op. Independent of US2 (verifier rejection — those fixtures don't exist yet at the end of US1) and US3 (driver invariant — that's a regression check).

### Tests for User Story 1 (MANDATORY per Constitution Principle VIII) ⚠️

> Write these tests FIRST. They MUST be observed FAILING against the unchanged tree before any implementation task in this story begins. (Failing-state: TableGen records for ops + types don't exist yet; round-trip fixtures fail to parse — `nsl-opt: error: unknown operation 'nsl.module'`.)

#### Type round-trip fixtures (3 fixtures; all [P])

- [X] T027 [P] [US1] **Done 2026-04-30 (TDD red)**: `test/Dialect/Types/bits_roundtrip.mlir` exercises `!nsl.bits<N>` for N ∈ {1, 8, 16, 64} on `nsl.wire`. Two-pass run-lines per FR-017; CHECK-LABEL + per-width CHECKs. Red-state confirmed via `nsl-opt module_roundtrip.mlir` → `error: custom op 'nsl.module' is unknown` (Phase 4 TableGen records pending).
- [X] T028 [P] [US1] **Done 2026-04-30 (TDD red)**: `test/Dialect/Types/struct_roundtrip.mlir` — `!nsl.struct<@MyRec>` on `nsl.reg` with sibling `nsl.struct @MyRec { ... }`. Two-pass.
- [X] T029 [P] [US1] **Done 2026-04-30 (TDD red)**: `test/Dialect/Types/mem_roundtrip.mlir` — `!nsl.mem<[256 x !nsl.bits<8>]>` and `!nsl.mem<[16 x !nsl.struct<@Word>]>` on `nsl.mem`. Two-pass.

#### Op round-trip fixtures (41 fixtures, post-Q6; all [P]; bulk authoring)

- [X] T030 [P] [US1] **Done 2026-04-30 (TDD red)**: `module-level/module_roundtrip.mlir` — empty + populated `nsl.module @M { ... }` forms.
- [X] T031 [P] [US1] **Done 2026-04-30 (TDD red)**: `module-level/struct_roundtrip.mlir` — multi-field `nsl.struct @Pair / @Wide` with mixed widths.
- [X] T032 [P] [US1] **Done 2026-04-30 (TDD red)**: `module-level/submodule_roundtrip.mlir` — `nsl.submodule @u_inner : @Inner` template-ref form.
- [X] T033 [P] [US1] **Done 2026-04-30 (TDD red)**: `module-level/connect_roundtrip.mlir` — `nsl.connect %dst, %src : !nsl.bits<8>` (Q3 strict-`mlir::Type` equality; both bits<8>).
- [X] T034 [P] [US1] **Done 2026-04-30 (TDD red)**: `storage/reg_roundtrip.mlir` — bits/struct result types + init attribute variants.
- [X] T035 [P] [US1] **Done 2026-04-30 (TDD red)**: `storage/wire_roundtrip.mlir` — bits-only result type per FR-013.
- [X] T036 [P] [US1] **Done 2026-04-30 (TDD red)**: `storage/variable_roundtrip.mlir` — bits-only result type.
- [X] T037 [P] [US1] **Done 2026-04-30 (TDD red)**: `storage/mem_roundtrip.mlir` — `!nsl.mem<[256 x !nsl.bits<8>]>` + `[1024 x !nsl.bits<32>]`.
- [X] T038 [P] [US1] **Done 2026-04-30 (TDD red)**: `control-terminal/func_in_roundtrip.mlir` — `nsl.func_in "do"(...) : !nsl.bits<8>` with-ret + no-ret variants.
- [X] T039 [P] [US1] **Done 2026-04-30 (TDD red)**: `control-terminal/func_out_roundtrip.mlir` — `nsl.func_out "done"(%r)` + nullary form.
- [X] T040 [P] [US1] **Done 2026-04-30 (TDD red)**: `control-terminal/func_self_roundtrip.mlir` — `nsl.func_self "fire"(%w)`.
- [X] T041 [P] [US1] **Done 2026-04-30 (TDD red)**: `action-block/alt_roundtrip.mlir` — case+case+default children inside seq+func.
- [X] T042 [P] [US1] **Done 2026-04-30 (TDD red)**: `action-block/any_roundtrip.mlir` — same shape as alt; verifier doesn't distinguish per FR-013.
- [X] T043 [P] [US1] **Done 2026-04-30 (TDD red)**: `action-block/if_roundtrip.mlir` — two-region (then/else) with empty-else allowed per FR-013.
- [X] T044 [P] [US1] **Done 2026-04-30 (TDD red)**: `action-block/parallel_roundtrip.mlir` — single-region inside `nsl.func`.
- [X] T045 [P] [US1] **Done 2026-04-30 (TDD red)**: `action-block/seq_roundtrip.mlir` — `nsl.seq { }` immediate-parent `nsl.func` per FR-013.
- [X] T046 [P] [US1] **Done 2026-04-30 (TDD red)**: `action-block/while_roundtrip.mlir` — both immediate `seq` parent and nested `seq → parallel → while` (Q2 Option B transitive walk).
- [X] T047 [P] [US1] **Done 2026-04-30 (TDD red)**: `action-block/for_roundtrip.mlir` — C-style three-operand `nsl.for %init, %cond, %step`.
- [X] T048 [P] [US1] **Done 2026-04-30 (TDD red)**: `action-helper/case_roundtrip.mlir` — case under both alt and any (variadic HasParent).
- [X] T049 [P] [US1] **Done 2026-04-30 (TDD red)**: `action-helper/default_roundtrip.mlir` — default under both alt and any.
- [X] T050 [P] [US1] **Done 2026-04-30 (TDD red)**: `atomic/transfer_roundtrip.mlir` — `nsl.transfer %dst, %src : !nsl.bits<8>` (SameOperandsElementType + SameOperandsShape).
- [X] T051 [P] [US1] **Done 2026-04-30 (TDD red)**: `atomic/clocked_transfer_roundtrip.mlir` — reg-target + wire-source typed bits<8>.
- [X] T052 [P] [US1] **Done 2026-04-30 (TDD red)**: `atomic/incdec_roundtrip.mlir` — `pre_inc` + `post_dec` kind variants. **Phase 4 review flag**: spec doesn't pin enum-attr text form; picked `#nsl<incdec_kind pre_inc>`.
- [X] T053 [P] [US1] **Done 2026-04-30 (TDD red)**: `atomic/call_roundtrip.mlir` — bare `@target(...)` and dotted `@ic.ready(...)` per Q5 Option A'.
- [X] T054 [P] [US1] **Done 2026-04-30 (TDD red)**: `atomic/finish_roundtrip.mlir` — bare `nsl.finish` nested under `nsl.proc → nsl.state` (transitive parent per Q2).
- [X] T055 [P] [US1] **Done 2026-04-30 (TDD red)**: `atomic/finish_method_roundtrip.mlir` — `nsl.finish_method @procInst`.
- [X] T056 [P] [US1] **Done 2026-04-30 (TDD red)**: `atomic/invoke_method_roundtrip.mlir` — `nsl.invoke_method @procInst(%a)`.
- [X] T057 [P] [US1] **Done 2026-04-30 (TDD red)**: `procedure/proc_roundtrip.mlir` — proc with first_state + two states + cross-state goto.
- [X] T058 [P] [US1] **Done 2026-04-30 (TDD red)**: `procedure/first_state_roundtrip.mlir` — single first_state pointing at sibling state.
- [X] T059 [P] [US1] **Done 2026-04-30 (TDD red)**: `procedure/state_roundtrip.mlir` — two states with goto crossover.
- [X] T060 [P] [US1] **Done 2026-04-30 (TDD red)**: `procedure/func_roundtrip.mlir` — bare `@reset` + dotted `@ic.ready` (Q5 StringAttr literal-dotted form).
- [X] T061 [P] [US1] **Done 2026-04-30 (TDD red)**: `procedure-helper/goto_roundtrip.mlir` — both state-form (inside `nsl.state`) and label-form (inside `nsl.seq`) per Q2 transitive walk.
- [X] T062 [P] [US1] **Done 2026-04-30 (TDD red)**: `system-task/sim_display_roundtrip.mlir` — format-string + var-arg and bare format-only.
- [X] T063 [P] [US1] **Done 2026-04-30 (TDD red)**: `system-task/sim_finish_roundtrip.mlir` — `nsl.sim_finish "done"`.
- [X] T064 [P] [US1] **Done 2026-04-30 (TDD red)**: `system-task/sim_init_roundtrip.mlir` — sim_init body holding sim_delay + sim_display.
- [X] T065 [P] [US1] **Done 2026-04-30 (TDD red)**: `system-task/sim_delay_roundtrip.mlir` — three int-literal cycles values nested under sim_init.
- [X] T066 [P] [US1] **Done 2026-04-30 (TDD red)**: `marker/fire_probe_roundtrip.mlir` — `nsl.fire_probe @do` resolving to sibling `nsl.func_in`.
- [X] T067 [P] [US1] **Done 2026-04-30 (TDD red)**: `marker/struct_cast_roundtrip.mlir` — `nsl.struct_cast %raw : !nsl.bits<16> to !nsl.struct<@S>` (the explicit bridge per Q3 Option A).
- [X] T068 [P] [US1] **Done 2026-04-30 (TDD red)**: `marker/field_roundtrip.mlir` — both struct-body declaration form and integer-indexed access form. **Phase 4 review flag**: int-index access syntax not pinned in spec; picked `{index = 0 : i64} : !nsl.struct<@Pair> -> !nsl.bits<8>`.
- [X] T069 [P] [US1] **Done 2026-04-30 (TDD red)**: `expansion-only/structural_generate_roundtrip.mlir` — `lower`/`upper`/`step` int attrs. **Phase 4 review flag**: spec doesn't pin loop-bound attr names; picked descriptive trio.

### Implementation — Type records (3 records; all [P])

- [X] T070 [P] [US1] **Done 2026-04-30**: `def NSL_BitsType : NSL_Type<"Bits", "bits">` added to `lib/Dialect/NSL/IR/NSLTypes.td` with `unsigned width` parameter + `assemblyFormat = "<` $width `>"`. Also flipped `useDefaultTypePrinterParser = 1` back in `NSLDialect.td` (Phase 2 left it 0 to dodge zero-typedef linker errors). T027 turns green once T084 wires `addTypes<>`.
- [X] T071 [P] [US1] **Done 2026-04-30**: `def NSL_StructType : NSL_Type<"Struct", "struct">` added with `::mlir::SymbolRefAttr name` parameter. T028 turns green.
- [X] T072 [P] [US1] **Done 2026-04-30**: `def NSL_MemType : NSL_Type<"Mem", "mem">` added with `unsigned depth` + `::mlir::Type elementType` parameters; assemblyFormat `< [ $depth x $elementType ] >`. T029 turns green.

### Implementation — Op records (40 records; bulk authoring; all [P] within a category, sequentially across categories)

- [X] T073 [P] [US1] **Done 2026-04-30**: module-level op records (`NSL_ModuleOp`, `NSL_StructOp`, `NSL_SubmoduleOp`, `NSL_ConnectOp`) added per data-model §2.1. Trait set: `Symbol` + `SymbolTable` + `NoTerminator` + `SingleBlock` for ModuleOp/ProcOp/StructOp; `HasParent<"::mlir::ModuleOp">` for ModuleOp; `HasParent<"ModuleOp">` for the rest. `hasVerifier = 1` on ModuleOp/StructOp/ConnectOp; bodies are empty stubs (Round 5 fills).
- [X] T074 [P] [US1] **Done 2026-04-30**: storage op records (`NSL_RegOp`, `NSL_WireOp`, `NSL_VariableOp`, `NSL_MemOp`) per data-model §2.2. `ParentOneOf<["ModuleOp", "ProcOp"]>` on RegOp; `ParentOneOf<["ModuleOp", "FuncOp"]>` on VariableOp; `HasParent<"ModuleOp">` on Wire/Mem. Each carries an SSA result; fixtures updated to use `%name = ...` form (consistency-fix during this round).
- [X] T075 [P] [US1] **Done 2026-04-30**: control-terminal op records (`NSL_FuncInOp`, `NSL_FuncOutOp`, `NSL_FuncSelfOp`) per data-model §2.3. SYN-4 deviation: spec/Phase-3 fixture spelling (`"do"(%a, %b) : !nsl.bits<8>`) was unparseable for variadic operands; switched to MLIR's standard `functional-type($args, results)` spelling — `(args) -> result`. Three fixtures updated to match.
- [X] T076 [P] [US1] **Done 2026-04-30**: action-block op records (`NSL_AltOp`, `NSL_AnyOp`, `NSL_IfOp`, `NSL_ParallelOp`, `NSL_SeqOp`, `NSL_WhileOp`, `NSL_ForOp`) per data-model §2.4. `IfOp` uses two `SizedRegion<1>` regions joined by `else` keyword; `SeqOp` immediate-parent `FuncOp`; `WhileOp`/`ForOp` carry `hasVerifier = 1` for Round 5's transitive-parent walk.
- [X] T077 [P] [US1] **Done 2026-04-30**: action-helper op records (`NSL_CaseOp`, `NSL_DefaultOp`) per data-model §2.5 with `ParentOneOf<["AltOp", "AnyOp"]>`.
- [X] T078 [P] [US1] **Done 2026-04-30**: atomic op records (`NSL_TransferOp`, `NSL_ClockedTransferOp`, `NSL_IncDecOp`, `NSL_CallOp`, `NSL_FinishOp`, `NSL_FinishMethodOp`, `NSL_InvokeMethodOp`) per data-model §2.6. `IncDecOp`'s kind enum uses TableGen `EnumAttr` (`IncDecKindAttr` mnemonic `incdec_kind` per fixture). `CallOp`/`InvokeMethodOp` use `functional-type` form per SYN-4.
- [X] T079 [P] [US1] **Done 2026-04-30**: procedure op records (`NSL_ProcOp`, `NSL_FirstStateOp`, `NSL_StateOp`, `NSL_FuncOp`) per data-model §2.7. Q5 Option A' implemented: `FuncOp::sym_name` is `SymbolNameAttr` (which is a `StringAttr` constraint upstream) — bare `@reset` and dotted `@ic.ready` both round-trip via standard MLIR symbol-name parsing.
- [X] T080 [P] [US1] **Done 2026-04-30**: procedure-helper op record (`NSL_GotoOp`) per data-model §2.8. `hasVerifier = 1` for Round 5's transitive-parent walk.
- [X] T081 [P] [US1] **Done 2026-04-30**: system-task op records (`NSL_SimDisplayOp`, `NSL_SimFinishOp`, `NSL_SimInitOp`, `NSL_SimDelayOp`) per data-model §2.9. Phase 4 SYN finding (carry-over from Phase 3): `nsl.sim_display` and `nsl.sim_delay` accept `ParentOneOf<["ModuleOp", "SimInitOp"]>` because Phase 3 fixtures place them in either; data-model §2.9's "ModuleOp only" claim is loose — fixtures define the actual parent set.
- [X] T082 [P] [US1] **Done 2026-04-30**: marker op records (`NSL_FireProbeOp`, `NSL_StructCastOp`, `NSL_FieldOp`, **`NSL_FieldDeclOp`** post-Q6) per data-model §2.10 + Q6 Option B. Phase 4 finding: `NSL_FieldDeclOp` does NOT use the `Symbol` interface (data-model §2.10 listed it but Phase 3 fixtures spell the field name as a bare string-literal `"a"`, not as a SymbolRef `@a` — Symbol trait would force `@`-prefix). Op carries `HasParent<"StructOp">` + `name: StrAttr` + `fieldType: TypeAttrOf<...>`.
- [X] T083 [P] [US1] **Done 2026-04-30**: expansion-only op record (`NSL_StructuralGenerateOp`) per data-model §2.11 with `lower`/`upper`/`step` int attrs.

### Dialect registration update

- [X] T084 [US1] **Done 2026-04-30**: `NSLDialect::initialize()` extended with `addOperations<>` (41 ops via `GET_OP_LIST`) + `addTypes<>` (3 types via `GET_TYPEDEF_LIST`) + `addAttributes<>` (`IncDecKindAttr` via `GET_ATTRDEF_LIST`). Also enabled `useDefaultAttributePrinterParser = 1` on the dialect for the `#nsl<incdec_kind ...>` round-trip. CMakeLists extended with explicit `mlir_tablegen` invocations for `NSLOpsEnums.{h,cpp}.inc` + `NSLOpsAttrDefs.{h,cpp}.inc` (the upstream `add_mlir_dialect` only emits ops/types/dialect headers; enums + attrdefs need separate invocations).

### Coverage-guard data update

- [X] T085 [US1] **Done 2026-04-30**: `.specify/m4_invariant_table.json` populated with the 41 op entries (post-Q6 — includes `nsl.field_decl`); each carries `name` + `category`; `invariants` arrays remain empty until Round 5 (T119).

### Verification

- [X] T086 [US1] **Done 2026-04-30**: dev-container build verification ran 335/335 lit + 209/209 ctest (211 total minus 2 disabled HelperEvaluator + 6 skipped constructive-Sn) green inside `ghcr.io/koyamanx/nsl-nslc:dev` with `-DNSL_ENABLE_ASAN=OFF`. `nsl-opt --show-dialects` lists the 7 expected dialects (`builtin, comb, fsm, hw, nsl, seq, sv`). `dialect_register_test` (`SingleRegistrationSucceeds` + `RegistrationIsIdempotent`) stays green. M0–M3 corpus stays green.

  **Phase 4 finish required 7 fixture-syntax fixes** (recorded as Phase 4 SYN-4/SYN-5/SYN-6 inline in the affected fixtures + TableGen `NSLOps.td`):
  - **SYN-5 (4 fixtures)**: `module-level/module_roundtrip.mlir`, `storage/reg_roundtrip.mlir`, `atomic/clocked_transfer_roundtrip.mlir`, `atomic/incdec_roundtrip.mlir` — `I64Attr` prints just `0` (the storage-type tag `: i64` is implicit). The Phase 3 fixtures spelled `= 0 : i64`, which left `: i64` unconsumed and broke the parser. Fix in fixtures only (canonical printer-output form is `= 0`).
  - **SYN-4 (1 fixture)**: `marker/fire_probe_roundtrip.mlir` — pre-existing SYN-4 sweep missed this fixture's `nsl.func_in "do"(%a) : !nsl.bits<8>` line; updated to MLIR-standard `functional-type` form `(args) -> result` to match the rest of the variadic-operand op fixtures.
  - **SYN-7 (1 TableGen change)**: `lib/Dialect/NSL/IR/NSLOps.td` `NSL_IfOp` — added `SingleBlock` trait (alongside the existing `NoTerminator`) so the parser auto-materializes an implicit `^bb0:` block when source spells an empty region as `{}`. Without `SingleBlock`, MLIR parses `{}` as a zero-block region and the `SizedRegion<1>` constraint then rejects it. (`NSL_AltOp` etc. already had this pairing — `NSL_IfOp` was the lone outlier.)
  - **SYN-6 (1 fixture)**: `expansion-only/structural_generate_roundtrip.mlir` — `attr-dict-with-keyword` requires the `attributes` keyword before the inline-attr brace block, otherwise the parser reads `{...}` as the body region and parses `lower = ...` as nested ops. Fix in fixture only; CHECK pattern updated for the printer's alphabetized attribute order.

  All seven changes match upstream MLIR conventions and do NOT alter the FR-010 op-table, FR-013 invariants, or any spec-resolved Q1/Q2/Q3/Q5/Q6 contract. Per FR-017, FR-018, FR-021, SC-001, SC-003, SC-004.

**Checkpoint** (achieved 2026-04-30): User Story 1 fully functional. `nsl-opt fixture.mlir` round-trips every op and every type. The dialect surface is structurally complete (41 ops + 3 types + 2 auto-terminators, post-Q6); only the verifier bodies (US2) and driver-invariant verification (US3) remain. **MVP deliverable**: contributors and M5 implementors can author and inspect `.mlir` IR using the full dialect at this point. Verified: 335/335 lit fixtures + 209/209 active ctest green inside `ghcr.io/koyamanx/nsl-nslc:dev`.

---

## Phase 4: User Story 2 — Verifier rejects malformed `nsl.*` ops with source-locating diagnostics (Priority: P1)

**Goal**: Every structural invariant enumerated in spec FR-013 is enforced by a verifier body — TableGen-trait-only for the bulk (~25 ops) or hand-written `verify()` body for the ~15 ops with non-trivial structural rules. Per Q2 Option B, the ~5 transitive-parent ops use a hand-written ancestor-walk via `findAncestorOfKind<T>`; the rest use TableGen `HasParent<X>` directly. Diagnostic message text follows MLIR's `op->emitOpError(...)` convention (substring-matched in fixtures per Q1 carve-out).

**Independent Test**: After this phase, every cell in FR-013 with ≥ 1 invariant has at least one `<op>_invalid_<reason>.mlir` fixture under `test/Dialect/<category>/` (~50 fixtures total). Each fixture's `// expected-error{{<substring>}}` matches the verifier's emitted diagnostic. `scripts/check_dialect_coverage.py` confirms paired-fixture existence per FR-021. US1's round-trip fixtures stay green (the new verifier bodies don't reject well-formed input).

### Tests for User Story 2 (MANDATORY per Constitution Principle VIII) ⚠️

> Write these tests FIRST. They MUST be observed FAILING against the unchanged tree before any implementation task in this story begins. (Failing-state: verifier bodies are empty stubs from US1; malformed input parses + verifies clean — no diagnostic emitted; `// expected-error` annotations are unmet.)

#### Bulk invalid-fixture authoring (~50 fixtures; all [P])

- [X] T087 [P] [US2] **Done 2026-04-30**: 6 module-level invalid fixtures authored under `test/Dialect/module-level/` (`module_invalid_nested.mlir`, `module_invalid_no_symname.mlir`, `struct_invalid_no_symname.mlir`, `struct_invalid_circular_field.mlir`, `submodule_invalid_wrong_parent.mlir`, `connect_invalid_type_mismatch.mlir`). Each uses `// RUN: nsl-opt --verify-diagnostics %s` + `// expected-error@+1 {{<substring>}}`. (`submodule_invalid_wrong_parent.mlir` per `/speckit-analyze` F3.)
- [X] T088 [P] [US2] **Done 2026-04-30**: 8 storage invalid fixtures authored under `test/Dialect/storage/` (`reg_invalid_wrong_parent.mlir`, `reg_invalid_bad_result_type.mlir`, `wire_invalid_wrong_parent.mlir`, `wire_invalid_bad_result_type.mlir`, `variable_invalid_wrong_parent.mlir`, `variable_invalid_bad_result_type.mlir`, `mem_invalid_wrong_parent.mlir`, `mem_invalid_bad_result_type.mlir`).
- [X] T089 [P] [US2] **Done 2026-04-30**: 3 control-terminal invalid fixtures authored under `test/Dialect/control-terminal/` (`func_in_invalid_wrong_parent.mlir`, `func_out_invalid_wrong_parent.mlir`, `func_self_invalid_wrong_parent.mlir`).
- [X] T090 [P] [US2] **Done 2026-04-30**: 8 action-block invalid fixtures authored under `test/Dialect/action-block/` (`alt_invalid_empty.mlir`, `any_invalid_empty.mlir`, `if_invalid_wrong_region_count.mlir`, `parallel_invalid_wrong_region_count.mlir`, `seq_invalid_wrong_parent.mlir`, `while_invalid_not_inside_seq.mlir`, `for_invalid_not_inside_seq.mlir`, `for_invalid_bad_loop_attrs.mlir`). Q2 Option B transitive-parent rules use `must be enclosed by 'nsl.seq'` substring per data-model §4 helper. (`if_*` / `parallel_*` per `/speckit-analyze` F6/F7.)
- [X] T091 [P] [US2] **Done 2026-04-30**: 2 action-helper invalid fixtures authored under `test/Dialect/action-helper/` (`case_invalid_wrong_parent.mlir`, `default_invalid_wrong_parent.mlir`).
- [X] T092 [P] [US2] **Done 2026-04-30**: 9 atomic invalid fixtures authored under `test/Dialect/atomic/` (`transfer_invalid_type_mismatch.mlir`, `clocked_transfer_invalid_dst_not_reg.mlir`, `clocked_transfer_invalid_type_mismatch.mlir`, `incdec_invalid_dst_not_reg.mlir`, `incdec_invalid_bad_kind.mlir`, `call_invalid_arg_count.mlir`, `finish_invalid_outside_proc.mlir`, `finish_method_invalid_no_symref.mlir`, `invoke_method_invalid_no_symref.mlir`). Q2 Option B `nsl.finish` transitive-parent rule covered.
- [X] T093 [P] [US2] **Done 2026-04-30**: 7 procedure invalid fixtures authored under `test/Dialect/procedure/` (`proc_invalid_no_symname.mlir`, `proc_invalid_two_first_states.mlir`, `first_state_invalid_outside_proc.mlir`, `first_state_invalid_no_target.mlir`, `state_invalid_outside_proc.mlir`, `state_invalid_no_symname.mlir`, `func_invalid_no_symname.mlir`).
- [X] T094 [P] [US2] **Done 2026-04-30**: 2 procedure-helper invalid fixtures authored under `test/Dialect/procedure-helper/` (`goto_invalid_not_in_seq_or_state.mlir`, `goto_invalid_bad_target.mlir`). Q2 Option B two-kind transitive-parent rule covered.
- [X] T095 [P] [US2] **Done 2026-04-30**: 4 system-task invalid fixtures authored under `test/Dialect/system-task/` (`sim_display_invalid_wrong_parent.mlir`, `sim_finish_invalid_wrong_parent.mlir`, `sim_init_invalid_wrong_parent.mlir`, `sim_delay_invalid_wrong_parent.mlir`).
- [X] T096 [P] [US2] **Done 2026-04-30**: 4 marker invalid fixtures authored under `test/Dialect/marker/` (`fire_probe_invalid_bad_target.mlir`, `struct_cast_invalid_type_mismatch.mlir`, `field_invalid_bad_index.mlir`, `field_invalid_type_mismatch.mlir`).
- [X] T097 [P] [US2] **Done 2026-04-30**: 1 expansion-only invalid fixture authored under `test/Dialect/expansion-only/` (`structural_generate_invalid_bad_loop_attrs.mlir`).

  **TDD red verification (per Constitution Principle VIII)**: Total 54 fixtures authored. Inside `ghcr.io/koyamanx/nsl-nslc:dev` running `cmake --build build-Release-noasan --target check-nslc`: 365/389 lit pass, 24/389 lit FAIL — exactly the fixtures whose target verifier is hand-written (T101–T118 implementation tasks). The remaining 30 fixtures pass at the trait-only level because the `HasParent` / `ParentOneOf` / `SameTypeOperands` / `Symbol-required-attr` traits already emit MLIR's standard diagnostics from US1 (those 30 effectively gate T099 and confirm the trait surface is correct). All 335 pre-existing US1 round-trip fixtures remain green. The 24 lit-failures are the TDD red set that T100–T118 turns green by filling in the empty `verify()` stubs in `lib/Dialect/NSL/IR/NSLOps.cpp` lines 40–62.

### Implementation — Verifier helpers (research §4, data-model §4)

- [X] T098 [US2] **Done 2026-04-30**: per F9 carry-over, `findAncestorOfKind<T>` was DROPPED — every transitive-parent verifier uses upstream `op->getParentOfType<T>()` inline. Added two helpers in the anonymous namespace at top of `lib/Dialect/NSL/IR/NSLOps.cpp`: `emitParentMismatch(op, kind)` emits `'<op>' op must be enclosed by 'nsl.<kind>'` and returns `failure()`; `isRegLikeValue(v)` returns true if `v` is the result of `RegOp` OR `FieldOp` whose source is recursively reg-like (covers field-of-reg-struct chains). Used by `WhileOp`/`ForOp`/`FinishOp` (single-kind transitive parent), `GotoOp` (two-kind), and `ClockedTransferOp`/`IncDecOp` (reg-like operand).

### Implementation — TableGen-trait-only verifiers (~25 ops; bulk update; all [P])

- [X] T099 [P] [US2] **Done 2026-04-30**: traits already in NSLOps.td from US1 — verified via 30/30 trait-fixture pass at parent commit 01a9db7; no edits needed in this round. The trait-only ops listed (`NSL_SubmoduleOp`, control terminals, `NSL_IfOp`/`ParallelOp`/`SeqOp`/`CaseOp`/`DefaultOp`, `NSL_TransferOp`/`FinishMethodOp`/`InvokeMethodOp`, `NSL_StateOp`/`FuncOp`, sim-task ops) all already carry the correct `HasParent`/`ParentOneOf`/`SameTypeOperands`/`Symbol`-required-attr traits in their TableGen records.

### Implementation — Hand-written verifier bodies (~15 ops; bulk update; all [P])

- [X] T100 [P] [US2] **Done 2026-04-30**: `ModuleOp::verify()` checks `sym_name` non-empty (defensive — generic-form `module_invalid_no_symname.mlir` fixture turns green). Struct-field non-circularity moved to `StructOp::verify()` per FR-013.
- [X] T101 [P] [US2] **Done 2026-04-30**: `StructOp::verify()` checks `sym_name` non-empty + field-list non-circular via DFS over `nsl.field_decl` children whose type is `!nsl.struct<@T>`. Turns `struct_invalid_no_symname.mlir` + `struct_invalid_circular_field.mlir` green.
- [X] T102 [P] [US2] **Done 2026-04-30**: `ConnectOp::verify()` checks strict `mlir::Type` equality on `dst`/`src` operands per Q3. Note: TableGen's `SameTypeOperands` trait verifier fires *first* and emits "same type", so `connect_invalid_type_mismatch.mlir` substring tightened from `operand type` to `same type` in the same commit.
- [X] T103 [P] [US2] **Done 2026-04-30**: `RegOp::verify()` checks result type ∈ {`BitsType`, `StructType`}. TableGen's `NSL_BitsOrStruct` trait fires first emitting "result #0 must be ..."; `reg_invalid_bad_result_type.mlir` substring tightened from `result type` to `result #0`.
- [X] T104 [P] [US2] **Done 2026-04-30**: `WireOp::verify()` checks `BitsType`. Same trait-vs-verify ordering fix as T103 — `wire_invalid_bad_result_type.mlir` substring tightened to `result #0`.
- [X] T105 [P] [US2] **Done 2026-04-30**: `VariableOp::verify()` checks `BitsType`. Substring tightened on `variable_invalid_bad_result_type.mlir` to `result #0`.
- [X] T106 [P] [US2] **Done 2026-04-30**: `MemOp::verify()` checks `MemType`. Substring tightened on `mem_invalid_bad_result_type.mlir` to `result #0`.
- [X] T107 [P] [US2] **Done 2026-04-30**: `AltOp::verify()` + `AnyOp::verify()` check the body region has ≥ 1 child of kind `CaseOp` or `DefaultOp`; emit `requires at least one case-or-default child` on miss. Turns `alt_invalid_empty.mlir` + `any_invalid_empty.mlir` green.
- [X] T108 [P] [US2] **Done 2026-04-30**: `WhileOp::verify()` + `ForOp::verify()` use `op->getParentOfType<SeqOp>()` (per F9 — `findAncestorOfKind` dropped) and emit `must be enclosed by 'nsl.seq'` via `emitParentMismatch` on miss. `ForOp` additionally checks `loop_bound` attribute (if present) is integer-typed. Turns `while_invalid_not_inside_seq.mlir` + `for_invalid_not_inside_seq.mlir` + `for_invalid_bad_loop_attrs.mlir` green.
- [X] T109 [P] [US2] **Done 2026-04-30**: `ClockedTransferOp::verify()` checks `isRegLikeValue(getDst())`. Turns `clocked_transfer_invalid_dst_not_reg.mlir` green.
- [X] T110 [P] [US2] **Done 2026-04-30**: `IncDecOp::verify()` checks `isRegLikeValue(getDst())` + kind-enum range (rejects raw `IntegerAttr` with value > `PostDec`). Turns `incdec_invalid_dst_not_reg.mlir` + `incdec_invalid_bad_kind.mlir` green.
- [X] T111 [P] [US2] **Done 2026-04-30**: `CallOp::verify()` walks the enclosing `nsl.module`'s body (control-terminal ops do NOT carry `Symbol` trait; their identifier lives in a `name` StrAttr). For each matching control-terminal sibling, checks operand-arg-count match. Turns `call_invalid_arg_count.mlir` green.
- [X] T112 [P] [US2] **Done 2026-04-30**: `FinishOp::verify()` uses `getParentOfType<ProcOp>()` and emits `must be enclosed by 'nsl.proc'` via `emitParentMismatch`. Turns `finish_invalid_outside_proc.mlir` green.
- [X] T113 [P] [US2] **Done 2026-04-30**: `ProcOp::verify()` checks `sym_name` non-empty + at-most-one `FirstStateOp` child. Turns `proc_invalid_no_symname.mlir` + `proc_invalid_two_first_states.mlir` green.
- [X] T114 [P] [US2] **Done 2026-04-30**: `FirstStateOp::verify()` walks the enclosing `nsl.proc`'s body and checks the `target` attribute names a sibling `nsl.state` whose `sym_name` matches. Defensive: skips siblings whose `sym_name` is missing (those siblings own their own `Symbol`-trait diagnostic). Turns `first_state_invalid_no_target.mlir` green AND prevents segfault on the `state_invalid_no_symname.mlir` interaction-case.
- [X] T115 [P] [US2] **Done 2026-04-30**: `GotoOp::verify()` uses two `getParentOfType<>` walks (`SeqOp` and `StateOp`) — emits `must be enclosed by 'nsl.seq' (label form) or 'nsl.state' (state-name form)` on miss; otherwise resolves the `target` symref against the enclosing `nsl.proc`'s symbol table and checks it names a sibling `nsl.state`. Turns `goto_invalid_not_in_seq_or_state.mlir` + `goto_invalid_bad_target.mlir` green.
- [X] T116 [P] [US2] **Done 2026-04-30**: `FireProbeOp::verify()` walks the enclosing `nsl.module`'s body and matches the `target` against the `name` StrAttr of every sibling `FuncInOp`/`FuncOutOp`/`FuncSelfOp` (those ops have no `Symbol` trait at M4). Emits a `does not name a sibling` diagnostic on miss. Turns `fire_probe_invalid_bad_target.mlir` green.
- [X] T117 [P] [US2] **Done 2026-04-30**: `StructCastOp::verify()` computes the struct side's total bit-width by summing `nsl.field_decl` children's `!nsl.bits<N>` widths and asserts equality with the bits side's width — emits `struct_cast width mismatch` on miss. `FieldOp::verify()` resolves the source struct, range-checks `index ∈ [0, numFields)`, and asserts the result type equals the field-decl's declared type at that index. Turns `struct_cast_invalid_type_mismatch.mlir` + `field_invalid_bad_index.mlir` + `field_invalid_type_mismatch.mlir` green.
- [X] T118 [P] [US2] **Done 2026-04-30**: `StructuralGenerateOp::verify()` checks `step != 0` (terminating-loop shape per FR-013 row). Turns `structural_generate_invalid_bad_loop_attrs.mlir` green.

### Coverage-guard data update

- [X] T119 [US2] **Done 2026-04-30**: `.specify/m4_invariant_table.json` updated — every op's `invariants` array now carries the per-cell invariants from spec.md FR-013 (one short kebab-case identifier per FR-013 sub-rule, e.g. `parent-builtin-module`, `sym-name-required`, `field-list-non-circular`). 41 op rows × 1–3 invariants per row. The coverage-guard CI script (`scripts/check_dialect_coverage.py`) now exercises real per-invariant fixture-existence assertions per FR-021.

### Verification

- [X] T120 [US2] **Done 2026-04-30**: `cmake --build build-Release-noasan --target check-nslc` (run inside `ghcr.io/koyamanx/nsl-nslc:dev`) reports **389/389 lit pass** at this round's tip; `ctest` reports **209/209 ran + 2 skipped/disabled = 211/211** total. Verifier diagnostic format matches `^[^:]+:\d+:\d+: error: 'nsl\.[a-z_]+' op .+$` (sampled across `must be enclosed by 'nsl.seq'`, `requires at least one case-or-default child`, `arg count mismatch`, etc.). All 24 TDD-red fixtures from Round-5 turn green; all 365 pre-existing fixtures (US1 round-trip + 30 trait-only invalid + 4 storage `result #0` invalid + connect `same type` invalid) remain green. Net delta: +24 invalid-fixture passes, 0 regressions.

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
- [X] T132 Polish — **DONE 2026-04-30** in commit `763eb73` (FU4 spec/data-model amendment): spec.md FR-004 + scope-interpretation block + Assumptions paragraph + data-model.md §5 row for `Compilation::Compilation` all updated from "Compilation class declared in design §11 MUST gain ... at M4" / "MODIFIED at M4" to "**created at M4**". Documents that the M3 driver used free functions per `lib/Driver/EmitTokens.cpp` / `EmitAST.cpp` / `Sema.cpp` precedent; design §11's class definition was target-state, not extant code; M4 introduces the class skeleton, M5 extends it. Per `nsl-mlir-impl` agent's Phase 2 follow-up FU4.
- [X] T133 [P] [US1] **Done 2026-04-30**: `test/Dialect/marker/field_decl_roundtrip.mlir` authored — round-trip fixture for the new `nsl.field_decl` op (per Q6 Option B). Pattern matches the brief: `nsl.struct @S { nsl.field_decl "a" : !nsl.bits<4> nsl.field_decl "b" : !nsl.bits<12> }`.
- [X] T134 [P] [US1] **Done 2026-04-30**: `test/Dialect/Types/struct_roundtrip.mlir` renamed — in-struct-body field-decl form is `nsl.field_decl` per Q6 Option B. Phase 4 also propagated the rename to four other fixtures that exhibit the same struct-body decl shape (`module-level/struct_roundtrip.mlir`, `storage/reg_roundtrip.mlir`, `marker/struct_cast_roundtrip.mlir`, `marker/field_roundtrip.mlir`, `Types/mem_roundtrip.mlir`) — without those updates the verifier (or the eventual symbol-trait machinery on `field_decl`'s parent `StructOp`) would reject the fixture. The marker `marker/field_roundtrip.mlir` keeps the access form `nsl.field %r {index = ...}` per Q6.

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
# Launch fixture authoring for all 41 op round-trip fixtures + 3 type fixtures together (post-Q6):
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
   - Developer A: US1 (41 op round-trip fixtures + 41 op TableGen records + 3 types, post-Q6)
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
