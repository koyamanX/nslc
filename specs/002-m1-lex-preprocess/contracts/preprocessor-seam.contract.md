<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: Preprocessor → Lexer Seam

**Owner**: `lib/Preprocess/Preprocessor.cpp` (emitter) + `lib/Lex/Lexer.cpp` (consumer)
**Spec FRs**: FR-013–FR-023 (preprocessor) + FR-005–FR-012 (lexer)
**Spec SCs**: SC-002, SC-003

This contract pins what crosses the preprocessor → lexer seam
(`pp.ebnf` note **P12** boundary), the closed set of compile-time
helpers, and the `#line` round-trip semantics.

## P12 boundary — what is allowed to cross the seam

After the preprocessor finishes, the input the lexer sees MUST
consist of **exactly one of**:

1. NSL tokens per `lang.ebnf` §13–15 (lexical grammar).
2. Canonical `#line` directives (per **P13**, see below).
3. Whitespace and comments (skipped by the lexer per `lang.ebnf §14`).

The following MUST NOT cross the seam:

- `#include`, `#define`, `#undef`, `#ifdef`, `#ifndef`, `#if`,
  `#else`, `#endif` — all consumed by the preprocessor.
- `%IDENT%` references — all resolved by the preprocessor (P3).
- Compile-time helper calls (`_int`, `_real`, `_pow`, …) — all
  evaluated by the preprocessor (P6).
- Float literals (e.g., `2.5e3`) — preprocess-time only (P7).
- Variant-3 form of `#line` — normalized to variant 1 or 2 (P13
  emitter rule).

The preprocessor MUST run an internal **post-pass assertion** under
non-NDEBUG builds that walks the output buffer and fires a hard
abort if any of the forbidden artifacts is observed (FR-021). In
release builds the post-pass is skipped for performance; the
fixture corpus catches the same regressions via FileCheck on
`-emit=tokens` output.

## Compile-time helper closed set (FR-017)

The closed set is the 22 identifiers enumerated in `pp.ebnf §3`
lines 282–288, reproduced verbatim here as the implementation's
authoritative reference:

```
_int     _real    _pow     _sqrt
_sin     _cos     _tan
_asin    _acos    _atan
_sinh    _cosh    _tanh
_log     _log10   _exp
_floor   _ceil    _round
_abs     _min     _max
```

The single source of truth is `include/nsl/Basic/HelperSet.def`
(research §6 — X-macro `.def` file consumed by both
`lib/Preprocess/HelperEvaluator.cpp` and the per-helper test
fixtures). Adding or removing an entry is a `pp.ebnf §3` amendment
that propagates here automatically.

### Helper return-type table (FR-017, P5)

| Helper | Arity | Returns | Behavior |
|--------|-------|---------|----------|
| `_int`   | 1 | integer | truncate-toward-zero (per pp.ebnf §3 commentary) |
| `_real`  | 1 | real    | widen integer to long double |
| `_pow`   | 2 | real    | `std::powl(b, e)` |
| `_sqrt`  | 1 | real    | domain error on `<0` (research §10) |
| `_sin` / `_cos` / `_tan` | 1 | real | per `<cmath>` long-double overloads |
| `_asin` / `_acos` | 1 | real | domain error outside `[-1, 1]` |
| `_atan`  | 1 | real    | full range |
| `_sinh` / `_cosh` / `_tanh` | 1 | real | per `<cmath>` |
| `_log`   | 1 | real    | natural log; domain error on `<=0` |
| `_log10` | 1 | real    | base-10 log; domain error on `<=0` |
| `_exp`   | 1 | real    | `e^x`; overflow → warning |
| `_floor` / `_ceil` / `_round` | 1 | real | per `<cmath>` |
| `_abs`   | 1 | int OR real | preserves arg kind (research §5) |
| `_min` / `_max` | 2 | int OR real | preserves common kind; widen to real if mixed |

### `#if` truth test (P4)

`#if <expr>` evaluates `<expr>` to a `PPValue` (integer or real)
per the helper-evaluator semantics above. The truth test is
"non-zero" on either kind. **No string comparison** — `#ifdef` is
the symbol-presence form for symbol-driven branching.

## `#line` round-trip semantics (P13, FR-020, FR-021, FR-035)

### Three input variants

The preprocessor recognizes all three variants from `pp.ebnf §2.4`:

1. `#line <decimal-int>`
2. `#line <decimal-int> <string-literal>`
3. `#line <anything-else>` — macro-expanded, then the result MUST be
   re-parsed as variant 1 or 2.

### Two output forms (canonical only)

The preprocessor MUST emit ONLY variant 1 or variant 2 in the
output stream — variant 3 is normalized away. Format:

```
#line <decimal-int>
#line <decimal-int> <string-literal>
```

with exactly one space between tokens, no leading whitespace,
trailing newline.

### Round-trip golden invariants (FR-035, golden `test/preprocess/line/`)

For every input `#line` variant N that lands in the input stream:
- The preprocessor's own `(file, line)` cursor is updated such that
  the next input line cites the new coordinates in any diagnostic.
- Exactly **one** canonical `#line` directive is emitted in the
  output stream (variant 1 if no path; variant 2 if a path; variant
  3 inputs map to whichever of 1/2 their post-expansion form
  resolves to).
- Every token emitted *after* the directive in the output stream
  carries a `SourceRange` that, when resolved through
  `SourceManager::resolveVirtual`, returns the post-`#line` virtual
  coordinates.
- A `#line LINENUM=0` directive is permitted; the next input line
  is numbered `1`.

### Implementation-side invariants (P13 implementation guidance)

- At the very start of an `#include`'d file's expanded content,
  emit `#line 1 "<filename>"` (variant 2). Tested by the include-
  stack golden in `test/preprocess/include-stack/`.
- On return from an `#include`, emit a variant-2 `#line` directive
  re-establishing the outer file's location at the line *after*
  the `#include` directive.

## Include-search-path order (P8, FR-013)

### Quote form (`#include "file"`)

Search order:
1. Directory of the file containing the `#include` directive.
2. Each `-I <dir>` argument from the CLI, in registration order.

### Angle form (`#include <file>`)

Search order: each entry of the `NSL_INCLUDE` environment variable
(colon-separated on POSIX), in PATH order. **`-I` is NOT consulted
for angle-form includes.**

`NSL_INCLUDE` is read **once** at `Preprocessor` construction time;
re-reading it on every `#include` would be a determinism leak
(Principle V — env vars MUST NOT vary mid-run).

## Cycle detection (FR-022)

`Preprocessor` maintains an `IncludeFrame` stack with a hard depth
cap of **256 levels**. Exceeding the cap raises:

```
<path>:<line>:<col>: error: #include cycle detected: <path-trace>
```

where `<path-trace>` is the absolute path of every frame in the
stack from outermost to innermost, joined by ` → `.

The 256 cap is **not user-configurable at M1**. A future PR adding
a `--max-include-depth` flag is a contract amendment.

## Macro expansion order inside `#define` body (P10)

Per **P10**:
1. `%IDENT%` splices first — look up the inner identifier in the
   `MacroTable` and splice the replacement TEXT (not the value —
   splicing is textual).
2. Helper calls evaluated left-to-right.
3. Operator-precedence reduction per `pp.ebnf §3` precedence table.

The final value (integer or real) becomes the macro's value for
future expansions and `#if` comparisons.

### Edge case: empty `#define` body

`#define X` (legal per pp.ebnf §2.2). A `%X%` reference produces
zero tokens at the use site; an `#if X` raises a "macro `X` has
empty body and cannot be evaluated as expression" diagnostic.

## Validation tests (per FR-034)

Every `Pn` (P1–P13) ships a passing fixture; every Pn with a
violatable rule ships a failing fixture asserting the canonical
diagnostic string from
[`diagnostic-output.contract.md`](./diagnostic-output.contract.md).

| Pn | Pass-fixture topic | Fail-fixture topic (if any) |
|----|--------------------|-----------------------------|
| P1 | line-orientation: `#define X 1` at column 0 vs column 5 | indented `  #define` rejected as preprocessor directive |
| P2 | passthrough: identifiers on non-directive lines NOT macro-expanded (only `%IDENT%` and helper-call sites are) | (none — invariant) |
| P3 | `#define W 8` then `reg buf_%W% [W];` | `%UNDEF%` → "undefined macro reference: '%UNDEF%'" |
| P4 | `#if 2+3 > 0` selects then-branch | (covered by P9 mismatched-conditional) |
| P5 | `#define X _int(_pow(2.0, 8.0))` → `256` | (none — invariant) |
| P6 | helpers permitted in `#define` body and `#if` condition | helper outside #define → "compile-time helper '_<NAME>' used outside #define / #if condition" |
| P7 | float in `#define` body reduced via `_int(...)` before substitution | float survives the seam → "float literal cannot cross the preprocessor seam" |
| P8 | `#include "f"` resolves via `-I`; `#include <f>` resolves via `NSL_INCLUDE` | (search-path failure raises generic "could not find include") |
| P9 | nested `#ifdef`/`#endif` correctly paired | extra `#endif` → "'#endif' without matching..."; missing `#endif` at EOF → "unterminated #if at end of file" |
| P10 | %IDENT% splice precedes helper eval inside `#define` body | (covered by P3 + P6) |
| P11 | one-line directive limit: `#define X` ends at newline | (none — invariant; backslash continuation not supported) |
| P12 | full pipeline: NSL tokens + `#line` only crosses the seam | (covered by P6 + P7) |
| P13 | `#line` round-trip golden (see above) | (none — assertion-only) |
