<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract — Highlight Captures Coverage

**Feature**: `010-t8-tree-sitter-grammar`
**Phase**: 1 (design / contracts)
**Date**: 2026-05-05

This contract freezes the **capture-name set** that T8's
`grammars/treesitter/queries/highlights.scm` MUST emit, and
the **parse-tree shapes** that produce each capture. CI's
`treesitter-highlights-golden` sub-step (research.md §12,
plan.md "CI integration") gates this contract.

The capture set is locked by Clarifications session
2026-05-05 Q3 → Option B and codified in spec.md FR-007.

---

## 1. Required-minimum capture set

| # | Capture | Triggered by | Sub-capture parent | Mandatory golden assertion? |
|---|---|---|---|---|
| 1 | `@keyword` | reserved keywords matched by `_keyword` token, **except** the sub-categorised ones below | (root) | yes — ≥ 1 occurrence |
| 2 | `@keyword.control` | `alt`, `any`, `if`, `else`, `seq`, `for`, `while` | `@keyword` | yes — ≥ 1 occurrence |
| 3 | `@keyword.control.flow` | `goto`, `return`, `finish` | `@keyword.control` | yes — ≥ 1 occurrence |
| 4 | `@keyword.modifier` | `interface`, `simulation` | `@keyword` | yes — ≥ 1 occurrence |
| 5 | `@keyword.storage` | `param_int`, `param_str`, `parameter` | `@keyword` | yes — ≥ 1 occurrence |
| 6 | `@type.builtin` | `reg`, `wire`, `mem`, `integer`, `variable` (storage-class keywords used as types) | `@type` | yes — ≥ 1 occurrence |
| 7 | `@type` | `module_block name:`, `declare_block name:`, `struct_declaration name:` | (root) | yes — ≥ 1 occurrence |
| 8 | `@function.call` | generic control-call fallback (after #18/#19 sub-captures) | `@function` | optional — covered transitively if #18/#19 fire |
| 9 | `@constant.macro` | `macro_identifier` token (`%IDENT%`) | `@constant` | yes — ≥ 1 occurrence |
| 10 | `@number` | `number_literal` token | (root) | yes — ≥ 1 occurrence |
| 11 | `@string` | `string_literal` token | (root) | yes — ≥ 1 occurrence |
| 12 | `@comment` | `line_comment` and `block_comment` tokens | (root) | yes — ≥ 1 occurrence each (line + block) |
| 13 | `@variable.register` | `register_declaration name:` (declaration site) AND `_expression` references resolved to a `register_declaration` binding | `@variable` | **yes — minimum sub-capture per FR-007** |
| 14 | `@variable.wire` | `wire_declaration name:` AND references resolved to a `wire_declaration` binding | `@variable` | **yes — minimum sub-capture per FR-007** |
| 15 | `@variable.memory` | `memory_declaration name:` AND references resolved to a `memory_declaration` binding | `@variable` | **yes — minimum sub-capture per FR-007** |
| 16 | `@function.proc` | `proc_definition name:` (definition site) | `@function` | **yes — minimum sub-capture per FR-007** |
| 17 | `@function.func` | `func_definition name:` (definition site) | `@function` | **yes — minimum sub-capture per FR-007** |
| 18 | `@function.call.proc` | `control_call callee:` resolved to a `proc_definition` binding | `@function.call` | **yes — minimum sub-capture per FR-007** |
| 19 | `@function.call.func` | `control_call callee:` resolved to a `func_definition` binding | `@function.call` | **yes — minimum sub-capture per FR-007** |
| 20 | `@label.state` | `state_definition name:` AND `goto_statement target:` resolved to a `state_definition` | `@label` | **yes — minimum sub-capture per FR-007** |

**Total required-minimum distinct assertions**: **17**
(captures #1–#7, #9–#12 contribute 11 assertions if #12
counts both line + block; sub-captures #13–#20 contribute 8;
#8 is transitive). SC-003 floor.

The implementation MAY add further sub-captures (e.g. a
dedicated control-terminal capture for FR-009, distinct from
#13–#20) provided their parents stay in this set
(spec.md Assumptions, capture-name-namespace).

**Reference-site resolution split.** Captures #13–#20 in the
table above describe the captures that the highlight set as a
whole produces; the production mechanism splits between the
tree-sitter query files and the consumer runtime:

| Position | Mechanism |
|---|---|
| Declaration sites | `queries/highlights.scm` field-binding patterns |
| LHS of `:=` / `=` operator | `queries/highlights.scm` operator-position patterns |
| Statement-position call site (`proc()`) | `queries/highlights.scm` `(control_call callee:)` pattern |
| Expression-position call site (`func()`) | `queries/highlights.scm` `(call_expression function:)` pattern |
| RHS-of-`=`/`:=` reference, S27 expression-position control terminal | **Consumer-runtime scope walk** — `editors/vscode/treesitter/highlight-provider.ts` resolves the identifier to its declaration and applies the declaration's capture name |

The standalone `tree-sitter test` runner exercises only the
first four rows above (the pure-syntactic captures). The last
row is verified manually via the US3 VS Code smoke-test
attestations (AS3.2). `grammars/treesitter/queries/locals.scm`
declares scopes + definitions for the VS Code provider's
scope walker; tree-sitter-cli's standard highlighter does NOT
propagate `@local.definition.<NAME>` to `@local.reference`
sites automatically (that is an editor-specific extension in
consumers like Helix). The goldens therefore assert only the
syntactic-resolvable sites; the per-fixture notes inside
`reg_vs_wire.nsl` and `control_terminal_s27.nsl` document the
runtime-verified positions explicitly.

---

## 2. Specific FR-009 control-terminal capture

FR-009 requires a dedicated capture for control-terminal names
used in expression position (per `S27` constructive constraint
— a control terminal evaluates to a 1-bit value). The exact
capture name is **plan-level** but MUST be **distinct from
captures #13–#20** in §1.

**Recommended name** (plan-level): `@variable.builtin.terminal`
or `@constant.builtin.terminal`. The implementation chooses
one; whichever is chosen MUST be used consistently across
`highlights.scm` and the
`test/tooling/treesitter/highlights/control_terminal_s27.nsl`
golden fixture.

---

## 3. Goldens — file-by-file capture-coverage table

The 6 golden fixtures under `test/tooling/treesitter/
highlights/` collectively cover every required-minimum
capture from §1.

| Fixture | Captures asserted | Min assertion count |
|---|---|---|
| `reg_vs_wire.nsl` | #13 `@variable.register` (decl + ref), #14 `@variable.wire` (decl + ref), #15 `@variable.memory` (decl + ref) | 6 |
| `proc_vs_func.nsl` | #16 `@function.proc`, #17 `@function.func`, #18 `@function.call.proc`, #19 `@function.call.func` | 4 |
| `state_goto.nsl` | #20 `@label.state` (definition + goto target = 2 sites of same capture) | 2 |
| `control_terminal_s27.nsl` | dedicated control-terminal capture per §2 | 1 |
| `macro_splice_ident.nsl` | #9 `@constant.macro` (≥ 3 splice positions: width, expression operand, identifier substitute) | 3 |
| `parser_note_disambiguation.nsl` | exercises N5/N2/N3/N6; no specific capture-coverage role beyond demonstrating that captures fire correctly when N5/N2/N3/N6 disambiguation runs | 0 (parse-shape correctness; covered by smoke gate) |
| **(any of the above with reserved keywords)** | #1–#7, #10, #11, #12 | distributed naturally across the fixtures |

---

## 4. Coverage gate

The golden gate (FR-015) is the dynamic enforcement of this
contract: every assertion in every golden fixture must match
the capture emitted by `queries/highlights.scm`.

When a new sub-capture is added to `highlights.scm`:

1. Add a row to §1 above (or §2 if it's the FR-009 control-
   terminal capture).
2. Add a corresponding inline assertion in the relevant
   golden fixture (or a new fixture if the construct doesn't
   appear in any existing one).
3. Run `npx tree-sitter test` and confirm the new assertion
   passes.

When a sub-capture is *removed* (rare; would be a regression
in coverage), the change MUST be Justified-by-deferral —
i.e. the responsibility moves up the three-tier precedence to
the LSP `semanticTokens` (T4) tier. The Justification MUST
land in spec.md's Assumptions block and a `Complexity
Tracking` entry MUST appear in plan.md.

---

## 5. Theme-mapping responsibility (out of scope)

T8 produces capture names; consumer themes map them to
colours. T8 does NOT ship themes. The mapping is a downstream
concern, owned by:

- **VS Code**: VS Code's built-in semantic-token theming maps
  `@variable.register` → the theme rule for
  `variable.register` if defined, falling back to `variable`.
  T8's extension shell does NOT amend any theme.
- **Neovim / Emacs / etc.** at T11: the editor's theme
  conventions map captures to colours; T8 produces standard
  tree-sitter capture names so existing conventions apply.

If a theme produces sub-optimal colour distinctions for NSL,
that's a theme bug (or a feature request to upstream theme
authors), not a T8 bug.
