<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Data Model — T1 TextMate Grammar + Language Configuration

**Feature**: `009-t1-textmate-grammar`
**Phase**: 1 (design / data-model)
**Date**: 2026-05-04

T1 is a static-artefact feature — no runtime database, no
persistence, no in-memory state. The "data model" here describes
the **schemas** of the four data artefacts T1 ships and the
**relationships** between them.

---

## 1. Entities

### 1.1 `KeywordSet.def` entry  (PRE-EXISTING, READ-ONLY consumer)

The X-macro single-source-of-truth for the NSL reserved-keyword
set. T1 does not modify this file; it reads it.

| Field | Type | Source | Notes |
|---|---|---|---|
| `enum_suffix` | identifier (`[a-z_][a-z0-9_]*`) | `KeywordSet.def` | becomes `tk_<enum_suffix>` in the lexer |
| `spelling` | string | `KeywordSet.def` | the literal source text — what the grammar must match |
| `group` | enum {`appendix3`, `practical`} | `KeywordSet.def` (group comments) | passed through to T1 only as informational; both groups receive the same TextMate scope category |

**Cardinality**: as of 2026-05-04, **42** entries (31 Appendix-3 +
11 practical additions). Tracks `nsl_lang.ebnf §15` per Principle I.

### 1.2 Token-category mapping  (NEW — T1-introduced)

A category-mapping table held inside
`scripts/gen_textmate_grammar.py`. Maps every keyword spelling to
its TextMate scope category sub-name per
`docs/design/nsl_tooling_design.md §4.1`.

| Field | Type | Notes |
|---|---|---|
| `spelling` | string | matches `KeywordSet.def` |
| `category` | enum (see below) | one of 8 keyword sub-categories |

**Category enum** (frozen at T1 PR-landing):

| Category | TextMate sub-scope | Spellings (as of 2026-05-04) |
|---|---|---|
| `declaration` | `keyword.declaration.nsl` | `declare`, `module`, `struct` |
| `control_block` | `keyword.control.block.nsl` | `alt`, `any`, `if`, `else`, `seq`, `for`, `while`, `generate` |
| `control_flow` | `keyword.control.flow.nsl` | `goto`, `return`, `finish` |
| `modifier` | `keyword.modifier.nsl` | `interface`, `simulation` |
| `storage_type` | `storage.type.{register,wire,memory,integer,param,control}.nsl` | `reg`, `wire`, `mem`, `integer`, `variable`, `param_int`, `param_str`, `parameter`, `func`, `function`, `func_in`, `func_out`, `func_self`, `proc`, `proc_name`, `state`, `state_name`, `first_state`, `label_name`, `label`, `invoke` |
| `port_direction` | `storage.modifier.direction.nsl` | `input`, `output`, `inout` |
| `support_type_clock` | `support.type.clock.nsl` | `m_clock`, `p_reset` |

**Cardinality**: ≥ 1 entry per `KeywordSet.def` row. The mapping
is exhaustive and asserted in `scripts/gen_textmate_grammar.py`'s
top-of-file consistency check (raises if a `KeywordSet.def`
spelling has no entry).

**`port_direction` rationale**: `inout` / `input` / `output` are
listed in `nsl_lang.ebnf §15` as reserved keywords but
`nsl_tooling_design.md §4.1` does not enumerate a port-direction
scope. T1 introduces `storage.modifier.direction.nsl` to honour
FR-001 (every keyword highlighted) and FR-002 (sub-categorised
per design §4.1 *intent*); the new sub-scope follows the
TextMate naming convention that `interface`/`simulation` use.

### 1.3 Built-in `_`-prefix system-name set  (NEW — T1-introduced)

Hand-authored list inside `scripts/gen_textmate_grammar.py`,
mirroring `nsl_tooling_design.md §4.1`.

| Field | Type | Values (frozen at T1) |
|---|---|---|
| `support_function_system` | string set | `_display`, `_monitor`, `_write`, `_finish`, `_stop`, `_readmemh`, `_readmemb`, `_init`, `_delay` |
| `support_variable_system` | string set | `_random`, `_time` |

Both sets receive the corresponding TextMate scope:
`support.function.system.nsl` and `support.variable.system.nsl`.

**Out of scope**: preprocessor helpers (`_int`, `_pow`, `_sin`, …)
listed in `pp.ebnf §3.1` — those never appear in `.nsl` source
because the preprocessor consumes them at the seam (P12). See
research.md §3.

### 1.4 Numeric-literal forms  (NEW — T1-introduced)

Hand-authored regex set inside `scripts/gen_textmate_grammar.py`,
mirroring `nsl_lang.ebnf §13` and `nsl_tooling_design.md §4.1`.

| Form | Example | Regex (Oniguruma) | Scope |
|---|---|---|---|
| Verilog-sized | `8'hFZ_3X`, `4'b1010`, `8'd255`, `3'o5` | `\b\d+'[bBoOdDhH][0-9a-fA-FxXzZuU_]+` | `constant.numeric.verilog.nsl` |
| Hex (C-style) | `0xDEADBEEF`, `0xFF_00` | `\b0[xX][0-9a-fA-F_]+` | `constant.numeric.hex.nsl` |
| Binary | `0b1010`, `0b1100_0011` | `\b0[bB][01_]+` | `constant.numeric.binary.nsl` |
| Octal | `0o17`, `0o12_34` | `\b0[oO][0-7_]+` | `constant.numeric.octal.nsl` |
| Decimal | `42`, `1_000_000` | `\b\d[\d_]*` | `constant.numeric.decimal.nsl` |

**Pattern ordering matters**: Verilog-sized must match before bare
decimal so `8'hFF` doesn't fragment into `8` + `'hFF`. The
generator's pattern list is order-sensitive — first-match wins
per Oniguruma's standard alternation semantics.

### 1.5 Operator categories  (NEW — T1-introduced)

| Category | Scope | Tokens |
|---|---|---|
| arithmetic | `keyword.operator.arithmetic.nsl` | `+`, `-`, `*`, `++`, `--` |
| bitwise | `keyword.operator.bitwise.nsl` | `&`, `\|`, `^`, `~` |
| shift | `keyword.operator.shift.nsl` | `<<`, `>>` |
| comparison | `keyword.operator.comparison.nsl` | `==`, `!=`, `<`, `<=`, `>`, `>=` |
| logical | `keyword.operator.logical.nsl` | `&&`, `\|\|`, `!` |
| assignment | `keyword.operator.assignment.nsl` | `=`, `:=` |
| extension | `keyword.operator.extension.nsl` | `#`, `'` |
| repeat | `keyword.operator.repeat.nsl` | `{`, `}` (in `{N{X}}` repeat-expression context) |

**Disambiguation note**: regex cannot distinguish reduction `&`
from bitwise `&` (parser note N2) nor sign-extend `#` from
preprocessor-line `#` (parser note N5). Per FR-021, T1 colours
both cases under the same scope; refinement is T8 (tree-sitter)
work.

### 1.6 Preprocessor directive set  (NEW — T1-introduced)

| Directive | Match pattern | Scope |
|---|---|---|
| `#include` | `^\s*#include\b` | `keyword.directive.preprocessor.nsl` |
| `#define` | `^\s*#define\b` | same |
| `#undef` | `^\s*#undef\b` | same |
| `#if` | `^\s*#if\b` | same |
| `#ifdef` | `^\s*#ifdef\b` | same |
| `#ifndef` | `^\s*#ifndef\b` | same |
| `#else` | `^\s*#else\b` | same |
| `#endif` | `^\s*#endif\b` | same |
| `#line` | `^\s*#line\b` | same |

All 9 directives match at line start (per `nsl_pp.ebnf §1`
line-orientation invariant). The line-start anchor is what makes
`#line` highlight as a directive while `#` in `q := #a + b;`
matches as the sign-extend operator under FR-008/FR-021.

### 1.7 Macro-reference form  (NEW — T1-introduced)

| Form | Match pattern | Scope |
|---|---|---|
| `%IDENT%` | `%[A-Za-z_][A-Za-z0-9_]*%` | `variable.other.macro.nsl` |

Per `nsl_pp.ebnf §4` and FR-007. Distinct scope from identifier
(`variable.other.*`) and keyword scopes — readers can see the
preprocessor seam at a glance.

### 1.8 Comment forms  (NEW — T1-introduced)

| Form | Match pattern | Scope |
|---|---|---|
| Line comment | `//.*$` | `comment.line.double-slash.nsl` |
| Block comment | `/\* … \*/` (non-nestable) | `comment.block.nsl` |

Block comments are non-nestable per `nsl_lang.ebnf §14`; the
TextMate begin/end pair handles non-nesting natively.

### 1.9 String-literal form  (NEW — T1-introduced)

| Element | Match pattern | Scope |
|---|---|---|
| String body | `"…"` | `string.quoted.double.nsl` |
| Backslash escape | `\\.` (any char after backslash inside a string) | `constant.character.escape.nsl` |

Strings are NSL-source-level — distinct from preprocessor
`#include` quoted form (which uses the same delimiters and
receives the string scope by inheritance from inside the
directive line).

### 1.10 Scope-test fixture  (NEW — T1-introduced)

A representative `.nsl` file with embedded inline scope
assertions in the format accepted by `vscode-tmgrammar-test`.

| Field | Type | Notes |
|---|---|---|
| `path` | filesystem path | one fixture per category file (see plan.md project-structure) |
| `lines` | NSL source | exemplar tokens; ≥ 1 per category |
| `assertions` | inline `// >…^^^…` lines | one assertion per token under test |

Assertion format (vscode-tmgrammar-test convention):
```nsl
declare hello {}
// <- keyword.declaration.nsl
//      ^^^^^ entity.name.declaration.nsl  ← (out of scope at T1)
```
The `// <-` form points at column 1 of the line above; the
`//        ^^^^^` form points at the marked range.

Assertions for declaration-name tokens (`hello` above) are
**omitted** at T1 because identifier scopes are deferred to T4/T8
per FR-020.

### 1.11 VS Code language-configuration  (NEW — T1-introduced)

Schema fields and required values are documented in
`contracts/language-config.contract.md`. Summary:

| Field | Required value |
|---|---|
| `comments.lineComment` | `"//"` |
| `comments.blockComment` | `["/*", "*/"]` |
| `brackets` | `[["{","}"], ["[","]"], ["(",")"]]` |
| `autoClosingPairs` | `{}`, `[]`, `()`, `""`  (NOT `''`) |
| `surroundingPairs` | same four |
| `wordPattern` | identifier-with-numeric-suffix regex |
| `indentationRules.increase` | `^.*\{[^}"']*$` |
| `indentationRules.decrease` | `^\s*\}` |

### 1.12 VS Code extension manifest  (NEW — T1-introduced)

Schema documented in research.md §7. Required top-level fields:
`name`, `displayName`, `description`, `version`, `engines`,
`categories`, `contributes.languages`, `contributes.grammars`.

---

## 2. Relationships

```
┌─────────────────────────────────┐
│ docs/spec/nsl_lang.ebnf §15     │   ← authoritative
│  (reserved_keyword production)  │
└─────────────────────────────────┘
                │
                │  manually mirrored (Principle I)
                ▼
┌─────────────────────────────────┐
│ include/nsl/Lex/KeywordSet.def  │   ← single source of truth
│  (X-macro: KEYWORD(suffix,spell))│
└─────────────────────────────────┘
        │           │           │           │
        │           │           │           │
        ▼           ▼           ▼           ▼
   TokenKind    KeywordSet  gen_keyword  gen_textmate
   enum         .cpp        _fixtures.py _grammar.py     (NEW T1)
                            (existing)         │
                                               ▼
                                  grammars/textmate/
                                    nsl.tmLanguage.json
                                               │
                                               ▼
                              gen_textmate_fixtures.py    (NEW T1)
                                               │
                                               ▼
                              test/tooling/textmate/
                                fixtures/all-keywords.nsl
                                scope-tests/all-keywords.spec
                                               │
                                               ▼
                                  vscode-tmgrammar-test
                                  (CI stage 3 sub-step)
```

The diagram makes Principle VII coupling visible: every consumer
of the keyword set reads `KeywordSet.def`. Adding a keyword
touches `KeywordSet.def` once; CI then forces every consumer
to regenerate, including the new T1 grammar.

---

## 3. State transitions

T1 ships static artefacts; there are no runtime state machines.

The **build-time / CI-time state machine** for the canonical
artefact is:

```
          ┌─────────────────────────────────────────────┐
          │ KeywordSet.def edited                       │
          └────────────────────┬────────────────────────┘
                               │
                               ▼
          ┌─────────────────────────────────────────────┐
          │ developer runs gen_textmate_grammar.py +    │
          │   gen_textmate_fixtures.py                  │
          └────────────────────┬────────────────────────┘
                               │
                               ▼
          ┌─────────────────────────────────────────────┐
          │ git diff includes:                           │
          │  - grammars/textmate/nsl.tmLanguage.json     │
          │  - test/tooling/textmate/fixtures/all-key…   │
          │  - test/tooling/textmate/scope-tests/all-…   │
          └────────────────────┬────────────────────────┘
                               │
                               ▼
                    ┌──────────┴──────────┐
                    │                     │
                  green                  red
                  (PR merges)         (CI fails — see below)
```

**Red-state classification** (which CI sub-stage fails for which
omission):

| Omission | Failing sub-stage | Failing check |
|---|---|---|
| Forgot to regenerate grammar | stage 2 (static-checks) | `gen_textmate_grammar.py && git diff --exit-code` |
| Forgot to regenerate fixtures | stage 2 (static-checks) | `gen_textmate_fixtures.py && git diff --exit-code` |
| Regenerated but forgot to commit | stage 3 (unit & layer) | `vscode-tmgrammar-test` finds new keyword in fixture without grammar entry → assertion fails |
| Added keyword to grammar without `KeywordSet.def` | stage 2 | re-run shows divergence |

---

## 4. Validation rules

Each is enforced by code (not policy):

| Rule | Enforcement |
|---|---|
| Every `KeywordSet.def` entry has a category-mapping entry | `gen_textmate_grammar.py` raises on missing entry |
| Every category-mapping entry's spelling matches a `KeywordSet.def` row | same script raises on orphan entry |
| Generated grammar JSON parses as valid JSON | `python -c "json.load(...)"` (cheap) — runs in stage 2 |
| Grammar JSON carries `_comment_top` SPDX header | `scripts/check_spdx.py` (already runs in stage 2) |
| Generated grammar matches what's in git | `git diff --exit-code` on regen — stage 2 |
| Fixture corpus contains ≥ 1 occurrence per `KeywordSet.def` row | `gen_textmate_fixtures.py` invariant |
| Every fixture assertion targets an actual fixture line | `vscode-tmgrammar-test` reports orphaned assertion |
| Every fixture line has at least one assertion | the runner's coverage check (configured) |
| `language-configuration.json` parses as valid JSON | stage 2 |
| `editors/vscode/syntaxes/nsl.tmLanguage.json` is the same content as the canonical (symlink target or build-step copy) | stage 2 byte-equality check |
