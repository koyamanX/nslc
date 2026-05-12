<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Quickstart — T8 Tree-sitter Grammar + Highlight Queries + VS Code WASM Consumer

**Feature**: `010-t8-tree-sitter-grammar`
**Phase**: 1 (design / quickstart)
**Date**: 2026-05-05

This document is the contributor on-ramp. It documents the
3 core developer flows (author the grammar, add a capture,
install the VS Code extension shell) plus the 2 CI-side
operations (smoke + golden gates).

Audience: a contributor with the `ghcr.io/koyamanx/nsl-nslc:dev`
container running, working in a worktree of the project.

---

## 0. Prerequisites

- Dev container running (`./scripts/dev-container.sh` or your
  preferred entry point).
- Node.js + `npm` available inside the container (already
  shipped per T1's research.md §1 "Containerization
  implication").
- `docker` available inside the container (for
  `tree-sitter build-wasm --docker`).

The first time you work on T8 inside a fresh container:

```bash
cd grammars/treesitter
npm ci          # installs tree-sitter-cli@0.22.x per package-lock.json
```

This is the **single source of truth** for the
tree-sitter-cli version (Clarifications Q1 → Option B). Do
not invoke `tree-sitter` from a globally-installed CLI; use
`npx tree-sitter` so the pinned version is used.

---

## 1. Author / amend the grammar

The grammar lives in `grammars/treesitter/grammar.js`. Two
regions:

| Region | Origin |
|---|---|
| **Production rules** (§§1–11 of `nsl_lang.ebnf`, `pp.ebnf §2`) | hand-authored |
| **Keyword block** (token rule `_keyword`) | **GENERATED** by `python scripts/gen_treesitter_grammar.py` from `KeywordSet.def` |

### 1.1 Adding a new production

Suppose you're adding a new NSL grammar production (a hypothetical
example for illustration only — Principle I requires the spec
edit to land first):

1. Edit `docs/spec/nsl_lang.ebnf` to add the production (under
   the appropriate section).
2. Update `docs/CLAUDE.md` §5 quick-map and §§4–7 line ranges
   if section boundaries shifted (Principle VII coupling).
3. Update
   `specs/010-t8-tree-sitter-grammar/contracts/grammar-coverage.contract.md`
   §1 with the new production row.
4. Edit `grammars/treesitter/grammar.js` to add a matching
   rule. Cite the spec anchor (`// nsl_lang.ebnf §X / lines
   YYY-ZZZ`).
5. Regenerate `parser.c`:

   ```bash
   cd grammars/treesitter
   npx tree-sitter generate
   ```

6. Confirm the smoke gate still passes:

   ```bash
   npx tree-sitter parse $(cat ../../test/tooling/treesitter/smoke/corpus.txt | grep -v '^#')
   ```

7. If the new production needs a distinct highlight, see §2
   below.

### 1.2 Adding a reserved keyword

Adding a reserved keyword goes through `KeywordSet.def`,
not `grammar.js`:

1. Edit `include/nsl/Lex/KeywordSet.def` (the new keyword
   row).
2. Run the T8 generator:

   ```bash
   python scripts/gen_treesitter_grammar.py
   ```

   This refreshes the keyword block of
   `grammars/treesitter/grammar.js`.
3. Run the T1 generator (same single-source-of-truth):

   ```bash
   python scripts/gen_textmate_grammar.py
   ```

4. Regenerate `parser.c`:

   ```bash
   cd grammars/treesitter && npx tree-sitter generate
   ```

5. Add a fixture assertion to one of the
   `test/tooling/textmate/fixtures/*.nsl` (T1) AND to one of
   the `test/tooling/treesitter/highlights/*.nsl` (T8)
   golden fixtures.
6. CI's regenerate-and-diff sub-steps for both T1 and T8
   ratify the change.

This is the Principle VII coupling guarantee: a keyword edit
without grammar regeneration fails CI.

---

## 2. Add a highlight capture

The highlight queries live in
`grammars/treesitter/queries/highlights.scm`. Authoring is
straightforward S-expression:

```scheme
;; assign @variable.register to register declaration sites
(register_declaration name: (identifier) @variable.register)

;; AND to references that resolve to a register binding
((identifier) @variable.register
 (#has-ancestor? @variable.register register_declaration))
```

Steps to add a new capture:

1. Decide the capture name. If it's the FR-009 control-
   terminal capture, follow
   `contracts/highlights-coverage.contract.md` §2. Otherwise,
   it must extend an existing parent capture from the §4.3
   base set.
2. Add a row to
   `contracts/highlights-coverage.contract.md` §1.
3. Edit `queries/highlights.scm` with the new query rule.
4. Add a fixture assertion under
   `test/tooling/treesitter/highlights/`. Use the inline
   format:

   ```nsl
   reg counter[8];
   //  ^^^^^^^ @variable.register
   ```

5. Run the golden test:

   ```bash
   cd grammars/treesitter
   npx tree-sitter test
   ```

6. CI's `treesitter-highlights-golden` sub-step runs the
   same command.

---

## 3. Install the VS Code extension shell locally

Three install paths, ordered by speed.

### 3.1 Folder-drop install via CI workflow artefact (recommended)

1. Visit the latest green CI run on the project's GitHub
   Actions page.
2. Download the `tree-sitter-nsl-wasm` workflow artefact.
3. Unzip; copy `tree-sitter-nsl.wasm` into
   `editors/vscode/treesitter/`.
4. Folder-drop the `editors/vscode/` directory into
   `~/.vscode/extensions/nsl-tooling-vscode-0.x.y/`
   (rename as appropriate to your local install).
5. Reload VS Code window (`Developer: Reload Window` from
   the command palette).
6. Open `examples/03_register.nsl` and confirm:
   - keywords coloured (T1 base layer)
   - register identifier `q` and wire identifier `w`
     coloured *distinctly* (T8 override)
   - no extension-host errors in `Help > Toggle
     Developer Tools > Console`.

This is the path SC-007 measures against ("under 5 minutes").

### 3.2 Folder-drop install via locally-built WASM

For contributors with the toolchain set up locally:

```bash
cd grammars/treesitter
npx tree-sitter build-wasm --docker
cp tree-sitter-nsl.wasm ../../editors/vscode/treesitter/
```

Then steps 4–6 from §3.1.

### 3.3 Tagged-release install

When tagged releases exist (paused per the deferral note;
will resume post-T12), download the `tree-sitter-nsl.wasm`
release attachment from the GitHub release page. Then
steps 4–6 from §3.1.

---

## 4. Run the test gates locally

Run all T8 CI sub-steps locally before pushing:

```bash
cd grammars/treesitter

# Stage 2: regenerate-and-diff
npx tree-sitter generate
git diff --exit-code parser.c grammar.json node-types.json

# Stage 2: WASM determinism
npx tree-sitter build-wasm --docker
sha256sum tree-sitter-nsl.wasm > /tmp/wasm.sha1
npx tree-sitter build-wasm --docker
sha256sum tree-sitter-nsl.wasm > /tmp/wasm.sha2
diff /tmp/wasm.sha1 /tmp/wasm.sha2

# Stage 3: smoke
npx tree-sitter parse $(cat ../../test/tooling/treesitter/smoke/corpus.txt | grep -v '^#')

# Stage 3: golden
npx tree-sitter test
```

The full project CI entry point (Constitution Principle IX
"reproducible locally" clause) bundles these into the
existing `./scripts/ci.sh`:

```bash
cd <repo-root>
./scripts/ci.sh stage2  # static checks (incl. T8 sub-steps)
./scripts/ci.sh stage3  # unit & layer tests (incl. T8 sub-steps)
```

---

## 5. Common gotchas

- **`npx tree-sitter` resolves to a globally-installed
  version**: don't install `tree-sitter-cli` globally inside
  the dev container. Always `cd grammars/treesitter && npx
  …` so the locally-pinned version wins.
- **`(ERROR)` nodes after a `nsl_lang.ebnf` edit**: the
  spec changed but `grammar.js` doesn't yet know about the
  new production. See §1.1 above.
- **Capture mismatch in golden test**: the test runner
  prints both the observed capture and the expected; check
  `queries/highlights.scm` for ordering (later patterns
  override earlier matches in tree-sitter's query semantics).
- **WASM byte-identity drift**: usually means
  `tree-sitter-cli` or `emscripten/emsdk` versions differ
  between two builds. Confirm `npm ci` and the docker image
  pin (research.md §3) match.
- **VS Code extension shows nothing after install**:
  check `tree-sitter-nsl.wasm` is present in
  `editors/vscode/treesitter/`. Per the contract
  (`contracts/vscode-extension.contract.md` §2), missing
  WASM degrades to T1 levels with a warning toast — no
  silent failure.

---

## 6. CI failure debugging

| CI sub-step | Likely cause | Fix |
|---|---|---|
| `treesitter-grammar-regen-diff` | grammar.js edited without `npx tree-sitter generate` | Run regen locally; commit refreshed `parser.c`/`grammar.json`/`node-types.json` |
| `treesitter-wasm-determinism` | upstream `tree-sitter-cli` or emscripten changed | Compare `package-lock.json` and pin a fresh patch range; document the bump in the PR |
| `treesitter-spdx` | new file in `grammars/treesitter/` or `editors/vscode/treesitter/` missing SPDX header | Add `// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception` line-1 to the affected file |
| `treesitter-smoke` | new construct in an `examples/*.nsl` not modelled in `grammar.js` | Add the missing rule per §1.1 |
| `treesitter-highlights-golden` | `highlights.scm` edited without updating fixture, or vice-versa | Re-run `npx tree-sitter test` and inspect the diff |

---

## 7. Where to go next

- Spec and clarifications: [`spec.md`](./spec.md)
- Implementation plan + structure: [`plan.md`](./plan.md)
- Phase 0 research (decisions and rejected alternatives):
  [`research.md`](./research.md)
- Entity catalogue + relationships:
  [`data-model.md`](./data-model.md)
- Frozen contracts (gateable in CI):
  [`contracts/`](./contracts/)
- Tasks (after `/speckit-tasks` runs): `tasks.md`
