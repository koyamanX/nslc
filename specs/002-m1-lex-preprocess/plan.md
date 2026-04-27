<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Implementation Plan: M1 — Lex + Preprocess (with Diagnostic Plumbing)

**Branch**: `002-m1-lex-preprocess` | **Date**: 2026-04-27 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/002-m1-lex-preprocess/spec.md`

## Summary

Land the first three compiler-track libraries — `nsl-basic` (1),
`nsl-preprocess` (2), `nsl-lex` (3) — and the source-locating
diagnostic engine that every later layer (parser, sema, MLIR pass,
CIRCT pass, ExportVerilog) builds on. M0 stood up the empty
nine-layer skeleton; M1 fills the first three layers with real
content and exposes the result through the `nslc -emit=tokens`
driver flag.

Deliverables, all mandated by the spec (FR cross-references in
parens):

- **`nsl-basic`** populates `include/nsl/Basic/`:
  `SourceLocation.h` (FileID + offset model), `SourceManager.h`
  (per-`FileID` `Buffer`, `#line` adjustment table, `(file, line,
  col)` ↔ offset queries), `Diagnostic.h` (`DiagnosticEngine` with
  text + smoke-only JSON output, `FixItHint` hooks for later
  milestones). FR-001, FR-024–FR-028, FR-038–FR-039.
- **`nsl-preprocess`** populates `include/nsl/Preprocess/`:
  `Preprocessor.h` (line-oriented scanner; full directive set per
  `nsl_pp.ebnf` §§1–5 + notes P1–P13; `%IDENT%` splicer; macro
  table; closed-set helper evaluator over the 22 helpers from
  `pp.ebnf` §3; bounded include recursion at 256 levels for cycle
  detection). FR-002, FR-013–FR-023, FR-034–FR-035, FR-039.
- **`nsl-lex`** populates `include/nsl/Lex/`: `Token.h` + `Lexer.h`
  (pull-model scanner over raw bytes; recognizes every reserved
  keyword in `lang.ebnf` §15; the four-base × four-digit-form
  numeric grid in §13; `_`-prefix classes per N11; the `#`
  disambiguation per N5; comments + whitespace per §14). FR-003,
  FR-005–FR-012.
- **`nslc -emit=tokens`** plus `-I` / `-D` / `NSL_INCLUDE` plumbing,
  threaded through `lib/Driver/EmitTokens.cpp` (a thin function
  that opens a file → preprocesses → lexes → prints tokens). The
  `tools/nslc/main.cpp` driver remains ≤ 60 lines per Principle II.
  FR-029, FR-030.
- **Test corpus** under `test/lex/`, `test/preprocess/`,
  `test/Driver/`, and `test_unit/`: one fixture per reserved
  keyword, per numeric form, per `_`-prefix class, plus pass+fail
  fixture pairs per `Pn` (P1–P13), the `#line` round-trip golden,
  the include-stack golden, and gtest unit suites for
  `SourceManager`, `DiagnosticEngine`, and the helper evaluator.
  FR-031–FR-037.

Three /speckit-clarify decisions (session 2026-04-27) frame the
scope: helper evaluation lives in the preprocessor at M1 (Q1);
JSON-mode diagnostic output is smoke-only at M1 (Q2);
`nslc -emit=tokens` is in M1 scope (Q3). Implementation latitude is
in *how* the lexer/preprocessor are structured, not *what* they
deliver.

## Technical Context

**Language/Version**: **C++17** across all three libraries (Constitution Build/Code/Licensing — C++20 is forbidden until amendment). Helper scripts (test corpus generators, fixture-emitters) in **Python 3.8+** matching M0 baseline.
**Primary Dependencies**: **LLVM + MLIR** at the CIRCT-pinned commit (vendored prebuilt; M0 §2). M1 uses LLVM only for `llvm::StringRef` / `llvm::ArrayRef` / `llvm::StringMap` / `llvm::MapVector` and the bundled **GoogleTest** + **lit + FileCheck** drivers — no `llvm::SourceMgr` reuse (research §3 explains why we ship our own `SourceManager`). **No new external dependencies introduced at M1.**
**Storage**: N/A. Compiler frontend; only in-memory state plus on-disk source files read from the filesystem (and via `NSL_INCLUDE` env var).
**Testing**: **GoogleTest** for unit-level fixtures of `SourceManager`, `DiagnosticEngine`, the helper evaluator, the macro table, and the `#line` adjustment lookup (under `test_unit/`). **lit + FileCheck** for token-stream goldens, P-note fixture pairs, the `#line` round-trip golden, and the include-stack golden (under `test/lex/`, `test/preprocess/`, `test/Driver/`) per Constitution Principle VI ("lit + FileCheck — no substitutes" for lowering tests; M1 is below the lowering layer but reuses the same convention because the test artifacts are textual goldens). Per-fixture pass+fail discipline (Principle VIII) for every `Pn`; per-keyword fixture for the §15 keyword grid; per-numeric-form fixture for the §13 grid.
**Target Platform**: **Linux x86_64** (M0 baseline). Other architectures and operating systems remain deferred.
**Project Type**: Compiler frontend — extends M0's layered library architecture with three real layers + driver glue.
**Performance Goals**: `nslc -emit=tokens` finishes in **< 1 s** on a representative single-file input on the reference host (informal; no per-line throughput SLO at M1 — deferred per spec coverage summary "Performance: Outstanding (low impact for foundation milestone)"). Tighter performance work waits for the audited-corpus regression at M7 where it has a measurement basis.
**Constraints**: **Byte-stable token output** (FR-038, Principle V) — two `nslc -emit=tokens` invocations on the same input + flag list MUST produce byte-identical stdout. **Deterministic macro-table iteration** (FR-039) — no hash-map-iteration-derived order in any output (research §4: `llvm::MapVector`). **`#line` survives the seam** (Constitution Principle IV) — preprocessor canonicalizes variant 3 to variant 1/2 (P13); lexer adjusts source-location tracking on consumption. **Helper closed-set is a single source of truth** (spec Assumptions) — `include/nsl/Basic/HelperSet.def` is the X-macro file consumed by both `lib/Preprocess/HelperEvaluator.cpp` and the per-helper test fixtures.
**Scale/Scope**: 3 libraries (~9 .cpp + ~5 .h files in `lib/Basic`, `lib/Preprocess`, `lib/Lex` + corresponding public headers); 1 driver flag wiring (`lib/Driver/EmitTokens.cpp` ~30 lines + `main.cpp` delta ~10 lines). Fixture-test count: ~70 keyword fixtures (every entry in `lang.ebnf` §15), ~24 number-form fixtures (4 bases × {plain, Z, X, U} = 16, plus boundary cases), 3 `_`-prefix N11 fixtures, ~6 string-literal fixtures, ~4 comment/whitespace fixtures, 26 P-note fixtures (pass+fail × 13), 1 `#line` round-trip golden, 1 include-stack golden, ~25 gtest unit cases. Total ≈ **160–180 fixtures** at M1 — large but uniform, generated from per-category templates (research §8).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

Evaluation against Constitution v1.4.0 (in `.specify/memory/constitution.md`):

| Principle | Applies to M1? | Status | Notes |
|---|---|---|---|
| **I. Spec Is Authoritative** | **Yes** | ✅ | M1 implements `pp.ebnf` (entire preprocessor) + `lang.ebnf` §§13–15 (entire lex surface) + parser notes N5 + N11 + preprocessor notes P1–P13 verbatim. **No `Sn`/`Nn`/`Pn` numbering changes.** The pre-clarify accuracy fix to FR-017 (helper closed-set 22 not 8) was a faithful read of `pp.ebnf` §3 lines 282–288; no spec amendment required. |
| **II. Layered Library Architecture** | **Yes — load-bearing** | ✅ | Three layers (1, 2, 3) instantiated via M0's `add_nsl_library` macro. Both `nsl-preprocess` and `nsl-lex` `DEPENDS nsl-basic`; neither depends on the other (siblings — preprocessor and lexer interact at the *driver-glue* layer in `lib/Driver/EmitTokens.cpp`, not by static link, so the layer table is preserved). `tools/nslc/main.cpp` stays ≤ 60 lines — `-emit=tokens` argument handling adds at most a single switch case (~10 lines); the actual work moves into `lib/Driver/EmitTokens.cpp`. |
| **III. Stock CIRCT Below `nsl` Dialect** | Forward-looking | ✅ | M1 ships no dialect or CIRCT-adjacent code. |
| **IV. Source-Locating Diagnostics** | **Yes — load-bearing (M1's primary anchor)** | ✅ | Every Token carries a `SourceRange` (FR-012). Every diagnostic renders to `path:line:col` (FR-025) including post-`#line` virtual coordinates. `#line` survives the preprocessor → lexer seam in canonical form (FR-020, FR-021) — Principle IV's "loss of `#line` fidelity is a hard invariant violation" is the #line round-trip golden test (FR-035). `DiagnosticEngine` supports both human and JSON output (FR-027) — JSON depth at M1 is smoke-only per /speckit-clarify Q2 with full schema lock deferred to T3. |
| **V. Inspectable, Deterministic Pipeline** | **Yes — gating** | ✅ | M1 adds the `-emit=tokens` stage flag (FR-029) — Principle V's "new stages MUST add their own `-emit=` flag" satisfied. **Determinism**: byte-stable token-stream output across two builds (FR-038, SC-005); macro-table iteration deterministic (FR-039 — research §4 picks `llvm::MapVector`); no env-var influence other than `NSL_INCLUDE` (FR-030); no embedded timestamps in token spellings; no pointer-address-derived ordering. |
| **VI. Layered Test Discipline** | **Yes — NON-NEGOTIABLE** | ✅ | Lexer tests on token streams (every reserved keyword, Z/X/U number forms, `_`-prefix per N11, comments) per Principle VI's "Lexer tests" bullet. Preprocessor tests covering directive set per FR-034 — pass+fail per Pn. lit + FileCheck for the lex/preprocess goldens. gtest for unit-level (`SourceManager`, `DiagnosticEngine`, helper evaluator, macro table). The "diagnostic-bearing rules test the diagnostic text" rule (Principle VIII) applies to P3 (undef macro), P6 (helper outside #define), P7 (float at seam), P9 (mismatched conditional), and the lexer's unterminated-string error — all five fail-fixtures cite the exact diagnostic string. |
| **VII. Spec ↔ Design Coupling** | **Yes** | ✅ | M1 implements `pp.ebnf` and `lang.ebnf` §§13–15 verbatim; **no edits to `docs/spec/*.ebnf` are required by this plan**. Two design-doc actions: (a) `docs/design/nsl_compiler_design.md` §12 (DiagnosticEngine) is implemented as written — no edits planned. (b) `CLAUDE.md` §1 row "Compile-time helpers `_int`/`_pow`/`_sin`/… → M1 (parse); M3 (eval)" gets a Principle-VII coupling-fix patch in a follow-up PR per /speckit-clarify Q1 resolution — disentangling the "preprocessor helper evaluator (M1)" from the "NSL-language Sema constant evaluator (M3)". That patch is **out of scope here** (separate PR by design — the fix is a CLAUDE.md edit only and shouldn't bundle with implementation work). |
| **VIII. Test-First Development** | **Yes — NON-NEGOTIABLE, gating** | ✅ | FR-036 + FR-037 codify the per-fixture TDD discipline. Tasks plan will sequence each behavior as: (1) test-author commit (observed failing on then-current tree) → (2) implementation commit (test passes). Per-Pn pairs (FR-034) and per-keyword fixtures (FR-031) are the test-first artifacts; the implementations of `Lexer`, `Preprocessor`, `HelperEvaluator`, `MacroTable`, `SourceManager`, `DiagnosticEngine` follow them. The pre-M7 carve-out for refactor exemption (Principle VIII) applies — the Verilog-diff condition (d) is vacuous. |
| **IX. Continuous Integration & Delivery** | **Yes** | ✅ | M0 wired the 6-stage pipeline. M1 fills previously-empty stage 3 (Unit & layer tests) and stage 4 (Lowering tests via lit + FileCheck) with real lex/preprocess content. Stages 5 (end-to-end) and 6 (formal) remain wired-but-empty (gated to M7 / M8). The local-reproduction `scripts/ci.sh` continues to be the single authoritative entry point. |
| **Build/Code/Licensing Standards** | **Yes** | ✅ | C++17 enforced by M0's `target_compile_features` + `set(CMAKE_CXX_EXTENSIONS OFF)`. LLVM/CIRCT conventions throughout (`SourceLocation` etc. mirror clang). Apache-2.0 WITH LLVM-exception SPDX header on every new file (M0's `check_spdx.py` runs against `git ls-files`; SC-009). |
| **Development Workflow** | Yes | ✅ | This plan was drafted via `/speckit-specify` → `/speckit-clarify` → `/speckit-plan`. AI-attribution per `CONTRIBUTING.md` §5 (already exercised in commit `6d1695d`). |
| **External Integrations** (Linear / GitHub Issues / CodeRabbit) | Yes | ✅ | M1 work tracked under Linear `NSLC-<N>` (feature-track). CodeRabbit gate applies. No project-level integration changes. |
| **Governance — Milestone Plan** | Yes | ✅ | M1 follows M0 directly per `README.md` §Roadmap. No milestone renumbering. The follow-up CLAUDE.md §1 coupling-fix is a routine PR per `CONTRIBUTING.md` §3.9 (no plan amendment). |

**Gate result: PASSES** on first evaluation. No violations to record in the Complexity Tracking section.

## Project Structure

### Documentation (this feature)

```text
specs/002-m1-lex-preprocess/
├── plan.md                                  # this file
├── spec.md                                  # /speckit-specify + /speckit-clarify output (704 lines)
├── research.md                              # Phase 0 — every Technical Context decision justified
├── data-model.md                            # Phase 1 — entities (SourceLocation, Token, Macro, …)
├── quickstart.md                            # Phase 1 — clone → build → exercise -emit=tokens
├── contracts/                               # Phase 1 — interface contracts
│   ├── nslc-emit-tokens.contract.md         # nslc -emit=tokens: stdout schema, exit codes, perf
│   ├── diagnostic-output.contract.md        # text + JSON canonical format; include-stack notes
│   └── preprocessor-seam.contract.md        # P12 boundary; helper closed-set; #line round-trip
├── checklists/
│   └── requirements.md                      # /speckit-specify validation
└── tasks.md                                 # /speckit-tasks output (NOT created by /speckit-plan)
```

### Source Code (repository root)

```text
nslc/
├── include/nsl/
│   ├── Basic/                               # M1 populates (M0 created empty dir)
│   │   ├── SourceLocation.h                 # NEW — SourceLocation, SourceRange, FileID
│   │   ├── SourceManager.h                  # NEW — Buffer per FileID, #line table
│   │   ├── Diagnostic.h                     # NEW — DiagnosticEngine, Severity, FixItHint
│   │   └── HelperSet.def                    # NEW — X-macro source-of-truth: 22 helpers (FR-017)
│   ├── Preprocess/                          # M1 populates
│   │   └── Preprocessor.h                   # NEW — main entry + IncludeSearchPath
│   └── Lex/                                 # M1 populates
│       ├── Token.h                          # NEW — Token + TokenKind enum
│       └── Lexer.h                          # NEW — Lexer class (pull model)
├── lib/
│   ├── Basic/
│   │   ├── CMakeLists.txt                   # MODIFIED — list sources via add_nsl_library
│   │   ├── SourceLocation.cpp               # NEW
│   │   ├── SourceManager.cpp                # NEW — file load, FileID alloc, #line apply
│   │   └── Diagnostic.cpp                   # NEW — text + smoke JSON renderer
│   ├── Preprocess/
│   │   ├── CMakeLists.txt                   # MODIFIED
│   │   ├── Preprocessor.cpp                 # NEW — pipeline driver, include stack, cycle guard
│   │   ├── DirectiveParser.cpp              # NEW — #include / #define / #if / #line dispatch
│   │   ├── MacroTable.cpp                   # NEW — insertion-ordered name → body map
│   │   ├── HelperEvaluator.cpp              # NEW — 22 closed-set helpers (FR-017, P5/P10)
│   │   ├── PPExpression.cpp                 # NEW — pp.ebnf §3 expression parser+evaluator
│   │   └── IdentSplicer.cpp                 # NEW — %IDENT% splicing (P3)
│   ├── Lex/
│   │   ├── CMakeLists.txt                   # MODIFIED
│   │   ├── Lexer.cpp                        # NEW — pull-model scanner over raw bytes
│   │   ├── Token.cpp                        # NEW — kind→string, spelling helpers
│   │   ├── KeywordSet.cpp                   # NEW — reserved-keyword recognizer (lang.ebnf §15)
│   │   └── NumberLiteral.cpp                # NEW — base × {plain, Z, X, U} grid recognizer
│   └── Driver/
│       ├── CMakeLists.txt                   # MODIFIED — add EmitTokens.cpp source
│       └── EmitTokens.cpp                   # NEW — open file → preprocess → lex → print tokens
├── tools/nslc/main.cpp                      # MODIFIED — add `-emit=tokens` switch case (≤ 60 lines preserved)
├── test/                                    # M1 grows the lit tree
│   ├── Driver/
│   │   └── emit-tokens.test                 # NEW — `nslc -emit=tokens` smoke + golden
│   ├── lex/
│   │   ├── keywords/                        # NEW — one fixture per `lang.ebnf` §15 keyword
│   │   ├── numbers/                         # NEW — base × {plain, Z, X, U} grid
│   │   ├── n5/                              # NEW — `#` line-marker vs sign-extend
│   │   ├── n11/                             # NEW — three `_`-prefix classes
│   │   ├── strings/                         # NEW — string-literal forms + escapes
│   │   └── comments/                        # NEW — single-line, block, whitespace
│   └── preprocess/
│       ├── p01/.../p13/                     # NEW — pass+fail per Pn (P1–P13)
│       ├── line/                            # NEW — #line round-trip golden (FR-035)
│       └── include-stack/                   # NEW — multi-file diagnostic golden (FR-026, SC-006)
├── test_unit/                               # M1 grows the gtest tree
│   ├── source_manager_test/                 # NEW — FileID alloc, #line table, byte-offset queries
│   ├── diagnostic_engine_test/              # NEW — text + JSON renderers, include-stack notes
│   ├── macro_table_test/                    # NEW — insertion-ordered iteration, undef/redefine
│   └── helper_evaluator_test/               # NEW — 22 helpers × edge cases
├── scripts/                                 # M1 may add small generators
│   └── gen_keyword_fixtures.py              # NEW (optional) — emits per-keyword fixture stubs from lang.ebnf §15
├── README.md                                # POSSIBLY MODIFIED — Building section gains `nslc -emit=tokens` example (small)
└── CLAUDE.md                                # MODIFIED — SPECKIT START/END marker → ./specs/002-m1-lex-preprocess/plan.md
```

**Structure Decision**: Continues M0's compiler layout. The three
M1 layers populate `include/nsl/{Basic,Preprocess,Lex}/` and
`lib/{Basic,Preprocess,Lex}/` respectively. The driver glue lives
in `lib/Driver/EmitTokens.cpp` — outside `tools/nslc/` so the
60-line discipline of `main.cpp` is preserved (Principle II). The
test corpus splits cleanly: lit fixtures under `test/lex/` and
`test/preprocess/` (where the artifact under test is a textual
golden), gtest unit fixtures under `test_unit/` (where the artifact
is a C++ assertion on internal state). No new top-level directories
are introduced; M0's `cmake/`, `scripts/`, `.github/workflows/` are
reused as-is.

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

No violations to track. Constitution Check (above) passes on first
evaluation. The post-design re-check at the end of `research.md`
records the same result.
