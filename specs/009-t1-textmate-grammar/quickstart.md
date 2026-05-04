# Quickstart — T1 TextMate Grammar + Language Configuration

**Feature**: `009-t1-textmate-grammar`
**Phase**: 1 (design / quickstart)
**Date**: 2026-05-04

This walks through the three flows a contributor will exercise on
T1: (1) install locally for visual sanity-check, (2) run the
scope tests (the test gate), (3) add a new keyword end-to-end.
It assumes the T1 PR has landed and the tree contains
`grammars/textmate/`, `editors/vscode/`, and
`test/tooling/textmate/`.

All commands execute on the host (no `nslc` build needed for any
of them). The dev container `ghcr.io/koyamanx/nsl-nslc:dev` is
**not required** for T1 — the only runtime is Node.js for the
scope-test runner.

---

## §1 Prerequisites

- Node.js ≥ 16 (anything VS Code's TypeScript projects support;
  the dev container ships ≥ 18).
- Python 3 (for the generator scripts; ships in the dev container
  and on virtually every Linux host).
- VS Code (any recent version, ≥ 1.70.0 per
  `editors/vscode/package.json`'s `engines.vscode`).

No NSL compiler build needed.

---

## §2 Install the grammar locally in VS Code (≤ 60 s — SC-004)

The fastest path: drop the extension folder into VS Code's
extensions directory.

### Linux / macOS

```bash
# from repo root
ln -s "$PWD/editors/vscode" ~/.vscode/extensions/nsl-0.1.0
```

### Windows (PowerShell)

```powershell
# from repo root
Copy-Item -Recurse editors\vscode "$env:USERPROFILE\.vscode\extensions\nsl-0.1.0"
```
*(Windows requires copy because symlink creation needs admin or
Developer Mode.)*

Restart VS Code. Open any `.nsl` file from the audited corpus
(or a hand-written one); keywords / numbers / strings / comments
should render under their respective scopes.

### Verify activation

```
:NSL Language Support     ←  open VS Code's extension panel; T1's
                              extension entry appears with the
                              version 0.1.0 and "Programming
                              Languages" category.
```

In the Command Palette: `Developer: Inspect Editor Tokens and
Scopes`. Click on a `module` keyword in the editor; the panel
shows:

```
foreground   #569CD6  (or your theme's mapping)
TextMate scope     keyword.declaration.nsl
                   source.nsl
```

If the scope reads `source.nsl` only (no `keyword.…` prefix), the
grammar didn't load — verify the symlink/copy and the
`editors/vscode/package.json` `contributes.grammars[0].path`.

---

## §3 Run the scope tests (the test gate — FR-016/017/018)

The scope tests are the test gate for T1. They run under
`./scripts/ci.sh` stage 3 inside CI; locally they invoke a
single npm-based runner.

### One-shot run

```bash
./scripts/ci.sh tooling-textmate
```

Expected output (≤ 10 s — SC-006):

```
[ci.sh] tooling-textmate
[ci.sh] running vscode-tmgrammar-test on test/tooling/textmate/scope-tests/*.spec
✓ all-keywords.spec    (42 assertions)
✓ all-numbers.spec     (12 assertions)
✓ all-operators.spec   (8 categories, 23 tokens)
✓ all-directives.spec  (9 directives)
✓ comments-and-strings.spec  (7 assertions, including embedded-keyword cases)
✓ macro-references.spec      (2 assertions)
[ci.sh] PASS — 93 assertions passed in 6 fixtures
```

Non-zero exit → at least one assertion failed; the runner prints
the failing assertion's file/line/column and observed-vs-expected
scope (per `contracts/scope-test-format.contract.md` §5).

### Watch-mode (during development)

```bash
cd editors/vscode  # or wherever node_modules is installed
npx vscode-tmgrammar-test \
  --grammar ../../grammars/textmate/nsl.tmLanguage.json \
  --tests   "../../test/tooling/textmate/scope-tests/*.spec" \
  --watch
```

Re-runs on every grammar / fixture / spec edit.

---

## §4 Add a new reserved keyword (worked example — FR-001 / SC-003)

Suppose `nsl_lang.ebnf §15` gains a new keyword `pipeline` (a
hypothetical extension). The full chain to land it:

### Step 1 — spec edit

```bash
# edit docs/spec/nsl_lang.ebnf §15 — add `"pipeline"` in the
# practical-additions group between `parameter` and `return`,
# keeping the alphabetical order of that group's lines.
$EDITOR docs/spec/nsl_lang.ebnf
```

### Step 2 — `KeywordSet.def`

```bash
# edit include/nsl/Lex/KeywordSet.def — add the matching
# KEYWORD(pipeline, "pipeline") line in the same group as the
# spec edit (practical-additions).
$EDITOR include/nsl/Lex/KeywordSet.def
```

### Step 3 — category-mapping (T1 introduces; the **only** T1
specific edit at this step)

Add the new keyword to the appropriate category in
`scripts/gen_textmate_grammar.py`'s category map. For
`pipeline` (a hypothetical control-block), it belongs under
`keyword.control.block.nsl`:

```python
# scripts/gen_textmate_grammar.py
KEYWORD_CATEGORY = {
    'declare':   'declaration',
    'module':    'declaration',
    # ...
    'pipeline':  'control_block',  # <-- new
    # ...
}
```

If you forget this step, the script raises:

```
RuntimeError: KeywordSet.def has 'pipeline' but no category-mapping
entry. Add `'pipeline': '<category>'` to KEYWORD_CATEGORY in
scripts/gen_textmate_grammar.py per data-model.md §1.2.
```

### Step 4 — regenerate

```bash
python3 scripts/gen_textmate_grammar.py
python3 scripts/gen_textmate_fixtures.py
python3 scripts/gen_keyword_fixtures.py   # also regenerate the lex fixtures (existing precedent)
```

`git diff` should show edits in:

- `include/nsl/Lex/KeywordSet.def` (manual)
- `docs/spec/nsl_lang.ebnf` (manual)
- `scripts/gen_textmate_grammar.py` (manual — the category-map line)
- `grammars/textmate/nsl.tmLanguage.json` (regenerated)
- `test/tooling/textmate/fixtures/all-keywords.nsl` (regenerated)
- `test/tooling/textmate/scope-tests/all-keywords.spec` (regenerated, if
  the runner config schema changes; usually unchanged)
- `test/lex/keywords/pipeline.test` (regenerated by
  `gen_keyword_fixtures.py` — existing M1 precedent)

### Step 5 — propagate per Principle VII (CLAUDE.md / docs)

Per Principle VII the spec amendment also needs:

- `docs/CLAUDE.md` §5 quick-map — add a row for `pipeline` if
  it introduces a new `Sn` / `Nn`. (For a plain keyword
  addition without a constraint, no quick-map row is needed.)
- `CLAUDE.md` §1 (project-root) language-feature roll-up —
  add a row noting which milestone delivers the new keyword
  through which layers.
- `docs/design/nsl_compiler_design.md` cross-references — only
  if the new keyword changes a section line range or design
  point.

### Step 6 — TDD discipline (Principle VIII)

Run the scope tests **before** committing the grammar regeneration
and confirm the new fixture line FAILS:

```bash
# stash the grammar regeneration but keep the fixture regeneration
git checkout grammars/textmate/nsl.tmLanguage.json

./scripts/ci.sh tooling-textmate
# -> FAIL: assertion at all-keywords.nsl:43 expected
#    keyword.control.block.nsl, got (no scope)

# now restore the grammar
git checkout HEAD -- grammars/textmate/nsl.tmLanguage.json   # or re-run gen
python3 scripts/gen_textmate_grammar.py

./scripts/ci.sh tooling-textmate
# -> PASS
```

The PR description should record both runs (or, with squash-merge,
the failing-state commit hash before squash) per Principle VIII's
no-retrofitted-tests clause.

### Step 7 — commit + PR

Single PR carrying:
- spec edit
- `KeywordSet.def` edit
- category-map edit
- all regenerated artefacts
- `Linear: NSLC-<N>` trailer (per Constitution External
  Integrations § Linear)
- CodeRabbit pass before merge per Principle IX

---

## §5 Add a new operator (worked example — FR-008)

Operators are not in `KeywordSet.def`. The flow is shorter:

```bash
# 1. spec edit  — add to nsl_lang.ebnf §11 (Expressions) or §4.1
$EDITOR docs/spec/nsl_lang.ebnf
$EDITOR docs/design/nsl_tooling_design.md

# 2. grammar edit — hand-edit the operators category in
#    scripts/gen_textmate_grammar.py's inline JSON template
$EDITOR scripts/gen_textmate_grammar.py

# 3. fixture edit — hand-edit test/tooling/textmate/fixtures/all-operators.nsl
$EDITOR test/tooling/textmate/fixtures/all-operators.nsl

# 4. contract edit — record the new binding
$EDITOR specs/009-t1-textmate-grammar/contracts/grammar-coverage.contract.md

# 5. regenerate + verify
python3 scripts/gen_textmate_grammar.py
./scripts/ci.sh tooling-textmate
```

Same Principle VIII (test-first) and Principle VII (contract +
spec coupling) discipline applies.

---

## §6 Inspect the grammar without VS Code

```bash
python3 -m json.tool grammars/textmate/nsl.tmLanguage.json | less
```

The file is human-readable; categories are obvious from the
`patterns` and `repository` keys. Useful when reviewing a PR
that changes the grammar without VS Code at hand.

---

## §7 Common failure modes & fixes

| Symptom | Likely cause | Fix |
|---|---|---|
| Scope test fails with "(no scope)" on a new keyword line | Forgot to regenerate `nsl.tmLanguage.json` after `KeywordSet.def` edit | `python3 scripts/gen_textmate_grammar.py` and recommit. |
| `gen_textmate_grammar.py` raises "no category-mapping entry" | New `KeywordSet.def` row but `KEYWORD_CATEGORY` not updated | Add the new entry per `quickstart.md §4 step 3`. |
| Stage 2 (static-checks) fails with "git diff is non-empty after regeneration" | Generator output drifted from committed file | Re-run all three `gen_*.py` scripts; commit the result. |
| Stage 2 (SPDX header) fails on `nsl.tmLanguage.json` | Missing `_comment_top` SPDX key | Confirm the generator emits the SPDX-laden `_comment_top` first; if you hand-edited the JSON, restore it. |
| `Developer: Inspect Editor Tokens and Scopes` shows only `source.nsl` | Grammar didn't load in VS Code | Verify the `editors/vscode/syntaxes/nsl.tmLanguage.json` symlink/copy resolves; restart VS Code. |
| `npx vscode-tmgrammar-test` fails with "command not found" | Node not installed or not on PATH | Install Node ≥ 16 (`apt-get install nodejs` / dev-container default). The test runner is a single `npx`-resolvable npm package. |
| Verilog-sized literal `8'hFF` highlights as `8` then `'hFF` separately | Pattern ordering in the grammar — Verilog-sized must match before bare decimal | Confirm `gen_textmate_grammar.py` emits the literal patterns in the order specified by `data-model.md §1.4` (Verilog-sized first, decimal last). |

---

## §8 Next milestones

T1 hands off to two milestones:

- **T2** (`nsl-fmt`) — the formatter. Once T2 lands, VS Code's
  `formatting` capability (T5) will use the same NSL-specific
  rules; the language-configuration's `indentationRules` becomes
  the floor, not the ceiling.
- **T3** (`nsl-lsp` skeleton) — the LSP. Diagnostics, hover,
  definition will start populating; T1's "best-effort"
  disambiguations get accurate answers from semantic tokens.
- **T8** (tree-sitter grammar) — replaces / complements T1's
  TextMate grammar with semantic identifier scoping. T1's
  scope-name conventions (per `nsl_tooling_design.md §4.1` and
  `contracts/grammar-coverage.contract.md`) carry over so VS Code
  themes don't need to know which back-end produced the scope.
