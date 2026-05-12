<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Implementation Plan: T8 — Tree-sitter Grammar + Highlight Queries + VS Code WASM Consumer

**Branch**: `010-t8-tree-sitter-grammar` | **Date**: 2026-05-05 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/010-t8-tree-sitter-grammar/spec.md`

## Summary

Deliver tooling-track milestone **T8** — the second tier of the
project's two-tier highlighter strategy
(`docs/design/nsl_tooling_design.md §4`), building on T1's
TextMate base layer with a context-aware tree-sitter parser that
distinguishes identifier *contexts* T1 explicitly left un-scoped
(`reg` vs `wire` vs `proc_name` vs `func_in` references), tags
control-terminal names per `S27`, handles `%IDENT%` macro splice
sites, and resolves the parser-note ambiguities (`N5`, `N2`, `N3`,
`N6`) T1 deferred. T8 ships:

1. **`grammars/treesitter/grammar.js`** — JavaScript grammar
   description covering productions §§1–11 of `nsl_lang.ebnf` plus
   the preprocessor seam from `nsl_pp.ebnf §2`. Keyword set
   sourced from `include/nsl/Lex/KeywordSet.def` (the same
   single-source-of-truth T1 consumes), via a new
   `scripts/gen_treesitter_grammar.py` that mirrors the
   `scripts/gen_textmate_grammar.py` precedent.
2. **`grammars/treesitter/queries/highlights.scm`** — tree-sitter
   query file emitting **the §4.3 base set + 8 sub-captures**
   (`@variable.register`, `@variable.wire`, `@variable.memory`,
   `@function.proc`, `@function.func`, `@function.call.proc`,
   `@function.call.func`, `@label.state`) per Clarifications
   session 2026-05-05 Q3 → Option B.
2a. **`grammars/treesitter/queries/locals.scm`** — companion
    query file emitting `@local.scope` / `@local.definition.<NAME>`
    / `@local.reference` captures. Tree-sitter-highlight walks the
    scope chain to resolve every `@local.reference` identifier to
    its nearest enclosing `@local.definition.<NAME>` and applies
    `@<NAME>` to the reference site automatically — closing the
    reference-side half of FR-007 / FR-008 / FR-009 without
    duplicating reference-position patterns in `highlights.scm`.
3. **`grammars/treesitter/parser.c`** (and companion
   `grammar.json` / `node-types.json`) — output of
   `tree-sitter generate`. Committed; CI verifies regenerate-and-diff.
4. **`editors/vscode/treesitter/`** — VS Code extension shell that
   activates on the `nsl` language ID (already registered by T1),
   loads `tree-sitter-nsl.wasm` via `web-tree-sitter`, and applies
   `highlights.scm` as a `DocumentSemanticTokensProvider`. Coexists
   with T1's TextMate contribution (T1 base layer; T8 overrides
   identifier captures).
5. **`test/tooling/treesitter/`** — smoke-parse fixtures (the
   in-tree `examples/01_*.nsl`–`examples/20_simulation_tb.nsl`
   corpus per Q4 → Option C) plus highlight-query golden test
   fixtures consumed by `tree-sitter test` and a new
   `./scripts/ci.sh` stage-3 sub-step.

**Technical approach**: tree-sitter CLI **pinned** to a specific
minor version per Clarifications Q1 → Option B (canonical pin:
`tree-sitter-cli@0.22.x`, recorded in
`grammars/treesitter/package.json`'s `devDependencies`). The
WASM artefact is **not committed** per Q2 → Option C; CI builds
`tree-sitter-nsl.wasm` and uploads it as a workflow artefact,
with tagged releases additionally attaching it to the release
page. The keyword block of `grammar.js` is generated from
`KeywordSet.def` via `scripts/gen_treesitter_grammar.py` so
spec ↔ grammar drift is mechanically gated (Principle VII)
identically to T1. Hand-authored regions (full grammar
productions §§1–11, preprocessor seam, `%IDENT%` escape-hatch
modelling) live alongside the generator's keyword block in the
single `grammar.js` file.

**Principle II (no duplication)**: T8 introduces a *parallel
parser* (a JS-described grammar that produces `parser.c`) outside
`libNSLFrontend.a`. This is the **same scoped exception** that
applied to T1's TextMate grammar — both tiers of the
two-tier-highlighter strategy in `nsl_tooling_design.md §4` are
permitted parallel parsers because the highlighter ecosystem
demands its own parser-input formats. Lint, formatter, and LSP
(T2/T3+/T6) remain pinned to `libNSLFrontend.a` and are
unaffected. **Principle V (determinism)** is satisfied by pinning
`tree-sitter-cli` (FR-017 regenerate-and-diff gate) plus by
documenting `tree-sitter build-wasm` byte-identity expectations
as an explicit CI assertion (SC-008). **Principle VIII (TDD)** is
satisfied by the smoke + golden fixtures landing first (observed
failing: smoke fails because no grammar exists; golden fails
because `highlights.scm` does not yet emit the captures), then
the grammar + queries land second.

## Technical Context

**Language/Version**: JavaScript ES6+ (tree-sitter `grammar.js`
DSL — runs on tree-sitter CLI's bundled Node.js); SchemeScheme
S-expression DSL for `queries/highlights.scm`; C99 for the
generated `parser.c`. Generator scripts in Python 3 (matches the
T1 `gen_textmate_grammar.py` precedent).

**Primary Dependencies**:
- **`tree-sitter-cli`** — pinned to `0.22.x` (specific minor
  version per Clarifications Q1 → Option B). Recorded in
  `grammars/treesitter/package.json`'s `devDependencies`. The
  CI cell installs exactly this pinned range; bumping is a
  deliberate PR (analogous to the M-track's deliberate
  LLVM/MLIR bumps via the dev-container). Pin governs
  `tree-sitter generate` output (FR-017 regenerate-and-diff)
  and `tree-sitter build-wasm` output (SC-008 byte-identity).
- **`web-tree-sitter`** — the WASM-based runtime for
  tree-sitter, consumed by the VS Code extension shell. Pinned
  to a specific patch version compatible with the
  `tree-sitter-cli` pin's ABI.
- **Node.js + npm** — already required by T1's
  `vscode-tmgrammar-test`. The dev container
  `ghcr.io/koyamanx/nsl-nslc:dev` already ships Node per the
  T1 research.md §1 "Containerization implication" entry; T8
  adds `tree-sitter-cli` and `web-tree-sitter` to the same
  Node toolchain in the dev container.
- **Python 3** — already required by `scripts/gen_*.py`
  precedent; no new dependency.
- **Emscripten** — required transitively by
  `tree-sitter build-wasm`. The dev container needs to ship
  `emscripten` (or use the prebuilt-via-docker fallback that
  `tree-sitter-cli` 0.22+ provides). See research.md §3.

**Storage**: N/A — pure static source artefacts plus generator
scripts plus committed generated `parser.c`. WASM is build
output, not committed.

**Testing**:
- **Layer**: tree-sitter test layer (new T-track layer; first
  use at T8). Per Constitution Principle VI's per-layer
  accepted-driver list, the compiler-test driver enumeration
  (lit + FileCheck for lowering / E2E; gtest / nsl-opt for
  unit / dialect) does not constrain non-compiler tooling
  tests; `tree-sitter test` (smoke + golden) is the
  conventional driver for this layer, identical in scoping to
  the T1 introduction of `vscode-tmgrammar-test`.
- **Smoke** (FR-014): `tree-sitter parse --quiet --stat` over
  every file in the smoke-fixture set; the run fails on any
  `(ERROR)` or `(MISSING)` node. Smoke-fixture set:
  `examples/01_*.nsl`–`examples/20_simulation_tb.nsl` at T8
  merge (per Q4 → Option C); `test/audited/<project>/` is
  added once P-VEN lands.
- **Golden** (FR-015): `tree-sitter test`-style inline
  capture-assertions in fixtures under
  `test/tooling/treesitter/highlights/`. Coverage: ≥ 1
  occurrence per capture in the FR-007 required-minimum
  set (≥ 17 distinct capture assertions per SC-003).
- **Regenerate-and-diff** (FR-017): a CI sub-step runs
  `tree-sitter generate` from the committed `grammar.js` and
  asserts `git diff --exit-code` against the committed
  `parser.c` (and `grammar.json`, `node-types.json`).
- **Determinism** (SC-008): a CI sub-step runs
  `tree-sitter build-wasm` twice and asserts byte-identity
  via `sha256sum`.

**Target Platform**: any tree-sitter-aware editor — VS Code
(via the T8 extension shell), Neovim (native tree-sitter), Emacs
(`tree-sitter.el`), Helix, Zed. Fast path at T8 is **VS Code
only**; Neovim / Emacs / Sublime wiring is **T11**.

**Project Type**: tooling artefact (in-tree, with one build-output
WASM published as a CI workflow artefact + tagged-release
attachment).

**Performance Goals**: SC-006 — smoke + golden tests complete in
≤ 60 s on a standard CI runner. (Realistically: smoke against
20 example files at ~5kB average parses in <1s using the
generated C parser; the dominant cost is `tree-sitter generate`
on cold cache, ~2-5s.)

**Constraints**:
- Zero compiler dependency (FR-016). Verified by running tests
  on a clean checkout that has not built `nslc`.
- Zero compiled-binary commit (preserves T1 SC-005 norm). The
  WASM is a CI workflow artefact, not a committed file (per
  Q2 → Option C).
- `parser.c` is committed source-derived C — analogous to
  bison/flex output that's traditionally committed in C
  ecosystems for ease of consumption. CI gates byte-equality
  via FR-017's regenerate-and-diff step.
- No `nvim-treesitter` upstream PR (FR-021 / deferred per
  T1/T12 deferral note).
- No VS Code Marketplace publication (FR-021 / deferred to T12,
  itself deferred).
- LSP `semanticTokens` (T4) overrides tree-sitter where both
  apply (FR-018 / `nsl_tooling_design.md §4` three-tier
  precedence).

**Scale/Scope**: NSL grammar productions §§1–11 (~50 rules);
preprocessor directives `pp.ebnf §2` (9 directives modelled as
top-level items per `nsl_tooling_design.md §4.3`); `%IDENT%`
escape-hatch in identifier and expression positions; 42
reserved keywords (current `KeywordSet.def` count, generated);
17 distinct highlight captures (12 §4.3 base + 8 sub-captures −
3 base captures whose semantic role is taken over by sub-captures
= 17 net); smoke-fixture corpus 20 files (`examples/*.nsl`,
~3,000 LOC total); golden fixture estimated ≤ 200 lines NSL plus
parallel inline capture assertions; total in-tree footprint:
- `grammar.js` ~600–800 lines
- generated `parser.c` ~30,000–60,000 lines (machine-generated,
  not human-reviewed)
- `queries/highlights.scm` ~80 lines
- VS Code extension shell ~200 lines TS/JS + a thin
  `package.json`
- fixture corpus uses existing `examples/*.nsl` (no new content)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

Per the project Constitution
(`.specify/memory/constitution.md` v1.7.0), the nine principles
apply as follows. Status legend: **PASS** — satisfied by
construction or by an explicit plan artefact; **N/A** —
principle does not apply to this layer.

| # | Principle | Status | Notes |
|---|---|---|---|
| I | Spec Is Authoritative | **PASS** | T8's `grammar.js` mirrors `nsl_lang.ebnf §§1–11` and `nsl_pp.ebnf §2`. The keyword block is generated from `KeywordSet.def` (same single-source-of-truth as T1). The "no silent AST drops" sub-clause does not apply (T8's tree-sitter parse tree is not the compiler AST; it's a separate parse tree consumed only by the highlighter — the compiler's M-track AST is the authoritative one). Tree-sitter does emit `(ERROR)` nodes for shape mismatches, which the smoke test asserts against. |
| II | Layered Library Architecture | **PASS** (scoped exception) | T8 introduces a *parallel parser* outside `libNSLFrontend.a`. This is the same scoped exception that applied to T1: the highlighter tier (`nsl_tooling_design.md §4`) is the *one* tier in the project that re-implements parsing, because the highlighter ecosystem (TextMate at T1, tree-sitter at T8) demands its own parser-input formats. The exception is bounded: lint (T2/T6), formatter (T2/T5), LSP (T3+) all remain pinned to the C++ front-end library. The scope of the exception is documented in spec.md FR-016 / FR-018 / FR-019 / FR-020 and Assumptions. |
| III | Stock CIRCT Below `nsl` Dialect | **N/A** | T8 is upstream of the entire compiler stack; no CIRCT involvement. |
| IV | Source-Locating Diagnostics | **N/A** | Tree-sitter runtime emits no user-facing diagnostics; the smoke test's `(ERROR)` node messages are runner output. The VS Code extension shell does not emit diagnostics — diagnostics are owned by T3 LSP. |
| V | Inspectable, Deterministic Pipeline | **PASS** | (a) `grammar.js` → `parser.c` is deterministic when `tree-sitter-cli` is pinned to a specific minor version (Clarifications Q1 → Option B); FR-017 regenerate-and-diff CI gate enforces. (b) `parser.c` → `tree-sitter-nsl.wasm` is deterministic when both `tree-sitter-cli` and `emscripten` are pinned; SC-008 CI assertion enforces by `sha256sum` two consecutive builds. (c) The generator script `scripts/gen_treesitter_grammar.py` follows the source-order iteration pattern established by `scripts/gen_textmate_grammar.py`. The "input is positively defined" clause: source files are `grammar.js` + `KeywordSet.def`; CLI flags are pinned-version invocations of `tree-sitter generate` and `build-wasm`. Build environment (CWD, mtime, hostname) is NOT input — Constitution v1.7.0 Principle V wording. |
| VI | Layered Test Discipline | **PASS** | T8 introduces a tree-sitter test layer (new under T-track). The Principle VI per-layer accepted-driver list enumerates compiler-test drivers — non-compiler tooling tests are out of that list's scope by construction (same precedent set by T1's introduction of `vscode-tmgrammar-test`). The new layer follows the same principle: test-first; per-feature fixture; observed-failing first. End-to-end audited-corpus regression (the seven projects) is **read-only** at T8 — T8 adds the corpus to its smoke-parse list once P-VEN lands but does not modify the corpus. The closed-list rule is preserved. |
| VII | Spec ↔ Design Coupling | **PASS** | `grammar.js`'s keyword block is generated from `KeywordSet.def`; the production-rule structure mirrors `nsl_lang.ebnf §§1–11` and `nsl_tooling_design.md §4.3`. CI gates a regenerate-and-diff check (FR-017), so a `KeywordSet.def` edit that lands without a grammar regeneration fails CI. The roll-up table in `CLAUDE.md` §2.4 already names T8 ("Tree-sitter grammar + highlight queries; VS Code WASM consumer"); no edit needed there. The CLAUDE.md §2.5 editor-integration matrix already lists VS Code's tree-sitter row at T8 ("✅ via extension using wasm tree-sitter"); no edit needed. |
| VIII | Test-First Development | **PASS** | The `tasks.md` ordering (delivered by `/speckit-tasks`) will require: (a) the tree-sitter CLI pin + Node toolchain landing first as a pure infra step, (b) the smoke-fixture wiring landing before `grammar.js` (observed failing because no grammar exists), (c) the golden fixture landing before `highlights.scm` (observed failing because no captures emit), (d) `grammar.js` + `parser.c` landing third (smoke turns green), (e) `highlights.scm` landing fourth (golden turns green), (f) the VS Code extension shell landing last (manual smoke of US3.1). The XFAIL-then-green progression is recorded in PR history (or in the PR description if squash-merged) per the no-retrofitted-tests clause. |
| IX | Continuous Integration & Delivery | **PASS** | The smoke + golden + regenerate-and-diff + WASM-determinism sub-steps integrate into `./scripts/ci.sh` stage 3 (unit & layer tests) and stage 2 (static checks — for the regenerate-and-diff drift gate). Each sub-step exits non-zero on failure. SPDX headers for the JSON / JS artefacts use the `_comment_top` JSON-key convention (precedent: T1 + `.github/branch-protection.json`) for JSON files; `// SPDX-License-Identifier:` line-1 comment for `grammar.js`, `highlights.scm`, the Python generator, and the VS Code extension TS/JS files. The Constitution's "Release artifacts" clause (no human-built artefacts) is honoured by uploading `tree-sitter-nsl.wasm` from CI builds only. |

**No Constitution violations.** The Principle II scoped exception
extends an existing precedent (T1's TextMate grammar), with the
boundary explicitly documented in spec.md. No `Complexity Tracking`
entries needed.

**Re-check after Phase 1 design** (2026-05-05): performed
against the generated `data-model.md`, `contracts/`
(grammar-coverage / highlights-coverage / vscode-extension),
and `quickstart.md`. No new violations surfaced. Two design
artefacts that warrant explicit Constitution re-grounding:

- **`SPDX.NOTICE` for generated `parser.c` /
  `grammar.json` / `node-types.json`** (research.md §9):
  Constitution Build/Code/Licensing standards require an
  SPDX header on every new file "in the comment syntax
  appropriate to the file format". For CLI-generated C and
  JSON files where injecting a header would break the
  regenerate-and-diff gate (FR-017), the project precedent
  is a sibling NOTICE file (`scripts/spdx_exceptions.txt`
  already lists similar exempted paths). Implementation step
  amends `scripts/check_spdx.py` and
  `scripts/spdx_exceptions.txt` to add the three generated
  paths. **PASS** — extends an existing precedent.
- **VS Code extension's manual smoke-test attestation** for
  AS3.1–AS3.3 (`contracts/vscode-extension.contract.md` §5):
  Constitution Principle VIII (TDD) requires tests to be
  observed failing first. Manual smoke-tests for VS Code
  extension activation are recorded in PR description
  (T1 set the same precedent for VS Code-side acceptance).
  AS3.4 (WASM determinism) is a CI assertion, not manual.
  **PASS** — three of four AS3.x are manual attestation
  (precedent-aligned with T1); AS3.4 is automated.

No `Complexity Tracking` entries needed.

## Project Structure

### Documentation (this feature)

```text
specs/010-t8-tree-sitter-grammar/
├── spec.md                                    # /speckit-specify output
├── plan.md                                    # this file (/speckit-plan output)
├── research.md                                # Phase 0 (/speckit-plan)
├── data-model.md                              # Phase 1 (/speckit-plan)
├── quickstart.md                              # Phase 1 (/speckit-plan)
├── contracts/
│   ├── grammar-coverage.contract.md           # Phase 1 — frozen rule-coverage table
│   ├── highlights-coverage.contract.md        # Phase 1 — frozen capture-name set
│   └── vscode-extension.contract.md           # Phase 1 — VS Code extension activation + provider
├── checklists/
│   └── requirements.md                        # /speckit-specify quality gate
└── tasks.md                                   # /speckit-tasks output (NOT this command)
```

### Source code (repository root)

```text
grammars/                                       # EXISTING (T1) — extended at T8
├── textmate/                                   # T1 (existing)
│   └── nsl.tmLanguage.json
└── treesitter/                                 # NEW at T8
    ├── package.json                            # pins tree-sitter-cli@0.22.x in devDependencies
    ├── package-lock.json                       # CI-installed lockfile (committed)
    ├── grammar.js                              # NEW — primary T8 source artefact
    ├── src/                                    # tree-sitter 0.22 generator output dir
    │   ├── parser.c                            # GENERATED & COMMITTED — output of `tree-sitter generate --no-bindings`
    │   ├── grammar.json                        # GENERATED & COMMITTED
    │   ├── node-types.json                     # GENERATED & COMMITTED
    │   └── tree_sitter/                        # bundled tree-sitter ABI headers (upstream content; SPDX-exempted-via-NOTICE)
    │       ├── alloc.h
    │       ├── array.h
    │       └── parser.h
    └── queries/
        ├── highlights.scm                      # NEW — primary highlight artefact
        └── locals.scm                          # NEW — scope/definition/reference queries for reference-site capture resolution; tree-sitter-cli's `tree-sitter test` does NOT propagate `@local.definition.<NAME>` to `@local.reference` sites (that's an editor-runtime feature in some consumers, not standard) — consumed by `editors/vscode/treesitter/highlight-provider.ts` at runtime

editors/                                        # EXISTING (T1) — extended at T8
└── vscode/                                     # T1 (existing — language-configuration.json, syntaxes/)
    ├── package.json                            # AMEND — register treesitter activation event
    ├── language-configuration.json             # T1 (existing)
    ├── syntaxes/
    │   └── nsl.tmLanguage.json                 # T1 (existing materialised copy)
    └── treesitter/                             # NEW subdirectory at T8
        ├── extension.ts                        # NEW — wires web-tree-sitter to VS Code
        └── highlight-provider.ts               # NEW — applies highlights.scm

scripts/
├── gen_treesitter_grammar.py                   # NEW — generates keyword block of grammar.js from KeywordSet.def
├── gen_textmate_grammar.py                     # T1 (existing — precedent for the new generator)
├── gen_keyword_fixtures.py                     # EXISTING — precedent
└── ci.sh                                       # AMEND — stage 2 gains treesitter regenerate-and-diff sub-step + WASM-determinism sub-step; stage 3 gains tree-sitter smoke + golden runner sub-step

test/
└── tooling/                                    # EXISTING tree (T1)
    ├── lit.local.cfg.py                        # T1 (existing — disables lit on this subtree)
    ├── textmate/                               # T1 (existing)
    │   ├── fixtures/
    │   ├── package.json
    │   └── package-lock.json
    └── treesitter/                             # NEW directory tree at T8
        ├── smoke/
        │   └── corpus.txt                      # one path per line — references examples/*.nsl
        └── highlights/
            ├── reg_vs_wire.nsl                 # tree-sitter-test format with inline (capture: ...) assertions
            ├── proc_vs_func.nsl
            ├── state_goto.nsl
            ├── control_terminal_s27.nsl
            ├── macro_splice_ident.nsl
            └── parser_note_disambiguation.nsl  # exercises N5/N2/N3/N6

examples/                                       # EXISTING (M-track) — read-only smoke-fixture source
├── 01_hello.nsl                                # 20 files curated by an earlier milestone
├── …
└── 20_simulation_tb.nsl

include/nsl/Lex/KeywordSet.def                  # EXISTING — single source of truth; T8 consumes (same as T1)

.github/workflows/                              # AMEND
└── ci.yml                                       # gain a "Build & upload tree-sitter-nsl.wasm" step that uploads as a workflow artefact (and attaches to release on tagged builds)
```

**Structure Decision**: `nsl_tooling_design.md §8` shared
directory layout names `grammars/` and `editors/` as the
canonical homes; T1 established `grammars/textmate/` and
`editors/vscode/`. T8 honours the same parent and introduces
**peer subdirectories**: `grammars/treesitter/` and
`editors/vscode/treesitter/`. This keeps the T1/T8
separation clean (each tier has its own subtree) while
avoiding a top-level new directory. The
`grammars/treesitter/package.json` + `package-lock.json` pair
is what pins `tree-sitter-cli` per Clarifications Q1 → Option
B; CI installs from the lockfile, mirroring how
`test/tooling/textmate/package.json` pins
`vscode-tmgrammar-test` for T1.

The smoke-fixture set (Q4 → Option C) is referenced via a
`test/tooling/treesitter/smoke/corpus.txt` line-list rather
than copied. This (a) avoids duplicating ~3,000 LOC of NSL,
(b) lets the M-track keep `examples/*.nsl` as its own
source of truth, and (c) means an `examples/` addition that
the T8 grammar can't parse fails CI immediately on the next
PR — coupling Principle VII to the example corpus too. When
P-VEN lands at M7, `corpus.txt` gains
`test/audited/<project>/` entries.

WASM artefact lifecycle (Q2 → Option C): CI builds in stage 2's
new "Build WASM" sub-step, asserts byte-identity (SC-008), and
uploads via `actions/upload-artifact@v4`. Tagged releases
attach via `softprops/action-gh-release` from the same job.
**The WASM is not committed.**

## Complexity Tracking

> Empty — Constitution Check passes on all 9 principles. The
> Principle II scoped exception (parallel parser in the
> highlighter tier) is a *narrowing* of an existing precedent
> set by T1, not a new violation. The boundary is documented
> in spec.md (FR-016 / FR-018 / FR-019 / FR-020) and in the
> Constitution Check table above.

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| _none_    | _none_     | _none_                              |
