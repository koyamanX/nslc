<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

---

description: "Task list for feature 010-t8-tree-sitter-grammar"

---

# Tasks: T8 — Tree-sitter Grammar + Highlight Queries + VS Code WASM Consumer

**Input**: Design documents from `/specs/010-t8-tree-sitter-grammar/`
**Prerequisites**: plan.md ✅, spec.md ✅, research.md ✅, data-model.md ✅, contracts/ ✅, quickstart.md ✅

**Tests**: Test tasks are **MANDATORY** per Constitution Principle VIII (Test-First Development, NON-NEGOTIABLE). For T8 the relevant test layer is the **tree-sitter test layer** (new T-track layer; introduced by this feature, parallel to T1's `vscode-tmgrammar-test` layer). Tests MUST be written and observed FAILING before the corresponding implementation tasks begin.

**Organization**: Tasks are grouped by user story (US1 / US2 / US3 from spec.md) to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3); Setup / Foundational / Polish phases carry no story label
- All paths are relative to the repository root

## Path Conventions

Per plan.md "Project Structure":

- `grammars/treesitter/` — tree-sitter grammar source + generated artefacts
- `editors/vscode/treesitter/` — VS Code extension shell (extends T1's `editors/vscode/`)
- `test/tooling/treesitter/` — smoke + golden fixtures
- `scripts/` — generators + CI integration
- `examples/` — pre-existing M-track corpus, read-only at T8 (smoke-fixture source per Q4 → Option C)

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization — directory scaffolding, toolchain pinning, SPDX bookkeeping. No story-specific work yet.

- [X] T001 Create `grammars/treesitter/` directory and author `grammars/treesitter/package.json` pinning `tree-sitter-cli@^0.22.0` in `devDependencies` and `web-tree-sitter@^0.22.0` in `dependencies` (per research.md §1; spec.md Clarifications Q1 → Option B); include `_comment_top` SPDX field. **Also authored**: `grammars/treesitter/.gitignore` (excludes `node_modules/`, `tree-sitter-nsl.wasm`, `bindings/` per research.md §1/§2)
- [ ] T002 Run `cd grammars/treesitter && npm install` to produce `grammars/treesitter/package-lock.json`; commit the lockfile (the source of truth for the pinned `tree-sitter-cli` patch version per Constitution Principle V determinism). **DEFERRED IN-SESSION**: requires npm-registry network access (sandbox blocks all but github.com); user runs locally inside the dev container
- [X] T003 [P] Create empty directory `grammars/treesitter/queries/` with a `.gitkeep` file (placeholder for `highlights.scm` arriving in US1)
- [X] T004 [P] Create empty directory `editors/vscode/treesitter/` with a `.gitkeep` file (extends T1's existing `editors/vscode/`)
- [X] T005 [P] Create empty directory tree `test/tooling/treesitter/{smoke,highlights,corpus}/` each with `.gitkeep` (corpus dir is for parser-shape snapshot tests if added post-T8)
- [X] T006 [P] Author `grammars/treesitter/SPDX.NOTICE` documenting that `parser.c`, `grammar.json`, and `node-types.json` inherit the project license via the generator (per research.md §9; sibling-NOTICE pattern)
- [X] T007 Author `test/tooling/treesitter/lit.local.cfg.py` (copy + adapt T1's `test/tooling/lit.local.cfg.py` to disable lit suffix-based discovery on this subtree — fixtures are consumed by `tree-sitter test`, not lit)
- [ ] T008 [P] Amend `scripts/spdx_exceptions.txt` to add the four generator-output paths `grammars/treesitter/parser.c`, `grammars/treesitter/grammar.json`, `grammars/treesitter/node-types.json`, `grammars/treesitter/package-lock.json` as SPDX-exempted-via-NOTICE (per research.md §9). **DEFERRED**: `scripts/check_spdx.py` rejects exception entries for files that do not yet exist (the staleness rule). T008 lands alongside T012 (tree-sitter generate produces the three generator artefacts) and after T002 (npm install produces the lockfile). The original Phase-1 placement was a tasks.md ordering bug surfaced during implementation and should be re-classified as Foundational; T008 is renumbered in spirit as T012a / Phase 2

**Checkpoint**: Directory scaffolding in place; toolchain pinned; SPDX bookkeeping deferred to Phase 2 alongside the generator-output that triggers it. Foundational phase can begin.

> **Implementation note (2026-05-05)**: T002 and T008 are deferred in-session due to sandbox network constraints (T002) and the `check_spdx.py` no-forward-looking-entries policy (T008). T002 must run inside the dev container; T008 lands alongside T012. The Phase 1 → Phase 2 boundary is otherwise clean.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Generator skeleton, minimum-viable grammar, CI scaffolding — everything that MUST exist before any user story can implement productions or fixtures. After Phase 2, `npx tree-sitter generate` succeeds, `parser.c` is committed, and CI sub-step placeholders are wired (gates are XFAIL or skipped pending US1/US2 content).

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

- [X] T009 Author `scripts/templates/grammar.js.template` — minimum-viable hand-authored production scaffolding that will become `grammar.js` (must define a `source_file` rule that accepts arbitrary tokens initially; cite spec anchors `nsl_lang.ebnf §1` etc. in comments); SPDX line-1 `// SPDX-…` header
- [X] T010 Author `scripts/gen_treesitter_grammar.py` — Python 3 generator that reads `include/nsl/Lex/KeywordSet.def` (X-macro single-source-of-truth, same one T1's `gen_textmate_grammar.py` consumes), splices the keyword block into `scripts/templates/grammar.js.template`, and writes `grammars/treesitter/grammar.js`; SPDX line-1 `# SPDX-…` header; mirror the structure of `scripts/gen_textmate_grammar.py`
- [X] T011 Run `python scripts/gen_treesitter_grammar.py` to write the initial `grammars/treesitter/grammar.js` stub (depends on T009 + T010); commit `grammar.js`. **Outcome**: 42 keywords spliced in source order; `--check` is byte-stable
- [ ] T012 Run `cd grammars/treesitter && npx tree-sitter generate` to produce `grammars/treesitter/parser.c`, `grammars/treesitter/grammar.json`, `grammars/treesitter/node-types.json`; commit all three (per research.md §4 — community default + downstream-consumer convenience + reviewer-visibility-of-state-changes). **DEFERRED IN-SESSION**: blocked on T002 (`npm install` needs network access); user runs locally inside the dev container
- [X] T013 Amend `scripts/check_spdx.py` to recognise SPDX.NOTICE-exempted paths from `scripts/spdx_exceptions.txt` (verify the existing exception mechanism handles directory-scoped exemptions; if not, extend it minimally — same precedent as T1's check_spdx.py amendments). **Outcome**: directory-prefix exception support already in place from prior PRs and `SPDX.NOTICE` already in `AUTO_EXEMPT_BASENAMES` (Phase 1), so the only edits were two minimal recipe additions: `.js` (`//`) and `.scm` (`;`), plus extending the `.in` template-suffix stripping to also strip `.template` (so `grammar.js.template` resolves to the `.js` recipe). Full-repo `--all` check: 1005 / 0 / 231
- [X] T014 Amend `scripts/ci.sh` to add five new T8 sub-steps: stage 2 gains `treesitter-spdx`, `treesitter-grammar-regen-diff`, `treesitter-wasm-determinism`; stage 3 gains `treesitter-smoke`, `treesitter-highlights-golden` (per research.md §12 / plan.md "CI integration"). Initially all five may be soft-skip or XFAIL until US1/US2 content lands; final activation happens in US2 T036. **Outcome**: stage-2 trio inserted between the T1 grammar regen-diff and the textmate mirror byte-equality; new `stage_tooling_treesitter` covering smoke + golden in stage 3, called from `stage_unit_tests` after `stage_tooling_textmate`; dispatcher entry `tooling-treesitter` + usage banner updated. All five sub-steps soft-skip cleanly in-sandbox (no `node_modules`)
- [X] T015 [P] Amend `.github/workflows/ci.yml` to add a new "Build & Upload tree-sitter-nsl.wasm" job step that runs after stage 2 passes: invokes `cd grammars/treesitter && npx tree-sitter build-wasm --docker`, uploads via `actions/upload-artifact@v4` as `tree-sitter-nsl-wasm`, and on tagged commits attaches the artefact via `softprops/action-gh-release` (per spec.md Q2 → Option C). **Outcome**: new `tree-sitter-wasm` job runs on bare `ubuntu-22.04` (no `container:`) for host-Docker access; `needs: static-checks`; `permissions: contents: write` for the release-attach step; falls back to `npm install` while `package-lock.json` is absent, switches to `npm ci` once T002 lands; precheck-skips if `grammar.js` is missing

**Checkpoint**: Phase-2 in-sandbox subset (T009/T010/T011/T013/T014/T015) landed in commit `042857f` on branch `010-t8-tree-sitter-grammar`. **Foundation NOT YET fully ready**: T002 + T012 + T008 must run inside the dev container before user-story work can begin (T012's `parser.c` is the gate the Phase-2 checkpoint actually requires).

> **Implementation note (2026-05-09)**: T009–T011, T013–T015 done in-sandbox under host-network restrictions. T002 (`npm install`), T012 (`tree-sitter generate`), and T008 (SPDX exception entries for parser.c / grammar.json / node-types.json / package-lock.json) require dev-container network access and are deferred together as a single follow-up commit. After that commit lands Phase 2 closes and Phase 3 (US1) unblocks.

---

## Phase 3: User Story 1 — Semantic identifier scopes (Priority: P1) 🎯 MVP

**Goal**: Distinguish `reg` / `wire` / `mem` / `proc` / `state` / `func` declaration and reference sites in the parse tree, and emit the FR-007 required-minimum capture set so editor consumers see five visually distinct identifier categories where T1 left them un-scoped. This is the headline value-prop T8 delivers over T1.

**Independent Test**: Open any `examples/*.nsl` containing all five identifier kinds in a tree-sitter-aware editor (or run `npx tree-sitter highlight examples/03_register.nsl --html` and inspect); confirm declaration- and reference-sites of `reg`/`wire`/`mem`/`proc`/`state`/`func` carry **distinct** captures per the FR-007 list.

### Tests for User Story 1 (MANDATORY per Constitution Principle VIII) ⚠️

> **NOTE**: Write these golden fixtures FIRST. Each MUST be observed FAILING against the unchanged tree (no captures yet) before any grammar.js or highlights.scm implementation in this story begins. Per Principle VIII no-retrofitted-tests, the failing-state commit hash is recorded in the PR description before merge.

- [X] T016 [P] [US1] Author golden fixture `test/tooling/treesitter/highlights/reg_vs_wire.nsl` — declares `reg q[8]`, `wire w`, `mem m[16][8]`; uses each in an expression; inline `tree-sitter test`-format assertions for `@variable.register` (declaration + reference, ≥ 2 sites), `@variable.wire` (declaration + reference, ≥ 2 sites), `@variable.memory` (declaration + reference, ≥ 2 sites); covers AS1.1, AS1.2 and `contracts/highlights-coverage.contract.md` §3 row 1. **Outcome**: 6 assertion sites; column-precision verified via a small in-sandbox Python checker that asserts each `^` lands on a non-whitespace character of the line above (no tree-sitter binary needed locally)
- [X] T017 [P] [US1] Author golden fixture `test/tooling/treesitter/highlights/proc_vs_func.nsl` — declares `proc compute {…}`, `func bar() …`; uses `compute()` and `bar()`; inline assertions for `@function.proc`, `@function.func`, `@function.call.proc`, `@function.call.func`; covers AS1.3 and `contracts/highlights-coverage.contract.md` §3 row 2. **Outcome**: 4 assertion sites; uses canonical `proc <name> seq { … }` from examples/09 plus `func <name> { return … }` from examples/07; bare `compute()` and `add(a, b)` exercise the control_call disambiguation (`callee:` binding lookup) per grammar-coverage.contract.md §1
- [X] T018 [P] [US1] Author golden fixture `test/tooling/treesitter/highlights/state_goto.nsl` — declares `state idle; state busy;`; uses `goto idle;`; inline assertions for `@label.state` at both definition and goto-target sites; covers AS1.4 and `contracts/highlights-coverage.contract.md` §3 row 3. **Outcome**: 4 assertion sites (2 definitions + 2 goto targets) — the contract's "≥ 2 sites of same capture" minimum exceeded for higher coverage
- [X] T019 [P] [US1] Author golden fixture `test/tooling/treesitter/highlights/control_terminal_s27.nsl` — exercises `S27` (constructive: control-terminal name as 1-bit value in expression position); inline assertion for the FR-009 dedicated control-terminal capture (name plan-level per `contracts/highlights-coverage.contract.md` §2; recommended `@variable.builtin.terminal`); covers AS1.5. **Outcome**: capture name **locked** to `variable.builtin.terminal` (the §2-recommended choice). T033 (highlights.scm) MUST emit this exact name. Fixture body mirrors examples/18 line 47 (`busy = work;` — proc_name in expression position)
- [X] T020 [P] [US1] Author golden fixture `test/tooling/treesitter/highlights/macro_splice_ident.nsl` — `reg q[%WIDTH%];`, `func %name%() …`, expression-position `%FOO%`; inline assertions for `@constant.macro` at ≥ 3 splice positions (data-model.md §1.6 row 5); covers AS1.6. **Outcome**: 4 assertion sites at 3 splice categories — declare-port width, reg width, identifier-substitute (in reg name and transfer LHS), expression-operand RHS
- [X] T021 [P] [US1] Author golden fixture `test/tooling/treesitter/highlights/parser_note_disambiguation.nsl` — exercises N5 (`#line` directive vs `#expr` sign-extend), N2 (unary reduction `&` vs binary bitwise `&`), N3 (`.{ ... }` aggregate vs field access), N6 (`instance.finish()`); assertions verify the parse tree's *shape* (no `(ERROR)` nodes, distinct rule names) rather than highlight captures; covers spec Edge Cases / contracts/grammar-coverage.contract.md §3. **Outcome**: zero highlight assertions per the contract row 6; correctness gated implicitly by the smoke gate (`treesitter-smoke` sub-step asserts no `(ERROR)`/`(MISSING)` nodes once the corpus.txt entry lands in T035). Source mirrors examples/16 (sign-extend `8 # sig`), examples/17 (concat-lvalue `.{tag, idx} = a`), and examples/18 (proc method access `worker.invoke()`)
- [ ] T022 [US1] Run `cd grammars/treesitter && npx tree-sitter test` and verify ALL six golden fixtures from T016–T021 FAIL (the stub grammar from T011 has no productions for these constructs, and `highlights.scm` does not yet exist). Record the failing-state commit hash in the eventual PR description per Principle VIII no-retrofitted-tests clause. **DEFERRED IN-SESSION**: blocked on T002 + T012 (need `npm install` + `npx tree-sitter generate`); user runs locally inside the dev container alongside the Phase-2 closeout commit. The failing-state commit hash should be recorded as the head of this commit-chain (T009–T021 inclusive) once observed

### Implementation for User Story 1

> **NOTE**: T023–T030 all edit the single file `scripts/templates/grammar.js.template` (sequential — same file). After each substantial edit, re-run T031–T032 locally to verify `tree-sitter generate` still succeeds and the smoke/golden tests behave as expected. Only T032's `parser.c` regeneration commit is required between tasks.

- [ ] T023 [US1] Author tree-sitter token rules in `scripts/templates/grammar.js.template`: `identifier`, `number_literal` (with all 5 forms + `Z`/`X`/`U` markers), `string_literal` (with backslash escapes), `macro_identifier` (`%IDENT%`), `line_comment` and `block_comment` as `extras`; cite `nsl_lang.ebnf §13` / `§14` and `nsl_pp.ebnf §4` per `contracts/grammar-coverage.contract.md` §1 lexical rows
- [ ] T024 [US1] Author tree-sitter productions in `scripts/templates/grammar.js.template` for the compilation-unit + preprocessor seam: `source_file: $ => repeat($._top_level_item)`; `_top_level_item` choice over preprocessor / struct / declare / module / top_level_parameter; `preprocessor_directive` covering `#include`/`#define`/`#undef`/`#if`/`#ifdef`/`#ifndef`/`#else`/`#endif`/`#line` (matched at line-start with `prec` boost — N5 disambiguation); cite `nsl_lang.ebnf §1` and `nsl_pp.ebnf §2`
- [ ] T025 [US1] Author tree-sitter productions in `scripts/templates/grammar.js.template` for `module_block`, `declare_block`, `struct_declaration`, `top_level_parameter` (`nsl_lang.ebnf §§3–5`); use `field()` for `name`, `modifier`, `port_list`, `body` per `contracts/grammar-coverage.contract.md` §1 rows for these productions
- [ ] T026 [US1] Author tree-sitter productions in `scripts/templates/grammar.js.template` for internal-structure rules: `register_declaration`, `wire_declaration`, `memory_declaration`, `proc_name_declaration`, `state_name_declaration` (`nsl_lang.ebnf §6`); each emits one node per declarator (the parser-shape mirror of the M-track AST's "no silent AST drops" Principle I sub-clause)
- [ ] T027 [US1] Author tree-sitter productions in `scripts/templates/grammar.js.template` for definitions: `func_definition` (covers both `func` and `function` keywords per S26 canonicalisation), `proc_definition`, `state_definition` (`nsl_lang.ebnf §7`); use `field('name', $.identifier)` so `highlights.scm` can match definition sites by field name
- [ ] T028 [US1] Author tree-sitter productions in `scripts/templates/grammar.js.template` for action statements: `par_block`, `alt_block`, `any_block`, `seq_block`, `if_statement` (with N1 statement-vs-expression handled by production-position), `for_statement`, `while_statement`, `generate_block` (`nsl_lang.ebnf §8`)
- [ ] T029 [US1] Author tree-sitter productions in `scripts/templates/grammar.js.template` for atomic actions and system tasks: `transfer_action` (`=` vs `:=` per S3), `control_call` with `field('callee', …)`, `finish_action`, `system_task_call` (`_display`/`_finish`/`_init`/`_delay`/etc.) (`nsl_lang.ebnf §9` + `§10`)
- [ ] T030 [US1] Author tree-sitter productions in `scripts/templates/grammar.js.template` for expressions §11 with parser-note disambiguation: `_expression` supertype + concrete subrules; `sign_extend_expr` (`#expr` per N5), `zero_extend_expr` (`'expr`), `bit_slice` (S15 — parse, Sema enforces compile-time), `concat_expression`, `dot_aggregate` (`.{ … }` per N3 two-character lookahead), `reduction_op` (unary, prefix position per N2), `bitwise_binary_op` (binary, infix position per N2), `proc_method_access` (`instance.finish()` per N6 + S21), `conditional_expression` (S14 — else mandatory)
- [ ] T031 [US1] Run `python scripts/gen_treesitter_grammar.py` to refresh `grammars/treesitter/grammar.js` (splices the keyword block from `KeywordSet.def` into the now-fully-authored template); commit `grammar.js`
- [ ] T032 [US1] Run `cd grammars/treesitter && npx tree-sitter generate` to produce updated `parser.c`, `grammar.json`, `node-types.json`; commit all three; verify `parser.c` is byte-identical to a second regeneration run (regenerate-and-diff readiness)
- [ ] T033 [US1] Author `grammars/treesitter/queries/highlights.scm` with the FR-007 required-minimum 20-capture set per `contracts/highlights-coverage.contract.md` §1: keyword captures (#1–#5 sourced from `_keyword` token kinds), `@type.builtin` (#6) and `@type` (#7), `@function.call` (#8), `@constant.macro` (#9), `@number`/`@string`/`@comment` (#10–#12), and the eight Q3-locked sub-captures (#13–#20: `@variable.register`/`@variable.wire`/`@variable.memory`, `@function.proc`/`@function.func`, `@function.call.proc`/`@function.call.func`, `@label.state`); plus the FR-009 dedicated control-terminal capture; SPDX line-1 `; SPDX-…` header
- [ ] T034 [US1] Run `cd grammars/treesitter && npx tree-sitter test` and verify ALL six goldens from T016–T021 now PASS; if any assertion fails, iterate on `grammars/treesitter/queries/highlights.scm` (T033) or grammar productions (T023–T030 → re-run T031–T032)

**Checkpoint**: At this point, User Story 1 is fully functional. Goldens green. The T8 grammar parses NSL with semantic identifier classification; an editor with the WASM grammar loaded sees five visually distinct identifier categories where T1 left them un-scoped. **MVP-shippable.**

---

## Phase 4: User Story 2 — Smoke + golden test gates wired into CI (Priority: P2)

**Goal**: Activate the README §Roadmap T8 row's stated test gate ("Tree-sitter parse tree on the audited corpus matches expected structure (smoke); highlight-query golden test"). Smoke gate parses every file in the in-tree `examples/*.nsl` corpus (per Q4 → Option C); golden gate runs the US1 fixtures via `tree-sitter test`. Both run on every PR and fail CI on any assertion failure.

**Independent Test**: Run `./scripts/ci.sh stage2` and `./scripts/ci.sh stage3` locally on a clean checkout; both stages pass. Manually break a fixture (e.g. add a syntax-malformed NSL file to the smoke corpus); CI fails with a localised diagnostic naming the file and byte offset.

### Tests for User Story 2 (MANDATORY per Constitution Principle VIII) ⚠️

> US2's "tests" are the gate-machinery acceptance checks. They are validated by the smoke-broke / golden-broke negative cases listed under T037 below. Per Principle VIII, the negative test cases are exercised in the implementation PR and recorded in the PR description.

### Implementation for User Story 2

- [ ] T035 [US2] Populate `test/tooling/treesitter/smoke/corpus.txt` — line-list of 20 paths, one per `examples/01_hello.nsl` through `examples/20_simulation_tb.nsl`; include comment lines (prefix `#`) documenting the future P-VEN-conditional addition of `test/audited/<project>/` paths once P-VEN lands at M7 (per Q4 → Option C and `data-model.md §1.5`); SPDX line-1 `# SPDX-…` header
- [ ] T036 [US2] Activate the five T8 sub-steps in `scripts/ci.sh` placeholders from T014: (a) `treesitter-spdx` validates SPDX headers on `grammar.js`, `highlights.scm`, `gen_treesitter_grammar.py`, `corpus.txt`, fixture files; (b) `treesitter-grammar-regen-diff` runs `npm ci` then `npx tree-sitter generate` and asserts `git diff --exit-code` against committed `parser.c`/`grammar.json`/`node-types.json`; (c) `treesitter-wasm-determinism` runs `npx tree-sitter build-wasm --docker` twice and `sha256sum`-compares; (d) `treesitter-smoke` reads `test/tooling/treesitter/smoke/corpus.txt` (stripping `#` comments) and runs `npx tree-sitter parse --quiet --stat` over every file, failing on any `(ERROR)` or `(MISSING)` node; (e) `treesitter-highlights-golden` runs `cd grammars/treesitter && npx tree-sitter test`. All five sub-steps exit non-zero on failure
- [ ] T037 [US2] Run `./scripts/ci.sh stage2` and `./scripts/ci.sh stage3` locally inside the dev container; verify all T8 sub-steps PASS. Then exercise the negative-test cases per Principle VIII: (i) revert one line of `grammar.js` and confirm `treesitter-grammar-regen-diff` FAILS with a localised diagnostic; (ii) add a deliberately-malformed `.nsl` file to the smoke corpus list and confirm `treesitter-smoke` FAILS naming the file and `(ERROR)` byte offset; (iii) flip one assertion in a golden fixture and confirm `treesitter-highlights-golden` FAILS with observed-vs-expected capture; restore all three after recording the failure modes in the PR description

**Checkpoint**: At this point, User Stories 1 AND 2 work together — every PR through CI ratifies the grammar's correctness on the production-coverage corpus, and any drift between `grammar.js` and `parser.c`/`highlights.scm` and the goldens fails immediately.

---

## Phase 5: User Story 3 — VS Code extension shell consuming the WASM tree-sitter build (Priority: P3)

**Goal**: Make the T8 colouring actually visible in VS Code. Folder-drop install model (per T1 precedent + Q2 → Option C consumption path). Coexists with T1's TextMate base layer.

**Independent Test**: Folder-drop the `editors/vscode/` directory into `~/.vscode/extensions/`, supply `tree-sitter-nsl.wasm` from a CI workflow artefact (or local `tree-sitter build-wasm`), reload VS Code, open `examples/03_register.nsl`. Verify (a) keywords retain T1 colouring (no regression), (b) register and wire references colour distinctly (T8 override), (c) no extension-host errors in DevTools Console.

### Tests for User Story 3 (MANDATORY per Constitution Principle VIII) ⚠️

> US3's tests are *manual smoke-test attestations* recorded in the PR description, per `contracts/vscode-extension.contract.md` §5 and the T1 precedent for VS Code-side acceptance. AS3.4 (WASM byte-identity) is automated via the `treesitter-wasm-determinism` CI sub-step (US2 T036) and does not need a separate test task here.

### Implementation for User Story 3

- [ ] T038 [P] [US3] Author `editors/vscode/treesitter/extension.ts` — `activate()` calls `Parser.init({locateFile: …})`, `await Language.load(path.join(extensionPath, 'tree-sitter-nsl.wasm'))`, registers `vscode.languages.registerDocumentSemanticTokensProvider` for the `nsl` language ID; on missing `tree-sitter-nsl.wasm`, logs a clear `vscode.window.showWarningMessage` and exits cleanly (graceful-degradation path per `contracts/vscode-extension.contract.md` §2 and AS3.3); SPDX line-1 `// SPDX-…` header
- [ ] T039 [P] [US3] Author `editors/vscode/treesitter/highlight-provider.ts` — implements `vscode.DocumentSemanticTokensProvider`'s `provideDocumentSemanticTokens(document, ct)` per `contracts/vscode-extension.contract.md` §3; runs the tree-sitter query from `highlights.scm` against the parse tree; converts each capture into a `SemanticTokens` entry mapping (line, column, length, tokenTypeIndex, tokenModifiersBitmap); declares the `SemanticTokensLegend` with the 20 entries from `data-model.md §1.4`; respects `CancellationToken`; SPDX line-1 `// SPDX-…` header
- [ ] T040 [P] [US3] Author `editors/vscode/treesitter/tsconfig.json` and an npm `compile` script in the amended `editors/vscode/package.json` (T041) so `extension.ts` and `highlight-provider.ts` compile to `extension.js` / `highlight-provider.js` for VS Code's CommonJS extension host
- [ ] T041 [US3] Amend `editors/vscode/package.json` per `contracts/vscode-extension.contract.md` §1: add `"main": "./treesitter/extension.js"`; ensure `activationEvents` contains `"onLanguage:nsl"` (T1's existing entry already covers this — verify); add `contributes.semanticTokenTypes` array of 20 entries matching the captures in `data-model.md §1.4`; add `contributes.semanticTokenScopes` mapping each token type to a TextMate-style scope name for theme fallback; add `dependencies.web-tree-sitter` pinned to a specific patch version compatible with `tree-sitter-cli@0.22.x` ABI (per research.md §1 / §7)
- [ ] T042 [US3] Build the VS Code extension locally: `cd editors/vscode && npm install && npm run compile`; download `tree-sitter-nsl.wasm` from the latest green CI workflow run (per quickstart.md §3.1) OR build locally via `cd grammars/treesitter && npx tree-sitter build-wasm --docker && cp tree-sitter-nsl.wasm ../../editors/vscode/treesitter/`; folder-drop the `editors/vscode/` directory into `~/.vscode/extensions/nsl-tooling-vscode-0.x.y/`
- [ ] T043 [US3] Manual smoke-test AS3.1 (extension loads without error): launch a fresh VS Code instance, open `examples/03_register.nsl`, open `Help > Toggle Developer Tools > Console`; confirm zero extension-host errors related to loading the WASM grammar; record the attestation (VS Code version, screenshot of the console showing no errors) in the eventual PR description per Principle VIII no-retrofitted-tests clause and `contracts/vscode-extension.contract.md` §5
- [ ] T044 [US3] Manual smoke-test AS3.2 (T1 base + T8 override): in the same VS Code session, on `examples/03_register.nsl`, visually verify (a) reserved keywords retain T1 colouring (no regression — same colour as in the T1-only state), (b) register identifier and wire identifier references colour *distinctly* via T8's `@variable.register` / `@variable.wire`; record attestation in PR description (screenshot showing the colour distinction)
- [ ] T045 [US3] Manual smoke-test AS3.3 (graceful degradation when uninstalled): rename `~/.vscode/extensions/nsl-tooling-vscode-0.x.y/treesitter/tree-sitter-nsl.wasm` to `tree-sitter-nsl.wasm.bak` (simulating missing-WASM consumer), reload VS Code, open `examples/03_register.nsl`, verify the warning toast appears, T1 colouring still applies, and no extension-host crash; record attestation in PR description; restore the WASM file after testing

**Checkpoint**: All three user stories are independently functional. T8 is shippable in its entirety.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Final integration verification, documentation roll-up updates, and PR-trailer / merge-gate readiness.

- [ ] T046 [P] Verify `CLAUDE.md` §2.4 ("Syntax highlighting") and §2.5 ("Editor integrations") tables are current — both already name T8 in the right cells (Tree-sitter row at T8; VS Code tree-sitter cell at T8); no edit expected, but confirm and record. Verify the SPECKIT START/END block already updated by `/speckit-plan` is still pointing at this feature
- [ ] T047 [P] Verify `docs/CLAUDE.md` task → section map ("Working on the syntax highlighter" entry, lines 293–577) still aligns; no edit expected
- [ ] T048 Run `./scripts/ci.sh all` (full local CI pipeline) inside the dev container; verify every stage passes — including the new T8 sub-steps from T036 alongside all pre-existing M-track and T1 stages. Note this is the local-equivalent of the full GitHub Actions run, per Constitution Principle IX "reproducible locally" clause
- [ ] T049 Compose the merge-gate PR description with: (a) `Linear: NSLC-<N>` trailer referencing the T8 feature-track Linear issue (post `/speckit-taskstoissues` if not already filed); (b) `Assisted-by:` trailers per `CONTRIBUTING.md §5`; (c) the failing-state commit hashes for T022 (US1 goldens-failing) and T037 (US2 negative-test exercises) per Principle VIII no-retrofitted-tests clause; (d) the manual smoke-test attestations from T043/T044/T045 (VS Code version + screenshots) per Principle VIII; (e) confirmation that T046–T048 pass

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately.
- **Foundational (Phase 2)**: Depends on Setup completion. T009 + T010 + T011 + T012 form the critical path (template → generator → grammar.js → parser.c). T013 / T014 / T015 can land in parallel after T012. **BLOCKS all user stories.**
- **User Stories (Phase 3+)**: All depend on Foundational phase completion. US1 + US2 + US3 can then proceed in parallel **if staffed** (different files, minimal cross-dependencies). Sequential P1 → P2 → P3 is the default solo-developer order.
- **Polish (Phase 6)**: Depends on US1 + US2 + US3 completion (US1 produces the goldens that US2 gates ratify; US3 consumes the grammar.js + WASM that US1 + Phase 2 produce).

### User Story Dependencies

- **US1 (P1)**: Requires Foundational complete. Independently testable via `npx tree-sitter test` on the goldens (no CI integration needed for solo testing).
- **US2 (P2)**: Requires Foundational complete. **Soft-depends** on US1 — US2's `treesitter-highlights-golden` sub-step has nothing to gate until US1's goldens exist; US2's `treesitter-smoke` sub-step gates the example-corpus parse independently of US1. Can be staffed in parallel with US1; both must merge together for CI to pass on the next PR.
- **US3 (P3)**: Requires Foundational complete + grammar.js productions complete (US1 T023–T032). US3's manual smoke-tests AS3.2 require US1's `highlights.scm` to exercise the override (otherwise AS3.2 has nothing to assert). Can be staffed in parallel with US2.

### Within Each User Story

- Tests (golden fixtures) MUST be written and observed FAILING before implementation (Constitution Principle VIII).
- Within US1's implementation: T023 (tokens) before T024–T030 (productions referencing tokens); T031 (regenerate keyword block) after T023–T030; T032 (regenerate parser.c) after T031; T033 (highlights.scm) after T032 (queries reference rule names); T034 (verify pass) last.
- Within US2: T035 (corpus.txt) before T036 (CI sub-steps that consume it); T037 (verify) last.
- Within US3: T038/T039/T040 in parallel; T041 after; T042 after T041; T043/T044/T045 sequential (manual VS Code session).

### Parallel Opportunities

- **Setup**: T003, T004, T005, T006, T008 are all `[P]` — can run as one batched commit.
- **Foundational**: T015 is `[P]` against T009–T014.
- **US1**: T016, T017, T018, T019, T020, T021 are all `[P]` (different fixture files; six can be authored in one batched commit).
- **US3**: T038, T039, T040 are all `[P]` (different files).
- **Polish**: T046, T047 are `[P]`.
- **Cross-story**: US1 + US2 + US3 are independently staffable once Foundational completes.

---

## Parallel Example: User Story 1 goldens

```bash
# Author all six golden fixtures in parallel (different files):
Task: "Author golden fixture test/tooling/treesitter/highlights/reg_vs_wire.nsl"
Task: "Author golden fixture test/tooling/treesitter/highlights/proc_vs_func.nsl"
Task: "Author golden fixture test/tooling/treesitter/highlights/state_goto.nsl"
Task: "Author golden fixture test/tooling/treesitter/highlights/control_terminal_s27.nsl"
Task: "Author golden fixture test/tooling/treesitter/highlights/macro_splice_ident.nsl"
Task: "Author golden fixture test/tooling/treesitter/highlights/parser_note_disambiguation.nsl"

# Then verify they all fail (sequential — single npx tree-sitter test invocation):
Task: "Run npx tree-sitter test and confirm all six goldens FAIL"
```

---

## Implementation Strategy

### MVP First (User Story 1 only)

1. Complete Phase 1 (Setup): T001–T008. Branch is on `010-t8-tree-sitter-grammar`; toolchain pinned; SPDX bookkeeping ready.
2. Complete Phase 2 (Foundational): T009–T015. Stub grammar.js + parser.c committed; CI scaffolding present (sub-steps may XFAIL pending content).
3. Complete Phase 3 (US1): T016–T034. **Goldens-first** per TDD; grammar.js productions; `highlights.scm`; goldens green.
4. **STOP and VALIDATE**: run `npx tree-sitter test` locally; confirm all six goldens green. The MVP is shippable: an editor with the WASM-loaded grammar shows distinct register/wire/proc/state/func captures. CI's smoke gate isn't yet enforced (US2 not done), but US1 is independently demonstrable.
5. Deploy/demo if ready (folder-drop install per US3 quickstart, but skipping the VS Code-extension wiring — load via `tree-sitter highlight --html` for a static demo, or via Neovim's native tree-sitter integration which doesn't need T8's VS Code shell).

### Incremental Delivery

1. Complete Setup + Foundational → Foundation ready.
2. Add User Story 1 → Test independently → Demo via `tree-sitter highlight` (MVP!).
3. Add User Story 2 → CI gates active → Every PR ratifies grammar correctness.
4. Add User Story 3 → VS Code consumer wired → Folder-drop install delivers the colouring in VS Code.
5. Polish phase finalises PR description and merges.

### Parallel Team Strategy

With multiple developers:

1. Team completes Setup + Foundational together (one PR, or one developer drives the foundation).
2. Once Foundational is done:
   - Developer A: User Story 1 (the meaty grammar + queries work)
   - Developer B: User Story 2 (CI integration + corpus.txt + negative tests)
   - Developer C: User Story 3 (VS Code TypeScript work; mostly independent)
3. Stories complete and integrate independently. Final integration in Phase 6.

---

## Notes

- `[P]` tasks = different files, no dependencies on incomplete tasks.
- `[Story]` label maps task to specific user story for traceability; Setup / Foundational / Polish phases carry no story label.
- Each user story is independently completable and testable: US1 via local `tree-sitter test`; US2 via local `./scripts/ci.sh stage{2,3}`; US3 via manual VS Code session.
- Verify tests fail before implementing (T022 explicitly demands this for US1; T037 demands negative-test exercises for US2; US3's manual attestations rely on the failing-state being observable).
- Commit after each task or logical group (Setup as one batched commit; Foundational as one or two commits; each US sub-phase as a commit; Polish as one final commit before PR).
- Stop at any checkpoint to validate the story independently.
- Avoid: editing `scripts/templates/grammar.js.template` from multiple tasks in parallel (single-file conflict — T023–T030 are deliberately sequential for this reason); cross-story dependencies that break US1/US2/US3 independence.
