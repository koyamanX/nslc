<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Data Model — T8 Tree-sitter Grammar + Highlight Queries + VS Code WASM Consumer

**Feature**: `010-t8-tree-sitter-grammar`
**Phase**: 1 (design / data-model)
**Date**: 2026-05-05

T8 is a static-artefact feature like T1 — no runtime database,
no persistence, no in-memory state. The "data model" here
describes the **schemas** of the seven data artefacts T8 ships
plus the **relationships** between them and the existing T1
artefacts they compose with.

---

## 1. Entities

### 1.1 `KeywordSet.def` entry  (PRE-EXISTING, READ-ONLY consumer — same as T1)

The X-macro single-source-of-truth for the NSL reserved-keyword
set. T8 does not modify it; T8 reads it via a parallel generator
identical in shape to T1's.

| Field | Type | Source | Notes |
|---|---|---|---|
| `enum_suffix` | identifier (`[a-z_][a-z0-9_]*`) | `KeywordSet.def` | becomes `tk_<enum_suffix>` in the lexer; ignored by T8 |
| `spelling` | string | `KeywordSet.def` | the literal source text — what the tree-sitter grammar's `keyword` rule must match |
| `group` | enum {`appendix3`, `practical`} | `KeywordSet.def` (group comments) | informational; T8 emits all keywords through the same `_keyword` rule |

**Cardinality**: 42 entries as of 2026-05-05 (same count T1
sees). Tracks `lang.ebnf §15` per Principle I.

**Coupling**: T8's regenerate-and-diff CI sub-step
(plan.md §"CI integration" / research.md §12) gates this
edge — a `KeywordSet.def` edit without a T8 grammar
regenerate fails CI, identical to T1's gate.

### 1.2 Tree-sitter grammar rule  (NEW — T8-introduced)

A node in `grammar.js`'s `rules` object. Defines either a
**named production** (visible in the parse tree) or a
**hidden internal rule** (`_`-prefixed; collapsed during
parse-tree construction).

| Field | Type | Notes |
|---|---|---|
| `name` | identifier | rule name; matches `nsl_lang.ebnf` production name where possible (e.g. `module_block`, `declare_block`) |
| `kind` | enum {named, hidden, supertype, inline} | hidden rules collapse; supertypes are abstract parents |
| `body` | tree-sitter DSL expression | `seq()`, `choice()`, `repeat()`, `optional()`, `field()`, `prec()`, `prec.left()`, `prec.right()`, `token()`, regex |
| `spec_anchor` | string | citation back to `nsl_lang.ebnf` line range or `nsl_pp.ebnf` line range; recorded as a JS comment above the rule |

**Cardinality**: target ~50–70 named rules (covering
`nsl_lang.ebnf` §§1–11 + `nsl_pp.ebnf §2`); ~20–30 hidden
internal rules.

### 1.3 Tree-sitter token  (NEW)

A leaf node in the parse tree, produced by tree-sitter's
internal lexer. Distinct from the rule entity above.

| Field | Type | Notes |
|---|---|---|
| `name` | identifier | token name; matches `nsl_lang.ebnf §13` lexical-element names (e.g. `identifier`, `number_literal`, `string_literal`, `line_comment`, `block_comment`, `macro_identifier`) |
| `regex` | regex (Oniguruma-flavour-equivalent JS regex) | the literal pattern recognised |
| `is_extra` | bool | whitespace and comments are `extras` (skipped between tokens) |

**Specific tokens (from research + design-doc skeleton):**

| Token | Regex | Notes |
|---|---|---|
| `identifier` | `/[A-Za-z][A-Za-z0-9_]*/` | NSL identifiers per `lang.ebnf §13`; `__` not allowed (S1) — but tree-sitter accepts and lets Sema enforce |
| `macro_identifier` | `/%[A-Za-z_][A-Za-z0-9_]*%/` | `%IDENT%` per `pp.ebnf §4`; the escape-hatch token |
| `number_literal` | choice of 5 patterns: bare-decimal, hex `0x…`, binary `0b…`, octal `0o…`, Verilog-sized `<width>'[bodh]…` | per `lang.ebnf §13`; `Z`/`X`/`U` markers and underscore separators included |
| `string_literal` | `/"([^"\\]|\\.)*"/` | with backslash-escape sequences |
| `line_comment` | `/\/\/[^\n]*/` | `extras` |
| `block_comment` | `/\/\*[\s\S]*?\*\// ` | `extras`; non-nestable per `lang.ebnf §14` |
| `directive_keyword` | `#(include|define|undef|ifdef|ifndef|if|else|endif|line)` | matched at line-start only via `prec()` |

### 1.4 Highlight capture  (NEW)

A capture name emitted by `queries/highlights.scm` against a
parse-tree shape. Drives editor-theme colouring.

| Field | Type | Notes |
|---|---|---|
| `capture_name` | string starting with `@` | tree-sitter capture-name convention |
| `query_predicate` | tree-sitter query S-expression | the parse-tree shape that produces the capture |
| `parent_capture` | string (optional `@…`) | the parent capture name in the dotted-segment hierarchy; consumer themes fall back to parent if no rule matches the leaf |

**Capture set** (frozen by Clarifications Q3 → Option B / spec
FR-007 / research.md §6):

| # | Capture | Parent | Triggered by |
|---|---|---|---|
| 1 | `@keyword` | (root) | reserved keywords `module`, `declare`, `struct`, `func`, `proc`, `state`, `goto`, `finish`, `return`, etc. |
| 2 | `@keyword.control` | `@keyword` | `alt`, `any`, `if`, `else`, `seq`, `for`, `while` |
| 3 | `@keyword.control.flow` | `@keyword.control` | `goto`, `return`, `finish` |
| 4 | `@keyword.modifier` | `@keyword` | `interface`, `simulation` |
| 5 | `@keyword.storage` | `@keyword` | `param_int`, `param_str`, `parameter` |
| 6 | `@type.builtin` | `@type` | `reg`, `wire`, `mem`, `integer`, `variable` (storage classes used as types) |
| 7 | `@type` | (root) | `module_block name:`, `declare_block name:`, `struct_declaration name:` |
| 8 | `@function.call` | `@function` | generic control-call fallback (specific kinds use sub-captures #11/#12) |
| 9 | `@constant.macro` | `@constant` | `macro_identifier` token |
| 10 | `@number` | (root) | `number_literal` token |
| 11 | `@string` | (root) | `string_literal` token |
| 12 | `@comment` | (root) | `line_comment` and `block_comment` tokens |
| 13 | `@variable.register` | `@variable` | declarations and references for `reg` |
| 14 | `@variable.wire` | `@variable` | declarations and references for `wire` |
| 15 | `@variable.memory` | `@variable` | declarations and references for `mem` |
| 16 | `@function.proc` | `@function` | `proc` definitions |
| 17 | `@function.func` | `@function` | `func` and `function` definitions |
| 18 | `@function.call.proc` | `@function.call` | invocation site of a `proc_name` |
| 19 | `@function.call.func` | `@function.call` | invocation site of a `func` |
| 20 | `@label.state` | `@label` | `state_name` declaration sites and `goto` targets |

> **Note**: total is 20 capture *names*. SC-003 asserts ≥ 17
> *distinct capture assertions*. The discrepancy is because
> some capture names (#10/#11/#12 `@number`/`@string`/`@comment`)
> are inherited as-is from the §4.3 base set without project-
> specific specialization, and the SC-003 count tracks the
> *required-minimum* assertions from the FR-007 list (the §4.3
> base set minus the bases superseded by sub-captures #13–#20,
> plus the 8 sub-captures themselves). The exact assertion
> count is verified by the golden test runner; the 17 number
> in SC-003 is the floor.

### 1.5 Smoke-fixture corpus  (NEW; references EXISTING `examples/*.nsl`)

The set of NSL files exercised by `tree-sitter parse` in the
smoke gate (FR-014 / SC-002).

| Field | Type | Notes |
|---|---|---|
| `path` | filesystem path (relative to repo root) | one entry per smoke-target file |
| `coverage_class` | enum {pre-P-VEN, post-P-VEN} | governs whether the entry is added unconditionally or only after P-VEN |

**Pre-P-VEN entries** (Clarifications Q4 → Option C):
20 paths, one per file in `examples/01_hello.nsl` ..
`examples/20_simulation_tb.nsl`.

**Post-P-VEN entries** (added when `P-VEN` lands at M7):
all `.nsl` / `.nslh` / `.inc` files under
`test/audited/{rv32x_dev,turboV,mmcspi,SDRAM_Controler,
mips32_single_cycle,ahb_lite_nsl,cpu16}/` (recursive).

**Storage**: a single line-list file
`test/tooling/treesitter/smoke/corpus.txt`. Comment lines
(prefix `#`) document classification but are stripped at
runtime.

### 1.6 Highlight golden fixture  (NEW)

An NSL file under `test/tooling/treesitter/highlights/` with
inline tree-sitter-test-format capture assertions per
research.md §8.

| Field | Type | Notes |
|---|---|---|
| `path` | filesystem path | typically named per the captures it exercises (e.g. `reg_vs_wire.nsl`) |
| `assertions` | list of (line, column-range, expected-capture) | inline `; <- @capture` style adjacent to fixture lines |

**Cardinality**: 6 fixture files (per plan.md "Project
Structure" tree):

| File | Captures exercised |
|---|---|
| `reg_vs_wire.nsl` | `@variable.register`, `@variable.wire` (declaration + reference, two captures × two sites = 4 assertions minimum) |
| `proc_vs_func.nsl` | `@function.proc`, `@function.func`, `@function.call.proc`, `@function.call.func` |
| `state_goto.nsl` | `@label.state` (declaration site), `@label.state` (goto target — same capture name; assertions test that both sites carry it) |
| `control_terminal_s27.nsl` | the dedicated control-terminal-value capture (FR-009) — exact name plan-level, distinct from #13–#20 |
| `macro_splice_ident.nsl` | `@constant.macro` (multiple `%IDENT%` positions: declaration width, expression operand, identifier substitute) |
| `parser_note_disambiguation.nsl` | exercises N5 (`#line` vs sign-extend `#expr`), N2 (reduction `&` vs bitwise), N3 (`.{`), N6 (proc-instance method access); asserts the parse tree doesn't mis-classify these |

### 1.7 Generated artefacts  (NEW; committed)

The output of `tree-sitter generate`. T8 commits all three
files per research.md §4.

| Field | Type | Notes |
|---|---|---|
| `parser.c` | C source | machine-generated; not human-reviewed; ~30k–60k lines |
| `grammar.json` | JSON | machine-generated; describes rules + tokens for downstream consumers |
| `node-types.json` | JSON | machine-generated; describes parse-tree node-types for tooling (graph/query authoring) |

**Coupling**: byte-identity to `grammar.js` is gated by
FR-017's regenerate-and-diff CI sub-step. The generated files
have **no SPDX header** (the upstream CLI's output has no
provision for one); a sibling `grammars/treesitter/SPDX.NOTICE`
file documents that they inherit the project license via the
generator (research.md §9). `scripts/check_spdx.py` is amended
to recognise the three paths as SPDX-exempted-via-NOTICE.

### 1.8 WASM artefact  (NEW; NOT COMMITTED)

The output of `tree-sitter build-wasm`.

| Field | Type | Notes |
|---|---|---|
| `tree-sitter-nsl.wasm` | WebAssembly module | NOT committed (Q2 → Option C); CI-built; uploaded as `actions/upload-artifact@v4` workflow artefact; tagged releases attach via `softprops/action-gh-release` |

**Determinism**: SC-008 / Constitution Principle V — CI builds
the WASM twice and `sha256sum`-compares. The
`tree-sitter-cli` pin (research.md §1) plus the
`emscripten/emsdk:3.x` docker-image pin (research.md §3)
together ensure byte-identical output.

### 1.9 VS Code extension shell  (NEW)

A TypeScript extension under `editors/vscode/treesitter/` that
loads the WASM artefact and applies highlight queries.

| Field | Type | Notes |
|---|---|---|
| `package.json` activation event | string | `onLanguage:nsl` |
| `extension.ts` `activate()` | function | calls `web-tree-sitter` `Parser.init({locateFile: …})`, loads `tree-sitter-nsl.wasm` from extension directory |
| `highlight-provider.ts` | class | implements `vscode.DocumentSemanticTokensProvider`; maps tree-sitter captures to VS Code `SemanticTokensLegend` |
| `SemanticTokensLegend` | array of `{tokenType, tokenModifiers}` | static; exactly 20 entries matching the captures in §1.4 |

**Coexistence with T1**: T1's `editors/vscode/package.json`
already registers the `nsl` language ID and its TextMate
contribution. T8 amends `package.json` to add the new
activation event plus the new contribution; the TextMate
contribution remains as the base layer.

---

## 2. Relationships

```
                                +-------------------------+
                                |  KeywordSet.def         |
                                |  (pre-existing, RO)     |
                                +-----+--------------+----+
                                      |              |
                                read by              read by
                                      |              |
                                      v              v
                       +--------------+--+    +-----+----------------+
                       | gen_treesitter   |   | gen_textmate         |
                       | _grammar.py (T8) |   | _grammar.py (T1)     |
                       +--------+---------+   +----------+-----------+
                                |                        |
                          generates                generates
                                |                        |
                                v                        v
                       +--------+---------+   +----------+-----------+
                       | grammar.js (T8)  |   | nsl.tmLanguage.json  |
                       | hand-authored    |   | (T1)                 |
                       | productions +    |   +----------------------+
                       | spliced kw block |
                       +--------+---------+
                                |
                       tree-sitter generate
                                |
                                v
                       +--------+----------------+
                       | parser.c, grammar.json, |
                       | node-types.json (T8)    |
                       +--------+----------------+
                                |
                       tree-sitter build-wasm
                                |
                                v
                       +--------+-------------+
                       | tree-sitter-nsl.wasm |        +-----------------------+
                       | (CI-built only)      +------> | VS Code extension     |
                       +----------------------+        | shell (T8)            |
                                                       +-----------------------+

                       +-----------------------+
                       | queries/highlights.scm|        +-----------------------+
                       | (T8, hand-authored)   +------> | applied by extension  |
                       +-----------------------+        | as DocumentSemantic   |
                                                        | TokensProvider        |
                                                        +-----------------------+

                       +----------------------+         +-----------------------+
                       | examples/*.nsl       |         | test/tooling/         |
                       | (M-track, RO)        +-------> | treesitter/smoke/     |
                       +----------------------+         | corpus.txt → smoke    |
                                                        | gate (FR-014)         |
                                                        +-----------------------+

                       +-----------------------+        +-----------------------+
                       | test/tooling/         |        | tree-sitter test      |
                       | treesitter/highlights/+------> | golden gate (FR-015)  |
                       | (T8 fixtures)         |        +-----------------------+
                       +-----------------------+
```

### Edge invariants

| Edge | Invariant | Enforced by |
|---|---|---|
| `KeywordSet.def` → `grammar.js` keyword block | byte-identical regeneration | CI stage 2 `treesitter-grammar-regen-diff` |
| `grammar.js` → `parser.c`/`grammar.json`/`node-types.json` | byte-identical regeneration with pinned `tree-sitter-cli` | CI stage 2 `treesitter-grammar-regen-diff` |
| `grammar.js` + `parser.c` → `tree-sitter-nsl.wasm` | byte-identical between two consecutive builds | CI stage 2 `treesitter-wasm-determinism` |
| `examples/*.nsl` ⊂ smoke-fixture set | every `.nsl` in `examples/` parses without `(ERROR)`/`(MISSING)` | CI stage 3 `treesitter-smoke` |
| `queries/highlights.scm` → golden fixtures | every required-minimum capture from §1.4 is asserted ≥ 1 time | CI stage 3 `treesitter-highlights-golden` |
| TextMate base layer (T1) ⊕ tree-sitter top layer (T8) | both apply to `.nsl`/`.nslh`/`.inc`; T8 overrides identifier captures only | VS Code's `DocumentSemanticTokensProvider` precedence rules |
| LSP `semanticTokens` (T4, future) > T8 > T1 | top-tier-wins precedence per `nsl_tooling_design.md §4` | VS Code's automatic semantic-tokens-over-TextMate behaviour; LSP uses higher-priority provider registration |

---

## 3. Lifecycle

T8 has a one-shot lifecycle (no runtime state). The artefact
build flow is:

1. **Author/edit `grammar.js`** — hand-edit productions or
   amend the keyword-block region; if `KeywordSet.def`
   changed, run `python scripts/gen_treesitter_grammar.py`
   to refresh the keyword block.
2. **Regenerate `parser.c`** — `npx tree-sitter generate`.
   Byte-equality with the committed `parser.c` is the
   regenerate-and-diff gate; if differences are intentional,
   stage them.
3. **Run smoke + golden tests locally** —
   `npx tree-sitter parse $(cat
   test/tooling/treesitter/smoke/corpus.txt)` and
   `npx tree-sitter test`.
4. **Build WASM (optional, for VS Code preview)** —
   `npx tree-sitter build-wasm --docker`.
5. **CI runs the same steps** — gates regenerate-and-diff
   (stage 2), smoke + golden (stage 3), WASM determinism
   (stage 2 separate sub-step), uploads WASM as workflow
   artefact (separate GHA step).

No state transitions; no persistence beyond git.
