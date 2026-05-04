---
description: "Task list for T1 — TextMate Grammar + Language Configuration for NSL"
---

# Tasks: T1 — TextMate Grammar + Language Configuration for NSL

**Input**: Design documents from `/specs/009-t1-textmate-grammar/`
**Prerequisites**: `plan.md` (required), `spec.md` (required), `research.md`, `data-model.md`, `contracts/`

**Tests**: Test tasks are **MANDATORY** per Constitution Principle
VIII (Test-First Development, NON-NEGOTIABLE). T1 introduces a new
test layer (TextMate scope tests via `vscode-tmgrammar-test`) for
which Principle VI's per-layer accepted-driver list does not
constrain the choice (its enumeration is for compiler test layers).
Every user story below MUST land its scope-test fixture + assertion
file FIRST and observe the runner FAILING against the unchanged
tree before any grammar pattern emission lands.

**Organization**: Tasks are grouped by user story to enable
independent implementation and incremental MVP shipping. T1's MVP
is User Story 1 (US1) — keyword / numeric / comment / string
colouring; the audited corpus becomes readable on GitHub the
moment US1 lands.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: User-story label (`[US1]`, `[US2]`, `[US3]`, `[US4]`)
- All file paths are relative to repo root unless noted

## Path Conventions

- Compiler-side files: `include/`, `lib/`, `tools/`, `test/lex/`, etc.
- T1-introduced files: `grammars/textmate/`, `editors/vscode/`,
  `test/tooling/textmate/`, `scripts/gen_textmate_*.py`
- T1 amends: `scripts/ci.sh` (one block of edits per task)

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create the directory tree and install the scope-test
runner. No grammar / fixture authoring yet.

- [ ] T001 Create the T1 directory tree: `mkdir -p grammars/textmate editors/vscode/syntaxes test/tooling/textmate/fixtures test/tooling/textmate/scope-tests` and add `.gitkeep` placeholders so empty subdirs survive the initial commit
- [ ] T002 [P] Author `test/tooling/textmate/package.json` declaring `vscode-tmgrammar-test` as a `devDependency` (pinned to a specific version per Constitution Principle V determinism); include the `_comment_top` SPDX header per `research.md §2`
- [ ] T003 [P] Author `test/tooling/textmate/.gitignore` excluding `node_modules/` and `package-lock.json` rebuild artefacts; commit `package-lock.json` itself for build reproducibility
- [ ] T004 Inside `test/tooling/textmate/`, run `npm install` to produce `package-lock.json`; verify `npx vscode-tmgrammar-test --help` succeeds

**Checkpoint**: Empty test scaffolding exists; the runner is invokable.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Create the generator skeleton (`scripts/gen_textmate_grammar.py`)
and its companion fixture generator. The skeleton must be the
single source of truth for both the grammar JSON and the fixture
corpus per `data-model.md §2` coupling diagram.

**⚠️ CRITICAL**: No user-story work can begin until this phase is
complete — every grammar emission task in Phase 3+ edits this
script.

- [ ] T005 Create `scripts/gen_textmate_grammar.py` skeleton (Python 3, SPDX header on line 1 per `scripts/gen_keyword_fixtures.py` precedent); the script reads `include/nsl/Lex/KeywordSet.def`, holds the `KEYWORD_CATEGORY` table per `data-model.md §1.2` covering all 42 spellings, raises with a localised error when a `KeywordSet.def` row has no category mapping (per quickstart §4 step 3 message), and emits a minimal `grammars/textmate/nsl.tmLanguage.json` containing only the `_comment_top` SPDX header, `name`, `scopeName: "source.nsl"`, `fileTypes: ["nsl","nslh","inc"]`, and an empty `patterns` array
- [ ] T006 [P] Create `scripts/gen_textmate_fixtures.py` skeleton (Python 3, SPDX header on line 1); the script reads `include/nsl/Lex/KeywordSet.def` and emits `test/tooling/textmate/fixtures/all-keywords.nsl` with one occurrence per keyword (no assertions yet — assertions are added in T010); the fixture is a valid NSL skeleton (e.g. each keyword wrapped in a comment-fenced minimal context) so the runner does not error on parse-edge cases
- [ ] T007 Run both generators twice and verify byte-stable output (Constitution Principle V): `python3 scripts/gen_textmate_grammar.py && cp grammars/textmate/nsl.tmLanguage.json /tmp/g1.json && python3 scripts/gen_textmate_grammar.py && diff /tmp/g1.json grammars/textmate/nsl.tmLanguage.json` returns 0; same for the fixture generator

**Checkpoint**: Foundation ready — user-story implementation can now begin.

---

## Phase 3: User Story 1 — NSL author opens a file and sees structure (Priority: P1) 🎯 MVP

**Goal**: Every reserved keyword from `nsl_lang.ebnf §15`, every
numeric form from §13, every comment / string form from §14, and
every operator from `nsl_tooling_design.md §4.1` highlights with
the correct TextMate scope. No comment-shadowed or string-shadowed
keyword wins the keyword scope. Audited-corpus NSL files become
readable in any TextMate-compatible viewer.

**Independent Test**: Run `./scripts/ci.sh tooling-textmate`
locally (or `npx vscode-tmgrammar-test ...` directly per
`quickstart §3`); all assertions in `all-keywords.spec`,
`all-numbers.spec`, `comments-and-strings.spec`, and
`all-operators.spec` pass. Open `rv32x_dev/main.nsl` (or hand-
written equivalent if P-VEN not yet landed — see spec Assumptions)
in VS Code with the T1 extension loaded; the Inspect-Tokens panel
shows expected scopes on representative tokens.

### Tests for User Story 1 (MANDATORY per Constitution Principle VIII) ⚠️

**Land these tests FIRST and observe FAILING against the unchanged
tree before T015–T020 implementation tasks. Record the failing-
state commit hash in the PR description per Principle VIII no-
retrofitted-tests clause.**

- [ ] T008 [P] [US1] Author `test/tooling/textmate/scope-tests/all-keywords.spec` (4-line YAML config per `contracts/scope-test-format.contract.md §2` pointing at `../fixtures/all-keywords.nsl` and `../../../grammars/textmate/nsl.tmLanguage.json`)
- [ ] T009 [P] [US1] Extend `scripts/gen_textmate_fixtures.py` to emit `// <- <scope>` assertion comments in `all-keywords.nsl` (one assertion per keyword line, scope determined by the `KEYWORD_CATEGORY` table per `data-model.md §1.2`)
- [ ] T010 [P] [US1] Hand-author `test/tooling/textmate/fixtures/all-numbers.nsl` per `contracts/scope-test-format.contract.md §1.2` covering all 5 numeric forms (decimal, hex, binary, octal, Verilog-sized × b/o/d/h) plus one each with Z/X/U markers and underscore separators; embed `// <-` and `//   ^^^` assertions per `data-model.md §1.4` scope bindings
- [ ] T011 [P] [US1] Author `test/tooling/textmate/scope-tests/all-numbers.spec` runner config pointing at the new fixture
- [ ] T012 [P] [US1] Hand-author `test/tooling/textmate/fixtures/comments-and-strings.nsl` per the contract: one line comment, one block comment, one string with backslash escape, one block comment containing a keyword spelling (e.g. `/* func */`), one string containing a keyword spelling (`"reg q[8];"`); embed assertions verifying the keyword spellings DO NOT carry `keyword.*` / `storage.*` scopes (negative-coverage assertions per `data-model.md §1.8 / §1.9`)
- [ ] T013 [P] [US1] Author `test/tooling/textmate/scope-tests/comments-and-strings.spec` runner config
- [ ] T014 [P] [US1] Hand-author `test/tooling/textmate/fixtures/all-operators.nsl` covering all 7 operator categories from `contracts/grammar-coverage.contract.md §5` (arithmetic / bitwise / shift / comparison / logical / assignment / extension) with multi-character variants (`==`, `<=`, `&&`, `||`, `++`, `--`, `:=`, `<<`, `>>`); embed scope assertions
- [ ] T015 [US1] Author `test/tooling/textmate/scope-tests/all-operators.spec` and run `cd test/tooling/textmate && npx vscode-tmgrammar-test --grammar ../../grammars/textmate/nsl.tmLanguage.json --tests "scope-tests/{all-keywords,all-numbers,comments-and-strings,all-operators}.spec"`; capture the failing-output verbatim into the PR description (Principle VIII)

### Implementation for User Story 1

- [ ] T016 [US1] Implement keyword pattern emission in `scripts/gen_textmate_grammar.py`: emit one `repository` group per category in `data-model.md §1.2` (`declaration`, `control_block`, `control_flow`, `modifier`, `storage_type` with sub-categories, `port_direction`, `support_type_clock`); each group's regex is `\b(spelling1|spelling2|…)\b` over the spellings in that category; include a top-level `patterns` reference to each group
- [ ] T017 [US1] Implement built-in `_`-prefix system-name pattern emission in `scripts/gen_textmate_grammar.py` per `data-model.md §1.3` (frozen list — `_display`/`_monitor`/…/`_random`/`_time`); two `repository` groups: `support.function.system.nsl` and `support.variable.system.nsl`
- [ ] T018 [US1] Implement numeric-literal patterns in `scripts/gen_textmate_grammar.py` per `data-model.md §1.4` and `contracts/grammar-coverage.contract.md §3`; emit patterns in the order Verilog-sized → hex → binary → octal → decimal (first-match wins; pattern ordering is the contract per `data-model §1.4`)
- [ ] T019 [US1] Implement comment + string + escape patterns in `scripts/gen_textmate_grammar.py` per `data-model.md §1.8` (line + non-nestable block) and `§1.9` (string with backslash-escape sub-scope); use TextMate `begin`/`end` rules so embedded keyword spellings do not match keyword scopes
- [ ] T020 [US1] Implement operator patterns in `scripts/gen_textmate_grammar.py` per `data-model.md §1.5` and `contracts/grammar-coverage.contract.md §5`; emit multi-character variants before single-character (per the pattern-ordering note in §5)
- [ ] T021 [US1] Regenerate the canonical artefact: `python3 scripts/gen_textmate_grammar.py && python3 scripts/gen_textmate_fixtures.py`; verify `git diff` shows updates only to `grammars/textmate/nsl.tmLanguage.json`, `test/tooling/textmate/fixtures/all-keywords.nsl` (with assertions appended), and no other files
- [ ] T022 [US1] Re-run the runner from T015's command line; verify ALL US1 assertions GREEN; capture the green output for the PR description (Principle VIII red-then-green pair)
- [ ] T023 [US1] Run `python3 scripts/check_spdx.py grammars/textmate/nsl.tmLanguage.json` and verify it passes — confirms the `_comment_top` SPDX-key convention from `research.md §2` is recognised

**Checkpoint**: User Story 1 fully functional — keywords, numbers,
comments, strings, and operators colour correctly across the
audited corpus. T1's MVP is shippable at this point even before
US2-US4 land.

---

## Phase 4: User Story 2 — VS Code editor affordances (Priority: P2)

**Goal**: NSL authoring in VS Code gains baseline affordances —
auto-close brackets, comment toggle, indent on `{ ⏎`, word-pattern
recognition. Drop-folder install completes in ≤ 60 s (SC-004).

**Independent Test**: Drop `editors/vscode/` into
`~/.vscode/extensions/nsl-0.1.0/`; restart VS Code; open a fresh
`.nsl` file; type `module foo {⏎` and confirm the auto-inserted
`}` lands on its own line with the cursor indented; select a
region and invoke the comment-toggle command and confirm the
selection is wrapped in `// `.

### Implementation for User Story 2

- [ ] T024 [P] [US2] Author `editors/vscode/language-configuration.json` per `contracts/language-config.contract.md` — exact field set in §§2–7 (comments, brackets, autoClosingPairs, surroundingPairs, wordPattern, indentationRules); include the `_comment_top` SPDX header per `research.md §2`; explicitly document in the contract-cited fields why `'` is excluded from `autoClosingPairs` (Verilog-sized literal protection per `language-config.contract.md §4`)
- [ ] T025 [P] [US2] Author `editors/vscode/package.json` per `research.md §7`: minimal extension manifest with `name: "nsl"`, `version: "0.1.0"`, `engines.vscode: "^1.70.0"`, `categories: ["Programming Languages"]`, and the `contributes.languages` + `contributes.grammars` entries from research §7; include the `_comment_top` SPDX header
- [ ] T026 [US2] Create the `editors/vscode/syntaxes/nsl.tmLanguage.json` symlink to `../../../grammars/textmate/nsl.tmLanguage.json` (via `ln -s` on Linux/CI, per `research.md §5`); verify the symlink resolves with `realpath editors/vscode/syntaxes/nsl.tmLanguage.json` matching the canonical path
- [ ] T027 [US2] Add a stage-2 sub-step `tooling-grammar-mirror` to `scripts/ci.sh` that asserts byte-equality between `grammars/textmate/nsl.tmLanguage.json` and `editors/vscode/syntaxes/nsl.tmLanguage.json` (handles the non-symlink Windows-extraction fallback per `research.md §5`); the sub-step exits non-zero on mismatch, halting CI
- [ ] T028 [US2] Manual verification (recorded in PR description): on a Linux host with VS Code installed, run `ln -s "$PWD/editors/vscode" ~/.vscode/extensions/nsl-0.1.0`, restart VS Code, open `test/tooling/textmate/fixtures/all-keywords.nsl`, run Command Palette → `Developer: Inspect Editor Tokens and Scopes`, click on a `module` keyword token and confirm the panel reports `keyword.declaration.nsl` + `source.nsl` (per `quickstart.md §2`); time the install start-to-end and verify ≤ 60 s for SC-004

**Checkpoint**: User Stories 1 and 2 both work — the grammar
colours correctly AND VS Code provides editor affordances.

---

## Phase 5: User Story 3 — Preprocessor directives + macro splices (Priority: P3)

**Goal**: `#include`, `#define`, `#undef`, `#if`, `#ifdef`,
`#ifndef`, `#else`, `#endif`, `#line` and `%IDENT%` references
all carry distinct scopes from NSL-language keywords; readers
see the preprocessor seam (P12) at a glance.

**Independent Test**: Run `npx vscode-tmgrammar-test --tests "scope-tests/all-directives.spec"`
and `…/macro-references.spec`; all assertions pass. Visually
inspect the rendered fixture in VS Code with a colour theme that
distinguishes `keyword.directive.preprocessor.nsl` from
`keyword.declaration.nsl`; confirm the difference.

### Tests for User Story 3 (MANDATORY per Constitution Principle VIII) ⚠️

- [ ] T029 [P] [US3] Hand-author `test/tooling/textmate/fixtures/all-directives.nsl` per `contracts/scope-test-format.contract.md §1.2` — one line per directive (`#include "foo.nsl"`, `#define WIDTH 8`, `#undef WIDTH`, `#if 1`, `#ifdef DEBUG`, `#ifndef RELEASE`, `#else`, `#endif`, `#line 42 "src.nsl"`); embed `// <-` assertions verifying each directive carries `keyword.directive.preprocessor.nsl` and that the body of `#include "foo.nsl"` carries `string.quoted.double.nsl` (mixed-scope assertion per `data-model.md §1.6`)
- [ ] T030 [P] [US3] Author `test/tooling/textmate/scope-tests/all-directives.spec`
- [ ] T031 [P] [US3] Hand-author `test/tooling/textmate/fixtures/macro-references.nsl` with at least one `%IDENT%` reference inside a `reg` declaration (canonical use per `nsl_pp.ebnf §4` examples — e.g. `reg q[%WIDTH%];`); embed assertions verifying `%WIDTH%` carries `variable.other.macro.nsl` per `data-model.md §1.7`
- [ ] T032 [P] [US3] Author `test/tooling/textmate/scope-tests/macro-references.spec`
- [ ] T033 [US3] Run `cd test/tooling/textmate && npx vscode-tmgrammar-test --grammar ../../grammars/textmate/nsl.tmLanguage.json --tests "scope-tests/{all-directives,macro-references}.spec"`; capture FAILING output for the PR description

### Implementation for User Story 3

- [ ] T034 [US3] Add directive patterns in `scripts/gen_textmate_grammar.py` per `data-model.md §1.6` and `contracts/grammar-coverage.contract.md §6` — line-start anchored (`^\s*#…\b`); all 9 directives share the same scope `keyword.directive.preprocessor.nsl`
- [ ] T035 [US3] Add macro-reference pattern in `scripts/gen_textmate_grammar.py` per `data-model.md §1.7` and `contracts/grammar-coverage.contract.md §7` — `%[A-Za-z_][A-Za-z0-9_]*%` → `variable.other.macro.nsl`
- [ ] T036 [US3] Regenerate `grammars/textmate/nsl.tmLanguage.json` and re-run the T033 command; verify ALL US3 assertions GREEN; capture green output for the PR description

**Checkpoint**: User Stories 1, 2, and 3 all work — preprocessor
seam visible.

---

## Phase 6: User Story 4 — Drift gate (CI integration) (Priority: P3)

**Goal**: When `nsl_lang.ebnf §15` (and therefore
`include/nsl/Lex/KeywordSet.def`) gains a keyword and the
contributor forgets to regenerate the grammar, CI fails the PR
with a localised error. When the grammar is regenerated and
regenerator output diverges from what's checked in, CI fails the
PR. Spec ↔ grammar drift becomes mechanically impossible per
Constitution Principle VII.

**Independent Test**: Add a fake keyword to `KeywordSet.def`
without regenerating; run `./scripts/ci.sh static-checks`; observe
the `tooling-grammar-regen-check` sub-step fail with a clear "the
grammar is stale" message identifying which file needs
regenerating. Revert.

### Implementation for User Story 4

- [ ] T037 [US4] Add the `tooling-textmate` sub-step to `scripts/ci.sh` stage 3 (`unit & layer tests`) per `contracts/scope-test-format.contract.md §3` — invokes `cd test/tooling/textmate && npx vscode-tmgrammar-test --grammar ../../grammars/textmate/nsl.tmLanguage.json --tests "scope-tests/*.spec"`; non-zero exit fails stage 3
- [ ] T038 [US4] Add the `tooling-grammar-regen-check` sub-step to `scripts/ci.sh` stage 2 (`static-checks`) per `data-model.md §3 / §4`: runs `python3 scripts/gen_textmate_grammar.py && python3 scripts/gen_textmate_fixtures.py && git diff --exit-code -- grammars/textmate/ test/tooling/textmate/fixtures/all-keywords.nsl test/tooling/textmate/scope-tests/all-keywords.spec` (the assertion-bearing files only); non-zero exit fails stage 2 with a clear "regenerate-and-commit" message
- [ ] T039 [US4] Empirical drift-gate verification: temporarily edit `include/nsl/Lex/KeywordSet.def` to insert `KEYWORD(testdrift, "testdrift")` plus add `'testdrift': 'declaration'` to `scripts/gen_textmate_grammar.py`'s `KEYWORD_CATEGORY` map; do **not** regenerate; run `./scripts/ci.sh static-checks`; verify stage 2 fails with the regen-check sub-step diagnostic; revert all three edits cleanly
- [ ] T040 [US4] Verify the YAML invocation under `.github/workflows/ci.yml` already dispatches stages 2 and 3 by name (no edits expected — `ci.sh` and the workflow YAML stay convergent per `scripts/ci.sh` head-comment "The .github/workflows/ci.yml file calls into the same stage-name dispatch so divergence between local and remote runs is impossible.")

**Checkpoint**: User Story 4 active — drift between
`KeywordSet.def` and the grammar fails CI mechanically.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Validate the spec's success criteria (SC-001 through
SC-006) end-to-end, run the quickstart walkthroughs, and re-run
the Principle VII coupling audit.

- [ ] T041 [P] Run `./scripts/ci.sh all` locally inside the dev container (`ghcr.io/koyamanx/nsl-nslc:dev`); verify all 6 stages pass including the new `tooling-textmate` and `tooling-grammar-regen-check` sub-steps; record the green run in the PR description
- [ ] T042 [P] Verify SC-005 — total package size: `du -sb grammars/textmate/ editors/vscode/ | awk '{sum+=$1} END {print sum}'` reports ≤ 50 KB
- [ ] T043 [P] Verify SC-006 — scope-test runtime: `time ./scripts/ci.sh tooling-textmate` reports ≤ 10 s wall clock; record the timing in the PR description
- [ ] T044 [P] Verify SC-001 — open at least three audited-corpus files (`test/audited/rv32x_dev/main.nsl` if P-VEN has landed; otherwise hand-write equivalents per spec Assumptions) in VS Code with the extension loaded; spot-check 10+ tokens via Inspect-Tokens panel; confirm zero comment-shadowed or string-shadowed keywords show keyword scopes; record the spot-check log in the PR description
- [ ] T045 [P] Verify SC-002 — the runner's GREEN output from T022, T036, and the full T041 run reports ≥ 50 keyword-category assertions (≥ 42 from `KeywordSet.def` plus the 8 numeric forms, 9 directives, 7 operator categories, 1 macro form, comment/string negative-coverage assertions); record the assertion count
- [ ] T046 [P] Walk through `quickstart.md §4` end-to-end (the worked "add a new keyword" example, with cleanup) and verify the documented procedure matches the actual generator script behaviour; if any step diverges, fix the script or the doc in the same PR
- [ ] T047 Re-run the Principle VII coupling audit: confirm `CLAUDE.md §2.4` (T1 row) and `§2.5` (T1 columns in the editor matrix) still describe the deliverable accurately; no edits expected per `plan.md` Constitution-Check (already correct); record the audit result in the PR description
- [ ] T048 Confirm the `<!-- SPECKIT START -->` block in `CLAUDE.md` points at this feature's `plan.md` (already updated in `/speckit-plan` step; verify the link still resolves)
- [ ] T049 Run `python3 scripts/check_spdx.py --all` and verify all new files (the Python generators, the JSON artefacts via `_comment_top`, the fixtures via `//`-prefixed SPDX header on line 1) pass; if any new file is missing an SPDX header, add one — do NOT add to `scripts/spdx_exceptions.txt` for project-authored content

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately.
- **Foundational (Phase 2)**: Depends on Setup completion (needs the directories from T001 and the runner from T004). **Blocks all user stories** because every user-story task either edits `gen_textmate_grammar.py` (introduced in T005) or runs the runner (installed in T004).
- **User Story 1 (Phase 3)**: Depends on Phase 2 completion. **MVP** — sufficient for T1 first ship.
- **User Story 2 (Phase 4)**: Depends on Phase 2 completion; functionally independent of US1 but the symlink in T026 points at the canonical artefact populated by Phase 3 — the symlink resolves before Phase 3 lands (it points at the empty-but-valid skeleton from T005), so US2 can proceed in parallel with US1 implementation.
- **User Story 3 (Phase 5)**: Depends on Phase 2 completion; functionally independent of US1 and US2 (different fixtures, different patterns in the same generator script).
- **User Story 4 (Phase 6)**: Depends on the existence of US1-US3 fixtures and the canonical artefact (so the regen-check has something to compare against). Practically, US4 lands last among the user-story phases.
- **Polish (Phase 7)**: Depends on all four user stories being complete.

### User Story Dependencies

- **US1 (P1) — MVP**: Independent.
- **US2 (P2)**: Independent of US1's *patterns*; depends on the *existence* of `grammars/textmate/nsl.tmLanguage.json` (created in T005, populated through T021). The symlink target is valid as soon as T005 runs.
- **US3 (P3)**: Independent of US1 and US2 (different fixtures, different patterns).
- **US4 (P3) — drift gate**: Depends on the existence of US1's fixtures (it runs the runner against them). Lands last because it codifies the regen-and-test discipline that the prior stories follow manually.

### Within Each User Story

Tests MUST be written and observed FAILING before implementation
per Constitution Principle VIII. Within each story:

- Phase A: fixtures + spec runner config (parallel between fixtures)
- Phase B: run runner; observe FAILING; capture output for PR
- Phase C: implementation in `scripts/gen_textmate_grammar.py`
  (sequential because all edits hit the same file)
- Phase D: regenerate canonical artefact; re-run runner; observe GREEN

### Parallel Opportunities

Inside Phase 1: T002 and T003 are parallel (different files).

Inside Phase 2: T005 and T006 are parallel (different files).

Inside Phase 3 — fixture authoring: T008/T009 (one file pair),
T010/T011 (different file pair), T012/T013 (different file pair),
T014/T015 (different file pair) — three of the four pairs can run
in parallel because they touch distinct fixture/spec files; the
fourth (T015) runs the aggregated runner so it is sequential.

Inside Phase 3 — grammar pattern emission: T016, T017, T018, T019,
T020 all edit `scripts/gen_textmate_grammar.py` so are
**sequential** (no [P] markers). T021 (regenerate) and T022
(re-run runner) are sequential.

Inside Phase 4: T024 and T025 are parallel (different files); T026
through T028 are sequential.

Inside Phase 5 — fixture authoring: T029/T030 and T031/T032 are
two parallel pairs.

Inside Phase 6: all sequential (each edits `scripts/ci.sh`).

Inside Phase 7: T041 through T046 are parallel (different
verification activities, no shared file edits); T047, T048, T049
are sequential.

---

## Parallel Example: User Story 1 fixture authoring

```bash
# After Phase 2 is done, four fixture-authoring streams run in parallel:
# (each edits its own fixture file + spec runner config; none collides)

Task: "T008+T009 — Author all-keywords.spec + extend gen_textmate_fixtures.py to emit assertions"
Task: "T010+T011 — Hand-author all-numbers.nsl with assertions + author all-numbers.spec"
Task: "T012+T013 — Hand-author comments-and-strings.nsl with assertions + author comments-and-strings.spec"
Task: "T014       — Hand-author all-operators.nsl with assertions"

# T015 runs after all four complete (it runs the runner and captures FAILING output).
```

Pattern emission (T016–T020) is **sequential** because every task
edits `scripts/gen_textmate_grammar.py`; do them in priority order:
keywords (T016) → system names (T017) → numerics (T018) →
comments/strings (T019) → operators (T020).

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1 (Setup) — directories + runner installed.
2. Complete Phase 2 (Foundational) — generator skeletons in place,
   determinism verified.
3. Complete Phase 3 (User Story 1) — keyword/numeric/comment/
   string/operator colouring functional and tested.
4. **STOP and VALIDATE**: User Story 1 alone delivers the spec's
   primary value. Audited corpus highlights correctly on GitHub
   the moment the canonical artefact lands.
5. Optional standalone PR-merge candidate: ship MVP if T2
   (`nsl-fmt`) has not yet started; otherwise bundle US2+ in the
   same PR.

### Incremental Delivery

1. MVP merge (Phases 1–3) → audited-corpus colouring live.
2. Add User Story 2 (Phase 4) → VS Code authoring affordances.
3. Add User Story 3 (Phase 5) → preprocessor seam visible.
4. Add User Story 4 (Phase 6) → drift gate active in CI.
5. Polish (Phase 7) → SC verification + Principle VII audit.

### Parallel Team Strategy (single contributor likely; documented for completeness)

After Phase 2 lands, US1 / US2 / US3 can split across three
contributors (or three concurrent agent invocations):

- Stream A: US1 fixtures + grammar patterns.
- Stream B: US2 language-config + extension manifest + symlink.
- Stream C: US3 fixtures + directive/macro patterns.

The three streams converge before US4 (Phase 6) and Polish
(Phase 7).

---

## Notes

- [P] tasks edit different files and have no dependencies on
  in-flight peer work; they are safe to fan out.
- [Story] labels enable traceability from a task back to the spec
  user story and from there to the FRs.
- Each user story is independently testable via its own subset of
  scope-test specs (US1 → all-keywords.spec / all-numbers.spec /
  comments-and-strings.spec / all-operators.spec; US2 → manual VS
  Code installation; US3 → all-directives.spec / macro-
  references.spec; US4 → empirical drift-gate test in T039).
- Verify red-state-before-green per Principle VIII: capture both
  outputs in the PR description so the no-retrofitted-tests clause
  is satisfied even under squash-merge.
- Commit at each checkpoint; the PR may bundle all phases or be
  split across multiple PRs (US1 standalone is mergeable).
- Avoid cross-story dependencies: do not edit US1's fixture in a
  US2 task; if a US2 change reveals a US1 bug, file a separate
  fix.
- Constitution Principle VII coupling: this feature's drift gate
  (US4) makes Principle VII *mechanical* for the keyword set —
  the gate refuses any PR where `KeywordSet.def` and the grammar
  diverge.
- Constitution Principle VIII discipline: every grammar-pattern
  task in Phase 3 / 5 has a paired test task in the same phase
  that lands first and observes FAILING.
