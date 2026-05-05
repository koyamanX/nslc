<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Research — T8 Tree-sitter Grammar + Highlight Queries + VS Code WASM Consumer

**Feature**: `010-t8-tree-sitter-grammar`
**Phase**: 0 (research / unknowns resolution)
**Date**: 2026-05-05

This document resolves every plan-level open question raised
during spec authoring (4 Clarifications session items) and
Constitution-Check evaluation. Each section records:
**Decision** / **Rationale** / **Alternatives considered**.

---

## 1. Tree-sitter CLI version pin (Clarifications Q1 → Option B)

**Decision**: **`tree-sitter-cli@0.22.x`** as a minor-version
range pin in `grammars/treesitter/package.json`'s
`devDependencies`, locked through `package-lock.json` (committed,
CI installs from the lockfile). Bumping is a deliberate PR
analogous to the M-track's deliberate LLVM/MLIR bumps via the
dev-container.

**Rationale**:
- `tree-sitter-cli` 0.22.x is the current stable line as of
  2026-05-05. It introduced ABI v14 + the `web-tree-sitter`
  WASM wrapper used by VS Code consumers; older lines (0.20,
  0.21) emitted ABI v13, which most modern editor runtimes
  still accept but is being deprecated.
- A **minor** range (not a single exact patch) absorbs
  bug-fix patches without forcing a regenerate-and-diff churn
  on every upstream patch release. The `package-lock.json`
  pins the exact patch installed by CI for byte-stable
  output (Constitution Principle V).
- The minor pin is renewed by editing `package.json` and
  re-running `npm install --package-lock-only`, then
  regenerating `parser.c` (one PR for the full bump). This
  matches the workflow T1 established for `vscode-tmgrammar-test`
  bumps.

**Alternatives considered**:
- **Pin `latest`** (Q1 Option A) — rejected: floats the
  regenerate-and-diff gate; SC-008 byte-identity becomes
  un-asserstable across CI runs separated by an upstream
  release. Per the Q1 → Option B answer.
- **Pin exact patch** (Q1 Option C) — rejected: creates
  weekly-cadence churn from upstream patch releases that
  don't change generated output. The minor-range strikes
  the right balance.
- **Dev-container pin** (Q1 Option D) — partially adopted:
  the dev container does ship the toolchain, but the
  package.json + lockfile is also kept in-tree for
  contributors who run `npm ci` outside the container. Both
  paths agree because the lockfile is the source of truth.

---

## 2. WASM artefact lifecycle (Clarifications Q2 → Option C)

**Decision**: `tree-sitter-nsl.wasm` is **not committed** to
the repo. CI builds it in stage 2's new "Build WASM" sub-step,
asserts byte-identity by running the build twice and comparing
`sha256sum` (SC-008), uploads it as a GitHub Actions workflow
artefact via `actions/upload-artifact@v4`, and (on tagged
release builds) additionally attaches it to the release page
via `softprops/action-gh-release`.

**Rationale**:
- Preserves T1's no-binary norm (SC-005: "0 bytes of
  compiled binary"); the WASM is a build output, not a source
  artefact.
- Workflow artefacts are downloadable from any green CI run
  (subject to GitHub's 90-day retention) — the documented
  "no-toolchain consumer install" path in SC-007.
- Tagged-release attachment matches the deferred-publication
  policy (`README.md §Roadmap` T1/T12 deferral note): binary
  available alongside source releases, but no Marketplace
  push.
- Determinism is testable purely CI-side: build, hash, build
  again, hash, compare. No working-tree assertion is needed.
- The `parser.c` is treated differently (committed) — see §4
  below.

**Alternatives considered**:
- **Don't commit; consumers run `tree-sitter build-wasm`
  locally via npm postinstall** (Q2 Option A) — rejected:
  imposes a Node + Emscripten toolchain dependency on every
  consumer who folder-drops the extension. Not consistent
  with SC-007 ("under 5 minutes install").
- **Commit `tree-sitter-nsl.wasm`** (Q2 Option B) — rejected:
  breaks the no-binary norm without offsetting benefit. CI
  byte-identity asserts cleanly without committing.
- **Commit only on tagged-release branches** (Q2 Option D) —
  rejected: complicates the branching model; equivalent to
  Option C minus the workflow-artefact path, which is
  strictly better.

---

## 3. Emscripten dependency for `tree-sitter build-wasm`

**Decision**: Use `tree-sitter-cli` 0.22's **bundled
`docker`-based build path** as the default; the dev container
gains an explicit `tree-sitter` Node-package install but does
NOT ship a full Emscripten SDK. CI uses the same docker-based
path, running tree-sitter inside a sub-container of the
dev-container.

**Rationale**:
- `tree-sitter build-wasm` 0.22+ supports a `--docker` flag
  that downloads `emscripten/emsdk:3.x` on first use, builds
  the WASM inside that sub-container, and emits the artefact
  to the host. This avoids vendoring a 1.5+ GB Emscripten
  SDK into our dev container.
- CI already runs inside the project's dev container
  (`ghcr.io/koyamanx/nsl-nslc:dev`); the docker-build path
  uses Docker-in-Docker, which the project's CI workflows
  already configure for the publish-images stage. Same
  capability set, no new permissions.
- For local contributors without docker, an alternative is
  to install Emscripten via `emsdk` and pass `--no-docker`
  to `build-wasm`. Document both paths in `quickstart.md`.

**Alternatives considered**:
- **Vendor full Emscripten SDK in the dev container** —
  rejected: 1.5 GB image size growth for one
  CI-stage-2-only step.
- **Use prebuilt WASM artefacts from upstream** — rejected:
  there is no upstream prebuilt WASM for our (yet-to-be-
  written) NSL grammar; this option only exists for
  upstream-maintained grammars (`tree-sitter-c`, etc.).

---

## 4. `parser.c` commit policy

**Decision**: **Commit `parser.c`**, `grammar.json`, and
`node-types.json` to the repo as generated-source artefacts.
CI gates byte-equality via FR-017's regenerate-and-diff
sub-step.

**Rationale**:
- Strong community default in the tree-sitter ecosystem.
  Every well-maintained tree-sitter grammar I surveyed
  (`tree-sitter-c`, `tree-sitter-rust`, `tree-sitter-python`,
  `tree-sitter-cpp`, `tree-sitter-bash`) commits the
  generated `parser.c`. Doing differently here would
  surprise consumers.
- Reviewers benefit from `parser.c` being visible in the PR
  diff: a grammar.js change that produces unexpectedly
  large parser-state changes is immediately visible. (CI
  alone can confirm byte-equality but won't surface
  *unexpected* size-of-change.)
- Downstream consumers — Neovim's `nvim-treesitter` (T11),
  Emacs's `tree-sitter.el` (T11), Helix, Zed — many fetch
  pre-generated `parser.c` from the grammar repo rather than
  running `tree-sitter generate` themselves.
- Regeneration is mechanically reproducible: the
  `tree-sitter-cli` pin (§1) governs `parser.c` byte
  output; CI's regenerate-and-diff (FR-017) catches drift.

**Alternatives considered**:
- **Don't commit `parser.c`; build in CI** — rejected:
  every consumer of the grammar (Neovim, Emacs, etc. at
  T11) would need the tree-sitter CLI installed at consume
  time. The tree-sitter community convention is to commit
  the generated parser; we follow it.
- **Commit `parser.c` but not `grammar.json`/
  `node-types.json`** — rejected: these JSON files are
  consumed by editor integrations (e.g. tree-sitter-graph,
  query authoring tools); committing the full set keeps
  the repo self-contained.

This decision was flagged in spec.md's checklist Notes
("plan-level decisions") and is now resolved in this
direction.

---

## 5. Smoke-fixture set choice (Clarifications Q4 → Option C)

**Decision**: At T8 merge, the smoke-fixture set is the
in-tree `examples/01_hello.nsl`–`examples/20_simulation_tb.nsl`
corpus (20 files, ~3,000 LOC). Reference these files via a
`test/tooling/treesitter/smoke/corpus.txt` line-list rather
than copying them. Once P-VEN (audited corpus vendoring) lands
at M7, `corpus.txt` gains entries for
`test/audited/<project>/` paths.

**Rationale**:
- Per Q4 → Option C: zero new authoring; `examples/*.nsl`
  was already curated for production coverage by an earlier
  milestone (it's the pedagogical corpus referenced by the
  M-track's M3 sema tests).
- A line-list `corpus.txt` keeps the M-track's `examples/`
  as its own source of truth and avoids a 3,000-line
  duplicate. An addition to `examples/` that the T8
  grammar can't parse fails CI immediately on the next PR
  — coupling Principle VII to the example corpus too.
- `examples/` covers every major language construct
  (1: hello, 2: gates, 3: registers, 4: counters, 5: ALU,
  6: alt vs any, 7: function, 8: func_self, 9: proc/seq,
  10: for, 11: while, 12: generate, 13: FSM, 14: memory,
  15: struct, 16: bit-ops, 17: concat-lvalue, 18:
  proc-methods, 19: param, 20: simulation_tb). This
  matches the audited-corpus production-coverage breadth.
- When the audited corpus lands, *both* sets stay in the
  gate. `examples/*.nsl` continues to validate
  construct-level coverage even if the audited corpus
  underuses some construct.

**Alternatives considered**:
- **Hand-write a new T8-specific representative fixture**
  (Q4 Option A) — rejected: duplicates `examples/`.
- **Skip the smoke gate until P-VEN lands** (Q4 Option B) —
  rejected: weakens FR-014 to vacuous and contradicts the
  README §Roadmap T8 row's stated test gate.
- **Block T8 merge on P-VEN** (Q4 Option D) — rejected:
  inverts the dependency direction; T8 is supposed to be
  parallel-trackable from M5 forward.

---

## 6. Capture-name set delivered at T8 (Clarifications Q3 → Option B)

**Decision**: T8 ships the `nsl_tooling_design.md §4.3` base
capture set **plus eight specific sub-captures**:
`@variable.register`, `@variable.wire`, `@variable.memory`,
`@function.proc`, `@function.func`, `@function.call.proc`,
`@function.call.func`, `@label.state`. Total: 17 distinct
captures asserted by the golden test (per SC-003).

**Rationale**:
- The headline value-prop (US1 / FR-008) is *distinguishing*
  `reg` references from `wire` references. A single
  `@variable` capture cannot satisfy "register and wire
  reference distinct captures"; the sub-capture set is the
  *minimum* required to make AS1.1–AS1.5 pass.
- Tree-sitter convention permits multi-segment capture
  names (`@variable.register`, `@function.call.proc`) and
  consumer themes routinely map them by parent (e.g.
  `@variable.register` falls back to `@variable` if no
  theme rule exists).
- Per Q3 → Option B and the spec's FR-007 / FR-008 / FR-009
  / FR-010 amendments.

**Alternatives considered**: All four options of Clarifications
Q3 are catalogued in spec.md's `## Clarifications` section;
the Q3 → Option B answer is what this research entry
implements.

**Capture → theme mapping risk**: themes that map only the
§4.3 base set will not see the §4.3-extension sub-captures
distinctly (everything falls back to the parent `@variable` /
`@function` / `@label` colour). This is acceptable per the
three-tier precedence (`nsl_tooling_design.md §4`): T8 is the
middle tier; if a user's theme hasn't learned the
sub-captures, they get T1's level of distinction (still an
improvement over T1 because reg/wire references are at least
both captured as `@variable.*`, where T1 left them un-scoped).
T4 (LSP `semanticTokens`) is the top tier and will refine
further when present.

---

## 7. VS Code extension shell — `web-tree-sitter` consumption pattern

**Decision**: The extension is a **standalone TypeScript
extension** under `editors/vscode/treesitter/`. It activates
on the `nsl` language ID (already registered by T1's
`editors/vscode/package.json`), loads `tree-sitter-nsl.wasm`
via the `web-tree-sitter` package (lazy-loaded on first
`.nsl` file open), and registers a
`vscode.languages.registerDocumentSemanticTokensProvider` that
applies `queries/highlights.scm` to map parse-tree captures
to VS Code's `SemanticTokensLegend`.

**Rationale**:
- `web-tree-sitter` is the upstream VS Code consumption
  pattern for tree-sitter grammars. It loads the WASM in a
  worker thread, parses incrementally as the document
  changes, and exposes the parse tree to JS.
- VS Code's `DocumentSemanticTokensProvider` is the
  canonical surface for applying parse-derived token
  classifications. It composes correctly with TextMate (T1
  is the base layer; T8 layered on top).
- Activation only on `.nsl` / `.nslh` / `.inc` files
  (declared via `activationEvents` + `onLanguage:nsl`)
  keeps extension-host startup cost zero on non-NSL
  workspaces.

**Alternatives considered**:
- **Use VS Code's built-in tree-sitter API** — rejected:
  VS Code 1.84+ has experimental tree-sitter grammar
  contribution points, but the API surface is still
  unstable and gated behind `proposed-api`. `web-tree-sitter`
  is the stable path.
- **Use `vscode-textmate` integration** — rejected: that's
  the path T1 is already on; T8 needs a real parse tree,
  not regex tokens.
- **Build a separate `vscode-treesitter-nsl` extension
  package** (rather than extending T1's `editors/vscode/`)
  — rejected: T1 + T8 ship together as one VS Code
  package per `nsl_tooling_design.md §8` shared
  directory layout. Splitting them would make folder-drop
  install confusing.

---

## 8. Tree-sitter test format — golden assertion syntax

**Decision**: Use **`tree-sitter test`'s native
parse-tree-snapshot format** for parser correctness
(under `test/tooling/treesitter/corpus/`) and
**`tree-sitter highlight --test`'s inline-comment format**
for highlight-query golden tests (under
`test/tooling/treesitter/highlights/`). Both formats are
documented at `https://tree-sitter.github.io/tree-sitter/cli/test`.

**Rationale**:
- Native tooling: `tree-sitter test` and `tree-sitter
  highlight --test` are bundled with the CLI; no extra
  driver dependency.
- Inline-comment format colocates assertions with fixtures,
  same shape as T1's `vscode-tmgrammar-test` `// <-` and
  `// ^^^` syntax (different syntax, same locality
  principle).
- Failing assertions point at file/line/column with both
  the observed and expected capture, satisfying FR-015's
  "test failure MUST identify the failing assertion by
  file/line/column and observed-vs-expected scope" wording
  (mirrored from T1 FR-017).

**Alternatives considered**:
- **Hand-rolled JS test harness** — rejected: reinvents
  what `tree-sitter test` already does.
- **Snapshot-test approach (dump full parse tree, diff
  against golden)** — partially adopted: that **is** what
  `tree-sitter test`'s `corpus/` format is (parse-tree
  snapshots in S-expression form). Used for parser-shape
  correctness; not for highlight queries (where inline
  capture-assertions are the right shape).

---

## 9. SPDX header convention for `grammar.js` and `highlights.scm`

**Decision**: `grammar.js` uses a `// SPDX-…` line-1 comment
(JavaScript line-comment syntax). `highlights.scm` uses a
`; SPDX-…` line-1 comment (Scheme-family line-comment
syntax — `tree-sitter highlight` accepts `;` as a comment
prefix in `.scm` files). `parser.c` and `grammar.json` /
`node-types.json` are GENERATED — no SPDX header expected; a
top-level `grammars/treesitter/SPDX.NOTICE` file documents
that these files inherit the project license via the
generator.

**Rationale**:
- Line-1 SPDX comment is the cross-language norm for
  human-authored sources (precedents: every existing
  C++/Python/Markdown file in the repo).
- For *generated* C / JSON files, the upstream tree-sitter
  CLI's output has no provision to emit a custom header;
  injecting one post-generation breaks the
  regenerate-and-diff CI gate (FR-017). The
  `SPDX.NOTICE` companion file is the project-wide
  convention precedent: repos that ship generated artefacts
  use a sibling NOTICE rather than fight the generator.

**Alternatives considered**:
- **Patch the generator to inject SPDX headers** — rejected:
  upstream churn risk; every `tree-sitter-cli` bump would
  potentially re-break the patch.
- **Generate `parser.c` outside the repo and never commit
  it** — see §4 above; rejected for downstream-consumer
  reasons.
- **Use a `_comment_top` JSON-key in `grammar.json` /
  `node-types.json`** — rejected: tree-sitter consumers
  may strict-validate the JSON shape and reject unknown
  keys. The companion-NOTICE approach is safer.

The project's `scripts/check_spdx.py` MUST be amended (T8
implementation step) to recognise the
`grammars/treesitter/parser.c`,
`grammars/treesitter/grammar.json`, and
`grammars/treesitter/node-types.json` paths as
SPDX-exempted-via-NOTICE — same pattern as
`scripts/spdx_exceptions.txt` already exempts certain
generated paths.

---

## 10. Performance budget verification

**Decision**: SC-006's "≤ 60 s on a standard CI runner" is
verifiable in CI by `time` instrumentation around the
smoke + golden + regenerate-and-diff + WASM-determinism
sub-steps; CI fails if total wallclock exceeds 60 s on a
GitHub-hosted `ubuntu-latest` runner.

**Rationale**:
- `tree-sitter parse` on the 20 `examples/*.nsl` files
  totalling ~3,000 LOC parses in <1 s with the generated C
  parser.
- `tree-sitter test` on the highlight golden takes
  <1 s for ~200 lines of fixture.
- `tree-sitter generate` from cold cache takes ~2-5 s for a
  ~600-line `grammar.js` with 17 captures.
- `tree-sitter build-wasm` (docker path) takes ~30-45 s on
  cold cache (Emscripten image pull) and ~10-15 s on warm.
  With CI cached docker layers, warm is the steady state.
- Total: 30-65 s steady-state, 50-70 s cold. Margin is
  tight; if the 60 s budget is breached in practice, the
  budget gets revisited (plan-level renegotiation, not a
  spec change — SC-006's "≤ 60 s" is the spec's target,
  not a hard constraint).

**Alternatives considered**:
- **Drop SC-006 to ≤ 120 s** — held in reserve if the
  cold-cache `build-wasm` step proves persistently
  out-of-budget. Will not pre-emptively relax.

---

## 11. Coupling to `KeywordSet.def` — identical to T1?

**Decision**: Yes — `scripts/gen_treesitter_grammar.py`
reads `include/nsl/Lex/KeywordSet.def` and emits the
keyword block of `grammar.js` (the `keywords` rule and
the `_keyword` token list inside `grammar.js`) in
source order. Hand-authored regions (productions §§1–11,
preprocessor seam, `%IDENT%` modelling) live in a
separate template file
(`scripts/templates/grammar.js.template`) that the
generator splices the keyword block into.

**Rationale**:
- Mirrors `scripts/gen_textmate_grammar.py` exactly. CI
  gates regenerate-and-diff on both grammars side by
  side. A `KeywordSet.def` edit that lands without a
  T1+T8 grammar regeneration fails both CI sub-steps.
- Same Principle VII coupling guarantee as T1.

**Alternatives considered**:
- **Hand-author the keyword list in `grammar.js`** —
  rejected: violates Principle VII coupling. The
  regenerate-and-diff gate becomes an audit-only check
  rather than a generative one.
- **Generate the entire `grammar.js`** — rejected: the
  hand-authored productions are too rule-specific to
  template effectively. Splicing the keyword block is
  the right boundary.

---

## 12. CI integration — which stage owns which sub-step?

**Decision**:
- **Stage 2 (static checks)** gains:
  - `treesitter-grammar-regen-diff`: runs `npm ci` in
    `grammars/treesitter/`, then `npx tree-sitter
    generate`, then `git diff --exit-code`.
  - `treesitter-wasm-determinism`: builds WASM twice,
    `sha256sum` compares.
  - `treesitter-spdx`: validates `grammar.js`,
    `highlights.scm`, generator-script SPDX line-1
    headers.
- **Stage 3 (unit & layer tests)** gains:
  - `treesitter-smoke`: `tree-sitter parse` over every
    file listed in
    `test/tooling/treesitter/smoke/corpus.txt`, fail on
    any `(ERROR)` or `(MISSING)` node.
  - `treesitter-highlights-golden`: runs
    `tree-sitter test` against
    `test/tooling/treesitter/highlights/`.
- **GitHub Actions workflow** gains a separate "Build &
  Upload tree-sitter-nsl.wasm" step (stage 4 / parallel
  to stage 5) that produces the workflow artefact and,
  on tagged commits, attaches it to the GitHub release.

**Rationale**:
- Static checks (stage 2) is where T1's regenerate-and-diff
  for the TextMate grammar lives; T8 follows the same
  convention.
- Unit & layer tests (stage 3) is where T1's
  `vscode-tmgrammar-test` runner lives; T8's tree-sitter
  test runner belongs in the same stage.
- The WASM upload is best as a separate GitHub Actions
  step rather than buried inside `./scripts/ci.sh` because
  workflow-artefact upload is a CI-runner concern
  (`actions/upload-artifact@v4`), not a `scripts/ci.sh`
  concern. `./scripts/ci.sh` remains the local-CI entry
  point per Constitution Principle IX "reproducible
  locally" clause.

**Alternatives considered**:
- **Single new "tree-sitter" stage** — rejected: would
  fork the existing 6-stage `./scripts/ci.sh` shape; less
  consistent with T1.
- **Bundle WASM upload inside `./scripts/ci.sh`** —
  rejected: workflow-artefact upload uses GitHub-Actions-
  specific actions; `./scripts/ci.sh` should stay
  CI-runner-agnostic.
