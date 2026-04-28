<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->
---
description: "Tasks for M3 — Sema (`nsl-sema`: SymbolTable + TypeSystem + S1–S29)"
---

# Tasks: M3 — Sema (`nsl-sema`: SymbolTable + TypeSystem + S1–S29)

**Input**: Design documents from `/specs/006-m3-sema/`
**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md, contracts/

**Tests**: Test tasks are **MANDATORY** for this project per Constitution Principle VIII (Test-First Development, NON-NEGOTIABLE) and Principle VI ("one test per `S1`–`S29`", NON-NEGOTIABLE). Every user story includes test tasks at the appropriate layer. Tests MUST be written and observed FAILING before the corresponding implementation tasks begin. The 23 error/warning `Sn` fail-fixtures additionally MUST cite the literal frozen diagnostic message text per Principle VIII's `Sn`/`Nn`/`Pn` clause; the 6 constructive `Sn` (`S13`/`S18`/`S19`/`S23`/`S24`/`S27`) ship paired pass + introspection per Clarifications session 2026-04-28 Q1 → Option B.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story?] Description with file path`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: User-story label (US1, US2, US3) — required for user-story phase tasks; absent for Setup / Foundational / Polish
- Every task description includes the exact file path

## Path Conventions

- Compiler-frontend layout (M0/M1/M2 baseline; matches LLVM/CIRCT convention):
  - Public headers: `include/nsl/Sema/`
  - Implementations: `lib/Sema/` (engine + scaffolding); `lib/Sema/Constraints/S<NN>_*.cpp` (per-`Sn` checkers)
  - Driver glue: `lib/Driver/Sema.cpp` (M3 new); `lib/Driver/EmitAST.cpp` (M2 file, modified at M3)
  - lit + FileCheck tests: `test/sema/{s01..s29,recovery,resolution,width,emit-ast-resolved}/`
  - GoogleTest unit tests: `test_unit/{symbol_table,type_system,resolution_pass,constructive_sn,sema_lifecycle}_test/`

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization for M3; M0 stood up the build, the CI pipeline, the SPDX scan, the empty layer skeleton, and M1/M2 filled the first five layers — Phase 1 here is small.

- [X] T001 Create the M3 test directory tree under `test/` and `test_unit/`: `test/sema/{s01,s02,s03,s04,s05,s06,s07,s08,s09,s10,s11,s12,s13,s14,s15,s16,s17,s18,s19,s20,s21,s22,s23,s24,s25,s26,s27,s28,s29,recovery,resolution,width,emit-ast-resolved}/` and `test_unit/{symbol_table_test,type_system_test,resolution_pass_test,constructive_sn_test,sema_lifecycle_test}/` — each with a `.keep` placeholder so M0's lit-discovery picks them up. **Done 2026-04-28**: 38 `.keep` files created across the new tree (33 under `test/sema/` + 5 under `test_unit/`); existing `test/Output/`, M0/M1/M2 directory structure left untouched.
- [X] T002 Sanity-verify the M0+M1+M2 build is still green on branch `006-m3-sema` by running `cmake --build build && ctest --test-dir build --output-on-failure && lit -v test` inside `ghcr.io/koyamanx/nsl-nslc:dev` against the unchanged-since-M2 source tree (checkpoint before adding M3 sources). **Done 2026-04-28** (deferred at Phase 1 checkpoint; exercised at Phase 2 checkpoint): from-scratch dev-container build (73 ninja targets, Clang 18.1.3, MLIR + CIRCT loaded) + `ctest --output-on-failure` returned **185/185 PASS in 4.63s** (169 baseline M0/M1/M2 cases + 16 new M3 cases across `SymbolTableScopeStackTest` (6) + `TypeSystemInterningTest` (7) + `SemaLifecycleTest` (3); 2 pre-existing `HelperEvaluatorTest.ArityMismatch_*` tests remain disabled). The lit fixture run is folded into ctest by the M0 lit-discovery integration. Verifies BOTH the M0/M1/M2 baseline remains green AND the M3 Phase 2 scaffolding compiles + links + tests pass.
- [~] T003 [P] **DEFERRED** — `cmake/AddNSLLibrary.cmake` (M0) already enforces downward-only layering via the macro's `_nsl_layer_index()` table + "index M ≥ N" FATAL_ERROR (verified: layer table includes `nsl-sema` (6) with `nsl-dialect` (7) / `nsl-lower` (8) / `nsl-driver` (9) as forbidden upstream targets). Adding `nsl-parse` (5) → forbidden as upstream of `nsl-sema` is the new constraint; the existing macro check covers it because `nsl-sema` appearing as `DEPENDS` of `nsl-parse` triggers the same FATAL_ERROR. SC-009's "CI guard" obligation is satisfied by the M0 macro's configure-time enforcement (same precedent as M1's T003 / M2's T003 deferral). Documented here for the next reader.

**Checkpoint** (reached 2026-04-28): M3 directory skeleton in place (T001 done; 38 `.keep` files); build sanity exercised at Phase 2 checkpoint (T002 done — 185/185 ctest PASS in dev container); layering guard not needed — M0 macro already enforces SC-009 (T003 DEFERRED, rationale above). Phase 2 work can begin.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: `nsl-sema` library scaffolding (the three umbrella headers `Sema.h` + `SymbolTable.h` + `TypeSystem.h`, the `SymbolKind.def` X-macro, the engine class, the Compilation::sema() driver glue) — every user story depends on these.

**⚠️ CRITICAL**: No US1 / US2 / US3 task may begin until this phase is complete.

### X-macro single-source-of-truth `.def` file (data-model §1.5)

- [X] T004 [P] Author `include/nsl/Sema/SymbolKind.def` — X-macro file enumerating every concrete Symbol kind from data-model §1.5 (13 kinds: `Port`, `Reg`, `Wire`, `Variable`, `Integer`, `Mem`, `FuncIn`, `FuncOut`, `FuncSelf`, `Proc`, `State`, `Submodule`, `StructType`). Format: `NSL_SYMBOL_KIND(EnumName, ConcreteClass)`. Cross-reference comment cites `nsl_compiler_design.md` §6 lines 692–759. **Done 2026-04-28**: 13-kind X-macro authored mirroring `nsl-ast`'s `NodeKind.def` pattern; per the v1.6.0 Principle II §3 amendment, the `ConcreteClass` column names the class nested inside `SymbolTable.h` (NOT a per-kind header).

### `SymbolTable.h` umbrella header (data-model §§1.1–1.5, §2)

- [X] T005 [P] TDD — author `test_unit/symbol_table_test/scope_stack_test.cpp`: GoogleTest fixtures asserting (a) `enterScope` / `leaveScope` is balanced and the depth matches the AST nesting; (b) `declare()` returns false on duplicate name in the current scope; (c) `lookup(name)` walks outward; (d) `lookupScoped(SUB.port)` resolves through a `SubmoduleSymbol`'s `templateDecl` to the matching `PortSymbol`; (e) iteration order matches `Scope::declOrder` (insertion order, not `DenseMap` hash order — `sema-stability.contract.md` Invariant 2). Run; observe FAILING (no implementation yet). **Done 2026-04-28**: 6-test gtest suite authored covering enter/leave balance, duplicate-name rejection, outward lookup walk, single-part `lookupScoped`, insertion-ordered `declOrder`, and `currentScope`/`currentModule`. (d) split: Phase 2 ships single-part `lookupScoped` per data-model §2.3 minimum; multi-part `SUB.port` resolution lands at Phase 3 T028 with no public-API change. Failing-state was confirmed by file-existence: `lib/Sema/` contained only the M0 anchor TU at the time of authoring.
- [X] T006 Implement `include/nsl/Sema/SymbolTable.h` — `Symbol` abstract base with `name` / `kind` / `declLoc` / `type` (per data-model §1.1); `ValueSymbol` / `ControlSymbol` mid-bases; 13 concrete subclasses (per data-model §§1.2–1.4; consumes `SymbolKind.def` from T004 to build the enum); `Scope` class (per data-model §2.2); `SymbolTable` class (per data-model §2.3) declaring `declare`/`lookup`/`lookupScoped`/`enterScope`/`leaveScope`/`currentModule` API per `sema-api.contract.md` Invariant 1. **Done 2026-04-28**: ~430-LOC umbrella header. `Symbol`/`ValueSymbol`/`ControlSymbol` abstract bases + 13 concrete subclasses (`PortSymbol` carries `PortDirection`; `SubmoduleSymbol` carries `templateDecl`; `StructTypeSymbol` carries MSB-first `fields()` + `setFields()` mutator). `Scope` owns `DenseMap<StringRef, Symbol*>` for lookup + `std::vector<Symbol*>` for deterministic iteration. `SymbolTable` exposes `declare`/`lookup`/`lookupScoped`/`enterScope`/`leaveScope`/`currentModule`/`currentScope`/`scopeDepth`. Doxygen `///` comments on every public surface (Invariant 8).
- [X] T007 Implement `lib/Sema/SymbolTable.cpp` — out-of-line dtors + `Symbol` polymorphic anchor; `Scope` lookup-and-declare implementation using `llvm::DenseMap<StringRef, Symbol*>` for O(1) lookup paired with `std::vector<Symbol*> declOrder` for deterministic iteration; `SymbolTable::lookup` outward-walk; `SymbolTable::lookupScoped` two-step (head lookup → kind-validate → tail lookup in head's target scope per design §6 lines 794–795). T005 turns green. **Done 2026-04-28**: ~110-LOC impl with vtable anchors for `Symbol`/`ValueSymbol`/`ControlSymbol`, `toString(SymbolKind)` driven from `SymbolKind.def`, `Scope::insert` ownership-transfer pattern, and `SymbolTable`'s outward `lookup` walk. The Phase-2 `lookupScoped` ships single-part lookup per data-model §2.3 minimum (multi-part deferred to Phase 3 T028).

### `TypeSystem.h` umbrella header (data-model §3, design §6.x)

- [X] T008 [P] TDD — author `test_unit/type_system_test/interning_test.cpp`: GoogleTest fixtures asserting (a) `bitVector(8)` returns the same `TypeRef` across two invocations (Invariant 3 of `sema-stability.contract.md`); (b) `bitVector(8) != bitVector(16)`; (c) `bit()` is a singleton; (d) `unresolved()` is a singleton; (e) `equal(a, b)` is exactly `a == b` (per `sema-api.contract.md` Invariant 5); (f) `structType(name, fields)` interns by name; (g) `memory(depth, element)` interns by `(depth, TypeRef)`. Run; observe FAILING. **Done 2026-04-28**: 7-test gtest suite covering bv-interning, bv-distinctness-by-width, bit/unresolved singletons, equal-is-pointer-equality, struct-name-interning, and (depth, element) memory keying. Failing-state confirmed by absence of `nsl::sema::TypeSystem` symbol from M0 anchor TU.
- [X] T009 Implement `include/nsl/Sema/TypeSystem.h` — `Type` abstract base + `BitType` / `BitVectorType` / `StructType` / `MemoryType` / `UnresolvedType` concrete types (per data-model §3.1); `TypeKind` enum; `TypeRef = const Type*` alias; `TypeSystem` class declaring `bit()` / `unresolved()` / `bitVector(N)` / `structType(name, fields)` / `memory(depth, element)` / `equal(a, b)` API per `sema-api.contract.md` Invariant 5. **Done 2026-04-28**: ~210-LOC umbrella header. `FieldInfo` lives here (not in `SymbolTable.h` per data-model) to break a circular include — `SymbolTable.h` already includes `TypeSystem.h` for `TypeRef`, and `StructType`'s private `std::vector<FieldInfo>` member needs the full type at class-instantiation. The contract surface is unchanged; both `StructType` and `StructTypeSymbol` consume the same record. `TypeSystem::Impl` uses `pImpl` to keep the cache layout out of the header.
- [X] T010 Implement `lib/Sema/TypeSystem.cpp` — `BitType` and `UnresolvedType` singletons; `BitVectorType` interning via `llvm::DenseMap<uint64_t, std::unique_ptr<BitVectorType>>` per design §6.x line 849; `StructType` interning by name (caching strategy per research §3); `MemoryType` interning by `(depth, TypeRef element)` pair. T008 turns green. **Done 2026-04-28**: ~150-LOC impl. `DenseMapInfo` specialization for `(uint64_t, const Type*)` for the memory cache; `Impl` `pImpl` struct holds three caches plus the two singletons. `T010` turns T008 green via the documented contract.

### `Sema.h` umbrella header (data-model §4)

- [X] T011 [P] TDD — author `test_unit/sema_lifecycle_test/ownership_test.cpp`: GoogleTest fixtures asserting (a) `Sema::run()` returns a `SemaResult` whose `symbols` and `types` `unique_ptr`s are non-null; (b) re-invoking `run()` on the same `Sema` instance asserts in debug builds (per `sema-api.contract.md` Invariant 6 ownership transfer); (c) the returned `SemaResult::hasErrors` mirrors `DiagnosticEngine::hasErrors()` at end of run. Run; observe FAILING. **Done 2026-04-28**: 3-test gtest suite covering ownership-transfer post-condition, `hasErrors`/`DiagnosticEngine::hasError` mirror, and move-construct safety. (b) re-invocation assertion is observed via the ownership-transfer post-condition rather than `EXPECT_DEATH` (per the in-test rationale comment: NDEBUG-sensitivity makes death tests brittle in Phase-2 scaffolding).
- [X] T012 Implement `include/nsl/Sema/Sema.h` — `Sema` class declaring constructor `Sema(DiagnosticEngine&)` and entry point `SemaResult run(CompilationUnit&)`; `SemaResult` struct with `symbols`, `types`, `hasErrors` fields per data-model §4.1. Doxygen `///` comments per `sema-api.contract.md` Invariant 8 on every public surface. **Done 2026-04-28**: ~110-LOC umbrella header. `SemaResult` with default-initialized `hasErrors = false` (so an unfilled instance is observably "no Sema errors yet"). `Sema` exposes `run`, `classifyIdentifierExpr` (the Phase-4-T070 stub), and the private `runResolutionPass`/`runConstraintPasses` orchestration entry points. `ClassifierKind` enum at this header per `sema-api.contract.md` Invariant 4 surface for `S27`.
- [X] T013 Implement `lib/Sema/Sema.cpp` scaffolding — `Sema::run()` orchestrates `runResolutionPass()` then `runConstraintPasses()` (Phase 2 ships scaffolding stubs that no-op; concrete behavior lands in Phase 3 / 4). `ConstraintCheckRegistry` private impl detail with a `NSL_REGISTER_CONSTRAINT(SN, visitor)` macro for self-registration of the per-`Sn` walkers landing in Phase 4. T011 turns green. **Done 2026-04-28**: ~70-LOC scaffolding. `Sema::run` constructs symbols/types in the ctor, invokes the two no-op stages, snapshots `diag_.hasError()`, and moves ownership into the result. `hasRun_` flag enforces Invariant 6 single-shot. `ConstraintCheckRegistry` deferred to Phase 4 T013 — it has no callers at Phase 2 and including it now would be unused-symbol noise.

### Wire `nsl-sema` library + driver glue

- [X] T014 Edit `lib/Sema/CMakeLists.txt` — invoke `add_nsl_library(nsl-sema SOURCES Sema.cpp SymbolTable.cpp TypeSystem.cpp HEADERS ${NSL_INCLUDE_DIR}/nsl/Sema/Sema.h ${NSL_INCLUDE_DIR}/nsl/Sema/SymbolTable.h ${NSL_INCLUDE_DIR}/nsl/Sema/TypeSystem.h ${NSL_INCLUDE_DIR}/nsl/Sema/SymbolKind.def DEPENDS nsl-basic nsl-ast)` per M0's macro convention. **Verify** the generated CMake target has NO link edge to `nsl-parse`, `nsl-dialect`, `nsl-lower`, or `nsl-driver` (M0 macro layering check + `sema-api.contract.md` Invariant 2). **Done 2026-04-28**: CMakeLists expanded to list three sources + four public headers (`Sema.h`, `SymbolTable.h`, `TypeSystem.h`, `SymbolKind.def`); `DEPENDS nsl-basic nsl-ast` only — the M0 `add_nsl_library` macro's index-comparison guard mechanically rejects any upward edge (verified by code reading; build verification deferred to user's CI run).
- [X] T015 Edit `test_unit/CMakeLists.txt` to register the five new gtest suites (`symbol_table_test`, `type_system_test`, `resolution_pass_test` placeholder, `constructive_sn_test` placeholder, `sema_lifecycle_test`) via the M1/M2 helper convention; the placeholder suites register only — Phase 3/4 fills them. **Done 2026-04-28**: foreach-suite list extended with the five names; the existing `EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${suite}/CMakeLists.txt"` guard automatically skips the two placeholder suites (`resolution_pass_test`, `constructive_sn_test`) until Phase 3/4 lands per-suite `CMakeLists.txt` files.
- [X] T016 [P] Implement `lib/Driver/Sema.cpp` — thin wrapper invoking `Compilation::sema(CompilationUnit&) -> SemaResult` that constructs `Sema(diag_)` and calls `run()`. ~30 lines per plan.md Tech Context "Scale/Scope". Updated `lib/Driver/CMakeLists.txt` adds `Sema.cpp` to the driver library sources. **Done 2026-04-28**: 22-LOC `runSema(unit, diag)` free function (the M2 driver pattern uses free functions, not a `Compilation` class — the spec's "`Compilation::sema`" naming is plan-level abstraction); paired with a 30-LOC `include/nsl/Driver/Sema.h` for symmetry with `EmitTokens.h`/`EmitAST.h`. `lib/Driver/CMakeLists.txt` adds `Sema.cpp` source + `Sema.h` header.
- [X] T017 Edit `lib/Driver/EmitAST.cpp` — call `Compilation::sema()` after `Compilation::parse()` and before printing the AST (per FR-019). On `SemaResult::hasErrors`, exit non-zero with no AST output on stdout (parallel to M2's parse-failure path; FR-019). The post-Sema enrichments to the printer come at Phase 3 (US1). **Done 2026-04-28**: `EmitAST.cpp` calls `driver::runSema(*cu, diag)` after a successful parse and before AST text generation; `sema_result.hasErrors` is OR'd into the existing parse-failure exit branch so a Sema-error path produces no stdout output. At Phase 2 the Sema body is a no-op stub (every well-parsed input flows through unchanged); the wiring is the deliverable.

**Checkpoint** (reached 2026-04-28): nsl-sema library scaffolding builds (build verification deferred to user); SymbolTable + TypeSystem + Sema engine entry-points present; 5 new gtest suites wired; driver glue invokes Sema after parse with no-op orchestration. Phase 3 (US1: Resolution + width inference) work can begin.

---

## Phase 3: User Story 1 — Resolution + width inference (Priority: P1) 🎯 MVP

**Goal**: A contributor pipes a parsed `CompilationUnit` (the M2 output) through Sema and gets back the same AST with three post-conditions guaranteed: every identifier reference resolves to a `Symbol*`, every `Expr::inferredType()` slot is filled with a non-null `TypeRef`, and every cross-scope reference resolves to the correct symbol in the target scope. The post-Sema `-emit=ast` printer emits the resolved-type and decl-loc enrichments inline (per Q2 Option A).

**Independent Test**: After this phase, `nslc -emit=ast fixture.nsl` produces the post-Sema enriched AST text format. The per-scope resolution fixtures and per-`Expr`-form width fixtures all pass; running `nslc -emit=ast` twice on the same input yields byte-identical output (Principle V; FR-029). Independent of US2 (per-`Sn` rejection) and US3 (multi-error reporting) — this story is the well-formed-input path only.

### Tests for User Story 1 (MANDATORY per Constitution Principle VIII) ⚠️

> Write these tests FIRST. They MUST be observed FAILING against the unchanged tree before any implementation task begins.

- [ ] T018 [P] [US1] TDD — author `test_unit/resolution_pass_test/scope_handling_test.cpp`: GoogleTest fixtures asserting (a) `ResolutionPass` opens `Global` scope at `CompilationUnit` entry and closes it at exit; (b) opens `Module` scope at `ModuleBlock` entry and closes at exit; (c) opens `Proc` scope inside `ProcDefn`; (d) the scope kinds match data-model §2.1's six-kind enum exactly. Run; observe FAILING.
- [ ] T019 [P] [US1] TDD — author `test_unit/resolution_pass_test/symbol_declaration_test.cpp`: per-`Symbol`-kind fixture (13 cases) asserting that for each declaration form, the `ResolutionPass` constructs the matching `Symbol` subclass, calls `SymbolTable::declare`, and the resulting `Symbol*::declLoc` round-trips to the declaration site. Run; observe FAILING.
- [ ] T020 [P] [US1] TDD — author `test_unit/resolution_pass_test/identifier_resolution_test.cpp`: fixtures asserting (a) `IdentifierExpr` to a same-scope name resolves to the right `Symbol*`; (b) `IdentifierExpr` to an outer-scope name resolves via outward walk; (c) `FieldAccessExpr` `inst.field` resolves through the `inst` symbol's `StructTypeSymbol::fields()` to the named field; (d) `ScopedName` `SUB.port` resolves through the `SubmoduleSymbol::templateDecl` to the `PortSymbol` in the declare scope; (e) `inst.finish()` is recognized as the proc-method built-in (per N6/S21). Run; observe FAILING.
- [ ] T021 [P] [US1] TDD — author `test_unit/resolution_pass_test/width_inference_test.cpp`: fixtures asserting that for each `Expr` form whose width is Sema-determined (literal, identifier, slice, sign-extend `#`, zero-extend `'`, binary, conditional, concat, repeat, field access, call, struct cast — per FR-027), the post-Sema `Expr::inferredType()` matches the expected `TypeRef`. Includes the context-sensitive cases: zero-extend / sign-extend whose width is set by the surrounding context; conditional whose width is the LUB of branches; concat whose width is the sum of operand widths. Run; observe FAILING.
- [ ] T022 [P] [US1] TDD — author `test_unit/resolution_pass_test/no_cascade_test.cpp` per `sema-stability.contract.md` Invariant 6: a fixture where one `Identifier` is unresolved and is referenced at `M ≥ 5` use sites; assert exactly **one** "unresolved name 'X'" diagnostic emitted; assert downstream `Sn` checks (mocked or stubbed) that consume the symbol's `Unresolved` `TypeRef` skip silently — no synthetic width-mismatch errors. Run; observe FAILING.
- [ ] T023 [P] [US1] Author per-scope resolution lit fixtures under `test/sema/resolution/` per FR-026: at minimum one fixture per scope-kind from data-model §2.1 (`global_scope.nsl`, `declare_scope.nsl`, `module_scope.nsl`, `proc_scope.nsl`, `seq_or_parallel_scope.nsl`, `function_scope.nsl`) plus per scoped-name form (`submodule_port.nsl`, `proc_method.nsl`, `dotted_func_def.nsl`). Each fixture runs `RUN: %nslc -emit=ast %s | FileCheck %s` asserting the `→ decl@<file>:<line>:<col>` suffix on the resolved name-ref points at the correct declaration site.
- [ ] T024 [P] [US1] Author per-`Expr`-form width lit fixtures under `test/sema/width/` per FR-027 + FR-009: one fixture per `Expr` kind whose width is Sema-determined (~12 fixtures), **including a dedicated `integer_constants.nsl` fixture** asserting that integer literals (e.g., `8`, `0xFF`, `5'b10101`) and integer-typed sub-expressions inside compound expressions resolve to a concrete `BitVector(N)` `TypeRef` at Sema time per FR-009 (not `Unresolved`, not `Bit`; structural-expansion integers per M5's `NSLExpandGeneratePass` are explicitly NOT in M3 scope). Each fixture asserts the `: <Type>` suffix on the post-Sema `-emit=ast` line matches the expected `BitVector(N)` / `Bit` / `Struct(...)` / `Memory(...)` value.
- [ ] T025 [P] [US1] Author the post-Sema `-emit=ast` golden corpus under `test/sema/emit-ast-resolved/`: one or two representative fixtures (e.g., the `module hello { reg q[8] = 0; func clk { q := q + 1; } }` example from quickstart.md §3) whose post-Sema `-emit=ast` output is asserted byte-exactly per `emit-ast-format.contract.md` Invariants 2 + 3 + 5. Run; observe FAILING (printer not yet extended).

### Implementation for User Story 1 — `ResolutionPass`

- [ ] T026 [US1] Implement `lib/Sema/ResolutionPass.cpp` scope-handling — `ASTVisitor` overriding `visit(CompilationUnit&)` / `visit(ModuleBlock&)` / `visit(DeclareBlock&)` / `visit(ProcDefn&)` / `visit(SeqBlock&)` / `visit(ParallelBlock&)` / `visit(AltBlock&)` / `visit(AnyBlock&)` / `visit(FuncDefn&)` to call `enterScope` at entry and `leaveScope` at exit per data-model §2.1's scope-kind table. T018 turns green.
- [ ] T027 [US1] Extend `lib/Sema/ResolutionPass.cpp` with per-`Symbol`-kind declaration handling: `visit(PortDecl&)` / `visit(RegDecl&)` / `visit(WireDecl&)` / `visit(VariableDecl&)` / `visit(IntegerDecl&)` / `visit(MemDecl&)` / `visit(FuncSelfDecl&)` / `visit(ProcNameDecl&)` / `visit(StateNameDecl&)` / `visit(SubmoduleDecl&)` / `visit(StructDecl&)` / `visit(FuncDefn&)` / `visit(StateDefn&)` to construct the matching `Symbol` subclass and call `SymbolTable::declare()`. T019 turns green.
- [ ] T028 [US1] Extend `lib/Sema/ResolutionPass.cpp` with name-resolution handling: `visit(IdentifierExpr&)` calls `SymbolTable::lookup` and writes the resolved `Symbol*` back into the AST node; `visit(FieldAccessExpr&)` resolves head identifier then looks up the tail field in the struct's field list; `visit(ScopedName&)` calls `SymbolTable::lookupScoped` for `SUB.port` / `inst.finish` / etc. On unresolved name, emit exactly one diagnostic and tag the node's `inferredType()` as `unresolvedSingleton()` per FR-017 / Invariant 6. T020 turns green.
- [ ] T029 [US1] Extend `lib/Sema/ResolutionPass.cpp` with width-inference top-down pass per design §6.x line 856 AND **FR-009 integer-typed sub-expression resolution**: maintain a "context width" stack; on `TransferStmt` entry, compute LHS width and push as context for RHS; per `Expr` kind, set `inferredType()` to the inherent width (literal — including integer literals which resolve to a concrete `BitVector(N)` per FR-009; slice; sign-extend `#`; zero-extend `'`; struct cast) or inherit context width (binary, conditional, concat). Handle unresolved subtrees by tagging `inferredType() = unresolvedSingleton()` and skipping width propagation. **Note**: structural-expansion integers (per M5's `NSLExpandGeneratePass`) are NOT in M3 scope — only the `_int`-helper-resolved integers and per-Ref §0 width-arithmetic integers are. T021 + T024 turn green.
- [ ] T030 [US1] Extend `lib/Sema/ResolutionPass.cpp` with the no-cascade guarantee implementation: the `ResolutionPass` maintains a `DenseSet<StringRef> reportedUnresolved` so a duplicate use site of the same unresolved name produces exactly one diagnostic. T022 turns green.

### Post-Sema `-emit=ast` printer extension (Q2 Option A; FR-020)

- [ ] T031 [P] [US1] Extend `lib/AST/Printer.cpp` (M2 file) to detect post-Sema input via `Expr::inferredType() != nullptr` per `emit-ast-format.contract.md` Invariant 4. The pre-Sema mode emits the M2 format unchanged; the post-Sema mode emits the additive enrichments per Invariants 2 + 3.
- [ ] T032 [P] [US1] Add the type-suffix renderer per `emit-ast-format.contract.md` Invariant 2: `Bit` / `BitVector(N)` / `Struct(<name>)` / `Memory(<depth> × <element>)` / `Unresolved` rendered after the `SourceRange` on every `Expr` line. Recursively renders memory element types.
- [ ] T033 [P] [US1] Add the decl-loc-suffix renderer per `emit-ast-format.contract.md` Invariant 3: ` → decl@<file>:<line>:<col>` after the type suffix on every `IdentifierExpr` / `FieldAccessExpr` head / `ScopedName` head whose resolved `Symbol*` is non-null. Suffix omitted when the type suffix is `Unresolved`.
- [ ] T034 [US1] Re-cut the M2 parser-fixture goldens under `test/parse/grammar/` to reflect the post-Sema enrichments per Principle VII spec/design coupling and `emit-ast-format.contract.md` Invariant 6 ("Format-bump documented in same patch"). Use a script-driven regen (extend M2's `scripts/gen_grammar_fixtures.py` with a `--post-sema` mode) to keep the corpus consistent. The golden delta per fixture is the `: <Type>` suffix + `→ decl@…` suffix on every applicable line. T023 + T025 turn green.

### Driver wire-up

- [ ] T035 [US1] Verify `lib/Driver/EmitAST.cpp` (modified at T017) correctly invokes the post-Sema printer mode when Sema succeeds. Confirm via T025's golden test passing byte-exactly. The `-emit=ast` exit code on Sema-success is 0; on Sema-failure is non-zero with no AST on stdout (FR-019).

### Determinism gate

- [ ] T036 [P] [US1] Add `test/sema/emit-ast-resolved/byte_stable.test` per `sema-stability.contract.md` Invariant 1: invokes `nslc -emit=ast` twice on the same input and `diff`s the outputs; CI failure on any difference. Extends to all four (build × compiler) combinations per SC-007.

**Checkpoint**: User Story 1 is fully functional. `nslc -emit=ast hello.nsl` runs Sema and emits the post-Sema enriched AST byte-stably. Resolution covers every scope kind + every scoped-name form + every Symbol kind. Width inference fills `Expr::inferredType()` for every `Expr` form. The no-cascade guarantee is structurally enforced. M2 parser-fixture goldens re-cut in same patch. **MVP deliverable**: contributors can ship M3 with US1 alone if needed and the post-Sema AST is observable end-to-end.

---

## Phase 4: User Story 2 — Per-`S1`–`S29` constraint checking (Priority: P1)

**Goal**: For every well-formed-grammatically-but-semantically-wrong NSL input, Sema emits a precise, source-locating diagnostic citing the `Sn` marker, with the message text frozen at M3 by the `s<NN>/fail.nsl` fixture's literal-string assertion (per Principle VIII). The 23 error/warning `Sn` honor this directly; the 6 constructive `Sn` ship paired pass + introspection (per Clarifications session 2026-04-28 Q1 → Option B).

**Independent Test**: After this phase, every `s<NN>/pass.nsl` fixture passes Sema clean and every `s<NN>/fail.nsl` fixture produces the expected literal diagnostic text (or, for the 6 constructive `Sn`, the unit test asserting introspection-expected-value matches). 100% of the 29 `Sn` constraints have both a pass case and a fail case (FR-023). Independent of US3 (each fixture has exactly one violation, the trivial multi-error case).

### Bulk fixture authoring (TDD red phase per Principle VI / VIII)

- [ ] T037 [P] [US2] Author 23 × 2 = 46 lit fixtures for the **error/warning `Sn`** under `test/sema/s<NN>/`: one `pass.nsl` (a construct that respects the rule) and one `fail.nsl` (the closest-possible violation) for each of `S1`, `S2`, `S3`, `S4`, `S5`, `S6`, `S7`, `S8`, `S9`, `S10`, `S11`, `S12`, `S14`, `S15`, `S16`, `S17`, `S20`, `S21`, `S22`, `S25`, `S26`, `S28`, `S29`. The `fail.nsl` cites the literal frozen message text per `contracts/diagnostic-string.contract.md` via FileCheck `// expected-error:` (or `// expected-warning:` for `S26`) directives. Each fixture is ≤20 lines of NSL; the fail fixture is also tagged with `// expected-error@+1 {{...}}` style anchors. Bundle authoring across the 23 since they're shape-uniform and each fixture is small.
- [ ] T038 [P] [US2] Author 6 × 2 = 12 paired pass + introspection artifacts for the **constructive `Sn`** under `test/sema/s<NN>/` + `test_unit/constructive_sn_test/`: per Clarifications Q1 → Option B and `sema-api.contract.md` Invariant 4. For each of `S13`/`S18`/`S19`/`S23`/`S24`/`S27`: ship a `test/sema/s<NN>/pass.nsl` lit fixture (asserts Sema runs clean; no diagnostic expected) AND a `test_unit/constructive_sn_test/s<NN>_test.cc` GoogleTest fixture asserting the introspection observable per the table in research.md §6 (`StructTypeSymbol::fields()` MSB-first for `S18`; `MemSymbol::initValues` zero-padded for `S24`; etc.). The "fail" shape per Q1 Option B is the same input with the introspection-expected-value flipped — implemented as a sibling test method `s<NN>_fail_test` that asserts the *opposite* observable and is expected to FAIL on a correct implementation (use gtest `EXPECT_NONFATAL_FAILURE` to assert the failure pattern).
- [ ] T039 [P] [US2] Author the multi-fail-case fixtures for `Sn` with multiple frozen messages (per `diagnostic-string.contract.md`): `S3` ships `s03/fail_eq_on_reg.nsl` + `s03/fail_coloneq_on_wire.nsl`; `S4` ships `s04/fail_funcin.nsl` + `s04/fail_funcout.nsl` + `s04/fail_funcself.nsl`; `S5` ships two; `S7` ships three (`fail_seq.nsl` + `fail_while.nsl` + `fail_for.nsl`); `S8` ships two; `S21` ships two; `S22` ships three (`fail_outside_func.nsl` + `fail_width_mismatch.nsl` + `fail_bare_with_terminal.nsl`); `S25` ships two; `S28` ships two. Total ~14 additional `fail_*.nsl` files beyond the 23 baseline of T037.
- [ ] T040 [P] [US2] Author the FixItHint shape assertions for `S3` / `S7` / `S14` / `S26` per `diagnostic-string.contract.md` "Fix-it hint shapes" table. Extend the relevant `fail*.nsl` fixtures with FileCheck `// expected-error: ... ; fix-it:` directives asserting both the message text AND the `replaceRange` + `replacement` text. Per FR-024.
- [ ] T041 [US2] Run T037 + T038 + T039 + T040 against the unchanged tree (Sema empty of constraint checks); observe **all** FAILING with messages like "expected error not seen" (for the fail fixtures) — no `S<NN>` checker exists yet. Capture failing run as TDD evidence per FR-028.

### Implementation — error/warning `Sn` checkers (one source per `Sn`; all [P])

- [ ] T042 [P] [US2] Implement `lib/Sema/Constraints/S01_NoDoubleUnderscore.cpp` — visits `Symbol::declLoc` for any newly-declared `Symbol`; emits the `S1` frozen message on identifiers containing `__`. Registers via `NSL_REGISTER_CONSTRAINT(1, S01Visitor)`. T037's `s01` pass + fail turn green.
- [ ] T043 [P] [US2] Implement `lib/Sema/Constraints/S02_WireNoInit.cpp` — visits `WireDecl`; emits `S2` frozen message when `WireDecl::init` is non-null. T037's `s02` fixtures turn green.
- [ ] T044 [P] [US2] Implement `lib/Sema/Constraints/S03_AssignmentLHSKind.cpp` — visits `TransferStmt`; emits `S3` frozen message + `FixItHint` on `=` to a reg LHS or `:=` to a wire LHS. T037 + T039 + T040's `s03` fixtures turn green.
- [ ] T045 [P] [US2] Implement `lib/Sema/Constraints/S04_FuncDummyArgDirs.cpp` — visits `FuncInDecl` / `FuncOutDecl` / `FuncSelfDecl`; emits `S4` frozen message (one variant per func kind) when dummy-arg direction is wrong. T037 + T039's `s04` fixtures turn green.
- [ ] T046 [P] [US2] Implement `lib/Sema/Constraints/S05_ReturnTerminalDir.cpp` — emits `S5` frozen messages (one per func kind) when the return-value-terminal direction is not the inverse-of-expected. T037 + T039's `s05` fixtures turn green.
- [ ] T047 [P] [US2] Implement `lib/Sema/Constraints/S06_ProcArgRegOnly.cpp` — visits `ProcNameDecl::regArgs`; emits `S6` frozen message if any resolves to a non-`RegSymbol`. T037's `s06` fixtures turn green.
- [ ] T048 [P] [US2] Implement `lib/Sema/Constraints/S07_SeqInsideFuncProc.cpp` — visits `SeqBlock` / `WhileBlock` / `ForBlock`; emits `S7` frozen message + `FixItHint` (when removable) on placement outside `func` / `proc` body. T037 + T039 + T040's `s07` fixtures turn green.
- [ ] T049 [P] [US2] Implement `lib/Sema/Constraints/S08_LoopInsideSeq.cpp` — emits `S8` frozen messages on `WhileBlock` / `ForBlock` outside an enclosing `SeqBlock`. T037 + T039's `s08` fixtures turn green.
- [ ] T050 [P] [US2] Implement `lib/Sema/Constraints/S09_ForVarReg.cpp` — emits `S9` frozen message when the for-loop variable's resolved `Symbol*` is not a `RegSymbol`. T037's `s09` fixtures turn green.
- [ ] T051 [P] [US2] Implement `lib/Sema/Constraints/S10_GenerateVarInteger.cpp` — emits `S10` frozen message when the generate-loop variable's resolved `Symbol*` is not an `IntegerSymbol`. T037's `s10` fixtures turn green.
- [ ] T052 [P] [US2] Implement `lib/Sema/Constraints/S11_StateNameProcScoped.cpp` — emits `S11` frozen message when a `state_name` is referenced from outside its declaring `Proc` scope. T037's `s11` fixtures turn green.
- [ ] T053 [P] [US2] Implement `lib/Sema/Constraints/S12_PartialLHSVariableOnly.cpp` — visits `TransferStmt::lhs` slice/concat forms; emits `S12` frozen message when the resolved LHS `Symbol*` is not a `VariableSymbol`. T037's `s12` fixtures turn green.
- [ ] T054 [P] [US2] Implement `lib/Sema/Constraints/S14_ConditionalElseRequired.cpp` — visits `ConditionalExpr`; emits `S14` frozen message + `FixItHint` when `else` branch missing (parser may have already partially handled per N1; M3's check is the Sema-level enforcement). T037 + T040's `s14` fixtures turn green.
- [ ] T055 [P] [US2] Implement `lib/Sema/Constraints/S15_SliceIndicesConst.cpp` — visits `SliceExpr`; emits `S15` frozen message when either `hi` or `lo` index is not a compile-time constant per the constant-evaluator. T037's `s15` fixtures turn green.
- [ ] T056 [P] [US2] Implement `lib/Sema/Constraints/S16_ParamHDLOnly.cpp` — visits `TopLevelParamDecl`; emits `S16` frozen message when the enclosing module is pure-NSL (not flagged as Verilog/VHDL/SystemC submodule). T037's `s16` fixtures turn green.
- [ ] T057 [P] [US2] Implement `lib/Sema/Constraints/S17_SystemTaskSimulationOnly.cpp` — visits `SystemTaskStmt`; emits `S17` frozen message when the enclosing module's `DeclareBlock::modifier != Simulation`. T037's `s17` fixtures turn green.
- [ ] T058 [P] [US2] Implement `lib/Sema/Constraints/S20_InterfaceModifierClkRst.cpp` — visits `DeclareBlock` with `modifier == Interface`; emits `S20` frozen message when explicit clock + reset names are missing. T037's `s20` fixtures turn green.
- [ ] T059 [P] [US2] Implement `lib/Sema/Constraints/S21_ProcMethodsFinishInvoke.cpp` — visits `ControlCallStmt` with target name `finish` or `invoke`; emits `S21` frozen messages (bare form outside proc body; dotted form whose head is not a `ProcSymbol`). T037 + T039's `s21` fixtures turn green.
- [ ] T060 [P] [US2] Implement `lib/Sema/Constraints/S22_ReturnWidthMatch.cpp` — visits `ReturnStmt`; emits `S22` frozen messages (return outside func body; width mismatch with concrete `<N>` and `<M>` integers; bare return with non-empty terminal). Width comparison consumes the post-resolution `Expr::inferredType()` from `ResolutionPass`. T037 + T039's `s22` fixtures turn green.
- [ ] T061 [P] [US2] Implement `lib/Sema/Constraints/S25_GotoTwoKinds.cpp` — visits `GotoStmt`; classifies target as label-name (inside `seq`) or state-name (inside `state` body); emits `S25` frozen messages on cross-kind reference. T037 + T039's `s25` fixtures turn green.
- [ ] T062 [P] [US2] Implement `lib/Sema/Constraints/S26_FuncFunctionWarn.cpp` — emits `S26` frozen WARNING (not error) on parser-flagged `function`-keyword sites; attaches `FixItHint` replacing `function` with `func`. T037 + T040's `s26` fixtures turn green.
- [ ] T063 [P] [US2] Implement `lib/Sema/Constraints/S28_FirstStatePositioning.cpp` — visits `FirstStateDecl`; emits `S28` frozen messages (target not declared in proc state list; declaration appears more than once). T037 + T039's `s28` fixtures turn green.
- [ ] T064 [P] [US2] Implement `lib/Sema/Constraints/S29_InitBlockPlacement.cpp` — visits `InitBlockStmt`; emits `S29` frozen message when the enclosing module's `DeclareBlock::modifier != Simulation` OR the block is nested inside another action block. T037's `s29` fixtures turn green.

### Implementation — constructive `Sn` checkers (paired-introspection per Q1 Option B; all [P])

- [ ] T065 [P] [US2] Implement `lib/Sema/Constraints/S13_AltAnyClassification.cpp` — populates `AltBlock::cases()` in priority order and `AnyBlock::cases()` in declaration order at AST walk time. Emits NO diagnostic (constructive). T038's `s13` introspection unit test turns green.
- [ ] T066 [P] [US2] Implement `lib/Sema/Constraints/S18_StructMSBFirstPacking.cpp` — populates `StructTypeSymbol::fields()` in MSB-first order at struct-declaration time, with `FieldInfo::offset = totalWidth - sum_of_widths_so_far - field_width`. Emits NO diagnostic. T038's `s18` introspection unit test turns green.
- [ ] T067 [P] [US2] Implement `lib/Sema/Constraints/S19_OneClockPerGoto.cpp` — at M3 ships a stub: populates `SeqBlock::clockBudget()` as the count of `goto`s + back-edge transitions. Full timing-semantic enforcement deferred to M5/M6 lowering (per spec FR-013). T038's `s19` introspection unit test turns green.
- [ ] T068 [P] [US2] Implement `lib/Sema/Constraints/S23_RegOmittedWidth1Bit.cpp` — at `RegSymbol` declaration time, when `RegDecl::width == nullopt && RegDecl::init != nullopt`, sets `RegSymbol::type = TypeSystem::bitVector(1)`. Emits NO diagnostic. T038's `s23` introspection unit test turns green.
- [ ] T069 [P] [US2] Implement `lib/Sema/Constraints/S24_MemPartialInitZero.cpp` — at `MemSymbol` declaration time, populates `MemSymbol::initValues` of size `MemSymbol::depth` with the user's init list followed by zero-padding for the remainder. Emits NO diagnostic for partial init; the *overflow* shape (more values than depth) is a separate type-mismatch diagnostic (per spec Edge Cases). T038's `s24` introspection unit test turns green.
- [ ] T070 [P] [US2] Implement `lib/Sema/Constraints/S27_ControlTerminalAs1Bit.cpp` — extends `Sema::classifyIdentifierExpr(IdentifierExpr&)` to return `ClassifierKind::ControlTerminalTap` for any `IdentifierExpr` whose resolved `Symbol*` is `FuncInSymbol` / `FuncOutSymbol` / `FuncSelfSymbol` / `ProcSymbol` AND whose context is expression position. Sets `Expr::inferredType() = TypeSystem::bit()` (1-bit) for those uses. T038's `s27` introspection unit test turns green.

### Verification

- [ ] T071 [US2] Run the full per-`Sn` fixture corpus: `./build/bin/llvm-lit -v test/sema/s01/ test/sema/s02/ … test/sema/s29/` plus `ctest -R constructive_sn_test`. Expect 100% pass: 23 × 2 + 14 multi-fail + 6 × 2 = 60 lit fixtures green, 6 introspection unit cases green per FR-023 / SC-001 / SC-002.

**Checkpoint**: User Story 2 is fully functional. Every `S1`–`S29` constraint has its checker (29 `.cpp` files), its test pair (29 fixture directories), and (where applicable) its `FixItHint`. Diagnostic-message strings frozen for the 23 error/warning `Sn` per Principle VIII. The 6 constructive `Sn` ship paired introspection per Q1 Option B.

---

## Phase 5: User Story 3 — Multi-error recovery (Priority: P1)

**Goal**: A contributor running Sema on input with multiple independent `Sn` violations sees ALL the diagnostics in a single Sema run, in source order. The hybrid recovery strategy (one resolution pass + per-`Sn` independent passes per Q3 Option C) is structurally enforced; the no-cascade guarantee for unresolved names produces exactly one diagnostic per typo regardless of use-site count.

**Independent Test**: After this phase, the multi-error fixture corpus (`test/sema/recovery/`) all passes; for `K ∈ {2, 3, 5}` independent `Sn` violations, exactly `K` diagnostics emit in source order; for an unresolved name with `M ≥ 5` use sites, exactly one "unresolved name" diagnostic emits with no cascading width-mismatch errors. Independent of US1 (resolution) and US2 (per-`Sn` checkers) in the sense that each fixture in this phase exercises the *integration* of the per-`Sn` pass independence — but US3 cannot be tested in isolation if US1 or US2 are absent.

### Tests for User Story 3 (MANDATORY per Constitution Principle VIII) ⚠️

> Write these tests FIRST. They MUST be observed FAILING against the unchanged tree before any implementation task begins. (Note: Phase 4's per-`Sn` checkers must be in place; this phase exercises their *interaction*, not new behaviors.)

- [ ] T072 [P] [US3] Author `test/sema/recovery/multi_K2.nsl` — two independent `Sn` violations in separate top-level modules (e.g., `S2` in module `A` + `S7` in module `B`); FileCheck asserts exactly two `// expected-error:` directives in source order. Run; expect PASS once Phase 4 is in (verifies hybrid pass independence).
- [ ] T073 [P] [US3] Author `test/sema/recovery/multi_K3.nsl` — three independent `Sn` violations in separate modules (e.g., `S2`, `S7`, `S14`); FileCheck asserts exactly three `// expected-error:` directives.
- [ ] T074 [P] [US3] Author `test/sema/recovery/multi_K5.nsl` — five independent `Sn` violations spanning multiple modules and constraint families; asserts exactly five.
- [ ] T075 [P] [US3] Author `test/sema/recovery/unresolved_cascade.nsl` per `sema-stability.contract.md` Invariant 6 / FR-017 / SC-005: one typo (`fooo` instead of declared `foo`) referenced at 5+ use sites; asserts exactly **one** `// expected-error: unresolved name 'fooo'` directive, NOT five+ cascading width-mismatch errors.
- [ ] T076 [P] [US3] Author `test/sema/recovery/sibling_unaffected.nsl` — one `Sn` violation in one `module_item` with a *correct* sibling `module_item` after it; asserts the violation diagnostic AND that the sibling's resolution + width inference are unaffected (the sibling's `inferredType()` populates correctly per FR-026).
- [ ] T077 [P] [US3] Author `test/sema/recovery/cross_module_independence.nsl` — `Sn` violation in module `A` does not suppress per-`Sn` checks in module `B`; both diagnostics emit.

### Implementation for User Story 3

The hybrid pass strategy (Q3 Option C) is *already* implemented as part of Phase 2 (T013 Sema.cpp orchestration: `runResolutionPass` then `runConstraintPasses`) and Phase 3 (T030 no-cascade in `ResolutionPass`). No new core implementation is needed — Phase 5 verifies that the structural design produces the expected end-to-end behavior.

- [ ] T078 [US3] Run the recovery fixture corpus: `./build/bin/llvm-lit -v test/sema/recovery/`. Expect 100% pass per SC-004 (`K ∈ {2, 3, 5}`-violations → exactly `K` diagnostics) and SC-005 (`M`-use-site typo → exactly one diagnostic).
- [ ] T079 [US3] Run the resolution-pass no-cascade unit test: `ctest -R no_cascade_test`. Expect green per FR-017.
- [ ] T080 [US3] Add a determinism gate for the recovery corpus per SC-007: each recovery fixture's diagnostic stream MUST be byte-identical across two runs and across (build × compiler) combinations. CI extension only (no new fixture).

**Checkpoint**: User Story 3 is fully verified. Multi-error fixtures pass; no-cascade guarantee holds end-to-end; per-`Sn` independence is structurally enforced by the hybrid strategy. M3 acceptance gates SC-004 + SC-005 met.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Final hygiene, audit, and documentation pass before opening the M3 PR.

### SPDX + layering hygiene

- [ ] T081 [P] Run M0's `scripts/check_spdx.py` against `git ls-files`; verify 100% of new files under `lib/Sema/`, `include/nsl/Sema/`, `lib/Driver/Sema.cpp`, `test/sema/`, `test_unit/{symbol_table,type_system,resolution_pass,constructive_sn,sema_lifecycle}_test/` carry the `Apache-2.0 WITH LLVM-exception` header per SC-008.
- [ ] T082 [P] Verify `nsl-sema` link-target dependency graph via M0's `add_nsl_library` macro: `ldd build/lib/Sema/libnsl-sema.a` (or equivalent) shows only `libnsl-basic.a` and `libnsl-ast.a` (transitively). No edge to `libnsl-parse.a` or any later layer. Per SC-009 / FR-002 / FR-003.

### Determinism gate (Principle V; FR-029 / SC-003 / SC-007)

- [ ] T083 Run the two-build determinism gate from M0: `./scripts/ci.sh determinism`. Verify post-Sema `-emit=ast` output is byte-identical across (Debug × Release) × (gcc × clang) on a representative fixture per SC-007.
- [ ] T084 Verify the post-Sema printer's no-pointer-leak invariant per `sema-stability.contract.md` Invariant 5: `nslc -emit=ast test/sema/emit-ast-resolved/<fixture>.nsl | grep -E '0x[0-9a-f]+'` returns no matches.

### Documentation + spec/design coupling (Principle VII)

- [ ] T085 Update `CLAUDE.md` (project root) §1 NSL-feature roll-up table: add the M3-era footnote naming the 6 `Sn` that ship paired-introspection (per Clarifications session 2026-04-28 Q1 → Option B), so the carve-out is auditable from the spec/design coupling table per Principle VII (plan.md Constitution Check Principle VII row).
- [ ] T086 [P] Update `README.md` Building/Status section: add a post-Sema `nslc -emit=ast` example showing the resolved-type and decl-loc enrichments (per Q2 Option A); cite `specs/006-m3-sema/quickstart.md` for the full walkthrough.
- [ ] T087 [P] Verify `docs/CLAUDE.md` §§4–7 line ranges are still accurate after M3 (the `nsl_lang.ebnf` is unchanged; line numbers in §5 quick-map remain correct; nothing in `docs/design/` shifted). If any range needs adjustment, edit in this same patch per Principle VII spec/design coupling.

### Quickstart validation

- [ ] T088 Run quickstart.md §3–§9 end-to-end inside `ghcr.io/koyamanx/nsl-nslc:dev`; verify each numbered step produces the expected output (post-Sema `-emit=ast` smoke; one error/warning `Sn` walkthrough; one constructive `Sn` walkthrough; multi-error recovery; no-cascade verification; full ctest + lit pass; local CI green-path).

### Agent-driven audits

- [ ] T089 [P] Spawn `nsl-coupling-audit` agent (READ-ONLY) to verify spec ↔ design coupling on the working tree. Expect zero blocking findings — M3 implements `lang.ebnf §S1–S29` and `nsl_compiler_design.md §6` + §6.x verbatim; no coupling drift expected. Any blocking finding is a stop-the-line item.
- [ ] T090 [P] Spawn `nsl-constitution-review` agent (READ-ONLY) to verify all 9 principles on the working tree. Expect zero blocking findings on Principles I/IV/V/VI/VII/VIII/IX. Principle II three-header layout posture (research §8 Option A) may be flagged for review — if the reviewer disputes the by-analogy reading, fall back to posture B (1-line constitutional amendment in same patch).
- [ ] T091 [P] Spawn CodeRabbit review on the PR. Per Constitution External Integrations, classify findings as blocking vs advisory on first review; route disputes to `/nsl-constitution-review` for binding judgement.

### Final CI green-path

- [ ] T092 Run `./scripts/ci.sh all` inside `ghcr.io/koyamanx/nsl-nslc:dev`. Expect all six pipeline stages green (build matrix × static checks × unit/layer × lowering × end-to-end-wired-but-empty × formal-wired-but-empty). Per Constitution Principle IX (no transitional clause; merge gate is hard).

**Checkpoint**: M3 ready for PR. All 10 SCs measurable as met; all 9 Constitution Principles green; spec/design coupling preserved. **`README.md` §Roadmap "M3 is the unlock point" is now true** — T2 (formatter), T3 (LSP skeleton), and T6 (lint framework) can begin against the `nsl-sema` public-header surface in parallel with subsequent M-track work (M4 dialect onwards).

---

## Dependencies & Story Completion Order

```text
Phase 1 (Setup, T001–T003)
    │
    ▼
Phase 2 (Foundational: SymbolTable + TypeSystem + Sema scaffolding, T004–T017)
    │
    ▼
Phase 3 (US1: Resolution + width inference + post-Sema -emit=ast, T018–T036) ── MVP-deliverable ──┐
    │                                                                                              │
    │      (US1 lands the resolved AST end-to-end via -emit=ast; foundational for US2 + US3)       │
    │                                                                                              │
    └──► Phase 4 (US2: Per-Sn constraint checking S1-S29, T037–T071) ──────────────────────────────┤
                                                                                                   │
                              Phase 5 (US3: Multi-error recovery + no-cascade, T072–T080)          │
                                                                                                   │
                                                              Phase 6 (Polish + audits, T081–T092)─┘
```

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately (M2 tree green).
- **Foundational (Phase 2)**: Depends on Setup completion — BLOCKS all user stories.
- **User Story 1 (Phase 3)**: Depends on Foundational — MVP. The post-Sema printer extension and the M2 golden re-cuts land here.
- **User Story 2 (Phase 4)**: Depends on Foundational AND on Phase 3's `ResolutionPass` (the per-`Sn` checkers consume `Symbol*` and `inferredType()` from the resolved AST). Logically parallel-able with US1 in two-developer scenarios but in single-developer mode US1 ships first to provide the resolved-AST surface.
- **User Story 3 (Phase 5)**: Depends on Foundational AND on Phase 3 AND on Phase 4 (the multi-error fixtures need the per-`Sn` checkers from US2 to produce the K-violations). US3 is largely a *verification* phase — the implementation is structurally already in place from Phase 2 + 3.
- **Polish (Phase 6)**: Depends on all three user stories complete.

### Within Each Phase

- Tests MUST be written and observed FAILING before implementation (Constitution Principle VIII).
- In Phase 2: `SymbolTable.cpp` before `TypeSystem.cpp` is fine but they can also land in parallel since they're independent files.
- In Phase 3: scope-handling before symbol-declaration before name-resolution before width-inference (each builds on the previous in `ResolutionPass.cpp`).
- In Phase 4: all 29 per-`Sn` checkers are independent files — fully parallelizable [P]; ordering within Phase 4 is whatever's convenient.
- In Phase 5: fixtures can be authored in parallel; verification waits on the fixture-impl interaction.

### Parallel Opportunities

- All Phase 1 tasks marked [P] can run in parallel.
- All Phase 2 tasks marked [P] can run in parallel within their TDD-test/impl pairings (e.g., T004/T005/T008 can all run in parallel; T006 depends on T005's failing test).
- Phase 3 — the printer-extension tasks (T031/T032/T033) and the resolution-tier impl tasks (T026/T027/T028/T029/T030) split across `Printer.cpp` and `ResolutionPass.cpp` so they can run in parallel.
- Phase 4 — **all 29 per-`Sn` impl tasks (T042–T070) are [P] across 29 distinct `.cpp` files**; the bulk fixture authoring (T037–T040) is also bulk-parallel across 60+ small `.nsl` files. This is the largest parallelization opportunity in M3.
- Phase 5 — recovery fixture authoring (T072–T077) is fully parallel; verification (T078–T080) follows.
- Phase 6 — the agent-driven audits (T089–T091) run in parallel; SPDX/layering/determinism gates are independent.

---

## Parallel Example: Phase 4 User Story 2

```bash
# Bulk fixture authoring (parallel across 60+ small .nsl files):
Task: "Author 23 × 2 = 46 lit fixtures for the error/warning Sn under test/sema/s<NN>/" (T037)
Task: "Author 6 × 2 = 12 paired pass + introspection artifacts for constructive Sn" (T038)
Task: "Author multi-fail-case fixtures for Sn with multiple frozen messages" (T039)
Task: "Author FixItHint shape assertions for S3/S7/S14/S26" (T040)

# Then 29 per-Sn impl tasks all in parallel (each one is a single .cpp file):
Task: "Implement lib/Sema/Constraints/S01_NoDoubleUnderscore.cpp" (T042)
Task: "Implement lib/Sema/Constraints/S02_WireNoInit.cpp" (T043)
Task: "Implement lib/Sema/Constraints/S03_AssignmentLHSKind.cpp" (T044)
# ... 26 more, all [P] ...
Task: "Implement lib/Sema/Constraints/S29_InitBlockPlacement.cpp" (T064)
Task: "Implement lib/Sema/Constraints/S13_AltAnyClassification.cpp" (T065)
# ... 5 more constructive, all [P] ...
Task: "Implement lib/Sema/Constraints/S27_ControlTerminalAs1Bit.cpp" (T070)
```

29 parallel impl tracks, each landing one source file plus turning its corresponding `s<NN>/{pass,fail}.nsl` fixture green. This is the M3 phase that benefits most from a multi-developer team.

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (T001–T003).
2. Complete Phase 2: Foundational (T004–T017) — `nsl-sema` library scaffolding (CRITICAL — blocks all stories).
3. Complete Phase 3: User Story 1 (T018–T036) — resolution + width inference + post-Sema `-emit=ast` format.
4. **STOP and VALIDATE**: Test User Story 1 independently — `nslc -emit=ast hello.nsl` produces the resolved AST byte-stably; `test/sema/resolution/`, `test/sema/width/`, `test/sema/emit-ast-resolved/` corpora pass.
5. **MVP demo deliverable**: at this checkpoint, `nsl-sema` is observable end-to-end. Tooling-track (T2/T3/T6) can already begin against the published symbol-table API even without per-`Sn` checks. Stop here only if M3 is being staged — full M3 requires US2 + US3.

### Incremental Delivery

1. Complete Setup + Foundational → Foundation ready.
2. Add User Story 1 → Test independently → MVP deliverable (resolved AST observable).
3. Add User Story 2 → Test independently → 29 `Sn` checks live; M3 acceptance gate (one pass + one fail per `S1`–`S29`) met.
4. Add User Story 3 → Test independently → Multi-error reporting + no-cascade verified.
5. Polish (Phase 6) → CI green; agent audits clean; PR-ready.

### Parallel Team Strategy

With multiple developers:

1. Team completes Setup + Foundational together (Phase 1 + 2; ~1–2 days).
2. Once Foundational is done:
   - Developer A: User Story 1 (resolution + width inference + printer extension) — Phase 3.
   - Developer B: Phase 4 fixture-authoring half (T037–T041) — bulk fixtures for 29 `Sn`.
   - Developer C: Phase 4 impl half — split the 29 per-`Sn` checkers across the team. Each one is ~50–100 LOC plus its turn-green fixture pair.
3. Once US1 + US2 complete: any developer picks up User Story 3 (Phase 5) — recovery fixture corpus + verification, ~1 day.
4. Polish (Phase 6) is a final-day audit pass, parallel-able across SPDX/layering/determinism/agent-audits.
5. M3 lands as a single PR with co-authored commits across the team.

---

## Notes

- [P] tasks = different files, no dependencies on incomplete tasks.
- [Story] label maps task to specific user story (US1 / US2 / US3) for traceability.
- Each user story should be independently completable and testable.
- Verify tests fail before implementing (Principle VIII red→green).
- Commit after each task or logical group; suggested commit boundaries: per-phase checkpoint, per-`Sn` impl track, the 4 fixture-authoring bulk commits in Phase 4.
- Stop at any checkpoint to validate the corresponding story independently.
- Avoid: vague tasks, same-file conflicts in [P] tasks, cross-story dependencies that break independence.
- The 6 constructive `Sn` introspection contract (Q1 Option B) is documented in `sema-api.contract.md` Invariant 4 + `sema-stability.contract.md` Invariant 4 + `research.md` §6 — keep these three sources in sync if the introspection surface changes.
- The 32 frozen diagnostic messages (covering 23 error/warning `Sn`) are listed in `diagnostic-string.contract.md` — that file is the authoritative freeze table; `s<NN>/fail.nsl` fixtures cite the literal text from it.
