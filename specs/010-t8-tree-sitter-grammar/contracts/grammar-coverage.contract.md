<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract — Grammar Coverage

**Feature**: `010-t8-tree-sitter-grammar`
**Phase**: 1 (design / contracts)
**Date**: 2026-05-05

This contract freezes the **rule-coverage table** between the
NSL spec (`docs/spec/nsl_lang.ebnf`, `docs/spec/nsl_pp.ebnf`)
and `grammars/treesitter/grammar.js`. CI's
`treesitter-grammar-regen-diff` sub-step (research.md §12,
plan.md "CI integration") gates coupling at the
`KeywordSet.def`-block level; this contract gates coupling at
the **rule-shape level**: every spec production listed here
MUST have a matching `grammar.js` rule, and every `grammar.js`
named rule MUST cite its spec anchor.

When a spec production is added, this table MUST gain a row in
the same change (Principle VII coupling).

---

## 1. Productions from `nsl_lang.ebnf`

| Spec § / lines | Production | `grammar.js` rule | Notes |
|---|---|---|---|
| §1 / 65–78 | compilation_unit | `source_file` | tree-sitter convention names the root rule `source_file` |
| §3 / 110–118 | struct_declaration | `struct_declaration` | named children: `name`, `field_list` |
| §3.1 / 120–136 | top_level_parameter | `top_level_parameter` | covers `param_int`, `param_str`, `parameter` |
| §4 / 138–178 | declare_block | `declare_block` | named children: `name`, `modifier`, `port_list` |
| §4 / 138–178 | declare_item | `_declare_item` (hidden) | choice of port / control-terminal / modifier |
| §5 / 180–194 | module_block | `module_block` | named children: `name`, `body` |
| §5 / 180–194 | module_item | `_module_item` (hidden) | choice of internal-structure / definition / action |
| §6 / 196–225 | reg_declaration | `register_declaration` | one node per declarator (multi-declarator → `seq` per Principle I "no silent AST drops" — except the M-track's AST owns that invariant; tree-sitter mirrors the shape) |
| §6 / 226–250 | wire_declaration | `wire_declaration` | same multi-declarator handling |
| §6 / 251–275 | mem_declaration | `memory_declaration` | depth + width fields |
| §6 / 276–290 | proc_name_declaration | `proc_name_declaration` | per S6 (proc_name args must be reg) |
| §6 / 291–314 | state_name_declaration | `state_name_declaration` | per S11 (state_name proc-scope) |
| §7 / 316–334 | func_definition | `func_definition` | covers `func` and `function` (S26 canonical: `func`); the keyword variation produces the same rule |
| §7 / 316–334 | proc_definition | `proc_definition` | named child: `name`, `body` |
| §7 / 316–334 | state_definition | `state_definition` | named child: `name`, `body` |
| §8 / 336–360 | par_block | `par_block` | parallel actions |
| §8 / 361–390 | alt_block | `alt_block` | priority alternation per S13 |
| §8 / 391–410 | any_block | `any_block` | parallel alternation per S13 |
| §8 / 411–430 | seq_block | `seq_block` | per S7 (seq only inside func/proc body) |
| §8 / 431–445 | if_statement | `if_statement` | named children: `condition`, `then`, `else` (N1 statement-vs-expression) |
| §8 / 446–455 | for_statement | `for_statement` | per S8/S9 |
| §8 / 456–462 | while_statement | `while_statement` | per S8 |
| §8 / 463–470 | generate_block | `generate_block` | per S10 (generate loop var must be integer); `init`, `condition`, `step`, `body` named fields |
| §9 / 472–500 | transfer_action | `transfer_action` | `=` and `:=` per S3 |
| §9 / 501–520 | control_call | `control_call` | named child: `callee` (matches both proc and func; sub-typing in highlights.scm via callee binding lookup) |
| §9 / 521–531 | finish_action | `finish_action` | `finish` keyword |
| §9 / 521–531 | system_task_call | `system_task_call` | `_display`, `_finish`, `_init`, `_delay`, etc.; per S17 (need simulation modifier) |
| §10 / 533–593 | system_task_arglist | `_system_task_arglist` (hidden) | inlined under `system_task_call` |
| §11 / 595–700 | expression | `_expression` (supertype) + concrete subrules | binary, unary, sign-extend `#`, zero-extend `'`, slice, concat, conditional |
| §11 / 595–700 | conditional_expression | `conditional_expression` | per S14 (else mandatory) |
| §11 / 595–700 | bit_slice | `bit_slice` | per S15 (indices compile-time — Sema enforces, parser accepts) |
| §11 / 595–700 | sign_extend_expr | `sign_extend_expr` | `#expr` form per N5 (line-marker disambiguation handled separately) |
| §11 / 595–700 | zero_extend_expr | `zero_extend_expr` | `'expr` form |
| §11 / 595–700 | concat_expression | `concat_expression` | `{a, b, c}` |
| §11 / 595–700 | dot_aggregate | `dot_aggregate` | `.{...}` per N3 (two-character lookahead) |
| §11 / 595–700 | reduction_op | `reduction_op` | unary `&`/`|`/`^` per N2 |
| §11 / 595–700 | bitwise_binary_op | `bitwise_binary_op` | binary `&`/`|`/`^` per N2 |
| §11 / 595–700 | proc_method_access | `proc_method_access` | `instance.finish()` / `instance.invoke()` per N6 + S21 |
| §12 / 707–712 | width_expr | `width_expr` | constant expressions (parser accepts; Sema constant-evaluates) |
| §12 / 707–712 | constant_expr | `constant_expr` | same |
| §13 / 714–740 | identifier | `identifier` (token) | `/[A-Za-z][A-Za-z0-9_]*/` |
| §13 / 741–760 | number_literal | `number_literal` (token) | choice of 5 numeric forms; `Z`/`X`/`U` markers |
| §13 / 761–770 | string_literal | `string_literal` (token) | with backslash escapes |
| §14 / 772–778 | line_comment | `line_comment` (extra token) | `extras` |
| §14 / 779–781 | block_comment | `block_comment` (extra token) | `extras` non-nestable |
| §15 / 783–824 | reserved keywords | `_keyword` (token rule) | **GENERATED** from `KeywordSet.def` by `scripts/gen_treesitter_grammar.py` |

---

## 2. Productions from `nsl_pp.ebnf`

| Spec § / lines | Production | `grammar.js` rule | Notes |
|---|---|---|---|
| §1 / 67–90 | top-level structure | absorbed into `source_file` | line-oriented; T8 accepts pre-preprocessor source |
| §2.1 / 103–120 | include_directive | `preprocessor_directive` | one rule covers all directives, with `kind` field (parsed at line-start) |
| §2.2 / 121–153 | define_directive | `preprocessor_directive` | same |
| §2.3 / 154–181 | conditional_directive | `preprocessor_directive` | `#if` / `#ifdef` / `#ifndef` / `#else` / `#endif` |
| §2.4 / 182–234 | line_directive | `preprocessor_directive` | `#line N "file"`; per N14 line-source-tracking (the parser preserves but the highlighter doesn't act on it) |
| §3 / 236–310 | compile_time_helper | absorbed into `_define_replacement` (hidden) | parser accepts the literal substring; highlighter scopes the helper-name token |
| §4 / 312–343 | macro_reference | `macro_identifier` (token) | `/%[A-Za-z_][A-Za-z0-9_]*%/`; the escape-hatch token consumed wherever an identifier or expression is expected |
| §5 / 345–390 | preprocessor lexical elements | reused via `extras` + same `identifier`/`number_literal` tokens | `_`-prefix names recognised by tree-sitter as identifiers; classification deferred to highlight queries (e.g. `_display` matches `system_task_call` rule, not generic identifier) |

---

## 3. Notes from `nsl_lang.ebnf` parser-note section

| Note | About | Tree-sitter handling |
|---|---|---|
| **N1** | `if` statement-vs-expression | `if_statement` named-rule and `conditional_expression` named-rule are distinct; production-position determines which fires (no regex disambiguation needed) |
| **N2** | `&` `|` `^` reduction-vs-bitwise | `reduction_op` (unary, expression-prefix position) vs `bitwise_binary_op` (binary, expression-infix); tree-sitter precedence levels disambiguate |
| **N3** | `.{` two-character lookahead | `dot_aggregate` rule uses `prec.dynamic()` + tree-sitter's native lookahead; correct by construction (T1 marked best-effort) |
| **N5** | `#` line-marker vs sign-extend | `preprocessor_directive` matched at line-start with `prec` boost; `sign_extend_expr` matched in expression position; correct by construction (T1 marked best-effort) |
| **N6** | proc-instance method access | `proc_method_access` rule with named `instance` and `method` children |
| **N10** | `label` reserved (mostly unused) | `label` is in the `_keyword` token list (generated from `KeywordSet.def`); rarely encountered in practice |
| **N11** | three classes of `_`-prefix names | system functions / system variables / user `_x` names; tree-sitter's lexer regex matches them all as `identifier`; classification happens in `highlights.scm` via specific rules |
| **N14** | `#line` source-location tracking | parsed but not acted on by the highlighter (location info lives in M-track AST; T8's parse tree is highlight-only) |

---

## 4. Coverage gate

The smoke gate (FR-014) is the dynamic enforcement of this
contract: every file in the smoke-fixture set must parse
without `(ERROR)` or `(MISSING)` nodes. If a production listed
in §1 or §2 above is exercised by `examples/*.nsl` and produces
an `(ERROR)` node, CI fails.

When a new production is added to `nsl_lang.ebnf` or
`nsl_pp.ebnf` post-T8:

1. Add a row to §1 or §2 above.
2. Add the corresponding rule to `grammars/treesitter/grammar.js`.
3. If the production interacts with the keyword set, regenerate
   the keyword-block via `scripts/gen_treesitter_grammar.py`.
4. Run `npx tree-sitter generate` to refresh `parser.c`.
5. If the new production needs a distinct highlight, add a
   capture to `queries/highlights.scm` and a golden-fixture
   assertion to `test/tooling/treesitter/highlights/`.
6. Update the production's spec anchor (line range cited in
   §1 / §2 above) when `docs/CLAUDE.md` line-range coupling
   shifts.

This is the spec-grammar-coupling contract for T8, parallel to
T1's `grammar-coverage.contract.md`.
