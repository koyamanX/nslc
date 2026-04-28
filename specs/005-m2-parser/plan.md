<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Implementation Plan: M2 — Parser + AST (with `-emit=ast`)

**Branch**: `005-m2-parser` | **Date**: 2026-04-27 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/005-m2-parser/spec.md`

## Summary

Land the next two compiler-track libraries — `nsl-ast` (4) +
`nsl-parse` (5) — and the `nslc -emit=ast` driver flag. M1
delivered the lexer, preprocessor, and source-locating diagnostic
engine; M2 turns the post-preprocess token stream into a
typed, source-locating AST that every later milestone (M3 sema,
M4 dialect, M5/M6 lowering, M7 end-to-end, M8 formal, M9 release)
will consume.

Deliverables, all mandated by the spec (FR cross-references in
parens):

- **`nsl-ast`** populates `include/nsl/AST/`: per-node-kind headers
  (`StructDecl.h`, `ModuleBlock.h`, `IfStmt.h`, `BinaryExpr.h`, …)
  plus the umbrella headers `ASTNode.h`, `ASTVisitor.h`,
  `NodeKind.h` (X-macro source-of-truth for the enum). Concrete
  classes mirror
  [`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md)
  §5 (lines 299–682) verbatim — `Decl` / `Stmt` / `Expr` mid-level
  bases, `std::unique_ptr` ownership, polymorphic `accept(ASTVisitor&)`.
  FR-001, FR-004, FR-005.
- **`nsl-parse`** populates `include/nsl/Parse/`: `Parser.h` (a
  thin recursive-descent driver). Implementation under
  `lib/Parse/` is split per grammar surface — `ParseDecl.cpp`,
  `ParseStmt.cpp`, `ParseExpr.cpp`, `ParseRecovery.cpp` — so the
  per-rule recovery sets (FR-021) live next to the productions
  they protect. Builds on `nsl-lex`'s pull-model `Lexer` and the
  M1 `SourceManager` / `DiagnosticEngine`. FR-002, FR-003,
  FR-006–FR-021.
- **AST printer** as a free function in `nsl-ast`
  (`include/nsl/AST/Printer.h`) — text-only S-expression-style
  dump (per /speckit-clarify Q2 → Option A). The driver invokes
  it; `nsl-parse` does not depend on it. FR-022 (text-only
  format), FR-031 (no pointer prints), FR-032 (deterministic
  iteration).
- **`nslc -emit=ast`** plus inheritance of M1's `-I` / `-D` /
  `NSL_INCLUDE` / `--diagnostic-format=json` flags, threaded
  through `lib/Driver/EmitAST.cpp` (a thin function that opens
  a file → preprocesses → lexes → parses → prints AST). The
  `tools/nslc/main.cpp` driver gains one switch case and remains
  ≤ 60 lines per Principle II. FR-022–FR-024, FR-030.
- **Test corpus** under `test/parse/`, `test/Driver/`, and
  `test_unit/`: one AST-snapshot fixture per grammar production
  in `lang.ebnf §§1–11`, one passing fixture-pair per
  parsing-observable parser-note (`N1`, `N2`, `N3`, `N5`, `N6`,
  `N7`, `N10`, `N11`, `N14`), per-recovery fixtures (multi-error
  per FR-021 corpus minimum: 2 top-level errors; 2 module-item
  errors; in-`seq` error followed by well-formed item),
  fail-cases per parser-note (where applicable), the `-emit=ast`
  format golden, and gtest unit suites for `ASTNode` /
  `ASTVisitor` / printer determinism. FR-025–FR-029.

Two /speckit-clarify decisions (session 2026-04-27) frame the
scope: **Q1** — the parser ships full multi-error recovery at
every grammar level (clangd / rust-analyzer pattern); **Q2** —
`-emit=ast` is text-only at M2, JSON deferred to T-track. M2's
implementation latitude is in *how* the recursive-descent parser
is structured, not *what* it accepts or what it emits.

## Technical Context

**Language/Version**: **C++17** across both libraries (Constitution
Build/Code/Licensing — C++20 is forbidden until amendment). The
parser is hand-written recursive descent (no parser-generator
dependency); `std::variant` and `std::optional` per the
constitution's preference. Helper scripts in **Python 3.8+**
matching M0/M1 baseline.
**Primary Dependencies**: **LLVM** at the CIRCT-pinned commit
(vendored prebuilt; M0 §2). M2 uses LLVM only for `llvm::StringRef`
/ `llvm::ArrayRef` / `llvm::SmallVector` / the bundled
**GoogleTest** + **lit + FileCheck** drivers — no `llvm::AST` or
clang AST reuse. **No new external dependencies introduced at M2.**
M1's `nsl-basic` (`SourceManager`, `DiagnosticEngine`),
`nsl-preprocess`, and `nsl-lex` are consumed via their public
headers exclusively.
**Storage**: N/A. AST is in-memory only; the AST printer writes to
`raw_ostream`-style sinks. No persistent AST store at M2 (the
incremental-CST cache is T-track infrastructure, not M-track).
**Testing**: **GoogleTest** for unit-level fixtures of `ASTVisitor`
exhaustiveness, the AST-printer determinism gate, and the
recovery-set bookkeeping (under `test_unit/`). **lit + FileCheck**
for AST-snapshot goldens, per-N-note fixture pairs, multi-error
recovery fixtures, the `-emit=ast` format golden, and the
fail-case parser-error fixtures (under `test/parse/` and
`test/Driver/`) per Constitution Principle VI ("lit + FileCheck —
no substitutes" for lowering tests; M2 reuses the convention
because the test artifacts are textual goldens). Per-fixture
pass+fail discipline (Principle VIII) for every `Nn` with a
violation case; per-grammar-production AST snapshot for every
production in `lang.ebnf §§1–11`.
**Target Platform**: **Linux x86_64** (M0/M1 baseline). Other
architectures and operating systems remain deferred.
**Project Type**: Compiler frontend — extends M1's three layers
with two more (4 + 5) and exposes the new `-emit=ast` driver
stage flag.
**Performance Goals**: `nslc -emit=ast` finishes in **< 1 s** on a
representative single-file input on the reference host (informal;
no per-line throughput SLO at M2 — deferred to M7's audited-corpus
regression where it has a measurement basis). Recovery overhead is
expected to be negligible — recovery sets are static `constexpr`
bitsets per rule; skipping forward to a recovery token is O(tokens
between error and resume).
**Constraints**: **Byte-stable AST output** (FR-030, Principle V) —
two `nslc -emit=ast` invocations on the same input + flag list MUST
produce byte-identical stdout. **No pointer-derived ordering**
(FR-031) — cross-references between AST nodes serialize via a
stable identifier (the target's `SourceRange` start, or a
zero-based monotonic node index). **Deterministic collection
iteration** (FR-032) — no `std::unordered_map` iteration anywhere
in the printer. **Recovery is the default** — no fail-fast mode
(per Q1). **Layered structure preserved** (FR-003) — `nsl-parse`
must NOT depend on `nsl-sema`, `nsl-dialect`, `nsl-lower`, or
`nsl-driver`; a CI guard (`scripts/check_layering.py` extension)
verifies this on every build.
**Scale/Scope**: 2 libraries (~4 .cpp + ~3 .h files in `lib/AST/`
+ `lib/Parse/`, plus ~45 per-node-kind headers in
`include/nsl/AST/` mirroring the AST hierarchy in
`nsl_compiler_design.md` §5); 1 driver flag wiring
(`lib/Driver/EmitAST.cpp` ~30 lines + `main.cpp` delta ~10 lines).
Fixture-test count: **~50 per-production AST snapshots** (one per
named EBNF production in `lang.ebnf §§1–11`), **~9 N-note
fixture pairs** (N1, N2, N3, N5, N6, N7, N10, N11, N14, with both
"interpretation A" and "interpretation B" passing inputs each →
~18 pass fixtures, plus ~3 fail fixtures for N10 warning, N14
malformed, and one N1 ambiguity case), **~6 multi-error
recovery fixtures** (FR-021 corpus minimum + a few additional
recovery-confidence cases), 1 `-emit=ast` format golden, ~12
gtest unit cases. Total ≈ **85–100 fixtures** at M2 — smaller
than M1's ~170 because the parser surface is more uniform (one
fixture per production captures broad coverage in a single
snapshot).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

Evaluation against Constitution v1.5.0 (in `.specify/memory/constitution.md`):

| Principle | Applies to M2? | Status | Notes |
|---|---|---|---|
| **I. Spec Is Authoritative** | **Yes** | ✅ | M2 implements `lang.ebnf §§1–11` (the entire grammar surface less §§13–15 lex which M1 owns) plus parser notes N1/N2/N3/N5/N6/N7/N10/N11/N14 verbatim. **No `Sn`/`Nn`/`Pn` numbering changes.** N4/N8/N9/N12/N13 were retired/moved (already documented in the `lang.ebnf` header) and are M1 territory. |
| **II. Layered Library Architecture** | **Yes — load-bearing** | ✅ | Two layers (4 + 5) instantiated via M0's `add_nsl_library` macro. `nsl-ast` `DEPENDS nsl-basic`; `nsl-parse` `DEPENDS nsl-lex nsl-ast nsl-basic`. Neither depends on `nsl-sema` or anything below it (Principle II downward-only flow). The `nsl-ast` per-node-kind-header exception (Principle II §3) is exercised here for the first time. `tools/nslc/main.cpp` stays ≤ 60 lines — `-emit=ast` argument handling adds at most a single switch case (~10 lines); the actual work moves into `lib/Driver/EmitAST.cpp`. The AST printer lives in `nsl-ast`, NOT in `nsl-parse`, so a future Sema-resolved printing path can extend it without breaking the parser-only path (FR-024). |
| **III. Stock CIRCT Below `nsl` Dialect** | Forward-looking | ✅ | M2 ships no dialect or CIRCT-adjacent code. |
| **IV. Source-Locating Diagnostics** | **Yes — load-bearing** | ✅ | Every AST node carries a `SourceRange` whose endpoints round-trip to the post-`#line` virtual coordinates (FR-018). Every parser diagnostic crosses through M1's `DiagnosticEngine` (FR-019) and renders to `path:line:col` in the canonical form (M1 FR-025). `#line` survives the parser by being consumed (not emitted into the AST) at every item-list position (FR-015 + N14). The post-`#line` virtual coordinates flow into the `-emit=ast` printer (FR-022). |
| **V. Inspectable, Deterministic Pipeline** | **Yes — gating** | ✅ | M2 adds the `-emit=ast` stage flag (FR-022) — Principle V's "new stages MUST add their own `-emit=` flag" satisfied. **Determinism**: byte-stable AST-printer output across two builds (FR-030, SC-003, SC-007); no pointer prints (FR-031); deterministic collection iteration (FR-032); no env-var influence other than `NSL_INCLUDE` (inherited from M1); no embedded timestamps in node spellings; no hash-map-iteration-derived order. |
| **VI. Layered Test Discipline** | **Yes — NON-NEGOTIABLE** | ✅ | Parser tests are AST-snapshot tests "covering every grammar production" per Principle VI's "Parser tests" bullet. Per-production fixtures (FR-025) + per-N-note fixture pairs (FR-026) + per-N-note fail-cases where applicable (FR-027) + multi-error recovery fixtures (FR-021) collectively exercise the parser surface. lit + FileCheck for AST-snapshot goldens. gtest for unit-level (`ASTVisitor` exhaustiveness, printer determinism, recovery-set bookkeeping). The "diagnostic-bearing rules test the diagnostic text" rule (Principle VIII) applies to every fail-case fixture. **No Sn tests at M2** — `S1`–`S29` lands at M3 per the milestone plan; the parser at M2 accepts grammar-conformant input even when a Sema constraint would later reject it (Assumptions). |
| **VII. Spec ↔ Design Coupling** | **Yes** | ✅ | M2 implements `lang.ebnf §§1–11` and `nsl_compiler_design.md` §§5 (AST hierarchy) + the Parser sketch in §4 verbatim; **no edits to `docs/spec/*.ebnf` are required by this plan**. Two design-doc actions to consider during implementation: (a) `docs/design/nsl_compiler_design.md` §5 lines 299–682 are implemented as written — no edits planned. (b) The `docs/CLAUDE.md` quick-map for parser notes (§5 line 233) lists N5/N6/N7/N10/N11/N14 by file:line; if any line numbers shift during implementation, the same patch updates `docs/CLAUDE.md` §§4–7 per Principle VII's line-range rule. |
| **VIII. Test-First Development** | **Yes — NON-NEGOTIABLE, gating** | ✅ | FR-029 codifies the per-fixture TDD discipline. Tasks plan will sequence each behavior as: (1) test-author commit (observed failing on then-current tree) → (2) implementation commit (test passes). Per-production AST-snapshot fixtures (FR-025) and per-N-note pairs (FR-026) are the test-first artifacts; the implementations of `Parser`, the per-decl/stmt/expr parse functions, the recovery sets, and the AST printer follow them. The pre-M7 carve-out for refactor exemption (Principle VIII) applies — the Verilog-diff condition (d) is vacuous. |
| **IX. Continuous Integration & Delivery** | **Yes** | ✅ | M0 wired the 6-stage pipeline; M1 filled stages 3 + 4 with lex/preprocess content. M2 grows stage 3 (Unit & layer tests) with parser AST-snapshots and stage 4 (Lowering tests via lit + FileCheck) with `-emit=ast` goldens. Stages 5 (end-to-end) and 6 (formal) remain wired-but-empty (gated to M7 / M8). The local-reproduction `scripts/ci.sh` continues to be the single authoritative entry point. **The Principle IX transitional clause was retired in commit `3b6decc` (Constitution v1.5.0)** — green CI is a hard merge gate for M2's PR. |
| **Build/Code/Licensing Standards** | **Yes** | ✅ | C++17 enforced by M0's `target_compile_features` + `set(CMAKE_CXX_EXTENSIONS OFF)`. LLVM/CIRCT conventions throughout. The `nsl-ast` per-node-kind-header exception (Principle II §3) is the only deviation — explicitly allowed. Apache-2.0 WITH LLVM-exception SPDX header on every new file (M0's `check_spdx.py` runs against `git ls-files`; SC-008). |
| **Development Workflow** | Yes | ✅ | This plan was drafted via `/speckit-specify` → `/speckit-clarify` → `/speckit-plan`. AI-attribution per `CONTRIBUTING.md` §5. |
| **External Integrations** (Linear / GitHub Issues / CodeRabbit) | Yes | ✅ | M2 work tracked under Linear `NSL-<N>` (feature-track; team prefix is `NSL` per memory note, not `NSLC`). CodeRabbit gate applies. No project-level integration changes. |
| **Governance — Milestone Plan** | Yes | ✅ | M2 follows M1 directly per `README.md` §Roadmap. No milestone renumbering. No constitution amendment required. |

**Gate result: PASSES** on first evaluation. No violations to record in the Complexity Tracking section.

## Project Structure

### Documentation (this feature)

```text
specs/005-m2-parser/
├── plan.md                                  # this file
├── spec.md                                  # /speckit-specify + /speckit-clarify output
├── research.md                              # Phase 0 — every Technical Context decision justified
├── data-model.md                            # Phase 1 — AST entities mirroring nsl_compiler_design.md §5
├── quickstart.md                            # Phase 1 — clone → build → exercise -emit=ast
├── contracts/                               # Phase 1 — interface contracts
│   ├── nslc-emit-ast.contract.md            # nslc -emit=ast: stdout schema, exit codes, perf
│   ├── parser-recovery.contract.md          # multi-error recovery surface; recovery-set documentation
│   └── ast-stability.contract.md            # determinism + format-stability invariants for AST printer
├── checklists/
│   └── requirements.md                      # /speckit-specify validation
└── tasks.md                                 # /speckit-tasks output (NOT created by /speckit-plan)
```

### Source Code (repository root)

```text
nslc/
├── include/nsl/
│   ├── AST/                                 # M2 populates (M0 created empty dir)
│   │   ├── ASTNode.h                        # NEW — base class + SourceRange + accept()
│   │   ├── ASTVisitor.h                     # NEW — polymorphic visitor; missing-override = compile error
│   │   ├── NodeKind.def                     # NEW — X-macro source-of-truth: every concrete node kind
│   │   ├── NodeKind.h                       # NEW — enum NodeKind + free helpers (consumes NodeKind.def)
│   │   ├── Type.h                           # NEW — TypeRef forward decl + nullptr "unresolved" sentinel
│   │   ├── Printer.h                        # NEW — print(CompilationUnit&, raw_ostream&) (text S-expr)
│   │   ├── Decl.h                           # NEW — abstract Decl base
│   │   ├── Stmt.h                           # NEW — abstract Stmt base
│   │   ├── Expr.h                           # NEW — abstract Expr base + inferredType slot
│   │   ├── CompilationUnit.h                # NEW
│   │   ├── StructDecl.h                     # NEW
│   │   ├── DeclareBlock.h                   # NEW
│   │   ├── ModuleBlock.h                    # NEW
│   │   ├── TopLevelParamDecl.h              # NEW
│   │   ├── PortDecl.h                       # NEW
│   │   ├── RegDecl.h                        # NEW
│   │   ├── WireDecl.h                       # NEW
│   │   ├── VariableDecl.h                   # NEW
│   │   ├── IntegerDecl.h                    # NEW
│   │   ├── MemDecl.h                        # NEW
│   │   ├── FuncSelfDecl.h                   # NEW
│   │   ├── ProcNameDecl.h                   # NEW
│   │   ├── StateNameDecl.h                  # NEW
│   │   ├── FirstStateDecl.h                 # NEW
│   │   ├── SubmoduleDecl.h                  # NEW
│   │   ├── StructInstDecl.h                 # NEW
│   │   ├── FuncDefn.h                       # NEW
│   │   ├── ProcDefn.h                       # NEW
│   │   ├── StateDefn.h                      # NEW
│   │   ├── TransferStmt.h                   # NEW
│   │   ├── IncDecStmt.h                     # NEW
│   │   ├── ControlCallStmt.h                # NEW
│   │   ├── BareFinishStmt.h                 # NEW
│   │   ├── SystemTaskStmt.h                 # NEW
│   │   ├── ReturnStmt.h                     # NEW
│   │   ├── EmptyStmt.h                      # NEW
│   │   ├── LabeledStmt.h                    # NEW
│   │   ├── GotoStmt.h                       # NEW
│   │   ├── InitBlockStmt.h                  # NEW
│   │   ├── DelayTaskStmt.h                  # NEW
│   │   ├── ParallelBlock.h                  # NEW
│   │   ├── AltBlock.h                       # NEW
│   │   ├── AnyBlock.h                       # NEW
│   │   ├── SeqBlock.h                       # NEW
│   │   ├── WhileBlock.h                     # NEW
│   │   ├── ForBlock.h                       # NEW
│   │   ├── IfStmt.h                         # NEW
│   │   ├── StructuralGenerate.h             # NEW
│   │   ├── LiteralExpr.h                    # NEW
│   │   ├── IdentifierExpr.h                 # NEW
│   │   ├── SystemVarExpr.h                  # NEW
│   │   ├── UnaryExpr.h                      # NEW
│   │   ├── BinaryExpr.h                     # NEW
│   │   ├── ConditionalExpr.h                # NEW
│   │   ├── ConcatExpr.h                     # NEW
│   │   ├── RepeatExpr.h                     # NEW
│   │   ├── SignExtendExpr.h                 # NEW
│   │   ├── ZeroExtendExpr.h                 # NEW
│   │   ├── SliceExpr.h                      # NEW
│   │   ├── FieldAccessExpr.h                # NEW
│   │   ├── CallExpr.h                       # NEW
│   │   ├── StructCastExpr.h                 # NEW
│   │   └── IncDecExpr.h                     # NEW
│   └── Parse/                               # M2 populates
│       └── Parser.h                         # NEW — Parser class (recursive descent)
├── lib/
│   ├── AST/
│   │   ├── CMakeLists.txt                   # MODIFIED — list sources via add_nsl_library
│   │   ├── ASTNode.cpp                      # NEW — out-of-line accept() + dtor anchors
│   │   ├── Printer.cpp                      # NEW — S-expression-style walker; deterministic
│   │   └── NodeKindNames.cpp                # NEW — enum-to-string for printer
│   ├── Parse/
│   │   ├── CMakeLists.txt                   # MODIFIED
│   │   ├── Parser.cpp                       # NEW — top-level parseCompilationUnit(); item dispatch
│   │   ├── ParseDecl.cpp                    # NEW — struct / declare / module / internal-decl parsers
│   │   ├── ParseStmt.cpp                    # NEW — par/alt/any/seq/if/for/while/generate + atomic
│   │   ├── ParseExpr.cpp                    # NEW — Pratt-style precedence walk over §11 forms
│   │   └── Recovery.cpp                     # NEW — recovery-set tables + skipUntil() primitive
│   └── Driver/
│       ├── CMakeLists.txt                   # MODIFIED — add EmitAST.cpp source
│       └── EmitAST.cpp                      # NEW — open file → preprocess → lex → parse → print AST
├── tools/nslc/main.cpp                      # MODIFIED — add `-emit=ast` switch case (≤ 60 lines preserved)
├── test/                                    # M2 grows the lit tree
│   ├── Driver/
│   │   └── emit-ast.test                    # NEW — `nslc -emit=ast` smoke + golden
│   └── parse/
│       ├── grammar/                         # NEW — one fixture per `lang.ebnf §§1–11` production
│       ├── notes/
│       │   ├── n01/                         # NEW — if statement-vs-expression
│       │   ├── n02/                         # NEW — &/|/^ reduction-vs-bitwise
│       │   ├── n03/                         # NEW — .{ two-char lookahead
│       │   ├── n05/                         # NEW — # sign-extend post-preprocess
│       │   ├── n06/                         # NEW — proc-instance method access
│       │   ├── n07/                         # NEW — dotted func def for submodule out
│       │   ├── n10/                         # NEW — `label` reserved-but-warned
│       │   ├── n11/                         # NEW — _-prefix system-task vs system-variable
│       │   └── n14/                         # NEW — line_marker consumed-not-emitted
│       └── recovery/                        # NEW — multi-error fixtures (FR-021 corpus + extras)
├── test_unit/                               # M2 grows the gtest tree
│   ├── ast_visitor_test/                    # NEW — exhaustive-visit assertion harness
│   ├── ast_printer_test/                    # NEW — determinism gate; no-pointer-leak
│   └── recovery_set_test/                   # NEW — recovery-token-set bookkeeping unit tests
├── scripts/
│   └── check_layering.py                    # MODIFIED — extend to forbid `nsl-parse`→`nsl-sema` link edges
├── README.md                                # POSSIBLY MODIFIED — Building/Status section gains `nslc -emit=ast` example (small)
└── CLAUDE.md                                # MODIFIED — SPECKIT START/END marker → ./specs/005-m2-parser/plan.md
```

**Structure Decision**: Continues M0/M1's compiler layout. The two
M2 layers populate `include/nsl/{AST,Parse}/` and
`lib/{AST,Parse}/` respectively. The driver glue lives in
`lib/Driver/EmitAST.cpp` — outside `tools/nslc/` so the 60-line
discipline of `main.cpp` is preserved (Principle II). The test
corpus splits cleanly: lit fixtures under `test/parse/` (where the
artifact under test is a textual AST snapshot) and `test/Driver/`
(driver flag golden), gtest unit fixtures under `test_unit/` (where
the artifact is a C++ assertion on internal state). The
per-node-kind-header pattern in `include/nsl/AST/` is the
architecturally significant new structure introduced by M2 — it
exercises the Principle II §3 exception for the first time.

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

No violations to track. Constitution Check (above) passes on first
evaluation. The post-design re-check at the end of `research.md`
records the same result.
