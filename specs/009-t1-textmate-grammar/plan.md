# Implementation Plan: T1 — TextMate Grammar + Language Configuration for NSL

**Branch**: `009-t1-textmate-grammar` | **Date**: 2026-05-04 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/009-t1-textmate-grammar/spec.md`

## Summary

Deliver tooling-track milestone **T1** — the first user-visible NSL
tooling deliverable, gating only on the spec (no compiler dependency).
T1 ships two JSON artefacts plus a scope-test fixture corpus:

1. **`grammars/textmate/nsl.tmLanguage.json`** — TextMate grammar
   covering every reserved keyword from `nsl_lang.ebnf §15`, every
   numeric form from `§13`, every comment / string / operator form
   from `§14` and `nsl_tooling_design.md §4.1`, every preprocessor
   directive from `nsl_pp.ebnf §2`, and the `%IDENT%` macro form
   from `§4`. Scope name is `source.nsl`; file extensions are
   `.nsl`, `.nslh`, `.inc`.
2. **`editors/vscode/language-configuration.json`** — VS Code editor
   metadata (comment toggles, bracket pairs, auto-close pairs,
   surround pairs, word pattern, indent rules) plus a minimal
   `package.json` extension-shell that points VS Code at both files.
3. **`test/tooling/textmate/`** — scope-test fixture corpus and
   assertion files consumed by an automated runner integrated into
   `./scripts/ci.sh` stage 3.

**Technical approach**: the grammar is **generated** from
`include/nsl/Lex/KeywordSet.def` (the established single-source-of-
truth, also consumed by the lexer and `scripts/gen_keyword_fixtures.py`)
plus a category-mapping table held in a new
`scripts/gen_textmate_grammar.py`. This makes Principle VII
spec ↔ design coupling **mechanical**: when `KeywordSet.def` gains a
keyword, regenerating the grammar in the same PR is a one-line
script invocation, and CI fails any PR where the generated grammar
diverges from `KeywordSet.def`. Hand-authored regions (operators,
literals, comments, preprocessor directives, `%IDENT%`) live in the
generator's category-mapping table; the keyword block is reproduced
mechanically from the X-macro file. Same pattern for the scope-test
fixtures (a parallel `scripts/gen_textmate_fixtures.py` extends the
existing `gen_keyword_fixtures.py` precedent).

Principle II is **trivially satisfied** — T1 has no parser at all
(TextMate is regex-only), so the no-duplication rule has nothing to
duplicate. Principle V (deterministic) is satisfied by the
generator's source-order iteration matching the existing
`gen_keyword_fixtures.py` precedent. Principle VIII (TDD) is
satisfied by the fixtures landing first (observed failing on the
unchanged tree because there is no grammar yet to assign scopes to
them) and the grammar landing second.

## Technical Context

**Language/Version**: JSON (TextMate grammar dialect — Oniguruma
regex flavor; VS Code language-configuration dialect — VS Code
schema). Generator scripts in Python 3 (matches existing
`scripts/gen_*_fixtures.py` precedent).

**Primary Dependencies**:
- **`vscode-tmgrammar-test`** — Node.js npm package; the de-facto
  standard scope-test runner for TextMate grammars (used by
  Microsoft's own `vscode/extensions/*-basics` packages). See
  research.md §1 for the rejected alternatives.
- **Python 3** — already required by the project per existing
  `scripts/gen_*_fixtures.py` and `scripts/check_spdx.py` precedent;
  no new dependency.
- **`jq`** — lightweight JSON validator for the build-time
  consistency check between `grammars/textmate/nsl.tmLanguage.json`
  and `editors/vscode/syntaxes/nsl.tmLanguage.json` (existing
  toolchain entry).

**Storage**: N/A — pure static JSON files plus generator scripts.

**Testing**:
- **Layer**: TextMate scope tests — a new test layer specifically
  for tooling-track JSON artefacts. Per Constitution Principle VI's
  per-layer accepted-driver list, the compiler-test driver
  enumeration (lit + FileCheck for lowering / E2E; gtest / nsl-opt
  for unit / dialect) does not constrain non-compiler tooling tests;
  `vscode-tmgrammar-test` is the conventional driver for this layer.
- **Format**: assertion files use `vscode-tmgrammar-test`'s `// >`
  syntax — each assertion line points back at a fixture line and
  asserts the expected scope at a specific column range.
- **Coverage**: ≥ 1 occurrence per reserved keyword (≥ 42 distinct
  keywords as of 2026-05-04 per `KeywordSet.def`), ≥ 1 per numeric
  form (≥ 8 forms), ≥ 1 per directive form (9 directives), ≥ 1 per
  operator category (8 categories), ≥ 1 macro reference, ≥ 1 line
  comment, ≥ 1 block comment, ≥ 1 string literal with escape.

**Target Platform**: any TextMate-compatible editor (VS Code,
Sublime Text 4, Atom, GitHub web, TextMate itself). Fast path:
VS Code via `editors/vscode/`.

**Project Type**: tooling artefact (in-tree, no compiled output).

**Performance Goals**: SC-006 — scope tests complete in ≤ 10 s on a
standard CI runner. (Realistically a few seconds — the runner
parses ~50 KB of JSON, applies regex to a few hundred lines of
fixture, checks ~100 assertions.)

**Constraints**:
- Zero compiler dependency (FR-019). Verified by running tests on
  a clean checkout that has not built `nslc`.
- Zero compiled-binary contribution (SC-005). All artefacts are
  source-controlled JSON / Python.
- Total package size ≤ 50 KB (SC-005).
- No `github-linguist/linguist` PR (FR-022 / deferred).
- No VS Code Marketplace publication (FR-022 / deferred to T12,
  which is itself deferred).

**Scale/Scope**: 42 reserved keywords (current `KeywordSet.def`
count); 11 system functions + 2 system variables (`HelperSet.def`
boundary applies — see research.md §3); 9 preprocessor directives
(`nsl_pp.ebnf §2`); 1 macro form (`%IDENT%`); 8 numeric forms;
~ 25 distinct operator tokens grouped into 8 categories. Fixture
corpus expected ≤ 200 lines NSL plus parallel assertion files;
total in-tree footprint ≤ 50 KB JSON + ≤ 30 KB fixtures.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

Per the project Constitution (`.specify/memory/constitution.md`
v1.7.0), the nine principles apply as follows. Status legend:
**PASS** — satisfied by construction or by an explicit plan
artefact; **N/A** — principle does not apply to this layer.

| # | Principle | Status | Notes |
|---|---|---|---|
| I | Spec Is Authoritative | **PASS** | T1 mirrors `nsl_lang.ebnf §15` mechanically via `KeywordSet.def` consumption. The "no silent AST drops" sub-clause does not apply (no parser). |
| II | Layered Library Architecture | **PASS** (trivially) | T1 has no C++ library, no parser, no AST — the no-duplication rule has nothing to duplicate. The deliverable is JSON + Python generators only. |
| III | Stock CIRCT Below `nsl` Dialect | **N/A** | T1 is upstream of the entire compiler stack; no CIRCT involvement. |
| IV | Source-Locating Diagnostics | **N/A** | TextMate runtime emits no diagnostics. Scope-test failure messages are runner output, not user-facing diagnostics. |
| V | Inspectable, Deterministic Pipeline | **PASS** | TextMate grammar is regex; deterministic by definition. Generator follows the established `gen_keyword_fixtures.py` source-order iteration pattern (Principle V wording quoted in `gen_keyword_fixtures.py` head comment). |
| VI | Layered Test Discipline | **PASS** | T1 introduces a new test layer (TextMate scope tests). The Principle VI per-layer accepted-driver list enumerates compiler-test drivers — non-compiler tooling tests are out of that list's scope by construction. The new layer follows the same principle (test-first; per-feature fixture; observed-failing first). |
| VII | Spec ↔ Design Coupling | **PASS** | The grammar's keyword block is generated from `KeywordSet.def`; the grammar's structure mirrors `nsl_tooling_design.md §4.1` token categories. CI gates a regenerate-and-diff check, so a `KeywordSet.def` edit that lands without a grammar regenerate fails CI. The roll-up table in `CLAUDE.md` §2.4 already names T1; no edit needed there. |
| VIII | Test-First Development | **PASS** | The `tasks.md` ordering will require fixture corpus + assertion files to land first (observed failing because no grammar exists), then the generator + grammar JSON, then re-run with all assertions green. The XFAIL-then-green progression is recorded in PR history (or in the PR description if squash-merged) per the no-retrofitted-tests clause. |
| IX | Continuous Integration & Delivery | **PASS** | The scope-test runner integrates into `./scripts/ci.sh` stage 3 (unit & layer tests) — see research.md §4. The runner exits non-zero on any assertion failure. SPDX headers for the JSON artefacts use the `_comment_top` JSON-key convention precedented by `.github/branch-protection.json` (existing in-tree practice). The Python generators carry `# SPDX-…` line-1 comments per existing `scripts/gen_*_fixtures.py` precedent. |

**No Constitution violations.** No `Complexity Tracking` entries
needed.

**Re-check after Phase 1 design**: pending; perform after generating
data-model, contracts, and quickstart.

## Project Structure

### Documentation (this feature)

```text
specs/009-t1-textmate-grammar/
├── spec.md                                    # /speckit-specify output
├── plan.md                                    # this file (/speckit-plan output)
├── research.md                                # Phase 0 (/speckit-plan)
├── data-model.md                              # Phase 1 (/speckit-plan)
├── quickstart.md                              # Phase 1 (/speckit-plan)
├── contracts/
│   ├── grammar-coverage.contract.md           # Phase 1 — frozen scope-binding table
│   ├── language-config.contract.md            # Phase 1 — VS Code language-config field set
│   └── scope-test-format.contract.md          # Phase 1 — runner assertion syntax
├── checklists/
│   └── requirements.md                        # /speckit-specify quality gate
└── tasks.md                                   # /speckit-tasks output (NOT this command)
```

### Source code (repository root)

```text
grammars/                                       # NEW directory; per nsl_tooling_design.md §8
└── textmate/
    └── nsl.tmLanguage.json                    # canonical generated artefact

editors/                                        # NEW directory
└── vscode/
    ├── package.json                           # minimal VS Code extension manifest
    ├── language-configuration.json            # comment / brackets / autoclose / indent
    └── syntaxes/
        └── nsl.tmLanguage.json                # symlink → ../../../grammars/textmate/nsl.tmLanguage.json

scripts/
├── gen_textmate_grammar.py                    # NEW — generates grammar from KeywordSet.def + category map
├── gen_textmate_fixtures.py                   # NEW — generates scope-test fixtures from KeywordSet.def
└── ci.sh                                       # AMEND — stage 3 gains a tooling-tests sub-step

test/
└── tooling/                                    # NEW directory tree
    └── textmate/
        ├── fixtures/
        │   ├── all-keywords.nsl                # generated; one occurrence per KeywordSet.def entry
        │   ├── all-numbers.nsl                 # hand-authored; bare/0x/0b/0o + Verilog-sized × b/o/d/h
        │   ├── all-operators.nsl               # hand-authored; one per operator category
        │   ├── all-directives.nsl              # hand-authored; one line per pp.ebnf §2 directive
        │   ├── comments-and-strings.nsl        # hand-authored; line/block comments + strings + escapes
        │   └── macro-references.nsl            # hand-authored; %IDENT% in expression position
        └── scope-tests/
            ├── all-keywords.spec               # generated assertions
            ├── all-numbers.spec
            ├── all-operators.spec
            ├── all-directives.spec
            ├── comments-and-strings.spec
            └── macro-references.spec

include/nsl/Lex/KeywordSet.def                 # EXISTING — single source of truth; T1 consumes
```

**Structure Decision**: `nsl_tooling_design.md §8` shared directory
layout names `grammars/textmate/` and `editors/vscode/` as the
canonical homes; this plan honours that. Symlink-vs-copy in
`editors/vscode/syntaxes/` is resolved as **symlink** with a
`scripts/ci.sh` build-step fallback that materialises a copy in
non-symlink-friendly environments (Windows zip extraction); see
research.md §5 for the alternatives evaluated. The
`test/tooling/` tree is new — it parallels existing
`test/lex/keywords/` (per `gen_keyword_fixtures.py` output) but
does NOT live under `test/lex/` because TextMate scope assertion
is not lexer testing — different driver, different artefact under
test, different CI cell.

## Complexity Tracking

> Empty — Constitution Check passes on all 9 principles with no
> deviations.

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| _none_    | _none_     | _none_                              |
