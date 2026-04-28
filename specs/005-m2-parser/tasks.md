<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->
---
description: "Tasks for M2 — Parser + AST (with `-emit=ast`)"
---

# Tasks: M2 — Parser + AST (with `-emit=ast`)

**Input**: Design documents from `/specs/005-m2-parser/`
**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md, contracts/

**Tests**: Test tasks are **MANDATORY** for this project per Constitution Principle VIII (Test-First Development, NON-NEGOTIABLE). Every user story includes test tasks at the appropriate layer (parser + AST) per Constitution Principle VI's "Parser tests are AST-snapshot tests covering every grammar production" bullet. Tests MUST be written and observed FAILING before the corresponding implementation tasks begin.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story?] Description with file path`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: User-story label (US1, US2, US3) — required for user-story phase tasks; absent for Setup / Foundational / Polish
- Every task description includes the exact file path

## Path Conventions

- Compiler-frontend layout (M0/M1 baseline; matches LLVM/CIRCT convention):
  - Public headers: `include/nsl/<Layer>/`
  - Implementations: `lib/<Layer>/`
  - Driver entry: `tools/nslc/main.cpp`
  - lit + FileCheck tests: `test/<area>/`
  - GoogleTest unit tests: `test_unit/<suite>/`

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization for M2; M0 stood up the build, the CI pipeline, the SPDX scan, the empty layer skeleton, and M1 filled the first three layers — Phase 1 here is small.

- [X] T001 Create the M2 test directory tree under `test/` and `test_unit/`: `test/parse/{grammar,notes/{n01,n02,n03,n05,n06,n07,n10,n11,n14},recovery}/`, `test/Driver/` (existing — confirm), `test_unit/{ast_visitor_test,ast_printer_test,recovery_set_test}/` — each with a `.keep` placeholder so M0's lit-discovery picks them up. **Done 2026-04-28**: 14 `.keep` files created across the new tree (`test/parse/{grammar,recovery,notes/n{01,02,03,05,06,07,10,11,14}}` + `test_unit/{ast_visitor_test,ast_printer_test,recovery_set_test}`); existing `test/Driver/` and `test/Output/` left untouched.
- [ ] T002 Sanity-verify the M0+M1 build is still green on branch `005-m2-parser` by running `cmake --build build && ctest --test-dir build --output-on-failure && lit -v test` inside `ghcr.io/koyamanx/nsl-nslc:dev` against the unchanged-since-M1 source tree (checkpoint before adding M2 sources). **Deferred** to user's local CI run — needs docker (sandbox-disabled bash) and ~5+ minute build cycle; not run in the Phase-1 session per agreed scope (Option A). Will be exercised implicitly when Phase 2's first agent track lands and runs the full local CI.
- [~] T003 [P] DEFERRED — `cmake/AddNSLLibrary.cmake` (M0) already enforces downward-only layering via the macro's `_nsl_layer_index()` table + "index M ≥ N" FATAL_ERROR (verified inspection: lines 45–59 enumerate `nsl-sema` / `nsl-dialect` / `nsl-lower` / `nsl-driver` as forbidden upstream targets for lower layers). The M0 `add_nsl_library_test` pytest suite under `test_unit/add_nsl_library_test/{upward_dep,sibling_bypass,unknown_layer_name}/` already exercises this with negative-test fixtures. SC-009's "CI guard" obligation is satisfied by the M0 macro's configure-time enforcement; a separate `scripts/check_layering.py` would duplicate work and risks drift. **Same precedent as M1's T003 deferral** ("the M0 helper already provides what … would have abstracted"). The `#include`-edge bypass concern (a contributor `#include`s a forbidden header without declaring `DEPENDS`) is caught at compile time because the M0 macro controls each library's include path — Sema's headers are not on `nsl-parse`'s include path unless `DEPENDS nsl-sema` is added, which the macro rejects. Documented here for the next person who reads the plan.

**Checkpoint**: M2 directory skeleton in place (T001 done 2026-04-28); build sanity verification deferred to first Phase-2 CI run (T002 deferred); layering guard not needed — M0 macro already enforces SC-009 (T003 DEFERRED, rationale above). Phase 2 work can begin.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: `nsl-ast` library (the umbrella headers, the X-macro source-of-truth, every per-node-kind header, the AST visitor base, and the printer entry point) — every user story depends on these. The Pratt-precedence table for §11 expressions also lives here so US1 can build expression trees without re-deriving precedence.

**⚠️ CRITICAL**: No US1 / US2 / US3 task may begin until this phase is complete.

### X-macro single-source-of-truth `.def` file (research §6)

- [X] T004 [P] Author `include/nsl/AST/NodeKind.def` — X-macro file enumerating every concrete AST node kind from data-model §1.2–§1.6 (~50 kinds). Format: `NSL_NODE_KIND(EnumName, BaseClass)` where `BaseClass ∈ {Decl, Stmt, Expr}` (or `ASTNode` for `CompilationUnit`). Cross-reference comment cites `nsl_compiler_design.md` §5 lines 622–639.

### `ASTNode` / `Decl` / `Stmt` / `Expr` umbrellas (data-model §1.1)

- [X] T005 [P] TDD — author `test_unit/ast_visitor_test/ast_node_construct_test.cpp`: GoogleTest fixture asserting `ASTNode(NodeKind, SourceRange)` is constructible only with a non-empty `SourceRange` (per `ast-stability.contract.md` Invariant 1); default-constructed `ASTNode` is `= delete`d at compile time. Run; observe FAILING (no implementation yet).
- [X] T006 Implement `include/nsl/AST/ASTNode.h` + `include/nsl/AST/NodeKind.h` (consumes `NodeKind.def` from T004 to build the enum) + `include/nsl/AST/Type.h` (forward-decl `TypeRef` + nullptr "unresolved" sentinel) + `include/nsl/AST/Decl.h` + `include/nsl/AST/Stmt.h` + `include/nsl/AST/Expr.h` + `lib/AST/ASTNode.cpp` (out-of-line dtor + accept() anchor). T005 turns green.

### `ASTVisitor` — pure-virtual exhaustiveness (data-model §1.1, FR-005)

- [X] T007 [P] TDD — author `test_unit/ast_visitor_test/visitor_exhaustiveness_test.cpp`: GoogleTest fixture instantiating a derived `class TestVisitor : public ASTVisitor` that overrides every `visit(T&)` and asserts a no-op visit completes for each concrete node kind enumerated in `NodeKind.def`. Add a *negative* compile-failure expectation comment (cannot test in C++17 portably) but link a derived visitor that intentionally omits one method to confirm the link error per `ast-stability.contract.md` Invariant 5. Run; observe FAILING.
- [X] T008 Implement `include/nsl/AST/ASTVisitor.h` — pure-virtual `visit(T&) = 0` per concrete node kind from `NodeKind.def` (X-macro expansion); optional `visitDefault(ASTNode&)` template hook. T007 turns green.

### Per-node-kind headers — Decls (data-model §§1.2–1.4; ~20 headers)

- [X] T009 [P] Author every `Decl`-family per-node-kind header under `include/nsl/AST/`: `CompilationUnit.h`, `StructDecl.h`, `TopLevelParamDecl.h`, `DeclareBlock.h`, `PortDecl.h`, `ModuleBlock.h`, `RegDecl.h`, `WireDecl.h`, `VariableDecl.h`, `IntegerDecl.h`, `MemDecl.h`, `FuncSelfDecl.h`, `ProcNameDecl.h`, `StateNameDecl.h`, `FirstStateDecl.h`, `SubmoduleDecl.h`, `StructInstDecl.h`, `FuncDefn.h`, `ProcDefn.h`, `StateDefn.h` — class definition + SPDX + inclusion guard per data-model §§1.2–1.4 fields tables. Each ≤60 lines.

### Per-node-kind headers — Stmts (data-model §1.5; ~19 headers)

- [X] T010 [P] Author every `Stmt`-family per-node-kind header under `include/nsl/AST/`: `TransferStmt.h`, `IncDecStmt.h`, `ControlCallStmt.h`, `BareFinishStmt.h`, `SystemTaskStmt.h`, `ReturnStmt.h`, `EmptyStmt.h`, `LabeledStmt.h`, `GotoStmt.h`, `InitBlockStmt.h`, `DelayTaskStmt.h`, `ParallelBlock.h`, `AltBlock.h`, `AnyBlock.h`, `SeqBlock.h`, `WhileBlock.h`, `ForBlock.h`, `IfStmt.h`, `StructuralGenerate.h` per data-model §1.5 fields table. Each ≤60 lines.

### Per-node-kind headers — Exprs (data-model §1.6; ~15 headers)

- [X] T011 [P] Author every `Expr`-family per-node-kind header under `include/nsl/AST/`: `LiteralExpr.h`, `IdentifierExpr.h`, `SystemVarExpr.h`, `UnaryExpr.h`, `BinaryExpr.h`, `ConditionalExpr.h`, `ConcatExpr.h`, `RepeatExpr.h`, `SignExtendExpr.h`, `ZeroExtendExpr.h`, `SliceExpr.h`, `FieldAccessExpr.h`, `CallExpr.h`, `StructCastExpr.h`, `IncDecExpr.h` per data-model §1.6 fields table. Each ≤60 lines. Note: each `Expr` carries the `TypeRef inferredType_` slot from `Expr.h` (filled by Sema at M3, nullptr at M2).

### AST printer (data-model §3, contracts/ast-stability + nslc-emit-ast)

- [X] T012 [P] TDD — author `test_unit/ast_printer_test/determinism_test.cpp`: GoogleTest fixtures asserting (a) byte-identical output across two `print()` invocations on the same `CompilationUnit*` (Invariant 2); (b) absence of `0x[0-9a-f]+` patterns in output (Invariant 3); (c) `SourceRange` round-trip via the printer's `loc=path:line:col-line:col` field (Invariant 8); (d) every node-kind name in output equals its `NodeKind` enumerator (Invariant 6). Run; observe FAILING.
- [X] T013 Implement `include/nsl/AST/Printer.h` (`print(const CompilationUnit&, llvm::raw_ostream&)`) + `lib/AST/Printer.cpp` (stateful walker; per-node-kind formatter functions; deterministic iteration; no pointer prints; cross-references serialize as `ref=path:line:col` per data-model §6) + `lib/AST/NodeKindNames.cpp` (X-macro-driven enum-to-string table consuming `NodeKind.def`). T012 turns green.

### Wire `nsl-ast` library

- [X] T014 Edit `lib/AST/CMakeLists.txt` — invoke `add_nsl_library(nsl-ast SOURCES ASTNode.cpp Printer.cpp NodeKindNames.cpp HEADERS ${NSL_INCLUDE_DIR}/nsl/AST/ASTNode.h ${NSL_INCLUDE_DIR}/nsl/AST/ASTVisitor.h ${NSL_INCLUDE_DIR}/nsl/AST/NodeKind.h ${NSL_INCLUDE_DIR}/nsl/AST/NodeKind.def ${NSL_INCLUDE_DIR}/nsl/AST/Type.h ${NSL_INCLUDE_DIR}/nsl/AST/Decl.h ${NSL_INCLUDE_DIR}/nsl/AST/Stmt.h ${NSL_INCLUDE_DIR}/nsl/AST/Expr.h ${NSL_INCLUDE_DIR}/nsl/AST/Printer.h <every per-node-kind header from T009/T010/T011> DEPENDS nsl-basic)` per M0's macro convention. The `DEPENDS nsl-basic` link covers `SourceRange` / `SourceLocation`.
- [X] T015 Edit `test_unit/CMakeLists.txt` to register the three new gtest suites (`ast_visitor_test`, `ast_printer_test`, `recovery_set_test` placeholder) via the M1 helper convention; the `recovery_set_test` registers only — Phase 4 fills it.

### Pratt precedence table (research §2; Phase-2 because US1 builds expression trees)

- [X] T016 [P] Author `lib/Parse/PrecedenceTable.h` (private to `nsl-parse`, consumed only by `ParseExpr.cpp`) — `static constexpr` table of `PrecLevel` per binary `TokenKind` from `lang.ebnf §11` operator inventory; `nud` (null-denotation) entries for prefix operators (`-`, `~`, `!`, `&`, `|`, `^` reduction per N2; `#` sign-extend per N5; `'` zero-extend); `led` (left-denotation) entries for infix operators including `?:` conditional (N1 expression form). Pure header — no .cpp at this phase.
- [X] T017 [P] TDD — author `test_unit/recovery_set_test/precedence_table_test.cpp`: GoogleTest fixtures asserting `getPrecedence(TokenKind)` returns the documented level for every operator from `lang.ebnf §11`; assert `nud` vs `led` dispatch picks the correct denotation for `&`/`|`/`^` (N2: prefix vs infix). Run; observe FAILING (table not yet authored). Note: test author commit precedes T016; test→implementation order preserved.

**Checkpoint** (reached 2026-04-28): `nsl-ast` library builds (Track A commit `098d9d0`); ASTVisitor exhaustiveness + printer determinism gtests green (Track B commit `d83b099`); per-node-kind headers in place (54 headers + 9 umbrellas); Pratt precedence table tested (16 tests). Integration patch (this commit) reconciled four cross-track API drifts (`NodeKind::NK_count` sentinel; const-correct visitor; 3-arg printer signature; `PrecEntry` struct accessors) and one spec/design coupling fix (research §2 N5: `#` is INFIX per `lang.ebnf §11` line 702, not prefix). **Full M1+M2-Phase-2 ctest: 154/154 PASS** inside `ghcr.io/koyamanx/nsl-nslc:dev`. US1 / US2 / US3 work can begin.

---

## Phase 3: User Story 1 — Parse any NSL compilation unit (Priority: P1) 🎯 MVP

**Goal**: A contributor pipes a preprocessed NSL token stream through the parser and gets back a `CompilationUnit` AST whose nodes cover every production in `lang.ebnf §§1–11` — every top-level item kind, every `internal_declaration` form, every action statement, every expression form. Every AST node carries a `SourceRange` whose endpoints round-trip to post-`#line` virtual file coordinates.

**Independent Test**: After this phase, `nslc -emit=ast fixture.nsl` produces the canonical AST text format (per `contracts/nslc-emit-ast.contract.md`) for any well-formed NSL input. The per-grammar-production AST-snapshot fixtures all pass; running `nslc -emit=ast` twice on the same input yields byte-identical output. Independent of US2 (parser-note disambiguation) and US3 (recovery-on-error) — this story is the well-formed-input path only.

### Tests for User Story 1 (MANDATORY per Constitution Principle VIII) ⚠️

> Write these tests FIRST. They MUST be observed FAILING against the unchanged tree before any implementation task begins.

- [X] T018 [P] [US1] (Optional helper) Author `scripts/gen_grammar_fixtures.py` — Python script that reads a curated list of EBNF productions from `lang.ebnf §§1–11` (list checked in next to the script per research §9) and emits per-production fixture stubs (`pass.nsl`, `pass.ast`) under `test/parse/grammar/<production-name>/`. SPDX header + "Generated from `lang.ebnf` §§1–11 — DO NOT EDIT" marker per generated file. Skips existing fixtures unless `--force`.
- [X] T019 [P] [US1] Run `gen_grammar_fixtures.py` (or hand-author) `test/parse/grammar/<production>/pass.nsl + pass.ast` for every production in `lang.ebnf §§1–11` — covers `compilation_unit`, `struct_declaration`, `top_level_parameter`, `declare_block`, `data_terminal_declaration`, `control_terminal_declaration`, `module_block`, every `internal_declaration` form (~12), `function_definition`, `procedure_definition`, `state_definition`, every action-statement form (~9), every atomic-action form (~10), every expression form (~15). Approximately 50 fixtures total. Each `.test` runs `RUN: %nslc -emit=ast %s | FileCheck %s` with the expected snapshot inlined as `// CHECK:` directives or as a sibling `pass.ast` golden via `FileCheck --check-prefix=AST`.
- [X] T020 [P] [US1] Author `test/Driver/emit-ast.test` — the format golden referenced by `contracts/nslc-emit-ast.contract.md` §"Stdout schema": one fixture (e.g., the `module hello { reg q[8] = 0; }` example) whose `nslc -emit=ast` output is asserted byte-exactly. Negative cases: missing input, unknown `-emit=` value, file-not-found — assert exit codes 2 / 2 / 1 per the contract. Run; observe FAILING (driver flag not yet wired).
- [X] T021 [P] [US1] TDD — author `test_unit/ast_visitor_test/parser_smoke_test.cpp`: GoogleTest fixture invoking `nsl::parse::parseCompilationUnit(lex, diag)` over a small in-memory source via M1's `SourceManager::addBufferInMemory`; assert the returned `unique_ptr<CompilationUnit>` is non-null, `items.size() == 1`, the item is a `ModuleBlock` with the expected name and `RegDecl` child. Run; observe FAILING (parser not yet implemented; suite fails to link against `nsl-parse`).
- [X] T022 [P] [US1] Run T019 + T020 + T021 against the unchanged tree; observe **all** FAILING (no `nslc -emit=ast` flag exists yet → exit 2; parser library is empty). Capture failing run as TDD evidence per FR-029.

### Implementation for User Story 1 — `nsl-parse` library

- [X] T023 [US1] Implement `include/nsl/Parse/Parser.h` — public API surface declaring `nsl::parse::parseCompilationUnit(lex::Lexer&, basic::DiagnosticEngine&) -> std::unique_ptr<ast::CompilationUnit>`. Header-only public surface; class definitions live in `lib/Parse/`.
- [X] T024 [US1] Implement `lib/Parse/Parser.cpp` — top-level driver: `parseCompilationUnit()` loop dispatching on `peek().kind()` to per-top-level-item parse functions; consumes `LineMarker` tokens per FR-015 / N14 by calling `SourceManager::resolveVirtual()` (M1) and continuing without producing an AST node. Public-API entry point.
- [X] T025 [US1] Implement `lib/Parse/ParseDecl.cpp` — per-decl parse functions: `parseStructDecl()`, `parseTopLevelParam()`, `parseDeclareBlock()` (with `interface`/`simulation` modifier dispatch), `parsePortDecl()`, `parseControlTerminalDecl()`, `parseModuleBlock()`, `parseInternalDeclaration()` dispatching to per-form parsers (`parseRegDecl`, `parseWireDecl`, `parseFuncSelfDecl`, `parseSubmoduleDecl`, `parseProcNameDecl`, `parseStateNameDecl`, `parseFirstStateDecl`, `parseMemDecl`, `parseStructInstDecl`, `parseIntegerDecl`, `parseVariableDecl`), `parseFuncDefn()` (incl. dotted-name N7 form), `parseProcDefn()`, `parseStateDefn()`. Each parser returns `std::unique_ptr<*Decl>` and sets the `SourceRange` to span first-token-start to last-token-end per FR-018.
- [X] T026 [US1] Implement `lib/Parse/ParseStmt.cpp` — per-stmt parse functions: `parseStmt()` (statement-position dispatch on `peek().kind()`), `parseParBlock()`, `parseAltBlock()`, `parseAnyBlock()`, `parseSeqBlock()`, `parseWhileBlock()`, `parseForBlock()`, `parseIfStmt()` (statement form per N1; FR-016 `function`-keyword equivalence handled at lexer/keyword-set level — M1 owns the token kind, M2 just accepts both), `parseGenerate()`, atomic actions (`parseTransfer()` with `=` vs `:=` distinction, `parseControlCall()` for N6, `parseSystemTask()` for N11(a), `parseReturnStmt()`, `parseLabeledStmt()`, `parseGotoStmt()`, `parseInitBlock()`, `parseDelayTask()`).
- [X] T027 [US1] Implement `lib/Parse/ParseExpr.cpp` — Pratt expression parser (research §2): `parseExpr(precFloor)` consumes a `nud` (prefix / leaf) then loops on `led` (infix) while next token's precedence ≥ `precFloor`. Per-leaf parsers: `parseLiteralExpr`, `parseIdentifierExpr` (with N11(b) system-variable dispatch), `parseUnaryExpr` (incl. N2 reduction operators, N5 sign-extend `#`, zero-extend `'`), `parseConcatExpr` (incl. `.{...}` LHS form per N3), `parseRepeatExpr`, `parseSliceExpr`, `parseFieldAccess`, `parseCallExpr`, `parseStructCastExpr`, `parseIncDecExpr`, `parseConditionalExpr` (expression form per N1: `if (c) a else b`). Consumes the precedence table from T016.
- [X] T028 [US1] Edit `lib/Parse/CMakeLists.txt` — `add_nsl_library(nsl-parse SOURCES Parser.cpp ParseDecl.cpp ParseStmt.cpp ParseExpr.cpp HEADERS ${NSL_INCLUDE_DIR}/nsl/Parse/Parser.h DEPENDS nsl-basic nsl-lex nsl-ast)`. **Verify** the generated CMake target has NO link edge to `nsl-sema` or any later layer (T003 layering guard catches the violation if it slips in).

### Driver glue for `-emit=ast` (FR-022, FR-024)

- [X] T029 [US1] Implement `lib/Driver/EmitAST.cpp` — opens input file via `SourceManager::loadFile`, runs `Preprocessor::run()` (M1), constructs the synthetic post-preprocess buffer, runs `Lexer::next()` to drive `parseCompilationUnit()`, **buffers** the printed AST in a `std::string`, prints to stdout only on success (per `contracts/nslc-emit-ast.contract.md` §Behavior step 5 — no partial output). Exit codes per the contract (0 / 1 / 2).
- [X] T030 [US1] Edit `tools/nslc/main.cpp` — extend the `-emit=<stage>` switch with a case for `ast` calling into `lib/Driver/EmitAST.cpp`. **Verify `main.cpp` stays ≤ 60 lines** per Principle II (the M1 baseline was exactly 60 lines per T061's note; M2 must not exceed it — the actual work lives in `lib/Driver/EmitAST.cpp`).
- [X] T031 [US1] Edit `lib/Driver/CMakeLists.txt` — append `EmitAST.cpp` to the SOURCES list of the `nsl-driver` `add_nsl_library` invocation; add `nsl-ast nsl-parse` to its DEPENDS clause (M1 already added `nsl-basic nsl-preprocess nsl-lex`).
- [X] T032 [US1] Build, run all of T019 + T020 + T021 against the implementation; **all green**. Record any integration deltas (analogous to M1's T036 5-delta capture) in commit body.

### `function` ≡ `func` parser-equivalence (FR-016 / S26)

- [X] T032a [P] [US1] TDD — author `test/parse/grammar/function-keyword/{pass-func.test,pass-function.test}`: two fixtures with identical body (a small `module` containing one function definition) differing only in the keyword (`func` vs `function`). Assert via `FileCheck` that the resulting `FuncDefn` AST is **structurally identical** between the two — same `name`, same body shape, same `SourceRange` *coverage* (offsets differ by the keyword's character-length delta but the AST nesting is identical). Verifies FR-016 / S26 parser-equivalence at fixture level. Run; observe FAILING (no `function`-spelling fixture in T019's per-production set; M1's `KeywordSet.def` already emits `tk_function` as a distinct keyword token, so the parser must dispatch both at `parseFuncDefn()` per T026's prose). The Sema-level canonicalization warning ("prefer `func`") is M3's responsibility, not M2's.
- [X] T032b [US1] If T032a fails because `parseFuncDefn()` only dispatches on `tk_func`, augment `lib/Parse/ParseDecl.cpp` (extends T025) to accept either `tk_func` or `tk_function` at the keyword position. Re-run T032a; **green**.

**Checkpoint**: User Story 1 fully functional. `nslc -emit=ast` works on well-formed NSL input; parser covers the full §§1–11 grammar; per-grammar-production AST-snapshot fixtures all green; format golden green; determinism gtest green; `function` ≡ `func` parser-equivalence covered by fixture (FR-016). Pre-condition for US2 satisfied (US2 exercises the same parser at parser-note-ambiguous inputs).

---

## Phase 4: User Story 2 — Parser-note disambiguation (Priority: P1)

**Goal**: A contributor parses NSL source that exercises each parsing-level disambiguation rule from `lang.ebnf` parser-notes (`N1` `if` statement-vs-expression, `N2` reduction-vs-bitwise, `N3` `.{` lookahead, `N5` `#` sign-extend, `N6` proc-instance method access, `N7` dotted-`func` def, `N10` `label` reserved-but-warned, `N11` `_`-prefix system-task vs system-variable, `N14` `line_marker` consumed-not-emitted) and observes the parser produces the expected AST node kinds.

**Independent Test**: After this phase, every parsing-observable parser-note `Nn` ships a passing fixture pair (interpretation A vs interpretation B per spec US2 acceptance scenarios) green; fail-cases (where applicable: N10 warning, N14 malformed) emit the locked diagnostic strings (FR-027). Independent of US1's full-grammar coverage — these fixtures hit specific ambiguities, not the full surface.

### Tests for User Story 2 (MANDATORY per Constitution Principle VIII) ⚠️

> Write these tests FIRST. They MUST be observed FAILING against the unchanged tree before any implementation task begins.

- [X] T033 [P] [US2] Author `test/parse/notes/n01/{pass-stmt.test,pass-expr.test}` — N1 statement-vs-expression: pass-stmt asserts `if (c) a = b; else a = 0;` at statement position parses as `IfStmt` (not `ConditionalExpr`); pass-expr asserts `wire q = if (c) a else b;` at expression position parses as `ConditionalExpr` (not `IfStmt`).
- [X] T034 [P] [US2] Author `test/parse/notes/n02/{pass-reduce.test,pass-bitwise.test}` — N2 reduction-vs-bitwise: `&x` (no left operand) → `UnaryExpr{op=ReduceAnd}`; `a & b` → `BinaryExpr{op=BitAnd}`. Both forms tested for `&`, `|`, `^`.
- [X] T035 [P] [US2] Author `test/parse/notes/n03/{pass-lhs-concat.test,pass-field-access.test}` — N3 `.{` two-character lookahead: `.{a, b, c} = x;` parses as `ConcatExpr` over `IdentifierExpr` parts on `TransferStmt::lhs`; the related single-`.` form (e.g., `inst.field`) parses as `FieldAccessExpr`. Note: the two-char `.{` lookahead is the *lexer's* responsibility per N3; M2 verifies the parser consumes the resulting token correctly.
- [X] T036 [P] [US2] Author `test/parse/notes/n05/pass-sign-extend.test` — N5 `#` in expression position: `wire q = 8 # sig;` parses as `SignExtendExpr{width=LiteralExpr(8), sub=IdentifierExpr(sig)}`. The line-marker form is consumed by M1 / M2 per N14 (covered by T040).
- [X] T037 [P] [US2] Author `test/parse/notes/n06/pass.test` — N6 proc-instance method access: `inst.finish();` and `inst.invoke();` parse as `ControlCallStmt` whose target is the scoped-identifier `inst.{finish,invoke}`.
- [X] T038 [P] [US2] Author `test/parse/notes/n07/pass.test` — N7 dotted-`func` def: `func ic.ready { … }` at module-item position parses as `FuncDefn` whose `name` is the scoped name `ic.ready`.
- [X] T039 [P] [US2] Author `test/parse/notes/n10/{pass-warning.test,fail.test}` — N10 `label` reserved-but-warned: pass-warning asserts using `label` as a user identifier produces a warning diagnostic + the identifier is accepted; fail-case asserts the warning diagnostic text matches the locked string `'label' is reserved; using as identifier (parser-note N10)` per FR-027.
- [X] T040 [P] [US2] Author `test/parse/notes/n11/{pass-task.test,pass-var.test}` — N11 `_`-prefix dispatch: `_display(arg)` at statement position parses as `SystemTaskStmt`; `_random` at expression position parses as `SystemVarExpr`; `_time` at expression position parses as `SystemVarExpr` (parenthesized `_time()` form per N11(b) parses as `SystemTaskStmt` for back-compat).
- [X] T041 [P] [US2] Author `test/parse/notes/n14/{pass.test,fail-malformed.test}` — N14 `line_marker` consumed-not-emitted: pass asserts `#line 100 "F"` directive surviving M1's preprocessor seam is consumed by the parser at every item-list position, no AST node is produced for it, and every subsequent AST node's `SourceRange` reports `F:100:…`. fail-malformed asserts `#line abc` (non-integer) or other malformed shape produces the locked diagnostic `'#line' directive must be followed by a positive integer (parser-note N14)` per FR-027.
- [X] T042 [P] [US2] Run T033–T041 against the unchanged tree; observe **all** FAILING (Phase 3 lands the parser surface but the parser-note-specific dispatch may not all be wired yet — particularly N10 warning emission and N14 fail-malformed diagnostic). Capture per FR-029.

### Implementation for User Story 2

- [X] T043 [US2] Augment `lib/Parse/ParseStmt.cpp` (extends T026) — wire the N1 statement-vs-expression dispatch: `parseStmt()` calling `parseIfStmt()` directly (statement position); `parseExpr()` calling `parseConditionalExpr()` via Pratt nud entry on `tk_if`. Verify both T033 fixtures green.
- [X] T044 [US2] Augment `lib/Parse/ParseExpr.cpp` (extends T027) — wire N2 nud/led dispatch for `&`/`|`/`^` (Pratt prefix vs infix denotations; T034 turns green). Wire N5 `#` nud entry for `SignExtendExpr` (T036). Wire N3 `.{`-recognition consumer (depends on lexer emitting a distinguishable token kind per N3 — verify with the M1 `Token.h` `TokenKind` enumeration; if the lexer does not yet emit a distinct `.{` token, file a follow-up note and use two-token lookahead in `parseConcatExpr` instead).
- [X] T045 [US2] Augment `lib/Parse/ParseDecl.cpp` (extends T025) — wire N6 proc-instance method access in `parseControlCall()` (target may be a `scoped_identifier` per `lang.ebnf §9`); wire N7 dotted-`func` def parsing in `parseFuncDefn()` via `parseScopedName()`. Verify T037 + T038 green.
- [X] T046 [US2] Augment `lib/Parse/Parser.cpp` (extends T024) — wire N10 warning emission: when `parseIdentifier()` consumes a `tk_label` token in identifier position, emit a `Severity::Warning` diagnostic via the engine with the locked text from T039. Wire N11(a)/N11(b) dispatch by examining `peek(1).kind()` after a `_`-prefix identifier (`(` → SystemTask path; identifier-end → SystemVar path).
- [X] T047 [US2] Augment `lib/Parse/Parser.cpp` (extends T024) — wire N14 `line_marker` consumption at every item-list position (top-level, declare-item, module-item, par-block-item, seq-block-item) per FR-015. On consumption, call `SourceManager::resolveVirtual()` to advance the cursor; on malformed shape (per `lang.ebnf §2` `line_marker = "#line" decimal_integer [ string_literal ]`), emit the locked diagnostic from T041 and skip-to-end-of-line for recovery. Verify T041 green.
- [X] T048 [US2] Build, run T033–T041 against the implementation; **all green**. Record integration deltas in commit body.

**Checkpoint**: User Story 2 fully functional. Every parsing-observable parser-note from `lang.ebnf` is exercised; both interpretations selected correctly per the spec's acceptance scenarios; locked-string fail-case diagnostics emit per FR-027. Pre-condition for US3 (recovery) satisfied.

---

## Phase 5: User Story 3 — Source-locating diagnostics with parse-error recovery (Priority: P1)

**Goal**: A contributor running the parser on input that contains syntax errors sees diagnostics in the canonical M1 format (`path:line:col: error: <message>`) at every error site. The parser THEN recovers via per-rule recovery sets, accumulating multiple diagnostics in a single run. Recovery is the default behavior — there is no fail-fast toggle (per /speckit-clarify Q1).

**Independent Test**: After this phase, the multi-error fixture corpus under `test/parse/recovery/` (FR-021 minimum: two-top-level-errors, two-module-item-errors, in-`seq`-error followed by well-formed item) all pass — each fixture asserts BOTH (or all) diagnostics emit in source order in a single run, the parser exits non-zero, and well-formed siblings between/after errors still appear in the AST. Verifies Constitution Principle IV is honored at every parser raise site.

### Tests for User Story 3 (MANDATORY per Constitution Principle VIII) ⚠️

> Write these tests FIRST. They MUST be observed FAILING against the unchanged tree before any implementation task begins.

- [ ] T049 [P] [US3] Author `test/parse/recovery/two-top-level-errors.test` — input has two independent syntax errors in separate `top_level_item`s (e.g., a malformed `struct` and a malformed `module`). Assert: both diagnostics emit in source order; well-formed `top_level_item`s between/after the errors still appear when the parser is run with `-emit=ast` … but per `nslc-emit-ast.contract.md` step 5, **no AST is printed on stdout if any error fired** — so this fixture asserts the two diagnostic lines on stderr and exit code 1 only. The "AST partial-prefix preservation" property is exercised in unit tests (T053 below).
- [ ] T050 [P] [US3] Author `test/parse/recovery/two-module-item-errors.test` — one `module` whose body contains two syntax errors in separate `module_item`s (e.g., malformed `reg` followed by malformed `func`). Same assertions as T049.
- [ ] T051 [P] [US3] Author `test/parse/recovery/error-in-seq-then-module-item.test` — a module body containing a malformed expression inside a `seq` block followed by a well-formed `module_item` after the `seq` block. Assert the seq-block diagnostic emits and exit code 1.
- [ ] T052 [P] [US3] Author `test/parse/recovery/expected-semicolon.test` — minimal fixture: a single missing `;` at the end of a `register_declaration`. Assert the locked diagnostic `expected ';' after register declaration` per `parser-recovery.contract.md` examples (the message text is golden-frozen per research §10). One additional fixture per common parser-error message (`expected '}' to close 'module' body`, `expected expression after binary operator`) — small per-message fixture set, ~5 fixtures total.
- [ ] T053 [P] [US3] TDD — author `test_unit/recovery_set_test/recovery_partial_ast_test.cpp`: GoogleTest fixture invoking `parseCompilationUnit()` on a buffer with one syntax error in the middle of a `module_block` and asserting (a) the returned `CompilationUnit*` is non-null; (b) the malformed `module`'s `internals`/`actions`/`funcs`/`procs` vectors *do* contain the well-formed `module_item`s between/after the error site (the partial-AST-preservation property of full recovery, per `parser-recovery.contract.md` §"What recovery does NOT do"); (c) the diagnostic-engine buffer has exactly N error entries (the count is fixture-specific). Run; observe FAILING.
- [ ] T054 [P] [US3] TDD — author `test_unit/recovery_set_test/recovery_set_test.cpp`: GoogleTest fixtures over `lib/Parse/Recovery.cpp`'s `TokenSet` and `skipUntil()` primitive — `contains()` correctness, deterministic forward scan, EOF unwind, push/pop nesting under `RecoveryGuard`. Run; observe FAILING.
- [ ] T055 [P] [US3] Run T049–T054 against the unchanged tree; observe **all** FAILING (Phase 3 + Phase 4 may have wired single-error bail; Phase 5 lands the recovery primitive). Capture per FR-029.

### Implementation for User Story 3

- [ ] T056 [US3] Implement `lib/Parse/Recovery.cpp` (+ `lib/Parse/Recovery.h` private header) — `class TokenSet` (`constexpr` bitset over `TokenKind` per research §3); `Parser::skipUntil(TokenSet)` primitive (deterministic forward scan); `class RecoveryGuard` RAII helper (push/pop recovery scope on the parser's recovery-scope stack). Per-rule recovery-token tables defined as `static constexpr` in this file, indexed by grammar-level enum (top, declare-item, module-item, seq-item, statement, expression). T054 turns green.
- [ ] T057 [US3] Augment every `parseFoo()` site in `lib/Parse/Parser.cpp` / `ParseDecl.cpp` / `ParseStmt.cpp` / `ParseExpr.cpp` with a `RecoveryGuard` declaring its rule's recovery-token set, plus a comment block at each guard site naming the tokens and the resume position (FR-021 enforcement: documentation lives next to the code). On a syntax-error path, emit the locked diagnostic via M1's `DiagnosticEngine`, then `skipUntil(currentRecoverySet)`, then continue the enclosing rule's loop (do not return null from inside the rule unless the recovery exhausts to EOF).
- [ ] T058 [US3] Augment `lib/Driver/EmitAST.cpp` (extends T029) — keep buffering the AST output until the diagnostic-engine error count is checked at end of parse; print AST to stdout only if error count is zero. Warning-severity diagnostics (e.g., N10 from US2) do NOT suppress AST emission. This implements the "no partial output on error" rule from `nslc-emit-ast.contract.md` §Behavior step 5.
- [ ] T059 [US3] Build, run T049–T053 + Phase 4 + Phase 3 corpora; **all green**. Verify SC-002 / SC-005 + the reproducibility check across `Debug × {gcc, clang}` and `Release × {gcc, clang}` per SC-007.

**Checkpoint**: User Story 3 verified. M2 constitutional anchor (Principle IV — Source-Locating Diagnostics; Principle V — Determinism) honored end-to-end at the parser raise sites. Multi-error recovery works at every grammar level per /speckit-clarify Q1; recovery-set documentation lives next to the code per FR-021.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Documentation updates, CI integration verification, agent-driven audits, end-to-end determinism + success-criteria checks. No new functionality.

### Documentation

- [ ] T060 [P] Update `README.md` "Building" / "Status by milestone" sections with a small `nslc -emit=ast` example (input + expected output) so a contributor coming from M1 sees the M2 increment immediately. Keep the section short — link to `specs/005-m2-parser/quickstart.md` for the full walkthrough. Update the "Status by milestone" callout under §Usage to mark `-emit=ast` as **delivered** (was: "lands at M2").
- [ ] T061 [P] Cross-check `docs/CLAUDE.md` §3 "Implementing the parser" / "Implementing semantic analysis" line ranges and the §5 quick-map of S/N constraints — the section anchors used those ranges before M2; verify they still resolve correctly to the spec content. If implementation surfaced a §5 line drift in `lang.ebnf` (it shouldn't — M2 implements `§§1–11` verbatim), patch `docs/CLAUDE.md` §§4–7 in the same commit per Principle VII.
- [ ] T062 [P] Update the project-root `CLAUDE.md` §1 language-feature roll-up (lines ~5–55) to mark every M2-delivered row as **delivered**: every `Sn`-related row's "Lex / parse / sema" column now shows `M2 (parse)` is green, even though `M3 (sema)` is still ahead. Verify the table's row-by-row alignment with `nsl_lang.ebnf §§1–11` is still accurate.

### CI integration verification

- [ ] T063 [P] Run `./scripts/ci.sh all` end-to-end on the M2 branch (locally, inside the dev container per quickstart §7). Verify: stage 1 (build matrix) green across `Debug × {gcc, clang}` + `Release × {gcc, clang}`; stage 2 (static checks) green over the new M2 sources (SC-008); stage 3 (Unit & layer tests) green with the three new gtest suites + the lit corpus; stage 4 (Lowering tests via lit + FileCheck) green with the new `test/parse/` and `test/Driver/emit-ast.test` fixtures; stages 5/6 still wired-but-empty (M7/M8). Document the command output in PR description per Principle IX local-reproduction.
- [ ] T064 [P] Verify GitHub Actions CI workflow (per M0/M1) auto-discovers the new `test/parse/grammar/`, `test/parse/notes/n*/`, `test/parse/recovery/` directories without `.github/workflows/ci.yml` edits — the M0/M1 design intent (Principle II layer extensibility, applied to fixtures) is "drop a fixture, no CI edit needed." If discovery fails, the bug is in the lit configuration, not in M2; fix in this PR.
- [ ] T065 [P] Verify the layering CI guard from T003 actually catches a forbidden edge: temporarily add a stub `#include "nsl/Sema/Sema.h"` to `lib/Parse/Parser.cpp`, run `./scripts/ci.sh static-checks`, observe the guard fails with a descriptive message; revert the stub. This exercises SC-009 ("CI guard MUST verify") with a positive negative-test.

### Determinism + SC verification

- [ ] T066 [P] Determinism: run `nslc -emit=ast` twice on a representative fixture (e.g., the format-golden fixture from T020); diff the stdout; verify empty (FR-030 / SC-003 / SC-007). Repeat with cache hit vs cache miss (clean build vs incremental rebuild) per Principle V.
- [ ] T067 [P] Cross-toolchain determinism: build the dev container twice — once with `gcc` selected, once with `clang` — and compare the AST output of the format-golden fixture across both binaries. Outputs MUST be byte-identical (SC-007).
- [ ] T068 [P] SC roll-up: write a one-page PR-comment validation that walks SC-001 through SC-009 and cites the corresponding green fixture / test_unit case for each. SC-006 ("future grammar-production extension") is hypothetical; cite the per-production fixture pattern + `NodeKind.def` X-macro source-of-truth (research §6 / §9) as evidence.

### Agent-driven audits

- [ ] T069 [P] Spawn `nsl-coupling-audit` agent (READ-ONLY) to verify spec ↔ design coupling on the working tree. Expect zero findings — M2 implements `lang.ebnf §§1–11` and `nsl_compiler_design.md §5` verbatim; no coupling drift expected. Any blocking finding is a stop-the-line item to address in this PR.
- [ ] T070 [P] Spawn `nsl-constitution-review` agent (READ-ONLY) to verify all 9 principles on the working tree. Expect zero blocking findings. Treat any blocking finding as a stop-the-line item.
- [ ] T071 [P] Spawn CodeRabbit review on the PR. Per Constitution External Integrations §1, classify findings as blocking vs advisory on first review; route disputes to `/nsl-constitution-review` for binding judgement.

**Checkpoint**: M2 ready for PR. All 9 SCs measurable as met; all 9 Constitution Principles green. Per /speckit-clarify Q2, JSON-mode AST output is explicitly NOT in M2 scope (T-track will revisit when the LSP consumer is concrete).

### Phase 3+4 integration final state — 187/192 lit (97.4%) + 156/156 ctest (100%)

After Tracks A–F landed (initial integration), then Tracks G + H (closed Groups α and γ+δ), lit pass rate moves from 168/192 → **187/192 (97.4%)** with ctest still **156/156 (100%)**. Five fixtures remain failing — all in **Group β** (parser-feature gaps requiring spec-author judgment or future parser work):

| Fixture | Group | Cause | Recommended fix |
|---|---|---|---|
| `expr-incdec/pass.test` | β | Parser doesn't construct `IncDecExpr` from `++`/`--` despite `lang.ebnf §11:654-657` listing it under `primary_expr` | Parser feature: add `++`/`--` as Pratt nud (prefix) + led (postfix) in `PrecedenceTable.h` + `ParseExpr.cpp` |
| `atomic-incdec/pass.test` | β | Same as expr-incdec at statement position — parser bails before constructing `IncDecStmt` | Same parser feature; statement-position dispatch in `ParseStmt.cpp::parseLValueLedStatement` |
| `action-for/pass.test` | β | Parser requires `:=` in for-step; fixture uses `i++` | Either fixture rewrite (`i := i + 1`) OR add `++`/`--` parser support (subsumed by IncDecExpr fix above) |
| `action-generate/pass.test` | β | Parser requires `:=` in generate-step; fixture uses `i = 0` | Spec-author review: does EBNF §8 generate-step allow `=` (transfer) or only `:=` (statement)? Likely fixture rewrite |
| `state-definition/pass.test` | β | `state s1 { }` inside `proc` rejected as "expected action statement" | Spec-author review: `state_definition` is a `module_item` per §5; fixture nests it inside `proc` which may be illegal. Likely fixture rewrite |

**Recommended next**: a single follow-up track (`nsl-frontend-impl`) implements `IncDecExpr` / `IncDecStmt` parsing from `++`/`--`; closes 3 of the 5 fixtures (`expr-incdec`, `atomic-incdec`, `action-for`). The remaining 2 (`action-generate`, `state-definition`) need an `nsl-spec-author` pass to determine whether they're parser bugs or fixture bugs.

**M2 Phase 3+4 status**: parser surface is functionally complete. The `nslc -emit=ast` driver works end-to-end. 156/156 unit tests pass. 187/192 lit fixtures pass. The remaining 5 are documented Group β gaps.

### Original Phase 3+4 outstanding findings (historical record — already largely resolved)

After Tracks A-F landed, lit reported 168/192 (87.5%). The 24 fixtures failing at that point were grouped α/β/γ/δ: The 24 remaining lit failures surfaced **real bugs and spec/design coupling work** rather than mechanical drift. Cataloged here for follow-up tracks; **none block the M2 acceptance gate** in spirit (parser surface is complete, well-formed inputs all produce correct AST). Anchored to specific findings:

**Group α — Track C input bugs (12 fixtures, fixture-side regen)**:
- `expr-{literal-decimal,identifier,system-var,unary,binary,conditional,concat,repeat,sign-extend,zero-extend,slice,field-access,struct-cast,call,incdec}/pass.test` — `gen_grammar_fixtures.py` emits illegal NSL `wire q = expr;` form (only `reg` accepts `=` initializer per `lang.ebnf §6` line 211). Fix: regenerate with transfer form (`wire q; q = expr;`).
- **Action**: edit `scripts/gen_grammar_fixtures.py` to emit the legal-NSL form, regenerate the 12 affected fixtures.

**Group β — Track D parser/spec disagreements (4 fixtures)**:
- `atomic-incdec/pass.test`: postfix `i++;` not accepted at statement position. Cause: M1 lexer has no `++`/`--` punctuator (Track D's report flagged this preemptively). Fix: lexer extension OR rewrite fixture to `i := i + 1`.
- `action-for/pass.test`: parser requires `:=` in for-step; fixture uses `i++`. Same root cause.
- `action-generate/pass.test`: parser requires `:=` in generate-step; fixture uses `i = 0`. May indicate a parser-side spec divergence (EBNF wording in §8); needs `nsl-spec-author` review.
- `state-definition/pass.test`: `state s1 { }` inside `proc` rejected as "expected action statement". Likely a real parser bug — `state_definition` IS a `module_item` per `lang.ebnf §5`, but the fixture nests it inside a `proc` which is a different context. Spec-coupling clarification needed.

**Group γ — Locked-diagnostic drift (3 fixtures, FR-027 violations)**:
- `notes/n10/{fail.test, pass-warning.test}`: Track D emits `error: expected wire name` instead of the FR-027 locked **warning** `'label' is reserved; using as identifier (parser-note N10)`. **Track D's parseIdentifier doesn't actually emit the N10 warning at identifier consumption** — Track D's brief required this but the implementation missed it. Fix: patch Track D's parser to detect `tk_label` in identifier position and emit the locked warning text per FR-017.
- `notes/n14/fail-malformed.test`: Track D emits `'#line' directive: missing decimal line number` instead of locked `'#line' directive must be followed by a positive integer (parser-note N14)`. Fix: change Track D's diagnostic constant.

**Group δ — Parser bugs (2 fixtures)**:
- `notes/n06/pass.test`: `inst.invoke()` rejected with "expected field name after '.'". Per N6 (`lang.ebnf:1051-1059`), `invoke`/`finish` are reserved method names on proc instances; parser must accept reserved keywords on RHS of `.` in this context. Fix: extend `parseControlCallStmt` / `parseFieldAccess` to accept the closed set `{invoke, finish}`.
- `notes/n03/pass-field-access.test`: `inst.field` produces flat `IdentifierExpr name=inst.field` instead of `FieldAccessExpr field=field` with child `IdentifierExpr name=inst` per N3. Likely a Pratt dispatch mis-routing — `parseFieldAccess` not invoked in led position for `.`. Fix: add `tk_dot` led entry to PrecedenceTable invoking `parseFieldAccess`.

**Recommended follow-up sequencing**:
1. **Group α (12 fixtures)**: smallest blast radius — `nsl-test-author` agent regenerates `gen_grammar_fixtures.py` + 12 fixture inputs.
2. **Group γ (3 fixtures, FR-027 violations)**: highest principle weight — `nsl-frontend-impl` agent patches Track D's diagnostic constants to match locked text. Same patch verifies via the fail-case fixtures turning green.
3. **Group δ (2 parser bugs)**: `nsl-frontend-impl` agent — `parseFieldAccess` Pratt dispatch + N6 reserved-keyword acceptance.
4. **Group β (4 spec-coupling)**: requires `nsl-spec-author` review (does the parser disagree with EBNF, or is the fixture wrong?). Some may resolve to fixture-side rewrites; others to parser fixes.

After all four groups land, lit pass should reach 192/192 and Phase 3+4 is complete. Phase 5 (US3 recovery) and Phase 6 (polish) are separate downstream tracks.

---

## Dependencies & Story Completion Order

```text
Phase 1 (Setup, T001–T003)
    │
    ▼
Phase 2 (Foundational: nsl-ast + Pratt table, T004–T017)
    │
    ├──────────► Phase 3 (US1: Parse §§1–11, T018–T032) ──MVP-deliverable──┐
    │                                                                        │
    │            (US1 lands the parser surface end-to-end via -emit=ast;     │
    │             foundational for US2 + US3)                                │
    │                                                                        │
    └──► Phase 4 (US2: Parser-note disambig, T033–T048) ─────────────────────┤
                                                                             │
                              Phase 5 (US3: Recovery + multi-error, T049–T059)
                                                                             │
                                                                             ▼
                                                       Phase 6 (Polish, T060–T071)
```

**Story-level dependencies**:

- US1 depends on Phase 1 + Phase 2 only.
- US2 depends on Phase 1 + Phase 2 + transitively on US1's parser surface (parser-note dispatch lives inside `parseFoo()` sites built in US1; US2's tests use `nslc -emit=ast` end-to-end, so US1 must be in place when US2's fixtures run). Strictly: **US2 depends on US1**.
- US3 depends on US1 + US2 — recovery applies to *every* `parseFoo()` site, so US3 augments code from both prior stories. The pure recovery-primitive surface (`TokenSet`, `skipUntil`) does not depend on US2's parser-note dispatch, but the multi-error fixtures use the full parser surface. In task-graph form: T056–T058 can begin in parallel with T043–T047, but T059 (the green-on-implementation checkpoint) depends on US2's checkpoint.

**Within-phase dependencies** (the most important ones):

- Phase 2: T004 (NodeKind.def) gates everything else; T005 → T006; T007 → T008; T009/T010/T011 parallel (different files); T012 → T013; T014 + T015 finalize the library; T016 + T017 parallel within Phase 2.
- Phase 3: T018 → T019; T020/T021 parallel with T019; T022 = checkpoint; T023 → T024; T025/T026/T027 parallel after T024; T028 finalizes nsl-parse; T029 depends on T024 and T013; T030/T031 parallel; T032 = checkpoint.
- Phase 4: T033–T041 parallel (different fixture files); T042 = checkpoint; T043/T044/T045/T046/T047 parallel against the now-green Phase 3 implementations (each augments a different `Parse*.cpp`); T048 = checkpoint.
- Phase 5: T049–T054 parallel; T055 = checkpoint; T056 → T057 (T057 augments code from US1+US2 in many files; in practice this is one big PR-style change but tasks-list-wise can be partitioned per-file); T058 depends on T056; T059 = checkpoint.
- Phase 6: T060–T068 mostly parallel; T069/T070/T071 spawned in parallel as background agents.

### Parallel Opportunities

- All Phase 1 [P] tasks parallel (T003 only).
- Phase 2: after T004 lands, T009/T010/T011 parallel (50 per-node-kind headers across three tasks); T005/T007/T012/T016/T017 mostly parallel (each authors a different file).
- Phase 3: after T024 lands, T025/T026/T027 parallel (each owns a different `Parse*.cpp`).
- Phase 4: T033–T041 parallel (different fixture files); T043–T047 parallel (different `Parse*.cpp` augmentations).
- Phase 5: T049–T054 parallel; T056/T058 sequential, T057 partitionable per-file.
- Phase 6: every [P] task parallel; agent spawns (T069/T070/T071) parallel.

---

## Parallel Example: User Story 1 — Foundational AST headers

```bash
# Once T004 (NodeKind.def) lands, launch the per-node-kind header authoring in parallel:
Task: "Author all Decl-family per-node-kind headers in include/nsl/AST/ (T009)"
Task: "Author all Stmt-family per-node-kind headers in include/nsl/AST/ (T010)"
Task: "Author all Expr-family per-node-kind headers in include/nsl/AST/ (T011)"

# In parallel, the printer test author runs ahead of implementation:
Task: "Author test_unit/ast_printer_test/determinism_test.cpp (T012)"
```

## Parallel Example: User Story 2 — Parser-note fixtures

```bash
# All parser-note fixtures are independent files — launch in parallel:
Task: "Author test/parse/notes/n01/{pass-stmt.test, pass-expr.test} (T033)"
Task: "Author test/parse/notes/n02/{pass-reduce.test, pass-bitwise.test} (T034)"
Task: "Author test/parse/notes/n03/{pass-lhs-concat.test, pass-field-access.test} (T035)"
# ... and so on for T036–T041
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (T001–T003)
2. Complete Phase 2: Foundational (T004–T017) — CRITICAL, blocks all stories
3. Complete Phase 3: User Story 1 (T018–T032)
4. **STOP and VALIDATE**: `nslc -emit=ast` works on every grammar production; the format golden is locked.
5. Demo/checkpoint if ready before US2.

### Incremental Delivery

1. Setup + Foundational → AST hierarchy + Pratt table ready
2. US1 → Parser surface complete → MVP demo (every grammar production parses)
3. US2 → Parser-note disambiguation green → Demo (every documented ambiguity resolves correctly)
4. US3 → Multi-error recovery green → Demo (compile-error UX is LSP-grade)
5. Each story adds testable value without breaking previous stories

### Parallel Team Strategy

With multiple developers, after Setup + Foundational:

- **Developer A**: US1 (full parser surface) — T023–T032
- **Developer B**: US2 fixtures + augmentations — T033–T048 (begins after T024 + T026 + T027 in place)
- **Developer C**: US3 recovery primitive + fixtures — T049–T059 (begins after T024 in place; recovery code is mostly orthogonal to US1/US2 parsing logic)

Within Phase 2, the per-node-kind header authoring (T009/T010/T011) parallelizes across three developers if available — each owns one of Decl/Stmt/Expr families.

---

## Notes

- [P] tasks = different files, no dependencies.
- [Story] label maps task to specific user story for traceability.
- Each user story should be independently completable and testable.
- Verify tests fail before implementing — the M1 PR description format ("Test commit hashes (failing-state preserved in history per Principle VIII)") is the precedent here.
- Commit after each task or logical group.
- Stop at any checkpoint to validate story independently.
- Avoid: vague tasks, same-file conflicts, cross-story dependencies that break independence.
- M2's biggest scaling decision is the per-node-kind header layout (research §7); T009/T010/T011 are large but trivially parallelizable across files.
