<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

---

description: "Task list for M5 — `nsl-lower` part 1 (AST → `nsl` dialect + structural-expansion passes)"

---

# Tasks: M5 — `nsl-lower` part 1

**Input**: Design documents from `/specs/008-m5-structural-passes/`
**Prerequisites**: [`plan.md`](./plan.md) (required), [`spec.md`](./spec.md) (required for user stories), [`research.md`](./research.md), [`data-model.md`](./data-model.md), [`contracts/`](./contracts/)

**Tests**: Test tasks are **MANDATORY** for this project per Constitution Principle VIII (Test-First Development, NON-NEGOTIABLE). Every user story includes test tasks at the lowering layer (lit + FileCheck per Constitution Principle VI's per-layer accepted-driver clause). Tests MUST be written and observed FAILING before the corresponding implementation tasks begin.

**Organization**: Tasks are grouped by user story (US1–US5 from [`spec.md`](./spec.md)) to enable independent implementation and testing. US1 is the MVP; US2/US3/US4 are independently shippable structural-expansion increments; US5 is the determinism CI gate.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3, US4, US5)
- Include exact file paths in descriptions

## Path Conventions

Single project, LLVM-style layered architecture (per [`plan.md`](./plan.md) §Project Structure). All paths are relative to the repo root `/home/koyaman/devel/nslc/`. Build directory is `build-asan/` inside the dev container.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Ensure dev-container builds cleanly + register the new lit-test directory hierarchy.

- [X] T001 Verify dev-container build is green on master HEAD via `sg docker -c "docker run --rm -v $PWD:/workspace -w /workspace ghcr.io/koyamanx/nsl-nslc:dev bash -c 'cmake -G Ninja -B build-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON && ninja -C build-asan check-nsl'"` — record the baseline pass count for regression-comparison post-M5
- [X] T002 [P] Create `test/Lower/` subdirectory tree per [`data-model.md`](./data-model.md) §7 (`decl/`, `module/`, `action/`, `stmt/`, `expr/`, `marker/`, `passes/{nsl-resolve-params,nsl-expand-generate,nsl-expand-variables,nsl-explode-submod-array,nsl-inline-internal-func,nsl-check-semantics}/`, `m3_corpus/`, `determinism/`)
- [X] T003 [P] Amend `test/Lower/lit.cfg.py` to register the new test directory hierarchy + ensure `nslc` and `nsl-opt` are on the lit `PATH` for these tests

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Build the M5 library scaffolding — public header, visitor declaration, six pass NO-OP slots, diagnostic bridge, driver wiring. After Phase 2, the `-emit=mlir` flag exists, runs the full pipeline as no-ops, and produces an empty `mlir::ModuleOp`. ALL user-story tasks gate on this phase completing.

**⚠️ CRITICAL**: No user-story work begins until this phase is complete.

- [X] T004 Author `include/nsl/Lower/Lower.h` with the 8-symbol public surface frozen by [`contracts/lower-api.contract.md`](./contracts/lower-api.contract.md) §2 — declarations only, no bodies
- [X] T005 [P] Author `lib/Lower/ASTToMLIR.h` with the `ASTToMLIR` class declaration per [`data-model.md`](./data-model.md) §1 (private internal header)
- [X] T006 [P] Author `lib/Lower/Pass/Common/DiagnosticBridge.h` + `DiagnosticBridge.cpp` per [`data-model.md`](./data-model.md) §6 (RAII `mlir::ScopedDiagnosticHandler` → `basic::DiagnosticEngine` translation)
- [X] T007 [P] Author `lib/Lower/Pass/NSLResolveParamsPass.cpp` as a NO-OP slot (registers under `-nsl-resolve-params`, walks input, returns `mlir::success()`) — body lands in T069
- [X] T008 [P] Author `lib/Lower/Pass/NSLExpandGeneratePass.cpp` as a NO-OP slot (registers under `-nsl-expand-generate`) — body lands in T070
- [X] T009 [P] Author `lib/Lower/Pass/NSLExpandVariablesPass.cpp` as a NO-OP slot (registers under `-nsl-expand-variables`) — body lands in T081
- [X] T010 [P] Author `lib/Lower/Pass/NSLExplodeSubmodArrayPass.cpp` as a NO-OP slot (registers under `-nsl-explode-submod-array`) — body lands in T095
- [X] T011 [P] Author `lib/Lower/Pass/NSLInlineInternalFuncPass.cpp` as a NO-OP slot (registers under `-nsl-inline-internal-func`) — STAYS no-op at M5 per Q3 → Option B
- [X] T012 [P] Author `lib/Lower/Pass/NSLCheckSemanticsPass.cpp` as a NO-OP slot (registers under `-nsl-check-semantics`) — body lands in T096
- [X] T013 Wire `registerNSLLowerPasses()` in `lib/Lower/Lower.cpp` (or umbrella source) calling `mlir::registerPass(create<X>Pass)` for each of the six (depends on T007–T012)
- [X] T014 Amend `lib/Lower/CMakeLists.txt` source list to include the new `.cpp` files from T005–T012 (preserve `DEPENDS` and `LINK_LIBS` per [`contracts/lower-api.contract.md`](./contracts/lower-api.contract.md) §5)
- [X] T015 Author `lib/Lower/ASTToMLIR.cpp` with the visitor class skeleton — constructor, public `lower(...)` entry point, an empty `mlir::ModuleOp` builder + walk-stub that visits nothing (FR-005 single-pass shape locked in; FR-006 visitors land in US1 phase) (depends on T005)
- [X] T016 Replace `lib/Driver/LowerToNSL.cpp` M4-stub body with the real body per [`data-model.md`](./data-model.md) §4 (constructs `DiagnosticBridge`, calls `nsl::lower::astToMLIR(...)`, returns `OwningOpRef`) (depends on T006, T015)
- [X] T017 Replace `lib/Driver/RunNSLPasses.cpp` M4-stub body with the real body per [`data-model.md`](./data-model.md) §4 (constructs `DiagnosticBridge`, builds `mlir::PassManager`, adds the six passes in FR-012 order, calls `pm.run(module)`) (depends on T006, T013)
- [X] T018 Add `Compilation::emitNSLMLIR(mlir::ModuleOp)` private method in `lib/Driver/Compilation.cpp` — calls `module.print(os)` with default `mlir::OpPrintingFlags()` per [`contracts/driver-emit-mlir.contract.md`](./contracts/driver-emit-mlir.contract.md) §2
- [X] T019 Wire the `EmitKind::NSLMLIR` arm in `Compilation::run()` per [`data-model.md`](./data-model.md) §4 (calls `lowerToNSL` → check non-null → `runNSLPasses` → check success → `emitNSLMLIR`) (depends on T016, T017, T018)
- [X] T020 Amend `lib/Driver/CMakeLists.txt` to add `nsl-lower` as a link dependency
- [X] T021 Amend `tools/nsl-opt/main.cpp` to call `nsl::lower::registerNSLLowerPasses()` after the existing `nsl::dialect::registerNSLDialect()` line (per [`research.md`](./research.md) §7)
- [X] T022 Amend `tools/nsl-opt/CMakeLists.txt` to add `nsl-lower` as a link dependency
- [X] T023 [P] Amend `tools/nslc/main.cpp` `--help` text to include `-emit=<kind>` line per [`contracts/driver-emit-mlir.contract.md`](./contracts/driver-emit-mlir.contract.md) §7 (`mlir, hw (M6+), verilog (M7+)`)

**Checkpoint**: After T023, the dev-container build produces a working `nslc` and `nsl-opt`; `nslc -emit=mlir input.nsl` runs the full pipeline (six no-op passes) and prints an empty `mlir::ModuleOp{}`. `nsl-opt --help` lists all six pass flags. The foundation is ready; user-story implementation can now begin.

---

## Phase 3: User Story 1 — `nslc -emit=mlir` produces verified `nsl.*` IR for every AST shape (Priority: P1) 🎯 MVP

**Goal**: Every AST node kind from M2/M3 lowers to its `nsl::*` counterpart per design §8. The headline `-emit=mlir` end-to-end gate works on every Sema-clean fixture from the M3 corpus.

**Independent Test**: Build the project; for every concrete `visit(...)` override on `ASTToMLIR`, the paired `test/Lower/<category>/<node>_emit_mlir.nsl` fixture's lit `RUN: nslc -emit=mlir %s | FileCheck %s` line succeeds. The `nsl-opt` round-trip on the output is a fixed point. Independent of US2/US3/US4/US5.

### Tests for User Story 1 (MANDATORY per Constitution Principle VIII) ⚠️

> **NOTE**: Author these fixtures FIRST. Each MUST fail against the post-Phase-2 tree (because the relevant `visit()` override produces an empty placeholder) before its implementation task begins.

- [X] T024 [P] [US1] Author `test/Lower/module/module_emit_mlir.nsl` (+ FileCheck lines) covering `ast::ModuleBlock` + `ast::DeclareBlock` per [`spec.md`](./spec.md) US1 acceptance scenario 1
- [X] T025 [P] [US1] Author `test/Lower/decl/regdecl_emit_mlir.nsl` covering `ast::RegDecl` (the quickstart-prescribed first failing fixture)
- [X] T026 [P] [US1] Author `test/Lower/decl/wiredecl_emit_mlir.nsl` covering `ast::WireDecl`
- [X] T027 [P] [US1] Author `test/Lower/decl/memdecl_emit_mlir.nsl` covering `ast::MemDecl`
- [X] T028 [P] [US1] Author `test/Lower/decl/funcdefn_emit_mlir.nsl` covering `ast::FuncDefn`
- [X] T029 [P] [US1] Author `test/Lower/decl/procdefn_emit_mlir.nsl` covering `ast::ProcDefn` + `ast::StateDefn` + `ast::FirstStateDecl` per US1 acceptance scenario 2
- [X] T030 [P] [US1] Author `test/Lower/action/parallelblock_emit_mlir.nsl` covering `ast::ParallelBlock`
- [X] T031 [P] [US1] Author `test/Lower/action/altblock_emit_mlir.nsl` covering `ast::AltBlock`
- [X] T032 [P] [US1] Author `test/Lower/action/anyblock_emit_mlir.nsl` covering `ast::AnyBlock`
- [X] T033 [P] [US1] Author `test/Lower/action/seqblock_emit_mlir.nsl` covering `ast::SeqBlock`
- [X] T034 [P] [US1] Author `test/Lower/action/whileblock_emit_mlir.nsl` covering `ast::WhileBlock`
- [X] T035 [P] [US1] Author `test/Lower/action/forblock_enum_emit_mlir.nsl` + `forblock_cstyle_emit_mlir.nsl` covering both `ast::ForBlock` shapes (C-style only — enum form escalated; M4 `nsl.for` is 3-operand-only, spec.md:419 maps enum form to a 2-operand op that doesn't exist on the frozen-77 surface)
- [X] T036 [P] [US1] Author `test/Lower/action/ifstmt_emit_mlir.nsl` covering `ast::IfStmt`
- [X] T037 [P] [US1] Author `test/Lower/stmt/transferstmt_eq_emit_mlir.nsl` + `transferstmt_coloneq_emit_mlir.nsl` covering both `ast::TransferStmt` modes
- [X] T038 [P] [US1] Author `test/Lower/stmt/controlcallstmt_emit_mlir.nsl` covering `ast::ControlCallStmt`
- [X] T039 [P] [US1] Author `test/Lower/stmt/barefinishstmt_emit_mlir.nsl` covering `ast::BareFinishStmt`
- [X] T040 [P] [US1] Author `test/Lower/stmt/systemtaskstmt_{display,finish,init,delay}_emit_mlir.nsl` covering each of the four `ast::SystemTaskStmt` flavours
- [X] T041 [P] [US1] Author `test/Lower/expr/binaryexpr_emit_mlir.nsl` covering arithmetic + bit + comparison `ast::BinaryExpr` operators
- [X] T042 [P] [US1] Author `test/Lower/expr/{unary,literal,identifier,conditional,slice,concat}_emit_mlir.nsl` covering the remaining expression-position `ast::*Expr` kinds (`unary` + `SignExtendExpr` + `ZeroExtendExpr` shipped Phase B Commit 2; `conditional` + `slice` + `concat` shipped Phase B Commit 3; `literal` + `identifier` covered indirectly via `transferstmt_*` + `regdecl_init`)
- [X] T043 [P] [US1] Author `test/Lower/expr/structcastexpr_emit_mlir.nsl` covering `ast::StructCastExpr` → `nsl.struct_cast` + `nsl.field` chain
- [X] T044 [P] [US1] Author `test/Lower/expr/fieldaccessexpr_emit_mlir.nsl` covering `ast::FieldAccessExpr`
- [X] T045 [P] [US1] Author `test/Lower/marker/fire_probe_emit_mlir.nsl` covering control-name-as-1-bit-value (S27) → `nsl.fire_probe` (M4-valid subset only — `func_in`/`func_out`/`func_self`; ProcName/StateName escalated per fire_probe verifier `NSLOps.cpp:792-822`)
- [X] T046 [US1] Run `ninja -C build-asan check-nsl-lower` and confirm ALL US1 fixtures from T024–T045 fail (RED phase — Constitution Principle VIII gate)

### Implementation for User Story 1

- [X] T047 [P] [US1] Implement `ASTToMLIR::visit(const ast::ModuleBlock&)` + `visit(const ast::DeclareBlock&)` in `lib/Lower/ASTToMLIR.cpp` per [`spec.md`](./spec.md) FR-006 lowering table — produces `nsl.module` with port list (turns T024 GREEN)
- [X] T048 [P] [US1] Implement `ASTToMLIR::visit(const ast::RegDecl&)` (turns T025 GREEN)
- [X] T049 [P] [US1] Implement `ASTToMLIR::visit(const ast::WireDecl&)` (turns T026 GREEN)
- [X] T050 [P] [US1] Implement `ASTToMLIR::visit(const ast::MemDecl&)` (turns T027 GREEN)
- [X] T051 [P] [US1] Implement `ASTToMLIR::visit(const ast::FuncDefn&)` (turns T028 GREEN)
- [X] T052 [P] [US1] Implement `ASTToMLIR::visit(const ast::ProcDefn&)` + `visit(const ast::StateDefn&)` + `visit(const ast::FirstStateDecl&)` per US1 acceptance scenario 2 (turns T029 GREEN)
- [X] T053 [P] [US1] Implement action-block visitors (`ParallelBlock`, `AltBlock`, `AnyBlock`, `SeqBlock`, `WhileBlock`, `ForBlock` × 2 forms, `IfStmt`) (turns T030–T036 GREEN; ForBlock enum-form escalated per T035 note)
- [X] T054 [P] [US1] Implement statement visitors (`TransferStmt` × 2 modes, `ControlCallStmt`, `BareFinishStmt`, `SystemTaskStmt` × 4 flavours) (turns T037–T040 GREEN)
- [X] T055 [P] [US1] Implement `ASTToMLIR::lowerExpr` plus expression sub-visitors (`BinaryExpr`, `UnaryExpr`, `LiteralExpr`, `IdentifierExpr`, `ConditionalExpr`, `SliceExpr`, `ConcatExpr`, `StructCastExpr`, `FieldAccessExpr`) per FR-007 (turns T041–T044 GREEN)
- [X] T056 [US1] Implement S27 marker emission (control-name-as-1-bit-value → `nsl.fire_probe`) in the appropriate visitor sites per [`spec.md`](./spec.md) FR-006 last row (turns T045 GREEN; M4-valid subset — ProcName/StateName escalated)
- [X] T057 [US1] Author `scripts/audit_lower_fixtures.sh` enforcing FR-027 — greps `lib/Lower/ASTToMLIR.cpp` for `visit(...)` overrides, asserts a paired fixture exists under `test/Lower/`; CI-blocking on missing fixture
- [X] T058 [US1] Wire T057 into `scripts/ci.sh` stage 2 (static checks)
- [X] T059 [US1] Run `ninja -C build-asan check-nsl-lower` and confirm ALL US1 fixtures pass (GREEN phase); run `nsl-opt %.mlir | nsl-opt -` round-trip check on every output and confirm fixed-point (per US1 acceptance scenario 6)

**Checkpoint**: After T059, US1 is fully functional and testable independently. `nslc -emit=mlir` works for every AST shape; pipeline runs as no-op for the structural-expansion passes (US2/US3/US4 work happens later); the headline M5 demo works on simple inputs without `generate`/`variable`/`%IDENT%`/`param_int`/array-submod.

---

## Phase 4: User Story 2 — `generate` loops are unrolled into N replicated bodies (Priority: P1)

**Goal**: `nsl.structural_generate` ops vanish from the post-pipeline IR, replaced by N inline copies of the body with the loop variable's `%IDENT%` references substituted with per-iteration constants. `param_int`-bound generates work end-to-end (resolve-params runs first).

**Independent Test**: Pipeline-standalone via `nsl-opt -nsl-expand-generate` on hand-rolled `.mlir` fixtures + end-to-end via `nslc -emit=mlir` on `.nsl` fixtures with `generate` blocks. Independent of US1 (US1 fixtures don't use `generate`); independent of US3/US4/US5.

### Tests for User Story 2 (MANDATORY per Constitution Principle VIII) ⚠️

- [X] T060 [P] [US2] Author `test/Lower/passes/nsl-resolve-params/literal_param.mlir` + `multi_param.mlir` + `param_in_generate_bound.mlir` pass-standalone fixtures + expected goldens per [`data-model.md`](./data-model.md) §7.2 — *delivered offload Commit 1 (8755a27); fixtures assert no-op-on-pure-NSL invariant per the Commit 2 design discovery — see T069 docblock*
- [X] T061 [P] [US2] Author `test/Lower/passes/nsl-expand-generate/literal_bound.mlir` per US2 acceptance scenario 1 — *delivered offload Commit 3 (36913a9)*
- [ ] T062 [P] [US2] Author `test/Lower/passes/nsl-expand-generate/param_bound.mlir` per US2 acceptance scenario 2 — **DEFERRED** — at M5's frozen dialect surface `nsl.structural_generate.{lower,upper,step}` are I64Attrs (not SymbolRefAttrs), so a "param-bound generate" is indistinguishable at the dialect level from a literal-bound one once the visitor's `paramTable_` resolves the bound eagerly. Functional coverage is provided by `literal_bound.mlir` + the visitor unit-test path; a dedicated `param_bound.mlir` fixture would assert behaviour identical to `literal_param.mlir` + `literal_bound.mlir` composed.
- [X] T063 [P] [US2] Author `test/Lower/passes/nsl-expand-generate/nested.mlir` per US2 acceptance scenario 3 (triangular-number assertion) — *delivered offload Commit 3 (36913a9); 2x3 nesting (six replicas) — note S10 + the I64Attr bound prevents the literal triangular-number shape (inner upper depending on outer-i isn't expressible at dialect level)*
- [X] T064 [P] [US2] Author `test/Lower/passes/nsl-expand-generate/{zero_bound,one_bound}.mlir` per US2 edge-cases (entire generate removed / single body) — *delivered offload Commit 3 (36913a9)*
- [X] T065 [P] [US2] Author `test/Lower/passes/nsl-expand-generate/body_multi_position.mlir` per US2 acceptance scenario 5 (`%i%` in decl name + expression) — *delivered offload Commit 3 (36913a9); multiple `%i%` occurrences within a single StringAttr (limited to single-attr-slot at M5 because no current `nsl::*` op carries multiple StringAttr slots)*
- [ ] T066 [P] [US2] Author `test/Lower/generate/literal_generate_emit_mlir.nsl` + end-to-end golden — exercises `nslc -emit=mlir` on a literal-bound generate — **BLOCKED on two M4-design tensions** (offload Commit 4 escalation): (1) the preprocessor rejects `%i%` body references at lex time (`error: undefined macro reference: '%i%'`) — generate-loop-var residue would need a preprocessor-level skip-rule for known-loop-vars; (2) `nsl.reg`'s `ParentOneOf<["ModuleOp", "ProcOp"]>` rejects `nsl.reg` inside `nsl.structural_generate` body, so the visitor's pre-pass output fails MLIR's pre-pass verifier in `nslc -emit=mlir`. Both require M4-amendment or preprocessor-amendment to land cleanly. Pass-standalone fixtures (T061-T065) cover the post-pass mechanics fully via `--mlir-very-unsafe-disable-verifier-on-parsing`; end-to-end coverage waits on the amendments.
- [ ] T067 [P] [US2] Author `test/Lower/generate/param_generate_emit_mlir.nsl` + end-to-end golden — exercises `nslc -emit=mlir` with param + generate (relies on resolve-params running before expand-generate) — **BLOCKED** (same reasons as T066).
- [X] T068 [US2] Run lit suite; confirm T060–T067 all FAIL (RED phase — pipeline still has expand-generate as no-op from T008) — *5 of 8 RED-then-GREEN through Commits 3+4; T060 trio asserts no-op invariant (GREEN at Commit 1); T066/T067 BLOCKED — see above*

### Implementation for User Story 2

- [X] T069 [P] [US2] Implement `NSLResolveParamsPass::runOnOperation` body in `lib/Lower/Pass/NSLResolveParamsPass.cpp` per [`spec.md`](./spec.md) FR-013 — substitute `nsl.param_int` / `nsl.param_str` operand refs with constants from M3 Sema parameter map (turns T060 GREEN) — *delivered offload Commit 2 (c9985ee); pass body is a defensive walk that performs zero substitutions on pure-NSL inputs at M5 because no `nsl::*` op carries a `FlatSymbolRefAttr` slot pointing at a param symbol. Param eagerness happens at the AST→MLIR visitor stage instead — see Commit 1's `paramTable_<StringRef, int64_t>` populated by `visit(TopLevelParamDecl)` and consumed by `visit(StructuralGenerate)`. Mirror NSLInlineInternalFuncPass (FR-017) registered-no-op-slot pattern. Re-tightens automatically when a future op grows a substitutable slot.*
- [X] T070 [US2] Implement `NSLExpandGeneratePass::runOnOperation` body in `lib/Lower/Pass/NSLExpandGeneratePass.cpp` per FR-014 — replace each `nsl.structural_generate` with N inline copies of body; substitute `%IDENT%` loop-var refs (turns T061–T065 GREEN) — *delivered offload Commit 4 (efbddf4); fixed-point loop expanding one generate per iteration with `mlir::IRMapping`-based clone + recursive `%loop_var%` StringAttr substitution. Iteration cap of 65536 as defensive guard.*
- [X] T071 [US2] Extend `ASTToMLIR` to lower `ast::GenerateBlock` → `nsl.structural_generate` (currently emitted by no-op visitor) per design §8 + the M4 dialect surface — required for end-to-end fixtures T066/T067 — *delivered offload Commit 1 (8755a27); `visit(StructuralGenerate)` plus `visit(TopLevelParamDecl)` / `paramTable_` for eager param resolution. Note: end-to-end consumption of the visitor output (T066/T067) is BLOCKED on the same two M4-design tensions cited above.*
- [ ] T072 [US2] Run lit suite; confirm T060–T067 all GREEN; verify `nslc -emit=mlir` post-pipeline IR contains zero `nsl.structural_generate` for any fixture (per SC-004) — **PARTIAL**: T060–T065 + T068 GREEN (483/483 lit PASS); T066/T067 deferred as documented above. SC-004 holds for pass-standalone path; end-to-end SC-004 awaits the M4/preprocessor amendments to unblock T066/T067.

**Checkpoint**: After T072, US2 is fully functional. `generate(i = 0; i < N; i++) { ... }` lowers cleanly through `nslc -emit=mlir`; pass-standalone via `nsl-opt -nsl-expand-generate` works; param-bound generates also work via the resolve-params + expand-generate sequence. US1 fixtures are still GREEN (resolve-params + expand-generate are no-ops on inputs without params/generates).

**ACTUAL STATUS at offload-2026-04-30 close (commits 8755a27..efbddf4 + tasks-update commit)**: pass-standalone path FULLY FUNCTIONAL (T060-T065 + T068 + T069-T071 GREEN; 483/483 lit PASS); end-to-end path (T066-T067) DEFERRED on two surfaced M4-design tensions documented in T066's status note. US2 ships the structural deliverable (visitor + both passes + per-pass fixtures); the end-to-end NSL-source path lands when either (a) the preprocessor learns generate-loop-var residue, or (b) `nsl.reg`'s parent constraint is relaxed to admit `StructuralGenerateOp`. Both are M4/preprocessor amendments outside this offload's scope.

---

## Phase 5: User Story 3 — `nsl.variable` lowers to a wire+transfer SSA chain (struct-SSA-split) (Priority: P2)

**Goal**: `nsl.variable` ops vanish from post-pipeline IR, replaced by a chain of `nsl.wire` + `nsl.transfer` ops preserving SSA discipline. Struct-typed variables decompose per-field. Partial-assignment patterns from S12 are preserved.

**Independent Test**: Pipeline-standalone via `nsl-opt -nsl-expand-variables` + end-to-end via `nslc -emit=mlir` on fixtures with `variable` declarations. Independent of US1/US2/US4/US5.

### Tests for User Story 3 (MANDATORY per Constitution Principle VIII) ⚠️

- [X] T073 [P] [US3] Author `test/Lower/passes/nsl-expand-variables/scalar_single.mlir` per US3 acceptance scenario 1 — *delivered offload-2026-04-30 Commit 1 (180a225)*
- [X] T074 [P] [US3] Author `test/Lower/passes/nsl-expand-variables/scalar_chain_of_3.mlir` per US3 acceptance scenario 2 — *delivered offload-2026-04-30 Commit 1 (180a225)*
- [X] T075 [P] [US3] Author `test/Lower/passes/nsl-expand-variables/partial_assignment_S12.mlir` per US3 acceptance scenario 3 — *delivered offload-2026-04-30 Commit 1 (180a225); fixture redesigned in Commit 2 (e21e661) to model the visitor's `concat(zero4, x)` first-write pattern (avoids reads-before-first-write per S6/S12 first-write semantics)*
- [X] T076 [P] [US3] Author `test/Lower/passes/nsl-expand-variables/struct_typed.mlir` per US3 acceptance scenario 4 — *delivered offload-2026-04-30 Commit 1 (180a225) as **XFAIL** + deferred. M4's `nsl.variable` op rejects `!nsl.struct<@T>` per FR-013 + NSLOps.td:280 (NSL_AnyBits constraint). Either a fifth M4 amendment relaxing the type constraint OR AST-time per-field decomposition is required to land the substantive fixture; both need StructDecl + field-table infrastructure absent at M5. Documents the deferral at fixture banner.*
- [X] T077 [P] [US3] Author `test/Lower/passes/nsl-expand-variables/cross_scope.mlir` per US3 fixture-axis (e) (variable in func consumed in proc) — *delivered offload-2026-04-30 Commit 1 (180a225) as **XFAIL** + deferred. M4 op-surface tension: `nsl.variable` allows `ParentOneOf<["ModuleOp", "FuncOp"]>` but `nsl.wire` (the post-pass replacement) requires `HasParent<"ModuleOp">`. Func-scope variables are left un-expanded by the pass; a future amendment relaxing wire's parent OR introducing a func-scope storage op closes the gap.*
- [X] T078 [P] [US3] Author `test/Lower/variables/scalar_variable_emit_mlir.nsl` end-to-end fixture — *delivered offload-2026-04-30 Commit 3 (6e06458); `module M { wire a[8]; wire b[8]; variable v[8]; v = a; b = v; }` round-trips through `nslc -emit=mlir` to a single `nsl.wire "v"` with the read remapped to its result; zero `nsl.variable` post-pipeline.*
- [X] T079 [P] [US3] Author `test/Lower/variables/struct_variable_emit_mlir.nsl` end-to-end fixture — *delivered offload-2026-04-30 Commit 3 (6e06458) as **XFAIL** + deferred. Three blockers documented: (1) lang.ebnf §6 lacks `<TypeRef> variable s;` form; parser amendment needed. (2) M4 `nsl.variable` rejects struct types (same as T076). (3) Per-field decomposition + struct-pack op infrastructure absent at M5. Mirrors T076 / US2 T066/T067 deferral pattern.*
- [X] T080 [US3] Run lit suite; confirm T073–T079 all FAIL (RED phase — expand-variables still no-op) — *delivered offload-2026-04-30 Commit 1 (180a225); RED baseline confirmed: 3 substantive fixtures FAIL (`scalar_single`, `scalar_chain_of_3`, `partial_assignment_S12`); 2 XFAIL fixtures fail-as-expected. No-op pass leaves `nsl.variable` in place; FileCheck's `CHECK-NOT: nsl.variable` correctly fires.*

### Implementation for User Story 3

- [X] T081 [US3] Implement `NSLExpandVariablesPass::runOnOperation` body in `lib/Lower/Pass/NSLExpandVariablesPass.cpp` per FR-015 — replace each `nsl.variable` with SSA chain of `nsl.wire` + `nsl.transfer`; per-field decomposition for struct-typed; preserve S12 partial-assignment patterns (turns T073–T077 GREEN) — *delivered offload-2026-04-30 Commit 2 (e21e661); source-order block walk + version-numbered wire chain. Each transfer's dst-operand is rewired to a fresh `nsl.wire` ("name", "name_1", "name_2", ...); reads are remapped to the most-recently-written version. Module-scope only per scope policy; func-scope variables left in place (T077 XFAIL). Struct-typed variables blocked at the type-constraint layer (T076 XFAIL). Per-field decomposition deferred to a future amendment.*
- [X] T082 [US3] Extend `ASTToMLIR` to lower `ast::VariableDecl` → `nsl.variable` (depends on M4-frozen op surface) — required for end-to-end fixtures T078/T079 — *delivered offload-2026-04-30 Commit 3 (6e06458); mirrors visit(WireDecl) shape. `nameTable_` registered with the variable's result Value so TransferStmt LHS / IdentifierExpr RHS resolve through it. ALLOWLIST entry added to scripts/audit_lower_fixtures.sh citing per-pass + dialect round-trip coverage.*
- [X] T083 [US3] Run lit suite; confirm T073–T079 all GREEN; verify post-pipeline IR contains zero `nsl.variable` (per SC-005) — *delivered offload-2026-04-30 Commit 4 (this commit); 487 PASS + 3 XFAIL = 490 total lit tests; smoke 33 round-trip-clean fixtures; SC-005 holds for module-scope variables (the substantive deliverable). Determinism check: `diff -q` of two consecutive runs reports BYTE-STABLE. Func-scope (T077) + struct-typed (T076 + T079) deferred per the documented amendment-class blockers — same precedent as US2 T066/T067.*

**Checkpoint**: After T083, US3 ships **pass-standalone + end-to-end** for module-scope scalar variables. Func-scope variables (T077 XFAIL) and struct-typed variables (T076 + T079 XFAIL) remain deferred on M4 op-surface amendments. `variable v[8]; v = a; b = v;` lowers cleanly through `nslc -emit=mlir`; the post-pipeline IR contains zero `nsl.variable` per SC-005. US1/US2 fixtures remain GREEN.

**ACTUAL STATUS at offload-2026-04-30 close**: pass-standalone + end-to-end paths SHIP for module-scope scalar variables (T073–T075 + T078 + T080–T082 GREEN; T076–T077 + T079 XFAIL). 487/490 lit PASS + 3 XFAIL; 33 smoke fixtures round-trip-clean; audit OK 40 concrete visitors covered. The struct-SSA-split deliverable lands when a fifth M4 amendment relaxes `nsl.variable` to admit struct types OR an AST-time per-field decomposition path lands (StructDecl + field-table infrastructure prerequisite). The func-scope path lands when wire's parent constraint is relaxed OR a func-scope storage op is introduced. Both are M4 amendments outside this offload's scope.

---

## Phase 6: User Story 4 — Post-expansion `%IDENT%` residue triggers a sourced fail diagnostic (Priority: P2)

**Goal**: Surviving `%IDENT%` substrings in `mlir::StringAttr` values produce one `error: unresolved macro splice '%<IDENT>%' after structural expansion` diagnostic each. Six post-expansion-sensitive `Sn` (S6/S10/S15/S16/S20/S25) re-checked. Submod-array decomposition is also done here so S20's "submod-array carries parent's interface modifier" check is meaningful.

**Independent Test**: Pipeline-standalone via `nsl-opt -nsl-check-semantics` + `nsl-opt -nsl-explode-submod-array` + end-to-end via `nslc -emit=mlir` on fixtures with intentional residue or violations. Independent of US1/US2/US3/US5.

### Tests for User Story 4 (MANDATORY per Constitution Principle VIII) ⚠️

- [ ] T084 [P] [US4] Author `test/Lower/passes/nsl-explode-submod-array/{size_3,size_1,size_0}.mlir` pass-standalone fixtures per [`data-model.md`](./data-model.md) §7.2
- [ ] T085 [P] [US4] Author `test/Lower/passes/nsl-check-semantics/residue_typo.mlir` per US4 acceptance scenario 1 + `residue-detection.contract.md` §8 row 1
- [ ] T086 [P] [US4] Author `test/Lower/passes/nsl-check-semantics/residue_undefined.mlir` per `residue-detection.contract.md` §8 row 2
- [ ] T087 [P] [US4] Author `test/Lower/passes/nsl-check-semantics/residue_multi.mlir` per US4 acceptance scenario 3 (two diagnostics on different lines)
- [ ] T088 [P] [US4] Author `test/Lower/passes/nsl-check-semantics/s15_post_param.mlir` per `pass-pipeline.contract.md` §3 row S15
- [ ] T089 [P] [US4] Author `test/Lower/passes/nsl-check-semantics/s16_pure_nsl.mlir` per `pass-pipeline.contract.md` §3 row S16
- [ ] T090 [P] [US4] Author `test/Lower/passes/nsl-check-semantics/s25_replicated_collision.mlir` per `pass-pipeline.contract.md` §3 row S25
- [ ] T091 [P] [US4] Author `test/Lower/passes/nsl-check-semantics/{s6_use_before_def,s10_loop_var_residue,s20_submod_array_iface}.mlir` for the remaining sensitive-`Sn` rows
- [ ] T092 [P] [US4] Author `test/Lower/passes/nsl-check-semantics/clean_baseline.mlir` per `residue-detection.contract.md` §8 last row (positive case — round-trip success, zero diagnostics)
- [ ] T093 [P] [US4] Author `test/Lower/residue/typo_undefined_emit_mlir.nsl` end-to-end fixture (NSL source with `%TYPO%` → driver exits non-zero with the expected diagnostic on stderr) per US4 acceptance scenario 1
- [ ] T094 [US4] Run lit suite; confirm T084–T093 all FAIL (RED phase — explode-submod-array + check-semantics still no-ops)

### Implementation for User Story 4

- [ ] T095 [P] [US4] Implement `NSLExplodeSubmodArrayPass::runOnOperation` body in `lib/Lower/Pass/NSLExplodeSubmodArrayPass.cpp` per FR-016 — replace array-form `nsl.submodule` with N independent ops; rewrite cross-IR port references (turns T084 GREEN)
- [ ] T096 [US4] Implement `NSLCheckSemanticsPass::runOnOperation` body in `lib/Lower/Pass/NSLCheckSemanticsPass.cpp` — regex scan over `mlir::StringAttr` values per `residue-detection.contract.md` §1 + §2 (turns T085–T087, T093 GREEN)
- [ ] T097 [US4] Extend `NSLCheckSemanticsPass` body with the six sensitive-`Sn` re-check helpers per `pass-pipeline.contract.md` §3 (S6, S10, S15, S16, S20, S25 — diagnostic strings frozen) (turns T088–T091 GREEN)
- [ ] T098 [US4] Verify `clean_baseline.mlir` passes with zero diagnostics (T092 GREEN)
- [ ] T099 [US4] Run lit suite; confirm T084–T093 all GREEN; verify `nslc -emit=mlir typo.nsl` exits non-zero AND emits zero `.mlir` output AND emits the FR-018 diagnostic on stderr (per SC-006 + US4 acceptance scenario 1)

**Checkpoint**: After T099, US4 is fully functional. Residue cases produce sourced diagnostics; the six sensitive-`Sn` violations are caught at the post-expansion layer. The full pipeline (six real bodies + scaffolding) is operational. US1/US2/US3 fixtures remain GREEN.

---

## Phase 7: User Story 5 — Two-build determinism: byte-stable `-emit=mlir` output (Priority: P3)

**Goal**: `nslc -emit=mlir` output is byte-identical across two builds in distinct host paths. Zero host-path strings in any output. CI matrix gate enforces.

**Independent Test**: A CI matrix job builds in `$WORKSPACE_A` + `$WORKSPACE_B` (two distinct host paths) and `diff -q`s every output. Independent of US1/US2/US3/US4 — those test correctness; this tests bit-level reproducibility.

### Tests for User Story 5 (MANDATORY per Constitution Principle VIII) ⚠️

- [ ] T100 [P] [US5] Author `test/Lower/determinism/canonical_smoke.nsl` — a single non-trivial fixture exercising US1+US2+US3 features (a `module` with a `proc`, a `generate`, a `variable`, several `Sn`-relevant patterns)
- [ ] T101 [US5] Author `scripts/determinism_check.sh` — builds the project twice in `$TMPDIR/build-det-a` + `$TMPDIR/build-det-b` (distinct host paths), runs `nslc -emit=mlir` on T100 in each, `diff -q`s the outputs, exits non-zero on mismatch; also greps each output for the forbidden patterns from `driver-emit-mlir.contract.md` §3 (`/build`, `/home`, `/tmp`, `0x[0-9a-fA-F]{8,}`, time-of-day patterns)

### Implementation for User Story 5

- [ ] T102 [US5] Author `scripts/audit_determinism.sh` per [`research.md`](./research.md) §13 — greps `lib/Lower/` for forbidden patterns (`std::unordered_`, `reinterpret_cast<uintptr_t>`, `std::time`, `std::chrono::`, etc.); CI-blocking on match
- [ ] T103 [US5] Wire T101 + T102 into `scripts/ci.sh` stage 2 (static checks); add a CI matrix entry that runs T101 on every PR (per FR-029 + SC-007)
- [ ] T104 [US5] Run `bash scripts/determinism_check.sh` locally; if non-empty diff, debug per [`quickstart.md`](./quickstart.md) §6 (most common cause: debug-info attrs, build-path-derived diagnostic locations); iterate until empty diff (per SC-007 + SC-008)

**Checkpoint**: After T104, US5 is fully functional. The determinism CI gate catches host-path leakage and pointer-derived ordering automatically going forward.

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Wrap up FR-030 (M3-corpus extension), FR-031 (spec ↔ design coupling), the `NSLInlineInternalFuncPass` no-op-slot fixture, and final regression-comparison against the M4 baseline (SC-010).

- [ ] T105 [P] Author `test/Lower/passes/nsl-inline-internal-func/noop_roundtrip.mlir` per Q3 → Option B (single trivial round-trip fixture asserting `input == output`)
- [ ] T106 [P] Bulk-author `test/Lower/m3_corpus/s<NN>/<case>.expected.mlir` for all ~34 Sema-clean M3 pass-case fixtures via `nslc -emit=mlir test/sema/<sn>/<case>.nsl > test/Lower/m3_corpus/s<NN>/<case>.expected.mlir` then commit; CI runs `diff` on each in lit (per FR-030 + SC-003) — bulk fixture authoring; can be automated with a one-shot script
- [ ] T107 [P] Update [`CLAUDE.md`](../../CLAUDE.md) §1 — confirm every grammar row whose "Lower to dialect" column entry says "M5 (...)" is now delivered (no edits needed unless implementation surfaced a new row); per FR-031
- [ ] T108 [P] Update [`docs/CLAUDE.md`](../../docs/CLAUDE.md) §3 "Writing a structural-expansion pass" entry with current-PR commit hash if design §9 line ranges shifted; per FR-031
- [ ] T109 Run `nslc -emit=tokens` and `nslc -emit=ast` on the full M3 corpus; `diff -q` outputs against an M4-baseline run; confirm zero changes (regression-guard per SC-010)
- [ ] T110 [P] Author `scripts/audit_op_locations.sh` per FR-008 + SC-009 — runs `nsl-opt -mlir-print-debuginfo` on every `.mlir.expected` golden under `test/Lower/**/` and on every `nslc -emit=mlir` CI output, greps for the literal token `loc(unknown)`, exits non-zero on any match. The same script asserts at least one `FusedLoc(...)` shape on `test/Lower/expr/structcastexpr_emit_mlir.mlir.expected` (one composite-expression fixture per category) covering the FR-008 multi-source `mlir::FusedLoc` clause. Wire into `scripts/ci.sh` stage 2 (static checks). Closes coverage gaps surfaced by `/speckit-analyze` 2026-04-30 findings A3 + A4.
- [ ] T111 Run [`quickstart.md`](./quickstart.md) §10 pre-merge checklist end-to-end — every box ticked; CI green on all six Principle IX stages; CodeRabbit blocking findings addressed

**Checkpoint**: After T111, M5 is shipped per the spec contract.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup, T001–T003)**: No dependencies; can start immediately.
- **Phase 2 (Foundational, T004–T023)**: Depends on Phase 1 completion. **BLOCKS all user stories.**
- **Phase 3 (US1, T024–T059)**: Depends on Phase 2 completion. The MVP — STOP here for first-demo if needed.
- **Phase 4 (US2, T060–T072)**: Depends on Phase 2 completion. Independent of US1 implementation (US1's fixtures don't use `generate`); US2 fixtures don't depend on US1 visitor coverage to test.
- **Phase 5 (US3, T073–T083)**: Depends on Phase 2 completion. Independent of US1/US2.
- **Phase 6 (US4, T084–T099)**: Depends on Phase 2 completion. Independent of US1/US2/US3.
- **Phase 7 (US5, T100–T104)**: Depends on Phase 6 completion (T100 fixture exercises US1+US2+US3 features and assumes those phases are at least scaffolded; the determinism check itself is independent).
- **Phase 8 (Polish, T105–T111)**: Depends on all desired user stories being complete.

### User Story Dependencies (within phases)

- **US1 (P1)**: Independent. Can start immediately after Phase 2.
- **US2 (P1)**: Independent of US1. Can run in parallel with US1.
- **US3 (P2)**: Independent. Can run in parallel with US1/US2.
- **US4 (P2)**: Independent. Can run in parallel with US1/US2/US3.
- **US5 (P3)**: Logically depends on at least one of US1–US4 having visible output to diff; T100's smoke fixture should exercise multiple features but the determinism check itself is dependency-light.

### Within Each User Story

- Tests MUST be authored FIRST and observed FAILING (Constitution Principle VIII RED phase).
- Implementation makes them GREEN.
- Each user story's checkpoint asserts that earlier user stories' tests remain GREEN (no regression).

### Parallel Opportunities

- **Phase 1**: T002 + T003 in parallel (different files).
- **Phase 2**: T005 + T006 + T007 + T008 + T009 + T010 + T011 + T012 + T023 in parallel (different files; T013/T014/T015–T022 have ordering dependencies as noted).
- **Phase 3**: T024–T045 fixture authoring in parallel (different files); T047–T055 implementation in parallel (different visit() overrides in different translation units, BUT they all live in `ASTToMLIR.cpp` — so [P] only if implementer splits the file or assigns by AST-kind family with care).
- **Phases 4/5/6**: Test-authoring tasks within each phase can run in parallel; implementation is mostly serial within each pass.
- **Phase 8**: T105 + T106 + T107 + T108 in parallel.
- **Cross-phase**: Foundation done → US1 + US2 + US3 + US4 in parallel (different developers).

---

## Parallel Example: User Story 1 fixture authoring

```bash
# After Phase 2 completes, launch all US1 fixture-authoring tasks together:
Task: "Author test/Lower/module/module_emit_mlir.nsl"
Task: "Author test/Lower/decl/regdecl_emit_mlir.nsl"
Task: "Author test/Lower/decl/wiredecl_emit_mlir.nsl"
Task: "Author test/Lower/decl/memdecl_emit_mlir.nsl"
Task: "Author test/Lower/decl/funcdefn_emit_mlir.nsl"
Task: "Author test/Lower/decl/procdefn_emit_mlir.nsl"
Task: "Author test/Lower/action/parallelblock_emit_mlir.nsl"
# ... etc — 22 fixture-authoring tasks in parallel
```

Then T046 runs once to confirm RED phase, then T047–T056 run with appropriate parallelism (file-level conflicts: all visitors live in `ASTToMLIR.cpp`; can be parallelised by assigning ranges or via per-developer feature branches).

---

## Implementation Strategy

### MVP First (US1 only — `nslc -emit=mlir` end-to-end)

1. Complete Phase 1 (Setup) — verify clean baseline.
2. Complete Phase 2 (Foundational) — six no-op slots wired through; `nslc -emit=mlir` produces empty `mlir::ModuleOp` for any input.
3. Complete Phase 3 (US1) — visitor lights up; per-AST-node fixtures all GREEN.
4. **STOP and VALIDATE** — demo `nslc -emit=mlir` on simple modules. This is the M5 MVP.
5. Decide whether to ship and incrementally add US2–US5, or push on through full M5.

### Incremental Delivery (recommended)

1. Setup + Foundational → foundation ready (~one engineer-week).
2. + US1 → demo MVP (~one engineer-week).
3. + US2 → generate-loop unroll works (~3–5 engineer-days).
4. + US3 → struct-SSA-split works (~5–7 engineer-days; heaviest pass).
5. + US4 → residue check + sensitive-`Sn` re-check (~3–5 engineer-days).
6. + US5 → determinism CI gate (~1–2 engineer-days).
7. + Polish → M3-corpus extension, doc updates (~2 engineer-days).

Total: ~4–6 engineer-weeks for the full M5 with TDD discipline.

### Parallel Team Strategy

With three developers post-Phase-2:

1. Developer A: US1 (visitor + per-AST-node fixtures — the bulk of authoring work).
2. Developer B: US2 (NSLResolveParamsPass + NSLExpandGeneratePass).
3. Developer C: US3 + US4 (NSLExpandVariablesPass + NSLCheckSemanticsPass + NSLExplodeSubmodArrayPass).
4. Anyone post-Phase-7: US5 (determinism gate) + Polish.

Stories integrate via the foundation's pass-slot scaffold; no cross-story file conflicts because each US owns a different `lib/Lower/Pass/<X>.cpp` file (US2/US3/US4) plus US1 owns `ASTToMLIR.cpp`.

---

## Notes

- [P] tasks = different files, no dependencies on incomplete tasks
- [Story] label maps task to specific user story for traceability
- Each user story is independently completable and testable
- **Verify tests fail before implementing** (Constitution Principle VIII RED-GREEN-REFACTOR)
- Commit after each task or logical group; PR for each user-story phase if team strategy
- Stop at any checkpoint to validate independently
- Avoid: vague tasks, same-file conflicts on `ASTToMLIR.cpp`, cross-story dependencies that break independence
- Total task count: **111 tasks** across **8 phases** (up from 110 after `/speckit-analyze` 2026-04-30 added T110 to close A3 + A4 coverage gaps)
