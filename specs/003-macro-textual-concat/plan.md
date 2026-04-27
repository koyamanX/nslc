<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Implementation Plan: Bare-Macro Textual Concatenation

**Branch**: `003-macro-textual-concat` | **Date**: 2026-04-27 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/003-macro-textual-concat/spec.md`

## Summary

Extend the M1 preprocessor's expression-evaluation path to do **textual substitution + re-tokenization** for bare-identifier macro references inside `#define` body and `#if` condition contexts. M1's `lib/Preprocess/PPExpression.cpp` resolves bare identifiers by recursively re-evaluating the macro body as a sub-expression — robust for simple cases but incompatible with the canonical pp.ebnf P5 example `_int(_pow(2.0, DEPTH.0))`, which requires `DEPTH` (substituted with `8`) to merge with the adjacent `.0` to form the float literal `8.0`.

Scope is intentionally small:

- **Spec amendment** (`docs/spec/nsl_pp.ebnf` P10) — clarifying paragraph that bare-identifier macro references in `#define` body and `#if` condition undergo textual substitution before expression tokenization, with adjacency rules matching `%IDENT%` (P3). Same line count constraint as the M1 follow-up coupling-fix PR (#3): the amendment fits within ±2 lines of pp.ebnf's current 559-line size to avoid cascading `docs/CLAUDE.md §5` line-reference drift.
- **Implementation delta** in `lib/Preprocess/` — add a textual-substitution pre-pass (likely a new private `MacroExpander.{h,cpp}`, callable from both `PPExpression::parse()` for `#if` conditions and from `IdentSplicer` / `Preprocessor` for `#define` body expansion). Keeps the existing `PPExpression` parser unchanged at the grammar level; only the input-preparation step changes.
- **One FR-037-locked diagnostic string** — `recursive macro expansion: <NAME>` (FR-007). Cycle detection at 256 levels (mirrors M1 FR-022 include-cycle bound).
- **Test corpus** — 3 new fixtures under `test/preprocess/`: P5 canonical (US1), adjacency without whitespace (US2), recursive expansion + cycle detection (US3). Plus optional reverts of M1's two workaround fixtures (`p10/pass.test`, `p12/pass.test`) to the canonical pp.ebnf form.

The whole feature is one M1-vintage PR: ~6–8 commits, ~200–400 source-line delta, single-PR-mergeable. Not a milestone.

## Technical Context

**Language/Version**: **C++17** (Constitution Build/Code/Licensing — unchanged from M1).
**Primary Dependencies**: **LLVM** (for `llvm::StringRef` / `llvm::StringMap`, already a transitive dep of `nsl-basic`); **GoogleTest** + **lit + FileCheck** for tests. **No new external dependencies.**
**Storage**: N/A.
**Testing**: **GoogleTest** for `MacroExpander` unit tests (under `test_unit/macro_expander_test/` — new dir following the M1 per-suite convention) + **lit + FileCheck** for the 3 new pp-fixtures.
**Target Platform**: **Linux x86_64** inside `ghcr.io/koyamanx/nsl-nslc:dev`.
**Project Type**: Small frontend feature delta on M1's `nsl-preprocess` library.
**Performance Goals**: Per SC-003 — a 3-level macro chain resolves end-to-end in **< 1 ms** on the reference Linux x86_64 host (informal at this feature's milestone). 256-deep cycle detection terminates in finite time; SC-004 enforces no stack overflow / infinite loop.
**Constraints**: **Byte-stable preprocessor output** (FR-016 / Principle V — unchanged from M1's invariants). **No M1 regressions** (SC-005 — the existing 113 lit + ctest fixtures continue to pass after this feature lands). **`pp.ebnf` line count stays at 559 ± 2** (SC-006 — avoid cascading drift in `docs/CLAUDE.md §5` line refs). **`tools/nslc/main.cpp` ≤ 60 lines** preserved (Principle II — no new flags introduced by this feature).
**Scale/Scope**: 1 spec-amendment paragraph (`pp.ebnf` P10); 1 new `lib/Preprocess/MacroExpander.{h,cpp}` (~200 lines); ~3 small edits to existing `lib/Preprocess/PPExpression.cpp` + `IdentSplicer.cpp` to call the new expander; 3 new lit fixtures + 1 new gtest unit suite (~5 test cases). Total delta ≈ **300–500 lines** including tests.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

Evaluation against Constitution v1.4.0:

| Principle | Applies? | Status | Notes |
|---|---|---|---|
| **I. Spec Is Authoritative** | **Yes — load-bearing** | ✅ | This feature includes a `pp.ebnf` P10 amendment (FR-001/FR-002 in spec.md). Spec wins by policy; the amendment + implementation land together in this PR (no transitional state where spec says one thing and implementation does another). No new `Sn`/`Nn`/`Pn` numbers — P10 is being clarified, not renumbered. |
| **II. Layered Library Architecture** | Yes | ✅ | All implementation work lives in `lib/Preprocess/` (layer 2). No new layers. `MacroExpander.h` is published at `include/nsl/Preprocess/MacroExpander.h` (matching the M1 precedent for `MacroTable.h` and `HelperEvaluator.h` — those headers are also public to allow direct gtest inclusion); the implementation `lib/Preprocess/MacroExpander.cpp` stays inside the layer. `tools/nslc/main.cpp` unchanged (no new CLI flags); 60-line cap preserved. |
| **III. Stock CIRCT Below `nsl` Dialect** | Forward-looking | ✅ | No dialect or CIRCT-adjacent code touched. |
| **IV. Source-Locating Diagnostics** | Yes | ✅ | The cycle-detection diagnostic (FR-007 locked string `recursive macro expansion: <NAME>`) renders through M1's `DiagnosticEngine` with proper `path:line:col` per FR-025 inherited from M1. SourceLocation propagation through textual substitution is a research §1 decision (use-site location chosen — matches C-preprocessor convention). |
| **V. Inspectable, Deterministic Pipeline** | Yes | ✅ | The textual-substitution pre-pass is a pure function of `(expression text, macro table state)` per FR-016. No env-var influence, no hash-derived ordering. The macro-table iteration order remains insertion-ordered (M1 FR-039). Two `nslc -emit=tokens` invocations on the same input continue to produce byte-identical stdout — verified by SC-005 and the existing M1 SC-005 fixture. |
| **VI. Layered Test Discipline** | **Yes — NON-NEGOTIABLE** | ✅ | 3 new lit fixtures + 1 gtest unit suite per FR-010..FR-013. Per-Pn invariant preserved: P5 (helpers) and P10 (expansion order) each gain a fixture exercising the new behavior. The cycle-detection fixture (FR-012) cites the FR-007 locked diagnostic string. lit + FileCheck for goldens; gtest for the unit suite. |
| **VII. Spec ↔ Design Coupling** | **Yes — load-bearing** | ✅ | The pp.ebnf P10 amendment lands in the SAME PR as the implementation (FR-001 + FR-008 are paired). No transitional "spec says one thing / impl does another" state. The `nsl-coupling-audit` agent should report no findings for this PR (the only acknowledged coupling-fix follow-ups from M1 are #1, #2, #3, #5 — all closed by PR #3 — and #4 which is THIS feature). |
| **VIII. Test-First Development** | **Yes — NON-NEGOTIABLE** | ✅ | Per FR-015 the 3 lit fixtures + 1 gtest suite land BEFORE the `MacroExpander` implementation. Tasks plan sequences each as test-author-commit (observed FAILING on M1-vintage tree) → impl-commit (observed PASSING). The locked diagnostic string (FR-007) is asserted in the cycle-detection fixture per the M1 FR-037 discipline. |
| **IX. Continuous Integration & Delivery** | Yes | ✅ | Reuses M0's 6-stage `ci.sh` pipeline. New gtest suite picked up by `test_unit/CMakeLists.txt`'s `foreach()` discovery loop (no top-level CMake edit). New lit fixtures picked up by lit's directory glob. No GitHub Actions workflow edits needed. |
| **Build/Code/Licensing Standards** | Yes | ✅ | C++17. SPDX header on every new file (`MacroExpander.{h,cpp}` + 3 `.test` files + 1 `.cpp` gtest source). LLVM/CIRCT conventions match M1 surrounding code. |
| **Development Workflow** | Yes | ✅ | Spec Kit pipeline: `/speckit-specify` (b85f882) → `/speckit-clarify` (no-op session) → `/speckit-plan` (this commit) → `/speckit-tasks` → `/speckit-implement`. AI-attribution per CONTRIBUTING §5. |
| **External Integrations** | Yes | ✅ | Linear feature-track issue (separate from PR; user files at PR-open time). CodeRabbit gate at PR-open. |
| **Governance — Milestone Plan** | Yes | ✅ | This is NOT a milestone — it's a routine post-M1 follow-up PR per `CONTRIBUTING.md` §3.9. The M1 row of `README.md` §Roadmap is unchanged. The pp.ebnf P10 amendment is a clarification (does NOT add a new `Pn` number); the project's monotonic-numbering invariant (Principle I) is unaffected. |

**Gate result: PASSES** on first evaluation. No violations to record.

## Project Structure

### Documentation (this feature)

```text
specs/003-macro-textual-concat/
├── plan.md                                  # this file
├── spec.md                                  # /speckit-specify output (354 lines, b85f882)
├── research.md                              # Phase 0 — 3 sections
├── data-model.md                            # Phase 1 — MacroExpander + extended PPValue rules
├── quickstart.md                            # Phase 1 — clone → build → exercise the canonical P5 example
├── contracts/                               # Phase 1 — 1 contract
│   └── macro-expansion-rules.contract.md    # textual-substitution semantics + cycle-detection diagnostic
├── checklists/
│   └── requirements.md                      # /speckit-specify validation (b85f882)
└── tasks.md                                 # /speckit-tasks output (NOT created by /speckit-plan)
```

### Source Code (repository root) — files added or edited by this feature

```text
nslc/
├── docs/spec/nsl_pp.ebnf                    # MODIFIED — P10 paragraph clarification (~3-line edit;
│                                            #  preserves the file's 559-line count per SC-006)
├── lib/Preprocess/
│   ├── MacroExpander.h                      # NEW (private) — textual-substitution + cycle-detection API
│   ├── MacroExpander.cpp                    # NEW (private) — implementation; ~150 lines
│   ├── PPExpression.cpp                     # MODIFIED — call MacroExpander before tokenize/parse;
│   │                                        #  ~10 lines changed, no new public symbols
│   ├── IdentSplicer.cpp                     # MODIFIED — call MacroExpander to handle bare-ident
│   │                                        #  forms inside #define body; ~5-line edit
│   └── CMakeLists.txt                       # MODIFIED — add MacroExpander.cpp to SOURCES
├── test/preprocess/
│   ├── p05/textual-concat.pass.test         # NEW — canonical pp.ebnf P5 example (US1)
│   ├── p10/recursive-expansion.pass.test    # NEW — 3-level chain (US3)
│   └── p10/cycle.fail.test                  # NEW — FR-007 locked-diagnostic fail-case (US3)
├── test_unit/macro_expander_test/           # NEW directory — gtest unit suite
│   ├── CMakeLists.txt                       # NEW — registers the suite (mirrors M1's per-suite pattern)
│   └── macro_expander_test.cpp              # NEW — ~5 test cases per FR-013
└── test_unit/CMakeLists.txt                 # MODIFIED — add `macro_expander_test` to the foreach() list
```

### Optional revert (per FR-014, SC-002)

```text
test/preprocess/p10/pass.test                # OPTIONAL revert from M1's _real(%DEPTH%) workaround to pp.ebnf P5 form
test/preprocess/p12/pass.test                # OPTIONAL revert from M1's _real(%DEPTH%) workaround to pp.ebnf P5 form
```

These reverts are an indicator that the M1 workaround surface has fully closed. They MAY be deferred to a follow-up PR if desired.

**Structure Decision**: Single small directory delta — 2 new private headers/sources in `lib/Preprocess/`, 3 new lit fixtures, 1 new gtest unit suite. Continues M1's pattern. No new public headers, no new layers, no new driver flags. The feature is **strictly additive** to M1: a single new internal class (`MacroExpander`) wired into existing entry points (`PPExpression::parse()`, `IdentSplicer::splice()`).

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

No violations to track. Constitution Check (above) passes on first evaluation. Post-design re-check at the end of `research.md` records the same result.
