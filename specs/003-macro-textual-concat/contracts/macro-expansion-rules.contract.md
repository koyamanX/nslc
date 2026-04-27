<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: Bare-Macro Textual Substitution Rules

**Owner**: `lib/Preprocess/MacroExpander.{h,cpp}` (new) + `lib/Preprocess/PPExpression.cpp` + `lib/Preprocess/IdentSplicer.cpp` (caller updates)
**Spec FRs**: FR-001..FR-009 (semantics) + FR-016/FR-017 (determinism)
**Spec SCs**: SC-001 (P5 example), SC-004 (cycle detection), SC-005 (no M1 regression)

This contract pins the **substitution semantics** so that any
implementer (today's PR, a future cleanup, or a downstream layer
that wants to reuse `MacroExpander`) can verify their
implementation against an external reference rather than
re-deriving from the spec.

## Substitution scope

Bare-identifier macro substitution is performed inside:

- **`#define` replacement bodies** (the text between the macro
  name and end-of-line). Per pp.ebnf P10 step 1 (amended).
- **`#if` condition expressions** (the text between `#if` and
  end-of-line). Per pp.ebnf P10 step 1 (amended).

It is NOT performed inside:

- NSL passthrough lines (module / declare bodies, expressions
  inside `module { }`). Per pp.ebnf P2 narrower-than-C semantics.
- `#include` paths (file names are quoted strings or angle-form,
  not subject to macro expansion).
- `#error` / `#warning` body text (none defined in NSL spec).
- String-literal content within a `#define` body or `#if`
  condition. Identifier scanning skips from `"` to matching `"`.

## Adjacency rules

Substituted text adjoins the surrounding source characters
**without** an inserted separator. Examples:

| Macro definition | Use site | Post-substitution | Token |
|---|---|---|---|
| `#define A 8` | `A.0` | `8.0` | `tk_float_lit` |
| `#define A 8` | `A` (alone) | `8` | `tk_decimal_lit` |
| `#define A 8` | `A_extra` | `A_extra` (still one identifier; not substituted because the lookup is exact-match against a contiguous identifier span) | `tk_identifier` |
| `#define A 8` | `A 0` (whitespace) | `8 0` | `tk_decimal_lit`, `tk_decimal_lit` (two tokens) |
| `#define A 8` | `_A` (leading underscore) | `_A` | `tk_system_*` (per N11) |

The "no whitespace insertion" rule is identical to pp.ebnf P3's
`%IDENT%` rule and ensures the canonical pp.ebnf P5 example
(`DEPTH.0` → `8.0`) works without surprises.

## Identifier-scanning rules

The expander recognizes identifiers using the same character set
as `lang.ebnf §13`:

```
identifier = [A-Za-z_][A-Za-z0-9_]*
```

The scan is **greedy**: the longest run of identifier characters
is taken as one identifier. So `A_extra` is scanned as a single
identifier (not as `A` + `_extra`); `A.extra` is scanned as `A`,
then `.`, then `extra` (three lexical positions).

A leading-`_` identifier IS subject to lookup. Per pp.ebnf §3, the
preprocessor's expression grammar treats helper names (`_int`,
`_pow`, ...) and N11 system names (`_random`, `_time`, ...) as
identifiers in the same lexical class. Lookup against `MacroTable`
uses exact match; if `_int` happens to be `#define`'d by user
code, the user's macro wins. The expander does NOT special-case
the closed-set helper names.

## Recursive expansion

Substituted body text is itself scanned for further macro
references and substituted recursively:

```
#define A B
#define B C
#define C 8

A      → B → C → 8                    (3 substitutions; depth = 3)
A.0    → B.0 → C.0 → 8.0              (same chain; result is float)
```

Recursion is performed at the **expander level**, not at the
parser level (the parser sees only the fully-substituted
character stream).

## Cycle detection

The expander tracks recursion depth via an explicit counter passed
through recursive calls. **Bound: 256 levels** (matches M1
FR-022's `#include` cycle bound for consistency).

When the depth limit is exceeded, the expander emits the
**FR-007-locked diagnostic**:

```
<path>:<line>:<col>: error: recursive macro expansion: <NAME>
```

The `<NAME>` placeholder is the name of the macro that triggered
cycle detection — typically the outermost name in the cycle, which
matches the `<NAME>` from the user's source perspective.

After emitting the diagnostic, the expander returns the
ORIGINAL (unsubstituted) text of the cycle-triggering identifier
unchanged. The downstream expression parser then surfaces an
"unknown identifier" or "unresolved expression" error, but the
expansion itself does not abort the entire `nslc` run — the cycle
is treated as a recoverable error (one diagnostic, then continue
processing other input).

## Determinism (FR-016)

Two `MacroExpander::expand()` invocations with the same arguments
MUST produce byte-identical output. This means:

- The macro-table iteration order does NOT influence the output
  (the expander uses `lookup()` only, not iteration).
- No environment-derived inputs (CWD, mtime, locale, hostname, env
  vars) influence the substitution.
- The cycle-detection counter is a function of the call stack, not
  of any global state.

## Mandatory diagnostic-string fixtures (FR-007 + FR-037 lock)

Per the M1 FR-037 discipline, the **cycle-detection fail-fixture**
(`test/preprocess/p10/cycle.fail.test`) MUST cite the exact
diagnostic message string. The locked text:

```
recursive macro expansion: <NAME>
```

(The `<NAME>` placeholder is replaced with the actual macro name
in any specific test's CHECK line — e.g., `recursive macro
expansion: A` for an `#define A A` self-cycle.)

Renaming or weakening this string later requires a contract
amendment in this file PLUS updating the corresponding
fail-fixture in the same change.

## Out of scope (recorded for future polish)

The following are deliberately NOT in this contract; future
follow-up PRs may extend the contract:

- `note: expanded from macro <NAME>` — per-expansion stack-trace
  notes. Track via Linear feature-track issue.
- Function-like macros (`#define F(x) ...`). pp.ebnf §2.2
  explicitly excludes; not in scope here.
- A `--max-macro-depth` CLI flag to override the 256-level bound.
  Not in scope; would require a Principle II flag-introduction
  PR.
