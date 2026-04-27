<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Feature Specification: M0 — Build & CI Scaffolding (with P-CI)

**Feature Branch**: `001-m0-build-ci-scaffolding`
**Created**: 2026-04-26
**Status**: Draft
**Input**: User description: "First milestone of our project"

> **Scope interpretation.** "First milestone" maps to **M0** (build
> scaffolding) in [`README.md`](../../README.md) §Roadmap. This spec
> bundles **M0** with **P-CI** (CI pipeline online) because (a) the
> roadmap states P-CI gates M0 "ideally" and is "mandatory by first
> non-trivial PR," (b) the project's `nsl-build-ci` skill description
> explicitly bundles them ("gates P-CI, M0, and the Principle IX merge
> gate"), and (c) M0's acceptance gate ("CI pipeline green on smoke
> target") requires P-CI to exist. Bundling confirmed in Clarifications
> session 2026-04-26 (Q1).

## Clarifications

### Session 2026-04-26

- Q: Should M0 + P-CI be bundled in feature `001`, or split? → A: **Keep
  bundled** in `001-m0-build-ci-scaffolding` — one feature, one PR
  series, one tasks.md, one merge. Splitting would create artificial
  cross-feature ordering since M0's own acceptance gate requires P-CI.
- Q: What dimensions does the CI build matrix have at M0? → A: **4
  builds** — `Debug × Release × {GCC, Clang}` on Linux x86_64. LLVM-style
  baseline matching `docs/design/nsl_compiler_design.md` §13 (GCC 9+,
  Clang 10+). macOS / additional platforms deferred (Principle IX
  permits adding later; dropping requires constitutional amendment).
- Q: How is the Principle IX "named-reason bypass" of a red CI gate
  authorized at the platform level? → A: **Repo-admin override + PR-
  description reason.** Branch protection enforces required-checks for
  everyone by default (including admins); the only bypass is GitHub's
  repo-admin "merge without waiting for required checks" override, and
  using it MUST record the named reason in the PR description (audit
  trail). LLVM-style governance.
- Q: Against which file list does the SPDX-header check run in CI? → A:
  **Full repo scan** (`git ls-files`) on every CI run (PR + push to
  `main`). Cheap at M0 scale; catches latent violations introduced by
  rebases, force-pushes, or files restored from stash that a PR-only
  scan would miss.
- Q: What format does `nslc --version` print? → A: **`nslc
  <git-describe>`** — single line, LLVM/CIRCT-style. CMake injects the
  string at configure time via `git describe --tags --always --dirty`.
  Pre-tag (M0) the output is `nslc 0.0.0-dev+g<sha>`; once tagged it
  becomes `nslc <tag>` (clean) or `nslc <tag>-<n>-g<sha>-dirty`
  (post-tag commits or dirty tree). Reflects actual built ref so bug
  reports are unambiguous.

## User Scenarios & Testing *(mandatory)*

### User Story 1 — A contributor builds the project from a fresh clone (Priority: P1)

After cloning the repo onto a Linux x86_64 host with the documented
prerequisites installed, a contributor runs a single configure-and-build
command and obtains (a) all nine library archives in their canonical
layer layout, (b) the `nslc` driver binary, and (c) a `lit`-based test
tree that discovers and runs at least one passing smoke test per
existing layer.

**Why this priority**: Until the ninefold library skeleton compiles, no
later milestone (M1 lexer, M2 parser, …, M9 release) has anywhere to
land. M0 is the floor on which every subsequent feature is built. A repo
a new contributor cannot build is non-functional regardless of how good
the documentation is.

**Independent Test**: Clone to a clean machine that satisfies the
documented prerequisites, run the documented bootstrap command, observe
a green build. Then run `nslc --version` and observe exit code 0 with a
non-empty version string. Does not depend on US2 or US3.

**Acceptance Scenarios**:

1. **Given** a fresh clone on a host that satisfies the documented
   prerequisites, **When** the contributor runs the documented
   configure-and-build command, **Then** all nine static libraries
   listed in `docs/design/nsl_compiler_design.md` §3 are produced, the
   `nslc` driver binary is produced, and the smoke `lit` invocation
   discovers and runs at least one passing test per existing layer.
2. **Given** a successful build, **When** the contributor runs `nslc
   --version`, **Then** the binary exits 0 and prints a single
   non-empty version string.
3. **Given** a successful build, **When** a tenth library is added by
   extending the `add_nsl_library` mechanism alone, **Then** no
   top-level `CMakeLists.txt` edit and no per-stage CI configuration
   edit is required for the new library to participate in the build and
   in the unit/layer-tests CI stage.

---

### User Story 2 — Every PR is automatically gated by CI (Priority: P1)

A contributor opens a pull request. Without manual intervention, CI runs
the six stages mandated by Constitution Principle IX in order — Build
matrix → Static checks → Unit/layer tests → Lowering tests → End-to-end
→ Formal — and a green run is the precondition for merge to `main`. The
same pipeline is re-runnable locally via a single documented entry point
so a contributor can reproduce any failure offline.

**Why this priority**: Constitution Principle IX is the merge gate;
without an operational CI pipeline, every guarantee the constitution
makes (Principle V determinism, Principle VI test discipline, Principle
VII spec/design coupling, Principle VIII TDD) is enforced only by hope.
P-CI is "M0 ideally; mandatory by first non-trivial PR" per `README.md`
§Roadmap — it shares P1 with US1.

**Independent Test**: Open a PR with a deliberate one-line breakage in
one stage (e.g., remove an SPDX header from a new file). Observe CI
marking the run red at the static-checks stage with a diagnostic naming
the offending file. Restore the header. Observe CI green and the merge
gate releasing. Run the documented local-reproduction command on the
same ref and observe the same per-stage result.

**Acceptance Scenarios**:

1. **Given** a PR opened against `main`, **When** CI fires, **Then**
   all six stages execute in the order listed in Principle IX, with
   each stage's pass/fail state surfaced to the PR review surface.
2. **Given** a CI failure in any stage, **When** the contributor runs
   the documented local-reproduction command on the same git ref,
   **Then** the same stage fails locally (within the tolerances allowed
   by host environment).
3. **Given** a CI run on commit `X`, **When** CI runs again on the same
   commit `X`, **Then** every stage's emitted artifacts are
   byte-identical between the two runs (Principle V determinism).
4. **Given** a red CI run, **When** any contributor or maintainer
   attempts to merge, **Then** the merge is blocked at the platform
   layer; merging requires either turning the run green or supplying
   the explicit, named-reason bypass authorization permitted by
   Principle IX (and recorded in the PR description).
5. **Given** a CI cache hit and a CI cache miss for the same ref,
   **When** the artifacts of both runs are compared, **Then** they are
   byte-identical (caches accelerate but never mask non-determinism).

---

### User Story 3 — License and provenance are enforced on every new file (Priority: P2)

When any contributor adds a new file in any PR, the static-checks stage
of CI verifies that the file carries the
`SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception` header in the
comment syntax appropriate for that file's extension, and rejects the
change with a clear file-path-and-line diagnostic if the header is
missing or malformed.

**Why this priority**: The project's licensing posture (Apache 2.0 with
LLVM Exception, deliberately matching LLVM/MLIR/CIRCT) only holds if
every file carries the identifier — silent license drift is
catastrophic but easy to introduce. The check belongs at M0 because it
costs nothing to enforce on day 0 and grows expensive to retrofit. It
is P2 (not P1) because it is a hygiene gate, not a foundation: the
project compiles without it, but cannot release safely without it.

**Independent Test**: Open a PR adding a single new file without an
SPDX header. Observe CI failing the static-checks stage with a
diagnostic citing the file path. Add the correct header. Observe CI
passing.

**Acceptance Scenarios**:

1. **Given** a PR adding one or more new files, **When** CI's
   static-checks stage runs, **Then** the SPDX-header presence script
   reports pass/fail per added file with a path-resolved diagnostic,
   and a single failure fails the stage.
2. **Given** a file with an SPDX identifier other than
   `Apache-2.0 WITH LLVM-exception`, **When** the SPDX check runs,
   **Then** the file is reported as failing with a diagnostic naming
   both the expected identifier and the observed identifier.
3. **Given** a file whose extension has no recorded comment syntax for
   SPDX header placement, **When** the SPDX check runs, **Then** the
   script either records the file as a known exception (configured in
   the version-controlled exception list) or fails loudly with a "no
   comment-syntax recipe registered for extension X" diagnostic —
   silent skipping is not permitted.

---

### Edge Cases

- A new library is added — the build, the test layer wiring, and CI
  stage 3 (unit & layer tests) all pick it up purely through
  `add_nsl_library`, with no top-level `CMakeLists.txt` or per-stage
  CI-config edits.
- The `nslc --version` smoke binary segfaults or exits non-zero — the
  build-matrix stage of CI fails immediately, before any later stage is
  wasted.
- A non-determinism leak (e.g., `__DATE__` macro, a hash-map-iteration-
  derived order) is introduced. The double-build determinism check (US2
  acceptance scenario 3) fires and the PR is blocked, naming the
  diverging artifact.
- A C++20-only construct is introduced. Build matrix entries on the
  oldest pinned compiler and clang-tidy report failures, naming the
  file and the construct.
- A PR submitter attempts to merge with `--no-verify` or
  `--no-gpg-sign`. The merge gate refuses unless the PR description
  carries the named-reason authorization permitted by Principle IX.
- A new file is added with a not-quite-right SPDX identifier (e.g.,
  bare `Apache-2.0` without the LLVM-exception clause). The SPDX check
  rejects it with both expected and observed identifier names.
- The end-to-end stage (Principle IX stage 5) and the formal stage
  (stage 6) have no test bodies yet because P-VEN (audited-project
  vendoring) gates M7 and the formal clause gates M8. They are
  expected to report a deliberate, documented "wired but no tests"
  status — not silent skip and not "green on no tests."
- Vendored or third-party files (e.g., upstream LLVM or future
  audited-project source under `test/audited/`) are not subject to the
  SPDX check; the script knows about the exception list explicitly,
  not by heuristic.

## Requirements *(mandatory)*

### Functional Requirements

**Build skeleton (M0):**

- **FR-001**: The build system MUST produce nine static library
  archives in the layer order enforced by `add_nsl_library`'s
  downward-only dependency guard (Principle II): `nsl-basic`,
  `nsl-preprocess`, `nsl-lex`, `nsl-ast`, `nsl-parse`, `nsl-sema`,
  `nsl-dialect`, `nsl-lower`, `nsl-driver`. (The `docs/design/
  nsl_compiler_design.md` §3 table is the canonical source; an
  earlier draft of this FR listed `nsl-parse` before `nsl-ast`,
  which would make Parse's `DEPENDS nsl-ast` an upward dep and
  violate Principle II — the order above is the implementation
  order. See `data-model.md` §entity 1 note.)
- **FR-002**: A reusable build macro named `add_nsl_library` (per the
  M0 row of `README.md` §Roadmap) MUST be the sole mechanism by which
  a library declares itself to the build system. Adding a library after
  M0 MUST NOT require editing the top-level `CMakeLists.txt`.
- **FR-003**: Every layer MUST own its public header(s) at the path
  defined in the §3 table; the header path is part of the layer's
  contract and is not negotiable per file.
- **FR-004**: The build MUST refuse to permit a layer to depend on
  layers above it in the §3 table or to introduce sibling dependencies
  that bypass the table (Constitution Principle II).
- **FR-005**: The build MUST produce an `nslc` driver binary whose
  `tools/nslc/main.cpp` is ≤ ~60 lines (delegating all behavior to
  `nsl-driver`), per Principle II.
- **FR-006**: `nslc --version` MUST exit 0 and print a single line of
  the form **`nslc <git-describe>`** to stdout (per Clarifications
  session 2026-04-26 Q5). The `<git-describe>` portion is the output
  of `git describe --tags --always --dirty` injected by CMake at
  configure time. Pre-tag, the line is `nslc 0.0.0-dev+g<sha>`; once
  tagged it is `nslc <tag>` (clean) or `nslc <tag>-<n>-g<sha>-dirty`
  (post-tag commits or dirty tree). This is the M0 smoke-acceptance
  test.
- **FR-007**: A `lit`-based test driver MUST be wired with at least
  one passing smoke test discovered per existing test layer directory,
  so that subsequent milestones add tests by file placement alone.

**Static-check tooling (M0):**

- **FR-008**: A `.clang-tidy` profile MUST be checked in and applied to
  every C++ source file, configured for LLVM/CIRCT conventions.
- **FR-009**: A `clang-format` configuration MUST be checked in
  (LLVM/CIRCT conventions); the static-checks stage MUST report any
  unformatted file by path.
- **FR-010**: An SPDX-header presence script MUST exist that, given a
  list of files, reports pass/fail per file with a clear diagnostic.
  It MUST recognize the comment syntax of every file extension actually
  present in the repository (per `CONTRIBUTING.md` §2 — `.md` HTML
  comments, `.cpp/.h` `//`, `.cmake` `#`, etc.). Files for which no
  recipe is registered MUST fail loudly. **In CI the script MUST run
  against the full set of tracked files (`git ls-files`) on every
  pull request and on every push to `main`** (per Clarifications
  session 2026-04-26 Q4) — PR-changed-only scoping is rejected as
  vulnerable to latent violations from rebases or force-pushes.
- **FR-011**: The SPDX-header script MUST be invokable both as a
  standalone CLI and as a CI-stage step, returning a non-zero exit code
  on any failure.
- **FR-012**: A version-controlled exception list MUST be the only
  mechanism by which a file is exempted from the SPDX check.

**CI pipeline (P-CI):**

- **FR-013**: CI MUST execute on every pull request opened against
  `main` and on every push to `main`.
- **FR-014**: CI MUST execute the six stages of Constitution Principle
  IX in order: (1) Build matrix — **four builds**, the cross product
  `Debug × Release × {GCC, Clang}` on Linux x86_64 (per Clarifications
  session 2026-04-26 Q2); (2) Static checks (clang-format + clang-tidy
  + SPDX-header check); (3) Unit & layer tests; (4) Lowering tests via
  lit + FileCheck; (5) End-to-end tests on the seven audited projects;
  (6) Formal verification via riscv-formal on `rv32x_dev`.
- **FR-015**: Stages whose payload has not yet landed (5 end-to-end
  pre-M7; 6 formal pre-M8) MUST run with an explicit "wired-but-empty"
  status that surfaces visibly to reviewers — not "green on no tests"
  and not silent skip.
- **FR-016**: A green CI run MUST be the platform-enforced precondition
  for merging a PR to `main`. Branch protection MUST enforce required-
  checks for **all merge attempts including those by repository
  administrators** (i.e., the GitHub "Do not allow bypassing the above
  settings" / "Include administrators" semantics). The only permitted
  bypass mechanism is the GitHub repo-admin "merge without waiting for
  required checks" override; **invoking it MUST require the named
  reason to be recorded in the PR description** (per Clarifications
  session 2026-04-26 Q3). No other bypass surface — `--no-verify`,
  `--no-gpg-sign`, force-push to `main`, or maintainer comment-only
  override — is acceptable.
- **FR-017**: CI MUST be re-runnable locally via a single documented
  entry point (e.g., `./scripts/ci.sh` per Principle IX). The local
  run MUST execute the same stages with the same gating semantics.
- **FR-018**: Two CI runs on the same git ref MUST produce
  byte-identical artifacts at every stage's output. A cache hit and a
  cache miss for the same ref MUST also produce byte-identical
  artifacts (Principle V).
- **FR-019**: Each CI stage MUST surface its pass/fail state and a
  path-resolved diagnostic (where applicable) to the PR review surface,
  such that a reviewer can identify the failing stage and root cause
  without reading raw log scrollback.
- **FR-020**: CI MUST refuse to mask a fresh-build failure with a
  cached success. Cache invalidation MUST be triggered by any change
  to the inputs that affect build output (per the Principle V
  definition of "input": source bytes + CLI flag list, nothing else).

**Process / governance enforcement (P-CI ↔ Constitution):**

- **FR-021**: A single, human-readable file (the local-reproduction
  entry point referenced in `README.md` / `CONTRIBUTING.md`) MUST be
  authoritative; divergence between this file and the remote CI
  configuration is a CI bug, not a feature.
- **FR-022**: The CI configuration MUST refuse environment-derived
  inputs (env vars, build path, hostname, mtime, locale, CWD) as
  inputs to determinism — anything that varies with those is a
  determinism leak per Principle V.

### Key Entities

- **Library skeleton** (×9): one per layer in
  `docs/design/nsl_compiler_design.md` §3. Attributes: layer name (e.g.,
  `nsl-basic`); public-header path under `include/nsl/<Layer>/`;
  declared upward dependencies; backing source directory under
  `lib/<Layer>/`. The single allowed instantiation mechanism is
  `add_nsl_library`.
- **Test layer**: one per library, plus the End-to-End directory tree.
  Attributes: associated library; conventional driver (gtest / Catch2
  for unit tests; lit + FileCheck for lowering and end-to-end per
  Principle VI). At M0, each is wired with a smoke fixture only.
- **CI stage**: one of six, ordered. Attributes: stage number (1–6);
  display name; gating semantics (block / non-block); command line;
  status reporter (pass / fail / wired-but-empty).
- **SPDX-header convention**: per file extension. Attributes:
  extension; comment opener (e.g., `<!--`, `//`, `(*`, `#`); the
  literal identifier string `Apache-2.0 WITH LLVM-exception`; an
  explicit, version-controlled exception list of paths.
- **Local-reproduction entry point**: a single executable script.
  Attributes: invocation path; documented in `README.md` and
  `CONTRIBUTING.md`; authoritative for what CI runs.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A contributor with the documented prerequisites
  installed can clone the repo and produce a green build (all nine
  library archives, `nslc` binary, smoke tests) with a single
  documented configure-and-build invocation, on a reference Linux
  x86_64 host.
- **SC-002**: `nslc --version` returns exit code 0 and prints a single
  line matching the regex `^nslc [0-9A-Za-z._+-]+$` (i.e., the
  `nslc <git-describe>` form mandated by FR-006) in under 100 ms on
  the reference host.
- **SC-003**: 100% of files newly added in any merged PR carry the
  `Apache-2.0 WITH LLVM-exception` SPDX identifier (measured by
  re-running the SPDX check across `git log --diff-filter=A` since
  M0 / P-CI landed).
- **SC-004**: 100% of pull requests opened against `main`
  automatically trigger the six-stage CI pipeline; 0% of PRs are
  merged into `main` while CI is red without a recorded named-reason
  bypass per Principle IX.
- **SC-005**: Two consecutive CI runs on the same git ref produce
  byte-identical artifacts at every stage's output (Principle V
  determinism).
- **SC-006**: A contributor running the documented local-reproduction
  command on the same git ref where remote CI reported pass/fail
  observes the same pass/fail at the same stage (Principle IX local
  reproducibility).
- **SC-007**: A reviewer opening a red CI run can identify the failing
  stage and the specific failing input (file path, test name, or
  diagnostic) within 10 seconds of opening the run page, without
  reading raw log scrollback.
- **SC-008**: Adding one additional library (hypothetically, in a
  later PR) requires editing exactly one new directory plus one
  `add_nsl_library` invocation; no top-level `CMakeLists.txt` edits
  and no per-stage CI configuration edits are needed (Principle II
  layer extensibility).

## Assumptions

- **Scope is M0 + P-CI bundled** (confirmed in Clarifications session
  2026-04-26, Q1). One feature, one PR series, one `tasks.md`, one
  merge. Justified by (a) `README.md` §Roadmap stating P-CI is "M0
  ideally; mandatory by first non-trivial PR," (b) the project's
  `nsl-build-ci` skill description bundling them ("gates P-CI, M0,
  and the Principle IX merge gate"), and (c) M0's own acceptance gate
  ("CI pipeline green on smoke target") requires P-CI to exist.
- Reference host is Linux x86_64 with the dependency stack documented
  in `docs/design/nsl_compiler_design.md` §13 available (LLVM + MLIR
  at the CIRCT-pinned commit; CIRCT main; GCC 9+ or Clang 10+; CMake
  ≥ 3.22; Catch2 or GoogleTest; lit + FileCheck from LLVM).
- The CI host is GitHub Actions (the project's existing workflow,
  GitHub-flavored `gh` CLI usage, and `.specify/integration.json`
  already presume GitHub).
- C++17 (per Constitution Build/Code/Licensing) and LLVM/CIRCT coding
  conventions are constitutional; this spec does not re-justify them.
- Audited-project vendoring (`P-VEN`) and golden VCDs (`P-VCD`) are
  out of scope here — they gate M7. CI stages 5 (End-to-end) and 6
  (Formal) are wired in M0 / P-CI but their bodies are empty until
  M7 / M8 land; they report a "wired but no tests" status, not silent
  skip.
- The driver (`nslc`) ships in M0 only as a smoke `--version` binary;
  real `-emit=` flags arrive incrementally from M1 onward.
- `clang-tidy` and `clang-format` configurations are project-wide
  artifacts; per-library overrides are not introduced at M0.
- Workflow project-enablement deliverables `P-LIN` (Linear) and `P-TS`
  (per `CONTRIBUTING.md` §3.8) are out of scope; this spec covers
  compiler-track enablement only (`P-CI`).
- The CI build matrix at M0 is **four builds**: the cross product
  `Debug × Release × {GCC, Clang}` on Linux x86_64 (Clarifications
  session 2026-04-26 Q2). Compiler-version pinning (e.g., GCC floor
  vs. ceiling), additional architectures, and additional operating
  systems are out of scope for M0; per Principle IX they MAY be added
  later but none may be dropped without a constitutional amendment.
