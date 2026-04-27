<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->
---
description: "Tasks for 004-clang-tidy-cleanup — retire CI static-checks debt"
---

# Tasks: clang-tidy Cleanup — Retire CI Static-Checks Debt

**Input**: Design documents from `/specs/004-clang-tidy-cleanup/`
**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md, contracts/

**Tests**: This feature has **no new test fixtures**. Test-First for a cleanup feature is interpreted per Principle VIII as: every per-category commit's RED-state evidence is the warnings-treated-as-errors count visible at HEAD~1, and the GREEN-state evidence is the count drop in this commit. The commit-message body cites both per FR-011 and the contract at `contracts/cleanup-commit.contract.md`. The full lit + ctest suites continue to act as the regression set (FR-007 + SC-003); no new fixtures are authored.

**Organization**: Phase 3 (US1) is the bulk of the work and splits into per-category tasks. Phase 4 (US2) is the constitution close-out. Phase 5 (US3) is the regression-prevention probe. Phase 6 is polish + agent audits.

## Format: `[ID] [P?] [Story?] Description with file path`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks).
- **[Story]**: User-story label (US1, US2, US3) — required for user-story phase tasks; absent for Setup / Foundational / Polish.
- Every task description includes the exact file path or directory scope.

## Path Conventions

- All work happens at the repo root and within `include/`, `lib/`, `tools/`, `test_unit/`, plus the project-policy artifacts `.clang-tidy` and `.specify/memory/constitution.md`.
- No new files, no new directories, no removed files.
- All measurements (warning counts, test pass rates) are taken inside the canonical container `ghcr.io/koyamanx/nsl-nslc:dev` per memory note `project_build_environment.md`.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Capture the master-HEAD baseline so every subsequent task has a measurable RED-state count.

- [X] T001 Sanity-verify the M1+M3 baseline is green inside `ghcr.io/koyamanx/nsl-nslc:dev`. Run `cmake --build build-Release-host && ctest --test-dir build-Release-host && cmake --build build-Release-host --target check-nslc`; expect 118/118 lit + 129/129 ctest passing on master HEAD `73e49ae`. Capture the `static-checks` warning count: `./scripts/ci.sh static-checks 2>&1 | grep "warnings treated as errors"` should report **927**.

**Checkpoint**: Baseline counts captured. Every subsequent commit's metadata (per `contracts/cleanup-commit.contract.md`) cites the running total.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: clang-format sweep across the entire tree per `research.md` §3 step 1. This MUST be the first edit or every subsequent fix re-trips formatting violations on the touched lines.

**⚠️ CRITICAL**: All Phase 3 user-story work depends on this phase being complete.

- [X] T002 Run `clang-format -i` across the entire source tree (`include/`, `lib/`, `tools/`, `test_unit/`, plus the relevant CMake support files if `.clang-format` covers them). Verify SPDX header preservation per FR-008 (`scripts/check_spdx.py --all` reports `295/0/128`). Run lit + ctest; expect GREEN. Commit per `contracts/cleanup-commit.contract.md` schema with `<scope>=tree`, `<category>=clang-format`, `<count>=325`.

**Checkpoint**: Tree is canonically formatted. Subsequent per-category commits won't fight the formatter.

---

## Phase 3: User Story 1 — CI static-checks gate goes green on master (Priority: P1) 🎯 MVP

**Goal**: Drive the `static-checks` warning count from 927 to 0 across the existing source tree, per `research.md` §1's per-category dispositions and §3's commit sequencing.

**Independent Test**: After this phase, `./scripts/ci.sh static-checks` exits 0 inside the canonical container with no warnings-treated-as-errors output. Independent of US2 (constitution edit) and US3 (regression-prevention probe).

### Tests for User Story 1 (Constitution Principle VIII) ⚠️

> Per FR-007 + SC-003: every per-category task below MUST run the full lit (118 fixtures) + ctest (129 cases) suites GREEN before its commit lands. The "tests" for this cleanup are the existing layered test suites — no new fixtures are authored.

### Implementation for User Story 1 (per-category, per `research.md` §3)

**Mechanical-fix block** — automated `--fix` works safely per category:

- [X] T003 [US1] `llvm-include-order` (12 sites) — **no-op**: the clang-format sweep in T002 already reordered the include blocks; post-T002 there are 0 llvm-include-order warnings remaining. No commit needed.
- [X] T004 [US1] `readability-uppercase-literal-suffix` (59 sites): `run-clang-tidy -checks='-*,readability-uppercase-literal-suffix' -fix`. Verify GREEN. Commit; expect `915 → 856`.
- [X] T005 [US1] `readability-braces-around-statements` (4 sites): same shape. Commit; expect `856 → 852`.
- [X] T006 [US1] `modernize-use-auto` (6 sites): same shape. Commit; expect `852 → 846`.
- [X] T007 [US1] `modernize-return-braced-init-list` (24 sites): same shape. Commit; expect `846 → 822`.
- [X] T008 [US1] `cppcoreguidelines-init-variables` (9 sites): same shape. Commit; expect `822 → 813`.

**Include-cleaner block** — done after the format sweep so reordered includes don't fight the cleaner:

- [X] T009 [US1] `misc-include-cleaner` (35 sites): `run-clang-tidy -checks='-*,misc-include-cleaner' -fix`. Verify GREEN. Commit; expect `813 → 778`.

**Bool-conversion block**:

- [X] T010 [US1] `readability-implicit-bool-conversion` (27 sites, 5 in `tools/nslc/main.cpp`): preserve the 60-line cap on `tools/nslc/main.cpp` per Principle II. Same fix shape. Commit; expect `778 → 751`.

**Const-correctness block** (biggest single category — split per-directory per `research.md` §3 step 5 to keep per-commit diff reviewable):

- [X] T011 [US1] `misc-const-correctness` (~150 sites in `include/`): scope to `include/nsl/**/*.h`. Verify GREEN. Commit; expect approximately `751 → 601`.
- [X] T012 [US1] `misc-const-correctness` (~100 sites in `lib/Basic` + `lib/Lex` + `lib/Driver`): scope to those three layer directories. Verify GREEN. Commit; expect approximately `601 → 501`.
- [X] T013 [US1] `misc-const-correctness` (~150 sites in `lib/Preprocess` + `lib/Parse` + `lib/AST` + `lib/Sema`): **CAUTION** — `lib/Preprocess` carries feature 003's `MacroExpander` + the FR-007 locked diagnostic. Verify the macro_expander_test gtest still passes byte-identically before commit. Commit; expect approximately `501 → 351`.
- [X] T014 [US1] `misc-const-correctness` (~30 sites in `tools/` + `test_unit/`): scope to `tools/` + `test_unit/`. Verify GREEN. Commit; expect approximately `351 → 317`.

**Identifier-naming block** — done after const-correctness so the resulting renames are already on `const` references:

- [ ] T015 [US1] `readability-identifier-naming` (114 sites): scoped per-directory if the diff is too large; one PR-friendly commit if the rename clusters tightly. Verify GREEN. Commit; expect `317 → 203`.

**Nodiscard block** — adds `[[nodiscard]]` to public APIs; cascades "you-dropped-the-return-value" warnings against existing call sites which are fixed in the same commit:

- [ ] T016 [US1] `modernize-use-nodiscard` (47 sites + cascade): `run-clang-tidy -checks='-*,modernize-use-nodiscard' -fix`. Then run a second pass to catch the cascade `[[nodiscard]]`-discarded-result warnings; either consume the result at the call site or wrap with `(void)`. Public-API surface frozen per FR-010 (attribute is allowed; signature change is not). Verify GREEN. Commit; expect `203 → 156`.

**Long-tail block** — combined into a single mechanical commit since each is small:

- [ ] T017 [US1] Long-tail categories combined into one commit: `modernize-loop-convert` (7), `readability-simplify-boolean-expr` (5), `cppcoreguidelines-special-member-functions` (5), `cppcoreguidelines-pro-bounds-constant-array-index` (5; some sites need `// NOLINTNEXTLINE` per `data-model.md` Entity 4), `cppcoreguidelines-pro-type-member-init` (4), `misc-unused-parameters` (3), `readability-use-anyofallof` (2), and the singletons (`readability-make-member-function-const`, `readability-isolate-declaration`, `misc-unused-using-decls`, `llvm-namespace-comment`, `cppcoreguidelines-owning-memory`). Verify GREEN. Commit; expect `156 → ~76`.

**Mixed-disposition per-site block**:

- [ ] T018 [US1] `readability-convert-member-functions-to-static` (17 sites): per-site judgment per `research.md` §1 row "MIXED". Fix the genuinely-static-utility ones (~8 sites likely); add `// NOLINTNEXTLINE(readability-convert-member-functions-to-static) <rationale>` for the semantic-membership ones (~9 sites). Each NOLINT must satisfy `data-model.md` Entity 4 schema (parenthesized category + one-line rationale). Verify GREEN. Commit; expect approximately `~76 → ~59`.

**Suppression block** — globally suppress the 5 categories whose refactor would exceed feature scope per `research.md` §1:

- [ ] T019 [US1] Update `.clang-tidy` (entity 1 of `data-model.md`) with the global allow-list + the rationale block for the 5 suppressed categories: `misc-non-private-member-variables-in-classes` (22), `cppcoreguidelines-avoid-const-or-ref-data-members` (16), `misc-no-recursion` (14), `readability-function-cognitive-complexity` (13), `cppcoreguidelines-avoid-do-while` (7). Verify the gate exits 0 (`./scripts/ci.sh static-checks 2>&1 | grep "warnings treated as errors"` returns empty). Commit; expect `~59 → 0`.

**Verification gate**:

- [ ] T020 [US1] Run `./scripts/ci.sh all` end-to-end inside the container — all 6 stages exit 0 (build-matrix, static-checks, unit-tests, lowering-tests, e2e-empty, formal-empty). Verify SC-001 and SC-002.

**Checkpoint**: US1 MVP fully functional. The static-checks gate is GREEN on the working tree. US2 (constitution edit) and US3 (regression probe) extend this.

---

## Phase 4: User Story 2 — Constitution Principle IX transitional clause is retired (Priority: P2)

**Goal**: Now that the gate is durably green at the working tree, edit `.specify/memory/constitution.md` to remove the transitional clause from Principle IX, leaving only the steady-state rule.

**Independent Test**: A reader of the post-cleanup constitution finds no "transitional" wording under Principle IX. The agent registry's mention of "land P-CI early to retire the Principle IX transitional clause" is no longer load-bearing.

### Implementation for User Story 2

- [ ] T021 [US2] Read the current Principle IX section of `.specify/memory/constitution.md` carefully. Identify the transitional-clause paragraph + any "Pipeline stages" / "Governance" cross-references that point at the transitional clause. Per `data-model.md` Entity 2: the steady-state rule text MUST remain BYTE-IDENTICAL; the close-out commit is a pure deletion of the transitional paragraph + lockstep removal of the cross-references.
- [ ] T022 [US2] Edit `.specify/memory/constitution.md` to remove the transitional clause from Principle IX and update the Governance / Pipeline-stages cross-references to drop their "transitional clause" pointers. Verify with `awk '/^### Principle IX/{p=1} p && /^### /{if (NR>1 && !/Principle IX/) p=0} p' .specify/memory/constitution.md | grep -c "transitional"` returning exactly **0** (per quickstart.md §6 + SC-004).
- [ ] T023 [US2] Verify the gate is still green: `./scripts/ci.sh all` exits 0 inside the container. The constitution edit is doc-only; it MUST not affect any code or test outcome. Commit per `contracts/cleanup-commit.contract.md` with `<count>=0` (the doc-only commit is the explicit exception in the contract).

**Checkpoint**: Constitution close-out done. SC-004 verified.

---

## Phase 5: User Story 3 — The static-checks gate stays green going forward (Priority: P3)

**Goal**: Verify that the `WarningsAsErrors: '*'` mechanism in `.clang-tidy` (per `research.md` §2) catches a deliberately-introduced single new warning. No new infrastructure to build — the existing CI gate IS the regression-prevention mechanism once debt = 0.

**Independent Test**: A test PR with one deliberate warning fails CI's static-checks stage; a test PR with one explicit `// NOLINTNEXTLINE(<category>) <rationale>` exemption passes.

### Implementation for User Story 3

- [ ] T024 [US3] Open a throwaway test branch off the post-Phase-4 HEAD; introduce a single deliberate `cppcoreguidelines-init-variables` warning (e.g., add `int foo;` inside an existing `.cpp` file). Push and observe CI: the `static-checks` stage MUST fail with the warning category and source location reported in the log (per US3 acceptance scenario 1 + SC-006).
- [ ] T025 [US3] On the same throwaway branch, add a `// NOLINTNEXTLINE(cppcoreguidelines-init-variables) probe-only test, never merged` comment above the introduced line. Push and observe CI: the `static-checks` stage MUST pass (per US3 acceptance scenario 2). Verifies the documented escape hatch.
- [ ] T026 [US3] Discard the throwaway branch (`git push origin :<probe-branch>`). The probe is verification only — it does not land on master. Document the verification result in the PR description for the main `004-clang-tidy-cleanup` branch.

**Checkpoint**: Regression-prevention mechanism verified. All 3 user stories complete.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Final SC roll-up + agent audits before PR-open.

### Final checks

- [ ] T027 [P] Run `python3 scripts/check_spdx.py --all` — every modified file's SPDX header is intact (FR-008). Expect `295/0/128` or higher (whatever the baseline + any new file count).
- [ ] T028 [P] Verify SC-005: `git rev-list --count master..HEAD` reports ≥ 4 commits (likely 12-15 given the per-directory const-correctness splits). Run `git bisect-friendly` sanity: pick 2 random commits in the feature-branch range, check out each, run `cmake --build build-Release-host` + `ctest --test-dir build-Release-host`; both must build clean and pass.
- [ ] T029 [P] SC roll-up: write a one-paragraph PR-comment summary citing each of SC-001..SC-006 and the corresponding evidence (commit SHA range for SC-005, awk grep result for SC-004, per-stage exit codes for SC-001, etc.).
- [ ] T030 [P] Verify FR-009: `grep -rn "TODO\|FIXME\|XXX\|HACK" --include="*.cpp" --include="*.h" --include="*.cmake" --include="CMakeLists.txt" include/ lib/ tools/ cmake/` returns empty. The cleanup MUST NOT introduce any TODO/FIXME workarounds (suppressions live in `.clang-tidy` global config or `// NOLINTNEXTLINE` per-site, never as TODO markers).

### Agent-driven audits

- [ ] T031 [P] Spawn `nsl-coupling-audit` agent (READ-ONLY) on the working tree. Expect **0 blocking findings**. The cleanup is style/correctness only; no spec/design coupling surface should change.
- [ ] T032 [P] Spawn `nsl-constitution-review` agent (READ-ONLY). Expect zero blocking findings; reaffirm Principles I (no spec change), VI (test-first preserved via per-commit lit+ctest gates), IX (transitional clause retired in T022).

**Checkpoint**: PR-ready. Static-checks GREEN, constitution close-out clean, regression-prevention verified, agent audits clean.

---

## Dependencies & Story Completion Order

```
Phase 1 (Setup, T001 — baseline capture)
    │
    ▼
Phase 2 (Foundational: clang-format sweep, T002)
    │
    ▼
Phase 3 (US1: per-category warning cleanup, T003–T020)
    │      ┌── T003–T010 mechanical-fix block (mostly parallel-safe across files but
    │      │    sequential by category to keep diffs reviewable)
    │      ├── T011–T014 const-correctness per-directory (sequential — same category
    │      │    cascades, must verify count drop after each)
    │      ├── T015 identifier-naming
    │      ├── T016 nodiscard + cascade
    │      ├── T017 long-tail combined commit
    │      ├── T018 mixed-disposition (NOLINTNEXTLINE per-site)
    │      ├── T019 .clang-tidy config update (suppressions block)
    │      └── T020 verification gate (./scripts/ci.sh all GREEN)
    │
    ▼
Phase 4 (US2: constitution close-out, T021–T023)
    │
    ▼
Phase 5 (US3: regression-prevention probe, T024–T026)
    │
    ▼
Phase 6 (Polish + audits, T027–T032)
```

**Story-level dependencies**:

- US1 depends on Phase 1 + Phase 2 only.
- US2 depends on US1 (you cannot retire a transitional rule that is still actively masking a red gate).
- US3 depends on US2 (the probe is performed AFTER the constitution close-out so the test PR faces the steady-state Principle IX rule).

**Within-phase dependencies** (the most important ones):

- Phase 3: T003–T010 mostly sequential by category (per `research.md` §3). T011–T014 sequential (same category cascade). T015 after const-correctness. T016 after identifier-naming. T017 after T016. T018 after T017. T019 after T018. T020 after T019.
- Phase 6: All `[P]` tasks parallelizable (different output files / agent invocations).

## Parallel Execution Examples

### Phase 3 — most tasks are sequential by design

Phase 3's per-category sequence is **deliberately** sequential per `research.md` §3 + memory note `feedback_clang_tidy_batch_unsafe.md`. The only parallelism is across the post-cleanup polish tasks in Phase 6.

### Phase 6 — final polish + audits all parallel

```text
[T027 SPDX check]            [T028 bisect sanity]
[T029 SC roll-up]            [T030 TODO/FIXME check]
[T031 nsl-coupling-audit]    [T032 nsl-constitution-review]
```

## Implementation Strategy

**MVP-first delivery**: After Phase 3 (US1) lands, the static-checks gate is green at the working tree — that's the natural cut line if schedule pressure forced a partial PR. US2 (constitution close-out) can land in a follow-up PR; US3 (regression probe) is verification-only and can be a comment on the main PR.

**Incremental delivery at PR boundary**: Single PR landing all 6 phases is the natural shape. Per CONTRIBUTING §5 squash-merge guidance, the per-category cleanup history is preserved in the merge commit's body (FR-011-driven commit metadata makes this a self-documenting log).

**Per-category-commit discipline (FR-004)**: Every commit on the feature branch must satisfy `contracts/cleanup-commit.contract.md`. This produces a bisect-friendly history (SC-005) where a regression at any commit lands on a buildable, lit-green, ctest-green tree.

**Total expected commit count**: ~12-15 commits on the feature branch (T002 + T003-T010 = 8 mechanical + T011-T014 = 4 const-correctness + T015 + T016 + T017 + T018 + T019 + T022). Comfortably above SC-005's "≥ 4" floor.

## Test-First Discipline (Constitution Principle VIII)

Per the spec.md FR-007 and the cleanup-commit.contract.md pre/post-conditions, every per-category commit's TDD evidence is the warning-count delta visible in CI:

1. **RED-state evidence** (before commit): `./scripts/ci.sh static-checks` reports `<previous-count>` warnings-treated-as-errors at HEAD~1.
2. **GREEN-state evidence** (after commit): `./scripts/ci.sh static-checks` reports `<new-count> < <previous-count>` warnings at HEAD. The commit-message `Categories cleared` block names the category and the delta.

The full lit (118) + ctest (129) suites act as the regression set per FR-007 + SC-003 — every commit must run them green before landing.

For the FR-007 / FR-037 locked diagnostic set inherited from M1 (`recursive macro expansion: <NAME>`, `undefined macro reference: '%<NAME>%'`, etc.), every per-category commit MUST verify these strings remain byte-identical. Renaming or weakening any of them is out of scope for this feature; if a clang-tidy fix would touch one, the disposition flips to "suppress" with a rationale.
