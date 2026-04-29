<!--
SYNC IMPACT REPORT
==================
Version change: 1.5.0 → 1.6.0
Bump rationale: MINOR — coordinated bundle of two header/test-shape
amendments surfaced by /speckit-analyze on feature 006-m3-sema:
(a) Principle II §3 single-public-header exception extended to
name `nsl-sema` alongside `nsl-ast`, reflecting M3's three-axis
decomposition (Sema engine + symbol table + type system) for
consumer-compile-cost reasons; (b) Principle VIII `Sn`/`Nn`/`Pn`
clause gains a "constructive carve-out" for constraints that
produce no diagnostic — paired pass + introspection assertion is
codified as the equivalent test shape (so Q1 Option B from
Clarifications session 2026-04-28 is honored under the literal
constitution). No principles removed; both edits are rule
*narrowings* (or codifications of existing intent), so MINOR
applies per the v1.4.0 versioning policy.

Previous version change: 1.4.0 → 1.5.0
Bump rationale: MINOR — Principle IX transitional clause retired.
After feature 004-clang-tidy-cleanup drove the static-checks gate
from 927 warnings-as-errors to 0, `./scripts/ci.sh all` exits 0 on
master HEAD across all six stages. The transitional fallback ("CI
not yet online; PR submitter runs the local equivalent") is no
longer load-bearing; the steady-state Principle IX rule (green CI
mandates merge gate) governs all PRs going forward. No principles
removed; transitional paragraph + 2 cross-references deleted.

Previous version change: 1.3.0 → 1.4.0
Bump rationale: MINOR — coordinated bundle of (a) one rule narrowing
(External Integrations § Linear: scope narrows to feature-track work
items only; bug reports move to GitHub Issues as canonical), (b) the
P-LIN team-prefix placeholder resolution (`<TEAM>-<N>` → `NSLC-<N>`),
and (c) seven clarifications and three process additions surfaced by
a documentation/process audit on 2026-04-26. No principles removed.

Edge call: MINOR vs MAJOR for the Linear scope narrowing. The change
redefines a stated normative rule (where bug reports are tracked).
Argued as MINOR because (a) it narrows rather than removes — bugs
gain a canonical home in GitHub Issues; nothing about feature
tracking in Linear changes; and (b) no work items exist yet under
the prior rule, so the change is forward-only with zero retroactive
non-compliance. Following this case, the versioning policy is
clarified explicitly: rule narrowing without removal is MINOR; rule
replacement or removal is MAJOR.

Prior history:
  - 1.0.0 (2026-04-25): Initial ratification.
  - 1.1.0 (2026-04-25): Added Principles VIII (TDD, NON-NEGOTIABLE)
    and IX (CI/CD, transitional).
  - 1.1.1 (2026-04-25): Audit-driven clarification + cross-doc fixes.
  - 1.2.0 (2026-04-25): Added External Integrations section
    (CodeRabbit, Linear).
  - 1.3.0 (2026-04-25): C++17 carve-out collapsed; Principle VI
    Delivery + Reference-VCDs sub-bullets added.
  - 1.4.0 (2026-04-26): Linear scope narrowed to feature-track work
    items (bug reports moved to GitHub Issues canonical); P-LIN
    team-prefix placeholder resolved; documentation/process audit
    fixes bundled.
  - 1.5.0 (2026-04-28): Principle IX transitional clause retired;
    steady-state CI green merge gate governs all PRs.

Modified principles (1.5.0 → 1.6.0):
  - II. Layered Library Architecture — single-public-header
    exception clause extended: now names BOTH `nsl-ast` (per-node-
    kind headers, established at 1.4.0) AND `nsl-sema` (three
    umbrella headers — `Sema.h` / `SymbolTable.h` / `TypeSystem.h`
    — for the engine + symbol-table + type-system three-axis
    decomposition, reducing consumer compile cost and matching
    clang's `Sema/` directory pattern). Both rationales are
    documented inline in the bullet.
  - VIII. Test-First Development — `Sn`/`Nn`/`Pn` clause gains a
    "constructive carve-out": when a constraint produces no
    diagnostic (it describes Sema's resolution behavior — layout,
    classification, or constructive population — rather than a
    shape Sema rejects), the fail-case test asserts a typed
    introspection observable on a Sema-public API with the
    expected-value flipped. The introspection observable's
    signature is frozen by the fail-case test, parallel to how the
    diagnostic message string is frozen for the typical case. This
    codifies Q1 Option B from feature 006-m3-sema's Clarifications
    session 2026-04-28.

Modified principles (prior 1.4.0 → 1.5.0):
  Principle IX transitional clause retired (see 1.5.0 SIR above).

Modified principles (prior 1.3.0 → 1.4.0):
  - II. Layered Library Architecture — `nsl-opt` reclassified as a
    developer/test tool (not a user-facing tooling-binary
    deliverable); `nsl-ast`'s "per-node-kind headers under one
    directory" exception promoted from a parenthetical in
    Build/Code/Licensing to its own Principle II clause for
    visibility.
  - V. Inspectable, Deterministic Pipeline — "input" defined
    positively (source bytes + CLI flags only; build environment
    is NOT input).
  - VI. Layered Test Discipline — audited-projects list explicitly
    declared closed (expansion requires a constitutional amendment);
    per-layer accepted test drivers enumerated.
  - VIII. Test-First Development — refactor-exemption "no Verilog
    diff on the audited corpus" condition gains a pre-M7 carve-out
    (vacuous before the M7 `-emit=verilog` end-to-end pipeline lands).

Modified sections:
  - External Integrations § Linear — bullet 1 narrowed: "All work
    items" becomes "All feature-track work items"; the Linear team
    and project name (`nslc`) and issue/branch ID prefix
    (`NSLC-<N>`) are now stated; placeholder resolved.
  - External Integrations — new "GitHub Issues" sub-section added
    after Linear, naming GitHub Issues as canonical for bug reports
    and excluding bugs from the Linear-GitHub mirror.
  - External Integrations § CodeRabbit — bullet 1 expanded: the
    contributor classifies findings as blocking vs advisory on
    first review; disputes route to `/nsl-constitution-review`.
  - External Integrations § Merge gate — clause 3 made explicit:
    PRs implementing a Linear feature-track issue MUST reference
    it; PRs fixing a GitHub Issue MUST reference it; doc-only PRs
    that originate from neither are exempt from clause 3 only when
    they qualify for the "Docs / comments" row of the application-
    criteria table in CONTRIBUTING.md §3.3.
  - Governance § Versioning policy — narrowing-vs-removal rule
    added; rule narrowing without removal is MINOR.
  - Governance § Runtime guidance — skill ↔ agent parallel-update
    rule added.
  - Governance — new "Disputed findings" bullet added documenting
    the escalation path.

Added principles: None.
Added sections: External Integrations § GitHub Issues (new sub-
section).
Removed sections: None.

Templates requiring updates: None directly (verified 2026-04-26).
  - .specify/templates/plan-template.md       ✅ aligned
  - .specify/templates/spec-template.md       ✅ aligned
  - .specify/templates/tasks-template.md      ✅ aligned (refs Principles VI/VIII only; both unchanged)
  - .specify/templates/checklist-template.md  ✅ aligned
  - .specify/templates/constitution-template.md ✅ aligned

Source-document propagations bundled into this amendment:
  - CONTRIBUTING.md: 5 occurrences of `<TEAM>-<N>` → `NSLC-<N>`;
    §3.1 Linear status row narrowed (no longer "all issues/tasks";
    P-LIN prefix-resolved status); §3.2 Phase 1 routing clarified
    (feature tasks → Linear; bugs → GitHub Issues); §3.4 CLAUDE.md
    description simplified; §3.8 P-LIN row updated to reflect
    partial-resolution; new §3.10 "Skill ↔ agent parallel-update
    protocol" added.

Follow-up TODOs:
  - P-LIN team-prefix placeholder is resolved (`NSLC-<N>`) but the
    formal P-LIN landed-state still requires the PR-trailer
    convention to be exercised in a first non-trivial PR.
  - The v1.2.0 SIR follow-up about `/speckit-taskstoissues` is
    refined: the skill routes feature-track tasks via GitHub →
    Linear-mirror; bug-report issues filed directly in GitHub
    Issues bypass the Linear mirror entirely (mirror config MUST
    EXCLUDE the bug-report label).
-->

# nslc Constitution

## Core Principles

### I. Spec Is Authoritative (NON-NEGOTIABLE)

`docs/spec/*.ebnf` is the single source of truth for what NSL **is**.
`docs/design/*.md` describes how the implementation **handles it**, and is
subordinate. If `docs/design/` and `docs/spec/` disagree, treat it as a bug
in `docs/design/` and report it; the spec wins by policy.

- Semantic constraints `Sn`, parser notes `Nn`, and preprocessor notes `Pn`
  use monotonically increasing numbering. Retired numbers MUST NOT be reused;
  append the next free integer instead.
- Upstream NSL reference PDFs are external sources cited in the headers of
  `docs/spec/*.ebnf`. They MUST NOT be committed to this repository. When
  `docs/spec/` and a PDF disagree, the deliberate choice in `docs/` — with
  the audited open-source NSL projects as ground truth — prevails, and the
  rationale MUST be recorded in the affected `.ebnf` header comment.

**Rationale.** The project's value comes from a single, audited, reviewable
language definition. Any drift between spec and implementation undermines the
guarantees the compiler offers; centralizing authority in `docs/spec/`
eliminates that drift by construction.

### II. Layered Library Architecture

The compiler is split into nine static C++ libraries with single public headers:
`nsl-basic`, `nsl-preprocess`, `nsl-lex`, `nsl-parse`, `nsl-ast`, `nsl-sema`,
`nsl-dialect`, `nsl-lower`, `nsl-driver`. The dependency direction flows
strictly downward per the layer table in
`docs/design/nsl_compiler_design.md` §3.

- Layers MUST NOT depend on layers above them, nor introduce sibling
  dependencies that bypass the table.
- The compiler driver (`tools/nslc/main.cpp`) MUST stay a thin entry point
  (~60 lines), delegating to `nsl-driver`. The binary name is `nslc`,
  matching the project name.
- **User-facing tooling binaries** are `nsl-lsp`, `nsl-fmt`, and `nsl-lint`.
  Each MUST reuse `libNSLFrontend.a` (preprocessor, lexer, parser, AST,
  sema, symbol table, diagnostics); duplicating any of these layers in a
  tool is prohibited. These three binaries appear in the T-track of the
  milestone plan (see `README.md` §Roadmap).
- **Developer/test tools** — currently `nsl-opt` (the project's `mlir-opt`
  for the `nsl` dialect) — are NOT user-facing deliverables and are NOT
  part of the T-track. They MUST still reuse `libNSLFrontend.a` where
  they consume its primitives.
- **`nsl-ast` and `nsl-sema` header layout exceptions.** Each library
  exposes a single public header except (a) `nsl-ast`, which exposes
  per-node-kind headers under one `include/nsl/AST/` directory, and
  (b) `nsl-sema`, which exposes three umbrella headers under
  `include/nsl/Sema/` — `Sema.h` (engine + `SemaResult`),
  `SymbolTable.h` (Symbol hierarchy + scope stack), and `TypeSystem.h`
  (Type hierarchy + interner). The two exceptions reflect different
  rationales: `nsl-ast`'s per-node-kind separation supports a class
  hierarchy that benefits from per-kind headers; `nsl-sema`'s
  three-axis decomposition reduces consumer compile cost (a tooling
  library — `nsl-fmt` / `nsl-lsp` / `nsl-lint` — that needs only the
  symbol/type sub-surface does not pull in the engine's resolution-pass
  visitors). Private headers stay in `lib/` for every library.

**Rationale.** "One front-end, several frontdoors" guarantees zero semantic
drift between the compiler and developer tools, makes incremental reparse in
the LSP cheap, and keeps each tool's tool-specific code small.

### III. Stock CIRCT Below the `nsl` Dialect

Everything below the project's own `nsl` MLIR dialect MUST be stock CIRCT —
the `hw`, `comb`, `seq`, `fsm`, and `sv` dialects and their upstream passes.

- Hand-rolled netlist, RTL, register-inference, or state-machine-lowering
  passes are NOT permitted; if a needed primitive is missing, the work
  belongs upstream in CIRCT, not here.
- Adding a new backend (e.g., FIRRTL, Chisel, Bluespec) means adding a new
  `nsl` → <target> lowering pass; bypassing the `nsl` dialect to emit a
  target directly is prohibited.
- Verilog emission goes through CIRCT's `ExportVerilog`. The pipeline
  terminates at a `hw + comb + seq + sv` form consumable by that exporter.

**Rationale.** The project's stated long-term ambition is to be usable inside
an LLVM-style ecosystem. Forking CIRCT functionality breaks that path and
guarantees future maintenance burden. The `nsl` dialect is the architectural
seam where project-specific work ends and shared infrastructure begins.

### IV. Source-Locating Diagnostics

Every AST node, every symbol-table entry, and every operation in the `nsl`
MLIR dialect MUST carry an NSL `SourceRange`. Every diagnostic emitted by any
stage of the pipeline (preprocessor, lexer, parser, sema, MLIR pass, CIRCT
pass) MUST render to a precise `file:line:col` in the user's original NSL
source.

- The `#line` directive — the only preprocessor directive that survives the
  preprocessor/parser seam — MUST be preserved through every later stage.
  Loss of `#line` fidelity is a hard invariant violation, not a polish item.
- The `DiagnosticEngine` MUST support both human and structured (JSON)
  output to remain usable from the LSP without rework.

**Rationale.** Hardware engineers using `nslc` see errors that originate
several lowerings away from their source. If those errors do not point back
to the right line, the compiler is unusable, regardless of how correct its
output is.

### V. Inspectable, Deterministic Pipeline

Each pipeline stage MUST be independently driveable from the command line
via a `-emit=` flag (`-emit=tokens`, `-emit=ast`, `-emit=mlir`, `-emit=hw`,
`-emit=verilog`). New stages MUST add their own `-emit=` flag.

- Stage outputs MUST be byte-stable for identical inputs and flags.
  **"Input" is defined positively as the literal byte content of the
  source file(s) referenced by the build command plus the CLI flag list
  — nothing else.** The build environment (env vars, hostname, build
  path, source-file mtime, CWD, locale) is NOT input; artifacts that
  vary with any of those represent non-determinism leakage. Forbidden
  non-determinism examples include pointer-address-derived ordering,
  hash-map iteration order, and embedded timestamps.
- Each stage's output MUST be loadable as the next stage's input where the
  abstraction permits (e.g., `nsl-opt` round-tripping `.mlir`).

**Rationale.** Determinism enables FileCheck-based testing, reproducible
builds, and meaningful diffs across compiler revisions. CLI inspectability
makes every layer of the pipeline a first-class debugging surface, not an
opaque internal step.

### VI. Layered Test Discipline (NON-NEGOTIABLE)

Each library MUST own a corresponding test layer, and the project's
canonical regression suite is the seven audited open-source NSL projects.

- **Lexer tests** assert on token streams (keyword recognition, `_`-prefix
  system names, `%IDENT%` expansion, Z/X/U number-literal corner cases).
- **Parser tests** are AST-snapshot tests covering every grammar production.
- **Sema tests**: one test MUST exist per semantic constraint `S1–S29` and
  one per parser note `N1–N14`. Adding a new `Sn`/`Nn` requires adding its
  corresponding test.
- **Dialect tests** use `nsl-opt` for round-trip verification of `.mlir`.
- **Lowering tests** use lit + FileCheck (`nslc -emit=hw foo.nsl |
  FileCheck foo.nsl`).
- **End-to-end tests**: `cpu16`, `mips32_single_cycle`, `ahb_lite_nsl`,
  `mmcspi`, `SDRAM_Controler`, `rv32x_dev`, `turboV` MUST each compile to
  Verilog and simulate equivalently to a reference VCD under Icarus
  Verilog and/or Verilator (CIRCT's primary simulator targets). A change
  that breaks any of these does not land. **This list is closed**:
  expanding it (adding an audited project) or shrinking it requires a
  constitutional amendment.
  - **Delivery.** Each audited project is vendored under
    `test/audited/<project>/` with a `PROVENANCE.md` recording the
    upstream URL, commit SHA, and license. Git submodules are NOT
    used; configure-time network fetches are NOT used. Updating an
    audited project is an explicit re-vendoring commit so the
    deterministic-pipeline guarantee of Principle V is preserved.
  - **Reference VCDs.** Each project carries
    `test/audited/<project>/golden/<scenario>.vcd` files generated
    from an external known-good source — the upstream NSL toolchain
    output for non-CPU projects; manually-authored or formal-validated
    reference for CPU projects. Each project's `golden/REGEN.md`
    documents the regeneration command so a golden can be
    reconstituted if the testbench is amended. Self-referential VCDs
    (regenerated from `nslc`'s own emitted Verilog at test time) are
    NOT acceptable as reference: they catch only `nslc`-vs-previous-
    `nslc` regressions, not NSL-correctness. Once the formal clause
    below is satisfied for a CPU project, formal verification
    SUPPLEMENTS the golden VCD for that project; it does not
    replace it.
- **Formal**: CPU projects (`rv32x_dev`) MUST be wired to riscv-formal for
  ISA compliance verification once the lowering supports it.

**Per-layer accepted test drivers.** Lexer, parser, sema, and dialect
tests MAY use any conventional driver (gtest, LLVM unit-test framework,
AST-snapshot harness, `nsl-opt`); lowering and end-to-end tests MUST
use lit + FileCheck (LLVM/CIRCT convention) — no substitutes.

**Rationale.** The audited projects collectively distilled the published
language reference; they are what defined "correct" during the spec
authoring. Any change that breaks them either reveals a real regression or a
spec ambiguity — both of which require explicit handling rather than silent
suppression.

### VII. Spec ↔ Design Coupling

A change to `docs/spec/*.ebnf` MUST update the corresponding sections of
`docs/design/*.md` in the same change, per the cross-reference table in
`docs/CLAUDE.md` §8.

- Adding or modifying an `Sn`, `Nn`, or `Pn` MUST update the quick-map in
  `docs/CLAUDE.md` §5 in the same change.
- Line ranges in `docs/CLAUDE.md` §§4–7 MUST be kept current when section
  boundaries in spec or design files shift.
- Implementation work that contradicts `docs/design/` MUST update
  `docs/design/` (and, if a language-level deviation is required,
  `docs/spec/`) in the same patch — never as a "follow-up".
- Reading whole files in `docs/` is the wrong default. Contributors (human
  or AI) MUST consult `docs/CLAUDE.md` §3 (task → section map) and use
  line-range views.

**Rationale.** The spec/design coupling is the single most error-prone surface
in the project — a stale design doc misleads every contributor who reads it.
Coupling the updates by policy, rather than by hope, is what keeps the two
layers honest.

### VIII. Test-First Development (TDD) — NON-NEGOTIABLE

For every new behavior, the corresponding test (at the appropriate layer
named in Principle VI) MUST be written first, MUST be observed failing
against the unchanged tree, and only then is the implementation accepted.
Red → Green → Refactor is the required cycle. Tests that pass on first run
against an unchanged tree are rejected as vacuous.

- **New behavior.** A contract or integration test (lexer, parser, sema,
  dialect, lowering, or end-to-end as appropriate) is written first; the
  PR or commit history MUST show it failing before the implementation
  commit lands.
- **Bug fix.** A regression test reproducing the bug MUST land in the same
  change. It MUST fail without the fix and pass with it; the failure mode
  MUST match the user-reported symptom (not a proxy for it).
- **New `Sn` / `Nn` / `Pn` constraint.** Both pass-case and fail-case Sema
  tests MUST exist. The fail-case test MUST cite the specific diagnostic
  message string that the constraint produces, so renaming or weakening
  the diagnostic later is caught automatically. **Constructive carve-out**:
  if the constraint produces no diagnostic — i.e., it describes Sema's
  resolution behavior (layout, classification, or constructive
  population) rather than a shape Sema rejects — the fail-case test
  MUST instead assert against a typed introspection observable on a
  Sema-public API, structured as the same input as the pass-case with
  the expected-introspection-value flipped (so the test fails iff Sema
  diverges from the spec's constructive rule). The introspection
  observable's signature is frozen by the fail-case test, parallel to
  the way the diagnostic message string is frozen for the typical case.
  Spec/design docs (Sn rows in `docs/spec/nsl_lang.ebnf`,
  `docs/design/nsl_compiler_design.md`, and the project-root
  `CLAUDE.md` §1 roll-up) MUST identify which `Sn` ship under the
  carve-out so the test-shape contract is mechanically auditable.
- **Refactor.** A behavior-preserving change is exempt from the
  new-test requirement only if (a) the existing test suite is fully green
  before and after, (b) no new diagnostics are produced, (c) no new IR
  ops or attributes appear in `-emit=mlir`, and (d) no Verilog diff
  results from `-emit=verilog` on the audited-project corpus. A passing
  test suite is the sole license to refactor without adding tests.
  **Pre-M7 carve-out:** condition (d) is vacuous before the M7
  `-emit=verilog` end-to-end pipeline lands; the other three conditions
  still apply.
- **No retrofitted tests.** A test added in the same commit as the
  feature it validates MUST still demonstrate failure: rebase or split
  the commit so the failing-state is preserved in history. Squash-merge
  is acceptable provided the PR description records the failing-state
  commit hash before the merge.

**Rationale.** A spec-driven hardware compiler has no room for silent
regression. The audited-project regression suite (Principle VI) catches
user-visible breakage but is too coarse for unit-level bugs; TDD at every
test layer is what feeds those layers and keeps them honest. Writing the
test first also forces the contributor to articulate the spec citation —
if the test cannot be written, the spec is unclear and `/speckit-clarify`
is the correct response, not silent improvisation.

### IX. Continuous Integration & Delivery

The project MUST maintain a CI pipeline that automatically verifies every
pull request and every push to `main` against this Constitution's
invariants. A green CI run is the precondition for merge to `main`.

- **Pipeline stages.** CI MUST execute, in order, on every PR and
  every push to `main`:
  1. **Build matrix** — `Debug` and `Release` builds on Linux x86_64.
     Additional platforms MAY be added; none MAY be dropped without a
     constitutional amendment.
  2. **Static checks** — clang-format, clang-tidy (against the project's
     `.clang-tidy` profile), and an SPDX-header presence check on every
     new or modified file (per Build, Code, and Licensing Standards).
  3. **Unit & layer tests** — lexer, parser, sema (covering every
     `Sn`/`Nn`), and `nsl-opt` round-trip of the `nsl` dialect.
  4. **Lowering tests** — lit + FileCheck on `-emit=mlir` and `-emit=hw`
     outputs.
  5. **End-to-end tests** — the seven audited NSL projects from
     Principle VI compiled to Verilog and simulated against reference
     VCDs (Icarus Verilog and/or Verilator).
  6. **Formal** — riscv-formal on `rv32x_dev` once the lowering supports
     it (gated identically to the formal clause of Principle VI).
- **Reproducibility.** CI MUST be re-runnable locally via a single
  documented entry point (e.g., `./scripts/ci.sh`). A failure that only
  manifests in CI is a CI bug, not a feature. Build caches MAY accelerate
  CI but MUST NOT mask non-determinism — a cache miss MUST produce
  byte-identical output to a cache hit (this is enforced by Principle V).
- **Merge gate.** Merging to `main` despite a red CI run is prohibited.
  If a CI failure is unrelated to the change under review, the unrelated
  fix lands first.
- **No bypass.** `--no-verify`, `--no-gpg-sign`, and equivalent commit-
  or push-time bypasses MUST NOT be used to dodge pre-commit, pre-push,
  or CI checks. If a hook fails, the underlying issue is fixed; bypasses
  are permitted only with explicit user authorization for a specific,
  named reason recorded in the PR description.
- **Release artifacts.** Tagged releases MUST publish reproducible
  binaries and source tarballs from CI; no human-built artifacts are
  attached to releases.

**Rationale.** The audited-project regression (Principle VI), the
determinism guarantee (Principle V), and the spec/design coupling
(Principle VII) are too large to enforce by hand at every change. CI is
what turns "MUST" into actual behavior. Making the pipeline reproducible
locally keeps "works on my machine" failures debuggable; the no-bypass
rule prevents the gate from quietly eroding under deadline pressure.

## Build, Code, and Licensing Standards

- **Language:** C++17 across the project — compiler, tooling, and
  every layer of `libNSLFrontend.a`. Use `std::variant` /
  `std::optional` in preference to raw unions or sentinel values, and
  RAII for all resource ownership. C++20 features (concepts, ranges,
  `std::format`, etc.) are reserved for a possible future migration
  governed by an explicit constitutional amendment; they MUST NOT
  appear in code or public headers today. Re-loosening this rule to
  permit C++20 once substantive C++17 code lands would warrant a
  MAJOR version bump.
- **Coding style:** LLVM/CIRCT conventions for naming, brace style, header
  guards, and file headers. Each library exposes a single public header
  (the `nsl-ast` exception is documented in Principle II); private
  headers stay in `lib/`.
- **License:** Apache License 2.0 with LLVM Exceptions, identical to LLVM,
  MLIR, and CIRCT. Every new file MUST carry an `SPDX-License-Identifier:
  Apache-2.0 WITH LLVM-exception` header on its first line, in the comment
  syntax appropriate to the file format (Markdown HTML comment, EBNF `(*
  *)` comment, LLVM-style C++/TableGen header block — see
  `CONTRIBUTING.md` §2 for the canonical examples).
- **Patent grant.** Apache 2.0 §3 plus the LLVM Exception together govern
  contributions and the encumbrance status of compiler-emitted Verilog. No
  contribution carrying an incompatible license may be added.
- **Build system:** CMake, LLVM-style (`add_nsl_library`, etc.), per
  `docs/design/nsl_compiler_design.md` §13.

## Development Workflow

- **Spec Kit pipeline.** Feature work flows through the Spec Kit commands:
  `/speckit-constitution` (this document) → `/speckit-specify` → optional
  `/speckit-clarify` and `/speckit-checklist` → `/speckit-plan` →
  `/speckit-tasks` → `/speckit-implement`, with `/speckit-analyze` available
  for cross-artifact consistency checks. Templates and scripts in
  `.specify/` are the contract; bespoke workflows that bypass them are
  prohibited.
- **Routing first.** Before opening a file in `docs/spec/` or
  `docs/design/`, contributors MUST consult `docs/CLAUDE.md` (the routing
  guide) and use line-range views. Reading whole files is the wrong
  default and costs ~40k tokens per file.
- **Sign-off.** Every commit by a human contributor MUST include a
  `Signed-off-by:` trailer (DCO-style; see `CONTRIBUTING.md` §4). The
  human submitter is responsible for reviewing all AI-generated content
  before committing, and for the change's overall correctness.
- **AI attribution.** AI assistants MUST NOT add `Signed-off-by:` trailers.
  AI-assisted commits MUST add an `Assisted-by: <AGENT_NAME>:<MODEL_VERSION>
  [TOOL1] [TOOL2]` trailer per `CONTRIBUTING.md` §5. Use judgement: a
  drafted section, a non-trivial restructure, or a generated table requires
  the trailer; a spell-check fix does not.
- **Pull requests.** PRs MUST satisfy the project-wide checklist in
  `CONTRIBUTING.md` §7 and MUST pass through CodeRabbit review (see
  External Integrations below). PRs touching `docs/` MUST additionally
  satisfy the documentation-specific checklist in
  `docs/CLAUDE.md` §11 (cross-reference updates, `Sn`/`Nn`/`Pn`
  quick-map, line ranges).
- **No upstream PDFs.** The upstream NSL reference manuals and tutorials
  cited in `docs/spec/*.ebnf` headers MUST NOT be committed.

## External Integrations

**CodeRabbit** is enabled for this project and serves four roles:

1. **Code review.** Every pull request opened against `main` MUST pass
   through CodeRabbit review. CodeRabbit's findings are advisory by
   default; findings that identify a violation of this Constitution, a
   `docs/spec/` ↔ `docs/design/` contradiction (Principle VII), or a
   regression in the audited-project suite (Principle VI) MUST be
   addressed before merge. **Classification authority:** the contributor
   classifies CodeRabbit findings as "blocking" vs "advisory" on first
   review; disputes route to `/nsl-constitution-review` for a binding
   judgement (constitutional findings are blocking by definition).
2. **Quality management.** CodeRabbit's quality and architectural-
   consistency feedback is an input to the contributor's judgment, not
   auto-applied. A pattern of repeated quality regressions across PRs is
   a signal to amend `CONTRIBUTING.md` or this Constitution, not to let
   the regression silently re-occur.
3. **Debugging support.** CodeRabbit's debugging suggestions are
   advisory. Root-cause analysis and the actual fix remain the
   contributor's responsibility under Principle VIII (TDD); a regression
   test reproducing the bug MUST land in the same change as the fix.
4. **Tasks planning.** Work items CodeRabbit identifies during review
   or PR walkthroughs MUST be routed to Linear (feature-track) or
   GitHub Issues (bug reports) per the rules below. CodeRabbit is not
   itself the system of record for tasks.

**Linear** is the canonical system of record for **feature-track work
items** — features, refactors, plan items, and follow-ups generated by
`/speckit-taskstoissues`. The Linear team and project are both `nslc`;
issue and branch IDs use the Linear-idiomatic uppercase team-key form
`NSLC-<N>`.

- All feature-track work items — features, refactors, CodeRabbit-
  generated planning items, and any TODO-equivalent that is not a
  user-reported bug — MUST be tracked as Linear issues. Ad-hoc
  `TODO`/`FIXME` comments left in code or chat are NOT the system of
  record; if work is needed, a Linear issue MUST be opened.
- PR descriptions and commit trailers SHOULD reference the originating
  Linear issue ID when one exists (e.g., `Closes NSLC-<N>` in the PR
  body, or a `Linear: NSLC-<N>` trailer in the commit message,
  positioned after `Signed-off-by` and `Assisted-by` per
  `CONTRIBUTING.md` §6).
- Tasks generated by `/speckit-tasks` and pushed via
  `/speckit-taskstoissues` land in **GitHub Issues** as the staging
  API; Linear's first-party GitHub integration MUST be configured to
  mirror those feature-track issues into the canonical Linear view.
  Bug-report issues (see "GitHub Issues" sub-section below) are NOT
  mirrored into Linear; the mirror configuration MUST EXCLUDE the
  bug-report label. Direct push from the skill to Linear (via a
  Linear MCP server) is a future option, not currently configured.

**GitHub Issues** is the canonical system of record for **bug
reports** — defects in already-merged behavior, regressions discovered
post-merge, and user-reported failures.

- A bug report MUST be filed as a GitHub Issue with reproduction
  steps and the expected/actual behavior. Bug reports are NOT
  mirrored into Linear (mirror config excludes the bug-report label).
- A PR fixing a bug MUST reference the GitHub Issue (e.g., `Fixes #<n>`
  in the PR body). Per Principle VIII, the PR MUST land a regression
  test reproducing the bug in the same change.
- The line between "feature work" (Linear) and "bug" (GitHub Issues)
  is set by where the change lands relative to merged code: defects
  in merged code = GitHub Issues; planned work, refactors, and
  unimplemented features = Linear. When ambiguous, default to
  Linear; the contributor's classification stands unless overridden
  by review.

**Merge gate composition.** A PR is mergeable only when **all** of the
following are true:

1. CI is green (Principle IX).
2. CodeRabbit review has run and all blocking findings (Constitution
   violations, spec/design contradictions, regression-suite breakage)
   are addressed.
3. The originating work item is referenced: PRs implementing a
   feature-track Linear issue MUST reference it (`Closes NSLC-<N>`
   or `Linear: NSLC-<N>` trailer); PRs fixing a GitHub bug-report
   issue MUST reference it (`Fixes #<n>`). PRs that originate from
   neither (e.g., a typo fix discovered while reading) are exempt
   from clause 3 only when they qualify for the "Docs / comments"
   row of the application-criteria table in `CONTRIBUTING.md` §3.3.

The CodeRabbit gate and the CI gate are independent — both apply. The
no-bypass rule of Principle IX extends to CodeRabbit: opening a PR
with CodeRabbit suppressed, or merging while CodeRabbit review is
deliberately skipped, is prohibited unless the user explicitly
authorizes a specific, named exception recorded in the PR description.

**Rationale.** Pinning these tools and their roles in the Constitution
prevents drift in the merge-gate definition as the project grows.
Splitting Linear (feature-track) from GitHub Issues (bugs) keeps each
system focused: Linear stays a forward-looking plan, GitHub Issues
stays the place where users file what's broken. Without classification
authority for CodeRabbit, reviews can either be rubber-stamped or
escalate every nit to a merge blocker. Both failure modes are common
in projects that adopt these tools without writing down what they
mean.

## Governance

This Constitution supersedes all other practices. In any contradiction with
style guides, READMEs, prior chat agreements, or assistant memory, the
Constitution wins.

- **Amendment procedure.** Amendments require a pull request that (a)
  modifies `.specify/memory/constitution.md`, (b) prepends an updated
  Sync Impact Report HTML comment at the top of the file, (c) propagates
  changes to dependent templates in `.specify/templates/` (or marks them
  ✅ aligned with rationale in the report), and (d) bumps the version per
  semantic-versioning rules below.
- **Versioning policy.** **MAJOR** for backward-incompatible removal or
  redefinition of a principle, of a governance rule, or of a hard
  invariant — i.e., the rule is dropped or replaced with an
  incompatible alternative. **MINOR** for adding a new principle/
  section, materially expanding existing guidance, or **narrowing a
  rule's scope without removal** (e.g., splitting one rule across two
  systems while preserving the underlying invariant). **PATCH** for
  clarifications, wording, and typo fixes that do not change
  semantics. When the bump type is ambiguous, the amendment PR MUST
  argue the choice in its description.
- **Compliance review.** All PRs and code reviews MUST verify compliance
  with this Constitution. Violations require either a fix or a justified
  entry in the `Complexity Tracking` table of the affected `plan.md`,
  with the simpler alternative explicitly named and rejected.
- **Disputed findings.** When a constitutional finding from
  `/nsl-constitution-review` or `/nsl-coupling-audit` is disputed, the
  resolution path is: (a) `/speckit-clarify` to identify any underlying
  spec ambiguity; (b) if the dispute is about constitutional
  interpretation rather than spec ambiguity, file an amendment PR via
  `/speckit-constitution`. Constitutional findings are blocking until
  resolved through one of these paths or withdrawn by the finding's
  author.
- **Runtime guidance.** This Constitution governs intent and policy.
  Concrete contributor guidance lives in `CONTRIBUTING.md` (project-wide);
  routing, editing rules, and the `docs/`-specific PR checklist for the
  `docs/` tree all live in `docs/CLAUDE.md`. Spec Kit automation lives
  in `.specify/` and the corresponding `/speckit-*` skills under
  `.claude/skills/`. Project-specific implementation skills under
  `.claude/skills/nsl-*/SKILL.md` and their parallel agent definitions
  under `.claude/agents/nsl-*.md` MUST be updated together: editing
  one without the other is forbidden, and a coupling check on this is
  part of `/nsl-constitution-review`.
- **Milestone plan.** Project delivery sequencing — the
  compiler-track `Mxx`–`Myy` table, the tooling-track `Txx`–`Tyy`
  table, and the project-enablement deliverables (`P-CI`, `P-VEN`,
  `P-VCD`, `P-LIN`, `P-TS`) — lives in `README.md` (compiler and
  tooling tracks plus `P-CI`/`P-VEN`/`P-VCD`), `CLAUDE.md` (NSL
  language-feature → milestone and tooling-feature → milestone
  lookup tables), and `CONTRIBUTING.md` §3.8–§3.9 (workflow
  `P-LIN`/`P-TS` and amendment rules). Milestone numbers (`M*`,
  `T*`) follow the monotonic-numbering rule of Principle I — retired
  numbers MUST NOT be reused; renumbering is forbidden.

**Version**: 1.5.0 | **Ratified**: 2026-04-25 | **Last Amended**: 2026-04-27
