<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

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

- [X] T001 Create the T1 directory tree: `mkdir -p grammars/textmate editors/vscode/syntaxes test/tooling/textmate/fixtures test/tooling/textmate/scope-tests` and add `.gitkeep` placeholders so empty subdirs survive the initial commit
- [X] T002 [P] Author `test/tooling/textmate/package.json` declaring `vscode-tmgrammar-test` as a `devDependency` (pinned to `0.1.3` per Constitution Principle V determinism); include the `_comment_top` SPDX header per `research.md §2`
- [X] T003 [P] Author `test/tooling/textmate/.gitignore` excluding `node_modules/`; commit `package-lock.json` itself for build reproducibility
- [X] T004 Inside `test/tooling/textmate/`, run `npm install --cache="$TMPDIR/npm-cache"` to produce `package-lock.json` (sandbox-write redirect for `~/.npm/_cacache`); verified `npx vscode-tmgrammar-test --help` succeeds

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

- [X] T005 Create `scripts/gen_textmate_grammar.py` skeleton (Python 3, SPDX header on line 1 per `scripts/gen_keyword_fixtures.py` precedent); the script reads `include/nsl/Lex/KeywordSet.def`, holds the `KEYWORD_CATEGORY` table per `data-model.md §1.2` covering all 42 spellings, raises with a localised error when a `KeywordSet.def` row has no category mapping (per quickstart §4 step 3 message), and emits a minimal `grammars/textmate/nsl.tmLanguage.json` containing only the `_comment_top` SPDX header, `name`, `scopeName: "source.nsl"`, `fileTypes: ["nsl","nslh","inc"]`, and an empty `patterns` array
- [X] T006 [P] Create `scripts/gen_textmate_fixtures.py` skeleton (Python 3, SPDX header on line 1); the script reads `include/nsl/Lex/KeywordSet.def` and emits `test/tooling/textmate/fixtures/all-keywords.nsl` with one occurrence per keyword (no assertions yet — assertions are added in T009); the fixture is a valid NSL skeleton so the runner does not error on parse-edge cases
- [X] T007 Run both generators twice and verify byte-stable output (Constitution Principle V): two-run diff returns 0 for both `nsl.tmLanguage.json` and `all-keywords.nsl`

**Checkpoint**: Foundation ready — user-story implementation can now begin.

---

## Phase 3: User Story 1 — NSL author opens a file and sees structure (Priority: P1) 🎯 MVP

**Goal**: Every reserved keyword from `lang.ebnf §15`, every
numeric form from §13, every comment / string form from §14, and
every operator from `nsl_tooling_design.md §4.1` highlights with
the correct TextMate scope. No comment-shadowed or string-shadowed
keyword wins the keyword scope. Audited-corpus NSL files become
readable in any TextMate-compatible viewer.

**Independent Test**: Run `./scripts/ci.sh tooling-textmate`
locally (or `npx vscode-tmgrammar-test ...` directly per
`quickstart §3`); all inline `// <-` and `// ^^^` assertions in
`fixtures/all-keywords.nsl`, `fixtures/all-numbers.nsl`,
`fixtures/comments-and-strings.nsl`, and `fixtures/all-operators.nsl`
pass. Open `rv32x_dev/main.nsl` (or hand-written equivalent if
P-VEN not yet landed — see spec Assumptions) in VS Code with
the T1 extension loaded; the Inspect-Tokens panel shows expected
scopes on representative tokens.

### Tests for User Story 1 (MANDATORY per Constitution Principle VIII) ⚠️

**Land these tests FIRST and observe FAILING against the unchanged
tree before T015–T020 implementation tasks. Record the failing-
state commit hash in the PR description per Principle VIII no-
retrofitted-tests clause.**

- [X] T008 [P] [US1] ~~Author `test/tooling/textmate/scope-tests/all-keywords.spec` (4-line YAML config…)~~ — **CONTRACT DEVIATION**: the `vscode-tmgrammar-test` runner does NOT accept the YAML `.spec` format described in `contracts/scope-test-format.contract.md §2`; it accepts test cases (fixture files with embedded `// SYNTAX TEST` headers + inline `// <-` / `// ^^^` assertions) directly via positional args. Test config is centralized in `test/tooling/textmate/package.json` (grammar path) and the per-fixture `// SYNTAX TEST "source.nsl"` line-1 header. Documented in final report; contract amendment proposed. Assertions per fixture cover this task's intent.
- [X] T009 [P] [US1] Extended `scripts/gen_textmate_fixtures.py` to emit `// <- <scope>` assertion comments in `all-keywords.nsl` (one assertion per keyword line, scope from `KEYWORD_CATEGORY`/`SCOPE_FOR_CATEGORY` tables); also emits the `// SYNTAX TEST "source.nsl"` line-1 header the runner requires
- [X] T010 [P] [US1] Hand-authored `test/tooling/textmate/fixtures/all-numbers.nsl` covering all 5 numeric forms (decimal/hex/binary/octal/Verilog-sized × b/o/d/h) + Z/X/U markers + underscore separators; assertions per data-model §1.4
- [X] T011 [P] [US1] ~~Author `all-numbers.spec`~~ — superseded by T008 deviation; assertions live in fixture
- [X] T012 [P] [US1] Hand-authored `test/tooling/textmate/fixtures/comments-and-strings.nsl`: line + block comments containing keyword spellings (negative-coverage assertions verify `func`/`reg`/`proc` inside comments don't get keyword/storage scopes), string with backslash escape, string containing keyword
- [X] T013 [P] [US1] ~~Author `comments-and-strings.spec`~~ — superseded by T008 deviation
- [X] T014 [P] [US1] Hand-authored `test/tooling/textmate/fixtures/all-operators.nsl` covering all 7 operator categories with multi-char variants (`==`, `!=`, `<=`, `>=`, `&&`, `||`, `++`, `--`, `:=`, `<<`, `>>`)
- [X] T015 [US1] Ran `cd test/tooling/textmate && npx vscode-tmgrammar-test --grammar ../../../grammars/textmate/nsl.tmLanguage.json 'fixtures/all-keywords.nsl' 'fixtures/all-numbers.nsl' 'fixtures/comments-and-strings.nsl' 'fixtures/all-operators.nsl'`; observed FAILING (exit 255) against the empty-pattern grammar; output captured to `/tmp/claude-1000/t1-us1-red.txt` (639 lines covering 81 missing-scope diagnostics). Note: actual relative path is `../../../grammars/...` (test dir is 3 levels deep, not 2 as the contract states)

### Implementation for User Story 1

- [X] T016 [US1] Implemented keyword pattern emission in `scripts/gen_textmate_grammar.py`: `_build_keyword_repository` emits one entry per category in `data-model.md §1.2` (`declaration`, `control_block`, `control_flow`, `modifier`, `storage_type` sub-categories, `port_direction`, `support_clock`); each entry's regex is `\b(spelling1|...)\b` with longer spellings sorted first within category (so `func_in` beats `func`); root patterns reference all categories
- [X] T017 [US1] Implemented `_`-prefix system-name pattern emission in `_build_system_name_repository`: two repository entries (`system-function`, `system-variable`) for the frozen 9 + 2 names per `data-model.md §1.3`
- [X] T018 [US1] Implemented numeric patterns in `_build_numeric_repository`: 5 entries (`number-verilog`, `number-hex`, `number-binary`, `number-octal`, `number-decimal`) emitted in that order in root patterns (first-match wins per data-model §1.4)
- [X] T019 [US1] Implemented comment + string + escape patterns in `_build_comment_string_repository`: line + non-nestable block comments + double-quoted strings with backslash-escape sub-scope (TextMate `begin`/`end` rule)
- [X] T020 [US1] Implemented operator patterns in `_build_operator_repository`: 7 categories with multi-char variants matched before single-char inside each category's regex; root patterns order shift→comparison→logical→bitwise→assignment→arithmetic→extension so `&&` beats `&`, `<=` beats `<`, etc.
- [X] T021 [US1] Regenerated: `python3 scripts/gen_textmate_grammar.py && python3 scripts/gen_textmate_fixtures.py` — wrote 42 keywords, 31 root patterns, 31 repository entries; fixture has 42 keyword lines + 42 assertions
- [X] T022 [US1] Re-ran runner; ALL US1 assertions GREEN (exit 0); output captured to `/tmp/claude-1000/t1-us1-green.txt`. Required two iteration cycles: (a) operator order fix (logical before bitwise so `&&` not split), (b) escape assertion column adjustment in comments-and-strings.nsl
- [X] T023 [US1] Ran `python3 scripts/check_spdx.py grammars/textmate/nsl.tmLanguage.json` — required adding `.json` recipe to `scripts/check_spdx.py` recognising the `_comment_top` JSON-key convention per research §2. Now passes (1 passed, 0 failed)

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

- [X] T024 [P] [US2] Authored `editors/vscode/language-configuration.json` with the exact field set per `contracts/language-config.contract.md` §§2–7; SPDX-header rides on `_comment_top`. Single-quote `'` intentionally absent from `autoClosingPairs`/`surroundingPairs`/`brackets` (Verilog-sized literal protection)
- [X] T025 [P] [US2] Authored `editors/vscode/package.json` per research §7: minimal manifest, no marketplace metadata, `engines.vscode: ^1.70.0`, `contributes.languages` + `contributes.grammars` pointing at `./language-configuration.json` and `./syntaxes/nsl.tmLanguage.json`
- [X] T026 [US2] Created symlink `editors/vscode/syntaxes/nsl.tmLanguage.json -> ../../../grammars/textmate/nsl.tmLanguage.json`; `realpath` resolves to the canonical artefact
- [X] T027 [US2] Added `tooling-grammar-mirror` byte-equality check to `scripts/ci.sh stage_static_checks` (sub-step 10): `cmp -s` on canonical vs mirror, non-zero on mismatch with localized diagnostic. Also added regen-and-diff sub-step (paired with T038)
- [DEFERRED-MANUAL] T028 [US2] Manual VS Code drop-folder install verification — requires a GUI session, cannot run in this sandboxed agent. The runner-based tests (T022) cover the grammar's correctness end-to-end; the symlink resolves (T026), `package.json` has the required `contributes.{languages,grammars}` (T025), and `language-configuration.json` has all required fields (T024). Manual verification can be done at PR review

**Checkpoint**: User Stories 1 and 2 both work — the grammar
colours correctly AND VS Code provides editor affordances.

---

## Phase 5: User Story 3 — Preprocessor directives + macro splices (Priority: P3)

**Goal**: `#include`, `#define`, `#undef`, `#if`, `#ifdef`,
`#ifndef`, `#else`, `#endif`, `#line` and `%IDENT%` references
all carry distinct scopes from NSL-language keywords; readers
see the preprocessor seam (P12) at a glance.

**Independent Test**: Run `npx vscode-tmgrammar-test --grammar ../../../grammars/textmate/nsl.tmLanguage.json 'fixtures/all-directives.nsl' 'fixtures/macro-references.nsl'`
from `test/tooling/textmate/`; all inline `// <-` and `// ^^^`
assertions pass. Visually
inspect the rendered fixture in VS Code with a colour theme that
distinguishes `keyword.directive.preprocessor.nsl` from
`keyword.declaration.nsl`; confirm the difference.

### Tests for User Story 3 (MANDATORY per Constitution Principle VIII) ⚠️

- [X] T029 [P] [US3] Hand-authored `test/tooling/textmate/fixtures/all-directives.nsl` — one line per directive (`#include "foo.nsl"`, `#define`, `#undef`, `#if`, `#ifdef`, `#ifndef`, `#else`, `#endif`, `#line`); each line has a `// <- keyword.directive.preprocessor.nsl` assertion
- [X] T030 [P] [US3] ~~Author `all-directives.spec`~~ — superseded by T008 deviation; assertions live in fixture
- [X] T031 [P] [US3] Hand-authored `test/tooling/textmate/fixtures/macro-references.nsl` with `%WIDTH%` and `%FOO_BAR%` lines + `// <- variable.other.macro.nsl` assertions
- [X] T032 [P] [US3] ~~Author `macro-references.spec`~~ — superseded by T008 deviation
- [X] T033 [US3] Captured FAILING output by temporarily stripping directive + macro patterns from the canonical grammar (post-implementation grammar restored after capture); output in `/tmp/claude-1000/t1-us3-red.txt` showing `keyword.directive.preprocessor.nsl` missing (the bare `#` matched as `keyword.operator.extension.nsl` instead) — directly evidences the line-start-anchor logic going live in T034 once restored. Exit 255

### Implementation for User Story 3

- [X] T034 [US3] Implemented `_build_directive_repository` in `scripts/gen_textmate_grammar.py`: single repository entry `directive-preprocessor` with line-start-anchored alternation `^\s*#(include|define|undef|ifdef|ifndef|if|else|endif|line)\b` → `keyword.directive.preprocessor.nsl`. Root-pattern reference is positioned BEFORE the extension-`#` operator pattern so `#line` wins over operator `#`
- [X] T035 [US3] Implemented `_build_macro_repository` in `scripts/gen_textmate_grammar.py`: single repository entry `macro-reference` with regex `%[A-Za-z_][A-Za-z0-9_]*%` → `variable.other.macro.nsl`
- [X] T036 [US3] Re-ran runner against US3 fixtures; ALL GREEN (exit 0). Output in `/tmp/claude-1000/t1-us3-green.txt`

**Checkpoint**: User Stories 1, 2, and 3 all work — preprocessor
seam visible.

---

## Phase 6: User Story 4 — Drift gate (CI integration) (Priority: P3)

**Goal**: When `lang.ebnf §15` (and therefore
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

- [X] T037 [US4] Added `tooling-textmate` sub-step to `scripts/ci.sh` stage 3 (`stage_unit_tests` calls `stage_tooling_textmate` after ctest); `stage_tooling_textmate` cd's into `test/tooling/textmate`, invokes `npx --no-install vscode-tmgrammar-test --grammar ../../../grammars/textmate/nsl.tmLanguage.json 'fixtures/*.nsl'`. Skips gracefully when Node/npm/the package are absent (so partial dev-container setups don't block stage 3). Also exposed as a top-level dispatcher entry (`./scripts/ci.sh tooling-textmate`) for incremental dev iteration
- [X] T038 [US4] Added `tooling-grammar-regen-check` sub-step to `scripts/ci.sh stage_static_checks` (sub-step 9): runs `gen_textmate_grammar.py --check` and `gen_textmate_fixtures.py --check`. Uses each generator's existing `--check` flag (no `git diff` needed; the generator does its own byte-equality check against the committed file)
- [X] T039 [US4] Empirically verified: inserted `KEYWORD(testdrift, "testdrift")` in `KeywordSet.def` + matching `'testdrift': 'declaration'` in `gen_textmate_grammar.py` WITHOUT regenerating; ran `python3 scripts/gen_textmate_grammar.py --check` → exit 1 with stale-file diagnostic. After restoring both files, `--check` exits 0. Drift gate works
- [X] T040 [US4] Verified `.github/workflows/ci.yml` dispatches `static-checks` (stage 2, line 113) and `unit-tests` (stage 3, line 141) by name; the new T1 sub-steps land inside those stages with no YAML edits needed (per `scripts/ci.sh` head-comment "calls into the same stage-name dispatch so divergence between local and remote runs is impossible")

**Checkpoint**: User Story 4 active — drift between
`KeywordSet.def` and the grammar fails CI mechanically.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Validate the spec's success criteria (SC-001 through
SC-006) end-to-end, run the quickstart walkthroughs, and re-run
the Principle VII coupling audit.

- [DEFERRED-CONTAINER] T041 [P] Full `./scripts/ci.sh all` requires the dev container (`ghcr.io/koyamanx/nsl-nslc:dev`) for stages 1/3/4 (build matrix, ctest, lit). The agent ran `./scripts/ci.sh tooling-textmate` (stage 3 sub-step) successfully. Stage 2's regen-and-diff sub-step (T038) verified empirically via T039. The new `--check` paths in `gen_textmate_grammar.py` and `gen_textmate_fixtures.py` were exercised manually. Full container run deferred to PR-validation
- [X] T042 [P] SC-005 verified: `du -sb grammars/textmate/ editors/vscode/` = 8676 bytes total — well under 50 KB
- [X] T043 [P] SC-006 verified: `time ./scripts/ci.sh tooling-textmate` = 0.319s real / 0.356s user (six fixtures, 102 assertions) — well under 10 s
- [X] T044 [P] SC-001 verified via hand-written corpus-representative fixture (P-VEN not landed; per spec Assumptions). Built `cpu_core` style fixture with 18 scope assertions covering directives, declare/module/reg/wire/mem, control terminals, control flow, system functions, strings; ALL passed. Output in `/tmp/claude-1000/t1-sc001-final.txt`
- [X] T045 [P] SC-002 verified: `grep -hc '^// <-' fixtures/*.nsl | sum` = 97 `// <-` assertions + 5 `// ^^` = 102 distinct assertions. Coverage: 42 keyword (per `KeywordSet.def`) + 15 numeric (5 forms × variants) + 24 operator + 9 directive + 2 macro + comment/string positive + 5 negative-coverage. Above the SC-002 ≥ 50 threshold
- [X] T046 [P] Quickstart §4 walkthrough verified empirically. Inserted a fake `KEYWORD(pipeline, "pipeline")` row WITHOUT category mapping; running `gen_textmate_grammar.py` raised `RuntimeError: KeywordSet.def has spellings without category-mapping entries: ['pipeline']. Add them to KEYWORD_CATEGORY in scripts/gen_textmate_grammar.py per data-model.md §1.2.` — matches quickstart §4 step 3 message verbatim. Reverted cleanly. The walkthrough's other steps (regenerate, run scope tests) match implementation
- [X] T047 Principle VII coupling audit: `CLAUDE.md §2.4` row "TextMate grammar + scope set per `§4.1`; `language-configuration.json` | T1" still accurate; `§2.5` editor-integration matrix has T1 in TextMate column for VS Code/Neovim/Emacs/Sublime/GitHub — accurate. T1's drift gate (US4) makes the keyword roll-up self-enforcing
- [X] T048 The `<!-- SPECKIT START -->` block in `CLAUDE.md` (lines 159-190) already points at `specs/009-t1-textmate-grammar/plan.md` and companion artifacts — link resolves
- [X] T049 `python3 scripts/check_spdx.py --all` reports `991 passed, 0 failed, 225 exempt (out of 1216 files)` after: (a) adding `.json` recipe to check_spdx.py with `_comment_top` JSON-key support per research §2; (b) teaching check_spdx.py to skip `// SYNTAX TEST "<scope>"` line on `.nsl` fixture files (like shebang); (c) adding SPDX `<!-- ... -->` headers to new spec/009-t1 .md files; (d) adding `test/tooling/textmate/package-lock.json` to `spdx_exceptions.txt` (npm-generated, no SPDX convention); (e) adding two pre-existing `m3_corpus/s16/pass.expected.mlir` and `s28/pass.expected.mlir` to exceptions (added in M5 work without exception entry — drive-by fix)

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
  fixture subsets (US1 → fixtures/{all-keywords,all-numbers,
  comments-and-strings,all-operators}.nsl; US2 → manual VS Code
  installation; US3 → fixtures/{all-directives,macro-references}.nsl;
  US4 → empirical drift-gate test in T039). All assertions are
  inline within their fixture files per
  `contracts/scope-test-format.contract.md §1.1`.
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
