<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->
# Feature Specification: clang-tidy Cleanup — Retire CI Static-Checks Debt

**Feature Branch**: `004-clang-tidy-cleanup`
**Created**: 2026-04-27
**Status**: Draft
**Input**: User description: "clang-tidy warnings — 927 warnings-as-errors."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - The CI static-checks gate goes green on master (Priority: P1)

A contributor opens a PR. The continuous-integration pipeline runs through its
six stages in order. The static-checks stage (clang-tidy + clang-format +
SPDX-header check) finishes with exit code 0. The contributor does not have to
override, ignore, or rationalize a known-failing gate; CI's verdict is the
correct verdict, and a clean static-checks output is the new normal.

**Why this priority**: The static-checks stage has been red since M0. Today it
is functionally a "rubber stamp" that everyone learns to ignore — that erodes
the credibility of the rest of the gate set (build-matrix, unit-tests,
lowering-tests). Restoring trust in this single gate is the highest-value
outcome of the cleanup, and unlocks every later improvement that depends on
the gate actually being a gate.

**Independent Test**: Run `./scripts/ci.sh static-checks` inside the project's
canonical build container against the master HEAD after this work merges and
observe a clean exit. Independent of US2 (constitution edit) and US3
(regression-prevention scaffolding).

**Acceptance Scenarios**:

1. **Given** the post-cleanup master HEAD inside the canonical build
   container, **When** the static-checks CI stage runs, **Then** it exits 0
   with no warnings-treated-as-errors output.
2. **Given** a contributor's clean local build at the post-cleanup master
   HEAD, **When** they run the same static-checks invocation locally,
   **Then** they get the same clean exit (the gate behaves the same in CI
   and on developer workstations).
3. **Given** a contributor opens a no-op PR (e.g., a typo fix in a
   non-source comment) at the post-cleanup HEAD, **When** the CI runs,
   **Then** their PR gets a green static-checks check without manual
   intervention.

---

### User Story 2 - Constitution Principle IX transitional clause is retired (Priority: P2)

The project's Constitution currently carries a transitional clause under
Principle IX (CI/CD) that permits merges while CI is incomplete. Once the
static-checks gate is green and the full six-stage pipeline runs end-to-end
without manual exemptions, the transitional clause is no longer load-bearing
and is removed from `.specify/memory/constitution.md`. Future PRs are bound
by the steady-state Principle IX text, which mandates an all-green CI
verdict before merge.

**Why this priority**: Removing the transitional clause is the explicit
"close-out" signal for the M0/M1 era of the project. It depends on US1 being
done (you cannot retire a transitional rule that is still actively masking a
red gate). It is P2 not P1 because the practical CI behavior is what matters
day-to-day; the constitution edit is the formal acknowledgement that follows.

**Independent Test**: A reader of `.specify/memory/constitution.md` finds
no "transitional clause" wording under Principle IX, and the steady-state
text reads as a single self-consistent rule. The agent registry's mention
of "land P-CI early to retire the Principle IX transitional clause" is no
longer load-bearing for future work.

**Acceptance Scenarios**:

1. **Given** the post-cleanup constitution, **When** a reader searches the
   document for the word "transitional", **Then** no occurrence remains
   under Principle IX.
2. **Given** a hypothetical PR that introduces a CI red gate, **When** the
   merge gate is invoked, **Then** the merge is blocked by the steady-state
   Principle IX rule (no fallback to the transitional clause exists).

---

### User Story 3 - The static-checks gate stays green going forward (Priority: P3)

A contributor working on an unrelated feature accidentally introduces a
single new clang-tidy warning. CI catches it on the contributor's PR before
merge, surfaces a clear actionable message, and the merge is blocked until
the warning is fixed or explicitly justified via a `.clang-tidy` config
change. The cleanup work in US1 does not silently regress over time.

**Why this priority**: Without a regression-prevention mechanism, the
cleanup work erodes as soon as the next 3-5 PRs land. P3 because it is
forward-looking maintenance, not the immediate cleanup itself; the
mechanism only earns its keep over weeks of subsequent PRs.

**Independent Test**: A test PR with a deliberate single clang-tidy
violation is opened against the post-cleanup master HEAD; CI's
static-checks stage fails on that PR with a clear pointer to the
offending file and category.

**Acceptance Scenarios**:

1. **Given** a PR that adds one new clang-tidy warning in a touched
   file, **When** static-checks runs, **Then** the stage fails with the
   warning's category and source location reported in the CI log.
2. **Given** a PR that explicitly adds a `// NOLINT` (or equivalent)
   suppression with a justification comment, **When** static-checks
   runs, **Then** the stage passes (the suppression is the documented
   escape hatch).

---

### Edge Cases

- **Hidden warnings revealed by a clean build.** Some warnings only appear
  when the codebase is fully consistent (e.g., `misc-include-cleaner`
  cascades). Each cleanup pass MUST do a fresh build before declaring
  success; intermediate "looks clean" states are not authoritative.
- **clang-format and clang-tidy disagreement.** clang-format violations
  appear in the same gate output and SHOULD be fixed first (so subsequent
  clang-tidy fixes don't fight the formatter).
- **Pre-existing TODO/FIXME suppression policy.** The audit found zero
  TODO/FIXME markers in the source tree. The cleanup MUST NOT introduce
  them as a workaround for a deferred fix; if a warning category cannot
  be reasonably fixed, it MUST be disabled in `.clang-tidy` with a
  one-line rationale.
- **Per-category overlap.** The largest category (`misc-const-correctness`,
  434 occurrences) often produces fixes that change function signatures,
  which can in turn produce new `modernize-use-nodiscard` (47) or
  `readability-convert-member-functions-to-static` (17) warnings.
  Sequencing matters; a single batch sweep is documented to break the
  build (memory note: "clang-tidy --fix batch is not safe to batch on
  this codebase").

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The CI `static-checks` stage MUST exit 0 on the master HEAD
  after this feature lands. "Exit 0" means: zero warnings-treated-as-errors,
  zero clang-format violations, zero SPDX-header failures.
- **FR-002**: Each warning category present in the M1-vintage baseline
  (top categories by occurrence count: `misc-const-correctness`,
  `readability-identifier-naming`, `readability-uppercase-literal-suffix`,
  `modernize-use-nodiscard`, `misc-include-cleaner`,
  `readability-implicit-bool-conversion`, `modernize-return-braced-init-list`,
  `misc-non-private-member-variables-in-classes`,
  `readability-convert-member-functions-to-static`,
  `cppcoreguidelines-avoid-const-or-ref-data-members`,
  `misc-no-recursion`, `readability-function-cognitive-complexity`,
  `llvm-include-order`) MUST either be fixed at all sites OR be explicitly
  disabled in `.clang-tidy` with a one-line rationale comment.
- **FR-003**: clang-format violations across the source tree MUST be
  cleared in the same feature, separately from the clang-tidy work,
  because both gate the same CI stage.
- **FR-004**: The cleanup work MUST be split across multiple commits
  by category. A single squash that touches every category at once is
  expressly disallowed because batch `--fix` is documented to break the
  build (memory note `feedback_clang_tidy_batch_unsafe.md`). Each
  per-category commit MUST leave the tree buildable in its own right.
- **FR-005**: After CI is green at master HEAD, the Constitution's
  Principle IX transitional clause MUST be removed from
  `.specify/memory/constitution.md` in a separate commit (so the act of
  retiring the clause is bisectable). The steady-state Principle IX
  rule remains.
- **FR-006**: A regression-prevention mechanism MUST be in place so a
  future PR that introduces a single new warning fails CI with a clear
  signal. The mechanism MAY be a `.clang-tidy` config tightening, a
  dedicated baseline file, or both — implementation choice deferred to
  `/speckit-plan`. The chosen mechanism MUST NOT require manual list
  upkeep on every PR.
- **FR-007**: Existing test fixtures (lit + ctest) MUST remain green
  throughout. The cleanup MUST NOT change observable lexer/parser/
  preprocessor behavior. Specifically, the macro-expansion cycle
  detection (FR-007 of feature 003) MUST remain intact, and all
  existing diagnostic strings (FR-037 family, M1-locked) MUST remain
  byte-identical.
- **FR-008**: Code reformatting MUST preserve SPDX headers exactly
  on the first line of every file. The `scripts/check_spdx.py --all`
  pass count MUST not regress.
- **FR-009**: The cleanup MUST NOT introduce TODO/FIXME/HACK/XXX
  comment markers as workarounds. If a warning is to be deferred, it
  is suppressed via `.clang-tidy` config with a rationale, not via a
  source-level marker.
- **FR-010**: Public-header API surfaces (`include/nsl/**/*.h`) MUST
  not gain or lose symbols. Cleanup is style/correctness only;
  signature changes (e.g., adding `[[nodiscard]]`) are allowed but
  function names, parameter shapes, and return types stay the same.
- **FR-011**: Each per-category cleanup commit MUST cite the category
  name, the file count touched, and the warning count cleared in its
  commit message body. This produces a self-documenting cleanup
  history that future contributors can use to understand which
  categories were fixed vs. suppressed and why.

### Key Entities

- **Warning category**: A named clang-tidy diagnostic class (e.g.,
  `misc-const-correctness`, `modernize-use-nodiscard`). Each category
  has an occurrence count, a "fix-vs-suppress" disposition, and a
  per-PR commit that addresses it.
- **`.clang-tidy` config**: The project's tidy configuration file at
  the repo root. Holds the explicit fix/suppress dispositions and
  documents the rationale for any suppressions.
- **Constitution transitional clause**: The portion of
  `.specify/memory/constitution.md` Principle IX that permits merges
  while CI is incomplete. Removed in the close-out commit.
- **Regression-prevention mechanism**: The forward-looking artifact
  (config tightening, baseline file, or pre-commit hook) that keeps
  the gate green over time. Form decided in `/speckit-plan`.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: After the cleanup PR(s) merge, `./scripts/ci.sh all`
  inside the canonical build container exits 0 across all six stages
  (build-matrix, static-checks, unit-tests, lowering-tests, e2e-empty,
  formal-empty). Today the static-checks stage exits non-zero with
  927 warnings-as-errors.
- **SC-002**: The total clang-tidy warnings-as-errors count drops
  from 927 to 0 on the post-cleanup master HEAD, verified by
  `./scripts/ci.sh static-checks 2>&1 | grep "warnings treated as
  errors"` returning empty.
- **SC-003**: All 118 lit fixtures and all 129 ctest cases continue
  to pass on the post-cleanup master HEAD (no regression in the
  layered test suite).
- **SC-004**: The `.specify/memory/constitution.md` file no longer
  contains the word "transitional" under the Principle IX section.
- **SC-005**: The cleanup work lands across N ≥ 4 separate commits
  on the feature branch (one per high-volume category, plus a
  close-out commit for the constitution edit). A bisect across the
  feature branch lands at a buildable, runnable tree at every commit.
- **SC-006**: A deliberately-introduced single new warning on a test
  PR fails CI's static-checks stage within 2 minutes of push and the
  failure log clearly names the offending file and warning category.

## Assumptions

- **Container is canonical.** All measurements (warning counts,
  test pass rates) are taken inside `ghcr.io/koyamanx/nsl-nslc:dev`,
  which is the project's pinned build environment per the existing
  memory note `project_build_environment.md`. Counts on a host
  toolchain may differ.
- **No new public APIs.** The cleanup is style/correctness only.
  Adding `[[nodiscard]]` or migrating to braced-init-list returns
  is fair game and counts as in-scope; renaming a public function
  or splitting a class is out of scope (file as a separate feature
  if needed).
- **clang-format is in scope.** Even though the user's input
  ("927 warnings-as-errors") refers to clang-tidy specifically,
  the same CI stage runs clang-format and SPDX checks. Treating
  them as one closure problem is the natural unit of work.
- **CI behavior is the steady-state target.** "Pass" means green
  on the matrix configuration that PRs face today
  (Release × {host, gcc-13, clang-18}, Debug × host). Cross-toolchain
  warning differences are resolved via `.clang-tidy` config (if a
  category is noisy on one toolchain only, it is suppressed with a
  rationale).
- **Suppression bar is explicit.** A category is suppressed (rather
  than fixed) only when fixing would (a) require an API change
  ruled out by FR-010, (b) require a refactor ruled out by feature
  scope, or (c) the diagnostic is provably a false positive on this
  codebase. Each suppression carries a one-line rationale comment
  in `.clang-tidy`.
- **Regression-prevention is forward-looking only.** This feature
  does not retroactively block historical commits that introduced
  warnings; it only ensures that NEW PRs going forward face a
  green gate.
- **Constitution edit is non-controversial.** Retiring the
  transitional clause is the agreed close-out condition (the
  agent registry text already describes it that way). No
  governance vote is required beyond the implementer's judgment
  that the gate is durably green.
