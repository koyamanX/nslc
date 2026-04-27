<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->
---
description: "Tasks for 003-macro-textual-concat — bare-macro textual concatenation"
---

# Tasks: Bare-Macro Textual Concatenation

**Input**: Design documents from `/specs/003-macro-textual-concat/`
**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md, contracts/

**Tests**: Test tasks are **MANDATORY** per Constitution Principle VIII (Test-First Development, NON-NEGOTIABLE). Per spec FR-015, fixtures land BEFORE implementation; the test-author commit MUST be observed FAILING on the M1-vintage tree before the implementation commit is accepted.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing.

## Format: `[ID] [P?] [Story?] Description with file path`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: User-story label (US1, US2, US3) — required for user-story phase tasks; absent for Setup / Foundational / Polish
- Every task description includes the exact file path

## Path Conventions

- All work confined to `lib/Preprocess/` (M1 layer 2) + `test/preprocess/` + `test_unit/` + `docs/spec/nsl_pp.ebnf` (P10 amendment).
- No new public headers; no new layers; no driver-flag changes; `tools/nslc/main.cpp` unchanged (60-line cap preserved per Principle II).

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Sanity-verify the M1 baseline + register the new gtest suite directory.

- [X] T001 Sanity-verify the M1 baseline is green inside `ghcr.io/koyamanx/nsl-nslc:dev`. Run `cmake --build build-Release-host && ctest --test-dir build-Release-host && /usr/local/bin/lit build-Release-host/test`; expect all 113 lit + 66 ctest passing on master HEAD `00f0225`.
- [X] T002 [P] Create the new gtest suite directory `test_unit/macro_expander_test/` with a stub `.keep` placeholder and a stub `CMakeLists.txt` that uses the M1 guarded-source pattern (no source files yet → suite not registered until T020 lands the actual `.cpp`).
- [X] T003 Edit `test_unit/CMakeLists.txt` foreach() loop to register `macro_expander_test` alongside the existing 10 M1 suite names. The guarded-source pattern in T002's CMakeLists keeps the build green during Phase 2 (no source files yet); T020 lands the .cpp and the suite turns active.

**Checkpoint**: M1 baseline green; new suite directory registered (no-op until Phase 3 fills it).

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: pp.ebnf P10 spec amendment per data-model entity 3 (FR-001 / FR-002). Per Principle VII, the spec amendment + implementation MUST land in the same PR — but per FR-015 (TDD), the fixtures + amendment land first, then the implementation. The amendment is foundational because it's the contract the fixtures + impl both reference.

**⚠️ CRITICAL**: Phase 3 / 4 / 5 user-story work depends on the P10 amendment being in place so the fixtures and code can cite it.

- [X] T004 Amend `docs/spec/nsl_pp.ebnf` P10 per data-model.md entity 3. Replace the existing 13-line P10 paragraph with the amended text that explicitly covers BOTH `%IDENT%` AND bare-identifier substitution in step 1, adjacency rules ("adjoins surrounding characters without inserted whitespace"), and the 256-level recursion bound. Verify `wc -l docs/spec/nsl_pp.ebnf` returns exactly **559** post-edit (SC-006's ±2 line budget; aim for 0 net change to preserve `docs/CLAUDE.md §5` line refs).

**Checkpoint**: pp.ebnf amended. Phase 3+ work can proceed.

---

## Phase 3: User Story 1 — Canonical pp.ebnf P5 example works (Priority: P1) 🎯 MVP

**Goal**: A contributor writes `#define DEPTH 8` + `#define MEMDEPTH _int(_pow(2.0, DEPTH.0))` and `%MEMDEPTH%` use sites emit the integer literal `256`. The bare identifier `DEPTH` is textually substituted into the helper-call argument list; post-substitution `8.0` is recognized as a `float_literal` and the helper evaluator computes `_pow(2.0, 8.0) = 256.0` → `_int(...)` = `256`.

**Independent Test**: After this phase, the fixture `test/preprocess/p05/textual-concat.pass.test` passes inside the dev container. Independent of US2 (adjacency edge cases) and US3 (recursive expansion + cycle detection); MVP cut line.

### Tests for User Story 1 (MANDATORY per Constitution Principle VIII) ⚠️

> Write these tests FIRST. They MUST be observed FAILING against the unchanged tree before any implementation task in this story begins.

- [X] T005 [P] [US1] Author `test/preprocess/p05/textual-concat.pass.test` — the canonical pp.ebnf P5 example per spec US1 acceptance scenario 1. Use `printf` (not `echo`) to write the multi-line input to `%t.nsl` per M1 lit-fixture lessons. CHECK lines assert the post-`%MEMDEPTH%` token stream contains `tk_decimal_lit\t256` (followed by the surrounding `reg buf[…]` tokens). Independent of US2's adjacency edge cases.
- [X] T006 [P] [US1] TDD — author the basic-substitution test cases in `test_unit/macro_expander_test/macro_expander_test.cpp`: `MacroExpander::expand("DEPTH", loc)` returns `"8"` when DEPTH is defined as `8`; `MacroExpander::expand("DEPTH.0", loc)` returns `"8.0"`; `MacroExpander::expand("UNDEF", loc)` returns `"UNDEF"` unchanged (FR-017). Assertions cite the data-model entity 1 invariants. Run; observe FAILING (no MacroExpander symbol exists yet).
- [X] T007 [US1] Run T005 + T006 inside the dev container against the unchanged-since-M1 tree. **Observe ALL FAILING** — T005 fails because `_int(_pow(2.0, DEPTH.0))` errors with `missing ')' in helper call '_pow'` (M1's PPExpression doesn't substitute `DEPTH` textually); T006 fails to LINK because `nsl::preprocess::MacroExpander` doesn't exist. Capture as the FR-015 / Principle VIII RED-state evidence; the commit message records the per-task SHAs.

### Implementation for User Story 1

- [X] T008 [US1] Implement `lib/Preprocess/MacroExpander.h` per data-model entity 1: class `MacroExpander` with `kMaxExpansionDepth = 256` constant, constructor taking `MacroTable&` + `DiagnosticEngine&`, public `expand(StringRef, SourceRange)` method, private `expandImpl(...)` recursive helper. SPDX header on line 1.
- [X] T009 [US1] Implement `lib/Preprocess/MacroExpander.cpp` (basic textual substitution; cycle detection arrives in US3): walk the input character stream left-to-right, lex identifiers via the same `[A-Za-z_][A-Za-z0-9_]*` regex used by `nsl-lex`, look up each identifier in the `MacroTable`, replace its character span with the macro body's text, resume scanning at the start of the substituted text (so recursion happens naturally). Skip identifier scanning inside `"..."` string literals. Per research §1's pre-pass design.
- [X] T010 [US1] Wire `MacroExpander` into `lib/Preprocess/PPExpression.cpp`: add a `MacroExpander` member field; in `parse()`, call `expander.expand(input_text, loc)` once before tokenizing. The expression parser then sees the already-substituted character stream. Per research §3 / data-model entity 1 invariants. ~10-line edit.
- [X] T011 [US1] Wire `MacroExpander` into `lib/Preprocess/IdentSplicer.cpp` so `#define` bodies that mix `%IDENT%` + bare identifiers BOTH undergo textual substitution per FR-009. The existing `%IDENT%` splice path stays; the new `MacroExpander` call augments it. ~5-line edit.
- [X] T012 [US1] Edit `lib/Preprocess/CMakeLists.txt` — add `MacroExpander.cpp` to the `add_nsl_library(nsl-preprocess SOURCES ...)` list. Private header `MacroExpander.h` does NOT go in the `HEADERS` install set (private to `lib/Preprocess/` per data-model entity 1).
- [X] T013 [US1] Build inside the container; run T005 + T006. **Observe GREEN.** US1's MVP test passes; macro_expander_test's basic cases pass.
- [X] T014 [US1] Determinism check: run `nslc -emit=tokens` on the US1 fixture twice; diff stdout; expect empty (FR-016 / SC-005's "no M1 regression").

**Checkpoint**: US1 MVP fully functional. The canonical pp.ebnf P5 example works end-to-end. US2 (adjacency tests) and US3 (recursion + cycles) extend this.

---

## Phase 4: User Story 2 — Adjacency-without-whitespace forms a single re-tokenized value (Priority: P2)

**Goal**: Verify that `DEPTH.0` (no whitespace between `DEPTH` and `.0`) merges via textual substitution to form `8.0` (a single `float_literal`), while `DEPTH .0` (with whitespace) stays as two distinct tokens. The substitution algorithm from US1 already implements this; US2 adds **edge-case test coverage** without new implementation.

**Independent Test**: Two new lit fixtures (one passing for the no-whitespace case, one passing or asserting expected error for the whitespace case) plus 3-4 additional `macro_expander_test.cpp` cases. All inside the dev container.

### Tests for User Story 2 (MANDATORY per Constitution Principle VIII) ⚠️

- [X] T015 [P] [US2] Author `test/preprocess/p05/adjacency-no-whitespace.pass.test` — exercise `_int(A.5)` with `#define A 4`; assert `tk_decimal_lit 4` (truncated from 4.5). Per spec US2 acceptance scenario 1.
- [X] T016 [P] [US2] Author `test/preprocess/p05/adjacency-with-whitespace.fail.test` — exercise `_int(B .5)` with `#define B 4`; assert the expected error per spec US2 acceptance scenario 2 (parse error citing the stray `.5`). Per pp.ebnf §3 expression grammar; the implementer chooses the exact diagnostic wording.
- [X] T017 [P] [US2] Extend `test_unit/macro_expander_test/macro_expander_test.cpp` with adjacency cases per the contract's adjacency table:
   - `expand("A.0")` with `#define A 8` → `"8.0"` (no whitespace)
   - `expand("A 0")` with `#define A 8` → `"8 0"` (whitespace preserved)
   - `expand("A_extra")` with `#define A 8` → `"A_extra"` (greedy identifier scan; `A_extra` is one identifier, not substituted)

### Implementation for User Story 2

- [X] T018 [US2] No new implementation needed — US1's `MacroExpander` already handles adjacency per data-model entity 1's invariants. **Verification only.** Run T015 + T016 + T017 inside the container; expect GREEN.

**Checkpoint**: Adjacency rules pinned at the test layer. The contract `macro-expansion-rules.contract.md` is exercised by both US1 and US2 fixtures collectively.

---

## Phase 5: User Story 3 — Recursive expansion + cycle detection (Priority: P2)

**Goal**: Multi-level macro chains (`A → B → C → 8`) resolve transitively via repeated substitution. Self-referential or transitively-recursive macros (`A → A` or `A → B → A`) are detected at the 256-level depth bound and produce the FR-007-locked diagnostic `recursive macro expansion: <NAME>`.

**Independent Test**: Two new lit fixtures (one passing 3-level chain, one fail-fixture cycle) plus 2-3 additional `macro_expander_test.cpp` cases. The cycle fixture asserts the locked diagnostic verbatim.

### Tests for User Story 3 (MANDATORY per Constitution Principle VIII) ⚠️

- [X] T019 [P] [US3] Author `test/preprocess/p10/recursive-expansion.pass.test` — 3-level chain `#define A B`, `#define B C`, `#define C 8`, `#define X _int(A.0)`; assert `%X%` use site emits `tk_decimal_lit 8`. Per spec US3 acceptance scenario 1.
- [X] T020 [P] [US3] Author `test/preprocess/p10/cycle.fail.test` — self-cycle `#define A A` and use-site `_int(A)`; assert `not nslc -emit=tokens %t.nsl 2>&1 | FileCheck %s` matches the FR-007 LOCKED diagnostic `error: recursive macro expansion: A`. The CHECK line cites the diagnostic string verbatim per the M1 FR-037 / Principle VIII discipline.
- [X] T021 [P] [US3] Extend `test_unit/macro_expander_test/macro_expander_test.cpp` with recursion + cycle cases:
   - 3-level chain assertion (`expand("A")` with `#define A B`, `#define B 8` → `"8"`)
   - Cycle detection at 256 (synthesize a 257-deep chain or a self-cycle; assert the diagnostic engine receives an error with the FR-007 locked message)
   - Failsoft semantics (per data-model entity 1: cycle detection emits diagnostic + returns the original unsubstituted text; downstream parser then fails)

### Implementation for User Story 3

- [X] T022 [US3] Extend `lib/Preprocess/MacroExpander.cpp`'s `expandImpl()` with the depth counter (per research §4): pass `depth` parameter through recursive calls; if `depth > kMaxExpansionDepth` (256), emit the FR-007 locked diagnostic via `diag_.report(Severity::Error, use_loc, "recursive macro expansion: " + name)` and return the unsubstituted text (failsoft per data-model entity 1).
- [X] T023 [US3] Build inside container; run T019 + T020 + T021. **Observe GREEN** including the FR-007 locked diagnostic match in T020.
- [X] T024 [US3] Run the full M1 + this-feature lit + ctest corpus inside the container. Expect **116/116 lit** (113 M1 + 3 new) + **all ctest green** (M1's 66 + this feature's expanded macro_expander_test). SC-005 (no M1 regressions) verified.

**Checkpoint**: All 3 user stories complete. The feature works end-to-end.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Optional cleanup of M1 workaround fixtures + final checks.

### Optional revert of M1 workarounds (FR-014, SC-002)

- [X] T025 [P] OPTIONAL — Revert `test/preprocess/p10/pass.test`'s M1 workaround. The current fixture uses `_real(%DEPTH%)` to dodge the M1 textual-concat gap; with this feature in place, the canonical `%DEPTH%.0` form works. Update the fixture to use the canonical form. Verify lit still passes.
- [X] T026 [P] OPTIONAL — Revert `test/preprocess/p12/pass.test`'s M1 workaround analogously. Verify lit still passes.

### Final checks

- [X] T027 [P] Run `./scripts/ci.sh` end-to-end inside the container — all 6 stages exit 0 (build-matrix, static-checks, unit-tests, lowering-tests, e2e-wired-but-empty, formal-wired-but-empty). The new gtest suite + 3 new lit fixtures should be picked up automatically.
- [X] T028 [P] Run `python3 scripts/check_spdx.py --all` — every new file (`MacroExpander.{h,cpp}`, `macro_expander_test.cpp`, the 4 new `.test` files, `macro_expander_test/CMakeLists.txt`) carries the SPDX header. Expect 0 failures.
- [X] T029 [P] SC roll-up: write a one-paragraph PR-comment summary citing each of SC-001..SC-006 and the corresponding green fixture / test_unit case. SC-006 (pp.ebnf line count ±2) verified by `wc -l docs/spec/nsl_pp.ebnf` returning exactly 559.

### Agent-driven audits

- [X] T030 [P] Spawn `nsl-coupling-audit` agent (READ-ONLY) on the working tree. Expect **0 findings**. The pp.ebnf P10 amendment lands in the same PR as the implementation per Principle VII; no spec/design drift surface introduced.
- [X] T031 [P] Spawn `nsl-constitution-review` agent (READ-ONLY). Expect zero blocking findings; reaffirm Principles I (spec-amendment land), VII (coupling honored), VIII (test-first preserved in commit graph).

**Checkpoint**: PR-ready.

---

## Dependencies & Story Completion Order

```
Phase 1 (Setup, T001–T003)
    │
    ▼
Phase 2 (Foundational: pp.ebnf P10 amendment, T004)
    │
    ▼
Phase 3 (US1: textual substitution + canonical P5 example, T005–T014)
    │
    ├──► Phase 4 (US2: adjacency edge cases, T015–T018)
    │            (no new impl; verification-only)
    │
    └──► Phase 5 (US3: recursion + cycle detection, T019–T024)
                 (extends MacroExpander with depth counter)
                                  │
                                  ▼
                           Phase 6 (Polish, T025–T031)
```

**Story-level dependencies**:

- US1 depends on Phase 1 + Phase 2 only.
- US2 depends on US1 (the substitution algorithm). Pure verification; no new code.
- US3 depends on US1 (extends the same `MacroExpander` class with the depth counter). Independent of US2.

**Within-phase dependencies** (the most important ones):

- Phase 3: T005/T006/T007 sequential (RED-state); T008→T009 (header before .cpp); T010/T011 parallel (different files); T012 finalizes; T013/T014 verification.
- Phase 5: T019/T020/T021 parallel (RED-state); T022 implementation; T023/T024 verification.

## Parallel Execution Examples

### Phase 3 — fixture authoring + implementation prep in parallel

```
[T005 p05/textual-concat.pass.test]   [T006 macro_expander_test.cpp basic cases]
                                  │
                                  ▼
                         T007 (RED checkpoint)
                                  │
                                  ▼
                         T008 → T009 (impl)
                                  │
                                  ▼
                  [T010 PPExpression]   [T011 IdentSplicer]
                                  │
                                  ▼
                            T012 → T013 → T014
```

### Phase 5 — fixture authoring in parallel

```
[T019 recursive-expansion]   [T020 cycle.fail (FR-007 locked)]   [T021 unit cases]
                              │
                              ▼
                         T022 (impl extension)
                              │
                              ▼
                         T023 → T024 (verification)
```

### Phase 6 — final polish + audits all parallel

```
[T025 revert p10 workaround]   [T026 revert p12 workaround]
[T027 ci.sh end-to-end]        [T028 SPDX check]
[T029 SC roll-up]              [T030 nsl-coupling-audit]
[T031 nsl-constitution-review]
```

## Implementation Strategy

**MVP-first delivery**: After Phase 3 (US1) lands, the canonical pp.ebnf P5 example works — that's the natural cut line if schedule pressure forced a partial PR. US2 (adjacency edge cases) and US3 (recursion + cycle detection) extend the increment. **If schedule cuts deeper still**, Phase 3 + Phase 5 (US1 + US3, skipping the US2 verification-only fixtures) is also a defensible cut — the cycle-detection fixture (T020) is the only place the FR-007 locked diagnostic is exercised.

**Incremental delivery at PR boundary**: Single PR landing all 6 phases is the natural shape. Per CONTRIBUTING §5 squash-merge guidance, the per-task TDD evidence (RED-state SHAs from T007, T021) is preserved in the merge commit's body or in the PR description.

**Parallelism opportunities**: Within each phase, all `[P]`-marked tasks above can be assigned to different contributors / agents simultaneously. This feature is small enough that a single contributor can drive it solo over a half-day; the parallelism markers are documentation, not a forced decomposition.

## Test-First Discipline (Constitution Principle VIII)

Every task pair in this plan is structured as **(test-author commit → implementation commit)**. The TDD evidence path is:

1. The `[P] [US?] Author <test_file>` task lands first as a discrete commit. The commit message is `test(preprocess): T<NNN> — <description>` and the PR description records the SHA + the observation that the test runs RED on this commit.
2. The corresponding implementation task lands next. The commit message is `preprocess: T<NNN> — implement <thing>` and the test runs GREEN.

For the FR-007 locked diagnostic (`recursive macro expansion: <NAME>`), the fail-fixture in T020 cites the **exact** message string per the M1 FR-037 / Principle VIII discipline. Renaming or weakening the string later requires updating both the contract (`macro-expansion-rules.contract.md`) and the fail-fixture in the same change.
