# T8 — Tree-sitter grammar + highlight queries + VS Code WASM consumer

Closes the T8 milestone from `README.md` §Roadmap (T-track row). Delivers a second-tier semantic highlighter on top of T1's TextMate base, with three independently-testable user stories:

* **US1 (P1)** — `grammars/treesitter/grammar.js` + `queries/highlights.scm` + `queries/locals.scm`. 96 tree-sitter rules covering §3–§11 of the NSL spec; FR-007 20-capture set + FR-009 `@variable.builtin.terminal`.
* **US2 (P2)** — Five CI sub-steps in `scripts/ci.sh` (`treesitter-spdx`, `treesitter-grammar-regen-diff`, `treesitter-wasm-determinism`, `treesitter-smoke`, `treesitter-highlights-golden`) plus the `tree-sitter-wasm` GitHub Actions job that builds and uploads `tree-sitter-nsl.wasm` from CI cells per Q2 → Option C.
* **US3 (P3)** — `editors/vscode/treesitter/{extension,highlight-provider,legend}.ts` — VS Code semantic-tokens provider that consumes the same query set at runtime for theme-side overrides. 21-entry `SemanticTokensLegend` (20 from data-model.md §1.4 + the FR-009 `variable.builtin.terminal` deviation documented in `data-model.md`/§1.5 and `contracts/vscode-extension.contract.md`).

## Verification

* `tree-sitter test` — **6 / 6 fixtures green**, 20 inline assertions PASS (control_terminal_s27, parser_note_disambiguation, proc_vs_func, reg_vs_wire, macro_splice_ident, state_goto).
* `tree-sitter parse` smoke gate over `test/tooling/treesitter/smoke/corpus.txt` — **21 / 21 OK** (20 in-tree examples + the parser_note_disambiguation fixture).
* Regenerate-and-diff byte-stability — `tree-sitter generate --no-bindings` is identical across two consecutive runs.
* SPDX `--all` — 1027 pass / 0 fail / 237 exempt (out of 1264).
* US2 negative-test exercises (T037, Principle VIII evidence): all three CI gates fail loud when their inputs are intentionally broken — see commit message `3922f15` for the observed failure-mode strings.

## Constitution Principle VIII — failing-state commit hash

Per Principle VIII (no-retrofitted-tests):

* **US1 fixtures-failing state**: `8cb8a6d` (`T8 Phase 3 US1 (tests-first): six golden highlight fixtures`). At that commit the Phase-2 token-permissive grammar stub could not parse the fixture content into any structured nodes, and `queries/highlights.scm` did not yet exist; every fixture's inline assertion would have FAILED. Implementation landed at `f43b510` → `317f7b6` → `3922f15`.

## Reference-site resolution split (FR-007 / FR-008 / FR-009)

`contracts/highlights-coverage.contract.md` §1 documents the in-tree position-by-position split:

| Position | Mechanism |
|---|---|
| Declaration sites | `queries/highlights.scm` field-binding patterns |
| LHS of `:=` / `=` operator | `queries/highlights.scm` operator-position patterns |
| Statement-position call site (`proc()`) | `queries/highlights.scm` `(control_call callee:)` pattern |
| Expression-position call site (`func()`) | `queries/highlights.scm` `(call_expression function:)` pattern |
| RHS-of-`=`/`:=` reference, S27 expression-position control terminal | **Consumer-runtime scope walk** in `editors/vscode/treesitter/highlight-provider.ts` per `queries/locals.scm` |

The standalone `tree-sitter test` runner exercises only the first four rows; the fifth is verified manually via the US3 VS Code smoke-test attestations (AS3.2). Per-fixture in-tree NOTE blocks in `reg_vs_wire.nsl` and `control_terminal_s27.nsl` document the runtime-verified positions explicitly.

## Test plan

* [x] `npx tree-sitter test` in `grammars/treesitter/` — all six fixtures green.
* [x] `./scripts/ci.sh tooling-treesitter` on the host (npm/npx available) — smoke + golden green.
* [x] `./scripts/ci.sh static-checks` inside the dev container — clang-format (211 files) green; SPDX `--all` green; tree-sitter regen-check green.
* [ ] **Manual smoke-test attestations (US3)** — AS3.1 / AS3.2 / AS3.3 require a human at a VS Code session per `contracts/vscode-extension.contract.md` §5. Owner: <attestation pending>. VS Code version + screenshots to be appended below before merge.
* [ ] **`./scripts/ci.sh all` on dedicated CI hardware** — the multi-cell C++ build + lit/ctest portion is a multi-hour run that should land via the GitHub Actions matrix on the PR push rather than locally.

## Trailers

```
Linear: NSL-<N>           ← post via `/speckit-taskstoissues` if not already filed
Assisted-by: Claude Code  ← per CONTRIBUTING.md §5
```
