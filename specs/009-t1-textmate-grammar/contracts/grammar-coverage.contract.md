<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract — Grammar Coverage

**Feature**: `009-t1-textmate-grammar`
**Phase**: 1 (design / contracts)
**Date**: 2026-05-04

This contract freezes the **(production → TextMate scope)**
binding table for T1. It is binding at the PR-landing point: any
change to a binding in this table requires updating both this
contract and the corresponding fixture-test assertion in the same
PR (Constitution Principles VII + VIII).

The "production" column refers to surface-syntax productions in
`docs/spec/nsl_lang.ebnf` (`§N`) or `docs/spec/nsl_pp.ebnf` (`§P.N`).

---

## §1 Reserved keywords (`lang.ebnf §15`)

The keyword set is enumerated in
`include/nsl/Lex/KeywordSet.def`. T1 reads that file as its
source of truth (research.md §6, data-model.md §1.1). The category
assignment below is the T1-introduced mapping (data-model.md §1.2).

### `keyword.declaration.nsl`

| Spelling | Source group |
|---|---|
| `declare` | Appendix-3 |
| `module` | Appendix-3 |
| `struct` | practical addition |

### `keyword.control.block.nsl`

| Spelling | Source group |
|---|---|
| `alt`, `any`, `seq`, `for`, `while`, `generate` | Appendix-3 |
| `if`, `else` | practical addition |

### `keyword.control.flow.nsl`

| Spelling | Source group |
|---|---|
| `goto`, `finish` | Appendix-3 |
| `return` | practical addition |

### `keyword.modifier.nsl`

| Spelling | Source group |
|---|---|
| `interface` | Appendix-3 |
| `simulation` | practical addition |

### `storage.type.register.nsl`

| Spelling | Source group |
|---|---|
| `reg` | Appendix-3 |

### `storage.type.wire.nsl`

| Spelling | Source group |
|---|---|
| `wire` | Appendix-3 |

### `storage.type.memory.nsl`

| Spelling | Source group |
|---|---|
| `mem` | Appendix-3 |

### `storage.type.integer.nsl`

| Spelling | Source group |
|---|---|
| `integer` | Appendix-3 |
| `variable` | Appendix-3 |

### `storage.type.param.nsl`

| Spelling | Source group |
|---|---|
| `param_int` | practical addition |
| `param_str` | practical addition |
| `parameter` | practical addition |

### `storage.type.control.nsl`

| Spelling | Source group |
|---|---|
| `func`, `func_in`, `func_out`, `func_self` | Appendix-3 |
| `proc`, `proc_name` | Appendix-3 |
| `state`, `state_name`, `label_name` | Appendix-3 |
| `label` (N10 — reserved-but-rare) | Appendix-3 |
| `function` (S26 synonym of `func`) | practical addition |
| `first_state` | practical addition |
| `invoke` | practical addition |

### `storage.modifier.direction.nsl`

| Spelling | Source group |
|---|---|
| `input`, `output`, `inout` | Appendix-3 |

### `support.type.clock.nsl`

| Spelling | Source group |
|---|---|
| `m_clock` (auto-synthesised clock per §15) | Appendix-3 |
| `p_reset` (auto-synthesised reset per §15) | Appendix-3 |

---

## §2 Built-in `_`-prefix names (parser note `N11`)

Frozen sets per data-model.md §1.3.

### `support.function.system.nsl`

`_display`, `_monitor`, `_write`, `_finish`, `_stop`, `_readmemh`,
`_readmemb`, `_init`, `_delay`

### `support.variable.system.nsl`

`_random`, `_time`

User-defined `_x` names that do NOT match the closed set above
remain **unscoped** at T1 (per `nsl_tooling_design.md §4.1`'s
"TextMate leaves [identifiers] un-scoped" rule).

---

## §3 Numeric literals (`lang.ebnf §13`)

Pattern ordering is significant — the grammar matches alternatives
in the order shown.

| Form | Regex | Scope |
|---|---|---|
| Verilog-sized | `\b\d+'[bBoOdDhH][0-9a-fA-FxXzZuU_]+` | `constant.numeric.verilog.nsl` |
| Hex (C-style) | `\b0[xX][0-9a-fA-F_]+` | `constant.numeric.hex.nsl` |
| Binary | `\b0[bB][01_]+` | `constant.numeric.binary.nsl` |
| Octal | `\b0[oO][0-7_]+` | `constant.numeric.octal.nsl` |
| Decimal | `\b\d[\d_]*` | `constant.numeric.decimal.nsl` |

**Underscore separators** (`1_000_000`, `8'b1010_0011`) are part
of the literal token (per `lang.ebnf §13`'s `digit_part`
rule). The whole literal — separators included — receives the
single numeric scope.

**Z/X/U value markers** (`8'hFZ_3X`, `4'bx10z`, `2'bUU`) are
matched by the Verilog-sized form's character class. The whole
literal receives `constant.numeric.verilog.nsl`.

---

## §4 String and comment forms (`lang.ebnf §14`, §13)

| Form | Regex | Scope |
|---|---|---|
| Line comment | `//[^\n]*` | `comment.line.double-slash.nsl` |
| Block comment | `/\*[^*]*\*+(?:[^/*][^*]*\*+)*/` | `comment.block.nsl` |
| String literal | `"(?:\\.|[^"\\])*"` | `string.quoted.double.nsl` |
| Backslash escape inside string | `\\.` | `constant.character.escape.nsl` |

Block comments are non-nestable per `lang.ebnf §14` ("NON-
nestable per Ref §0"). The TextMate `begin`/`end` rule honors
non-nesting natively.

---

## §5 Operators (`nsl_tooling_design.md §4.1`)

| Category | Scope |
|---|---|
| arithmetic | `keyword.operator.arithmetic.nsl` |
| bitwise | `keyword.operator.bitwise.nsl` |
| shift | `keyword.operator.shift.nsl` |
| comparison | `keyword.operator.comparison.nsl` |
| logical | `keyword.operator.logical.nsl` |
| assignment | `keyword.operator.assignment.nsl` |
| extension | `keyword.operator.extension.nsl` |

Token sets per category (operator literals listed in a code block
to avoid markdown-table column-counting issues with the literal
`|` and `||` operators):

```text
arithmetic   +  -  *  ++  --
bitwise      &  |  ^  ~
shift        <<  >>
comparison   ==  !=  <=  >=  <  >
logical      &&  ||  !
assignment   :=  =
extension    #  '
```

**Pattern ordering** within a category matters for
multi-character operators: `==` must match before `=`, `<=`
before `<`, `>=` before `>`, `<<` before `<`, `>>` before `>`,
`++` before `+`, `--` before `-`, `&&` before `&`, `||` before
`|`. The grammar generator emits longer patterns first.

**Best-effort disambiguation** (per FR-021):
- `&` / `|` / `^` are coloured under the bitwise category in all
  positions — the reduction-expression form (parser note N2) is
  context-sensitive and not detectable by regex. T8 (tree-sitter)
  refines.
- `#` is coloured under the extension category in all positions
  except line-start `#line` / other directives — the sign-extend
  form (parser note N5) shares the character. T8 refines.

---

## §6 Preprocessor directives (`pp.ebnf §2`)

All directives match at line start and receive
`keyword.directive.preprocessor.nsl`.

| Directive | Match regex |
|---|---|
| `#include` | `^\s*#include\b` |
| `#define`  | `^\s*#define\b` |
| `#undef`   | `^\s*#undef\b` |
| `#if`      | `^\s*#if\b` |
| `#ifdef`   | `^\s*#ifdef\b` |
| `#ifndef`  | `^\s*#ifndef\b` |
| `#else`    | `^\s*#else\b` |
| `#endif`   | `^\s*#endif\b` |
| `#line`    | `^\s*#line\b` |

Body of the directive line is colored under whatever scopes
its tokens normally carry (e.g. `#include "foo.nsl"` colors
`#include` as the directive scope and `"foo.nsl"` as the string
scope). This is the standard TextMate "directive prefix +
body-rules-still-apply" pattern.

---

## §7 Macro reference (`pp.ebnf §4`)

| Form | Regex | Scope |
|---|---|---|
| `%IDENT%` | `%[A-Za-z_][A-Za-z0-9_]*%` | `variable.other.macro.nsl` |

Distinct from the directive scope; readers see in-source
substitution sites at a glance.

---

## §8 What this contract intentionally does NOT cover

The following are explicitly **out** of T1; their scopes either
do not exist yet or are produced by a later milestone. Do not
add T1 grammar rules that try to assign these scopes — refinement
belongs at T4 / T8.

- `entity.name.module` / `entity.name.function.func` /
  `entity.name.function.proc` / `entity.name.function.state` —
  identifier scopes for declaration sites. `nsl_tooling_design.md
  §4.1` enumerates them but T1 does not assign them; declaration-
  site identifiers stay unscoped at T1. → T8 (tree-sitter).
- `variable.other.register` / `variable.other.wire` /
  `variable.other.memory` / `variable.parameter` /
  `variable.other.control` — reference-site identifier scopes.
  Same: T8 (tree-sitter) or T4 (LSP `semanticTokens`).
- The `S27` constructive constraint that control-terminal names
  used in expression position carry semantic value — T8
  refinement.
- `proc_name` vs `state_name` reference-context disambiguation
  per `N6` (proc-instance method access). T8.
- `func` vs `function` synonym recognition (S26) at the
  highlighter level — T1 colours both as `storage.type.control.nsl`
  with no synonym tagging; T4 / T8 may emit a quick-fix-style
  refinement.

---

## §9 Versioning / amendment policy

- **Adding a row** to any §1 sub-table when `KeywordSet.def`
  gains a keyword: update both this contract and the
  `gen_textmate_grammar.py` category map in the same PR. CI fails
  if either is missing per data-model.md §3.
- **Reclassifying** a keyword (e.g. moving `invoke` from
  `storage.type.control.nsl` to a new `keyword.control.flow.nsl`)
  is a breaking change at the user-theme level — VS Code themes
  may colour scopes differently. Reclassification PRs MUST list
  affected fixture assertions and update this contract.
- **Adding a new operator / new directive / new numeric form** as
  a result of a `lang.ebnf` or `pp.ebnf` amendment: same
  rule — update spec → update grammar → update this contract,
  all in one PR. Spec ↔ design coupling per Principle VII.
