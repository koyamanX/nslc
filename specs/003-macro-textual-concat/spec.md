<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Feature Specification: Bare-Macro Textual Concatenation in `#define` / `#if`

**Feature Branch**: `003-macro-textual-concat`
**Created**: 2026-04-27
**Status**: Draft
**Input**: User description: "Implement bare-macro textual concatenation in preprocessor #define body and #if expression contexts (pp.ebnf P5 example DEPTH.0 → 8.0)"

> **Scope interpretation.** This is the deferred research-§12 #4
> follow-up from M1's [`research.md`](../002-m1-lex-preprocess/research.md)
> §12 + Track B (`worktree-agent-ad972c5ee88c7899d`) Open Question
> #3 from the M1 implementation. M1's `lib/Preprocess/PPExpression.cpp`
> implements bare-identifier macro references by *re-parsing the
> macro's body as a sub-expression* — robust for simple cases but
> incompatible with the canonical pp.ebnf P5 example
> `_int(_pow(2.0, DEPTH.0))`, which requires the macro `DEPTH` to be
> textually substituted into the surrounding token stream so that
> `DEPTH.0` post-substitution forms the float literal `8.0`. This
> spec pins the spec-level semantics + the implementation change.

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Author writes the canonical pp.ebnf P5 example without workarounds (Priority: P1)

A contributor authoring NSL preprocessor macros writes the literal
example shown in `docs/spec/nsl_pp.ebnf` P5 (lines 451–454):

```nsl
#define DEPTH 8
#define MEMDEPTH _int(_pow(2.0, DEPTH.0))
```

After preprocessing, `%MEMDEPTH%` substitutions in the user's code
emit the integer literal `256`. The bare identifier `DEPTH` inside
the helper-call argument list is **textually substituted** with its
macro body (`8`), yielding token sequence `…2.0, 8.0…` after
re-tokenization, which the helper evaluator then computes as
`_pow(2.0, 8.0) = 256.0` → `_int(…)` = `256`.

**Why this priority**: This is the canonical example in the project
spec — `nsl_pp.ebnf` P5 documents this exact pattern as the
intended user-facing semantics. Until it works, the spec is
internally inconsistent with the implementation (Principle VII
coupling violation, currently mitigated by the spec-amendment
approach this feature finalizes). Authors writing to the published
spec see a working example.

**Independent Test**: Write the two-line `#define` chain above plus
a passthrough line `reg buf[%MEMDEPTH%];`, run
`nslc -emit=tokens fixture.nsl`, observe that the emitted token for
the `%MEMDEPTH%` use site is `tk_decimal_lit 256`. Independent of
US2 (which exercises adjacency without dot) and US3 (which
exercises recursive expansion).

**Acceptance Scenarios**:

1. **Given** `#define DEPTH 8` and `#define MEMDEPTH _int(_pow(2.0, DEPTH.0))`,
   **When** the preprocessor expands `%MEMDEPTH%` at a use site,
   **Then** the emitted token is the integer literal `256`.
2. **Given** `#define WIDTH 8` and `#define MASK ((1 << WIDTH) - 1)`,
   **When** the preprocessor expands `%MASK%`, **Then** the emitted
   token is the integer literal `255` (the bare `WIDTH` is
   textually substituted with `8`, then expression evaluation
   yields `((1 << 8) - 1) = 255`).
3. **Given** `#define X 1` only, **When** the preprocessor sees
   `module foo { reg X; }` (NSL passthrough — no `#define` body
   and no `#if` condition), **Then** the bare identifier `X` is
   NOT expanded — it lexes as `tk_identifier "X"`. (P2 narrower-
   than-C invariant, unchanged.)

---

### User Story 2 — Adjacency-without-whitespace forms a single re-tokenized value (Priority: P2)

The pp.ebnf P5 example relies on `DEPTH` being adjacent to `.0`
(no whitespace separator). After textual splicing, the resulting
characters `8.0` re-tokenize as a single `float_literal`. With
whitespace separation, the two tokens stay separate.

**Why this priority**: Adjacency semantics are the load-bearing
detail that distinguishes textual splicing (`DEPTH` + `.0` → float
`8.0`) from token-level splicing (which would produce
`8 . 0` → three tokens). The pp.ebnf P5 example does not work
without it.

**Independent Test**: Two single-line fixtures —
`#define A 4` then `_int(A.5)` (adjacent) → integer `4` (truncated
from `4.5`); and `#define B 4` then `_int(B .5)` (whitespace) →
syntax error or distinct token sequence (`4`, `.`, `5`) per the
expression grammar.

**Acceptance Scenarios**:

1. **Given** `#define A 4` and a use site `_int(A.5)` (no
   whitespace between `A` and `.5`), **When** the preprocessor
   evaluates the helper, **Then** the result is the integer
   literal `4` (4.5 truncated toward zero).
2. **Given** `#define B 4` and a use site `_int(B .5)` (whitespace
   between `B` and `.5`), **When** the preprocessor evaluates,
   **Then** the diagnostic engine raises a parse error citing the
   stray `.5` (or another implementer-chosen behavior consistent
   with the pp.ebnf §3 expression grammar — pinned to "error" at
   the contract level).

---

### User Story 3 — Recursive bare-identifier expansion (Priority: P2)

Macros that reference other macros expand transitively at the
textual-substitution layer. `#define A B` + `#define B 8` makes
`%A%` produce `8`; equivalently in expression context, `#define X
A.0` with the same macro chain yields the float `8.0`.

**Why this priority**: Real-world preprocessor usage has chained
macros (e.g., `#define WORD_BITS 32` + `#define WORD_MASK
((1 << WORD_BITS) - 1)`). Without recursion, layered abstractions
break.

**Independent Test**: Three-level chain fixture:
`#define A B` → `#define B C` → `#define C 8` → `#define X _int(A.0)`
yields the integer literal `8`.

**Acceptance Scenarios**:

1. **Given** `#define A B`, `#define B 8`, and `#define X _int(A.0)`,
   **When** the preprocessor evaluates `%X%`, **Then** the result
   is the integer literal `8`. (`A` expands to `B`, `B` expands to
   `8`, then `8.0` is the float literal handed to `_int`.)
2. **Given** a self-referential cycle `#define A A` (or any
   transitive cycle `A → B → A`), **When** the preprocessor
   expands a use site referencing `A`, **Then** the expansion
   detects the cycle and emits a diagnostic citing
   `recursive macro expansion: <name>` (canonical message string,
   pinned by FR-007 below).

---

### Edge Cases

- A macro whose body is empty: `#define X` then `_int(X.0)` —
  textual splice produces `_int(.0)` which evaluates to `0`
  (`.0` is a valid float literal). The empty-body splice does not
  produce a syntax error.
- A macro whose body is itself a parenthesized expression:
  `#define X (1+2)` then `_int(X.0)` — the splice would produce
  `_int((1+2).0)` which is a syntax error (no `.` operator after
  `)`). This raises a parse-time diagnostic; it is not a textual-
  concat feature failure but a normal syntax error.
- A macro reference that does NOT match a defined macro:
  `_int(UNDEF.0)` — the bare identifier `UNDEF` is not defined,
  so it is left as-is. Expression evaluation then raises an
  "unknown identifier" error (existing FR-017 behavior).
- A `%IDENT%` form mixed with bare identifier: `_int(%A%.0)` and
  `_int(A.0)` MUST produce identical output for the same `#define
  A 8`. Both forms are textual-substitution semantics; the
  `%IDENT%` form is just explicit.
- `#if` condition with bare identifier: `#define DEPTH 8` then
  `#if DEPTH > 4` evaluates to true. Bare-identifier substitution
  applies inside `#if` exactly as inside `#define` body.
- Recursion-depth budget: a macro chain longer than 256 levels (the
  same bound used for `#include` cycles per M1 FR-022) raises the
  cycle diagnostic. This bound is implementation-specific but
  documented and not user-configurable in this feature.

## Requirements *(mandatory)*

### Functional Requirements

**Spec amendment (Principle VII coupling — load-bearing for this feature):**

- **FR-001**: `docs/spec/nsl_pp.ebnf` P10 (expansion ordering inside
  `#define` body) MUST be amended to make explicit that bare-
  identifier macro references in `#define` body and `#if` condition
  expressions are **textually substituted with the macro's body
  text** (similar to `%IDENT%` per P3) **before** the expression
  parser tokenizes the resulting characters. Adjacency rules
  match `%IDENT%`: characters from the substituted body adjoin the
  surrounding source text without an inserted separator. The result
  is then re-tokenized and evaluated per P10 step 3 (operator-
  precedence reduction).
- **FR-002**: `docs/spec/nsl_pp.ebnf` §3 `pp_primary_expr` rule MUST
  remain unchanged in shape (the rule still lists `identifier` as a
  primary form). The amendment in FR-001 governs *how* identifiers
  are resolved before the rule is applied — not the rule itself.

**Implementation deltas (in M1's `lib/Preprocess/`):**

- **FR-003**: The preprocessor MUST perform bare-identifier macro
  substitution as a textual pre-pass over the `#define` body /
  `#if` condition characters, BEFORE the expression parser
  tokenizes the input. The substitution operates on the raw
  characters; re-tokenization happens after all substitutions
  resolve.
- **FR-004**: When a bare identifier is recognized as a defined
  macro, its replacement is the macro's body text verbatim
  (matching the `%IDENT%` splice contract from P3). No automatic
  whitespace insertion at substitution boundaries.
- **FR-005**: Recursive expansion is performed transitively. If
  `#define A B` and `#define B 8`, the bare identifier `A`
  resolves to `8` after two substitution rounds.
- **FR-006**: A recursion cycle is detected via a per-expansion
  call-stack depth bound of **256 levels** (matching the M1
  FR-022 include-cycle bound for consistency). Exceeding the
  bound raises a diagnostic and aborts the current expression
  evaluation.
- **FR-007**: The cycle-diagnostic message text is **locked** at
  `recursive macro expansion: <NAME>` per the FR-037-locked
  diagnostic-string discipline established in M1. The
  `<NAME>` placeholder is the macro that triggered cycle
  detection (typically the outermost name in the cycle).
- **FR-008**: The textual-substitution pre-pass MUST run BEFORE
  helper-call evaluation (P10 step 2) so that helper arguments
  see the substituted form. Implementer note: this changes
  P10's step ordering subtly — the existing step 1
  (`%IDENT%` splices) and the new bare-identifier substitution
  occur together as "all textual substitutions"; only after
  ALL textual substitutions resolve does step 2 (helper
  evaluation) run.
- **FR-009**: The bare-identifier substitution is restricted to
  `#define` replacement bodies and `#if` condition expressions.
  Bare identifiers in NSL passthrough lines (module / declare
  bodies, etc.) are NOT macro-expanded (P2 narrower-than-C
  semantics is preserved).

**Test gates (per Constitution Principles VI + VIII):**

- **FR-010**: A new test directory `test/preprocess/p05/` MUST gain
  a fixture exercising the canonical pp.ebnf P5 example
  (`_int(_pow(2.0, DEPTH.0))` → `256`). The fixture replaces or
  augments the existing `p05/pass.test` from M1 (which currently
  uses the `_real(%DEPTH%)` workaround).
- **FR-011**: A new test directory `test/preprocess/p10/` MUST
  gain a fixture exercising recursive bare-identifier expansion
  (the 3-level chain from US3). Replaces or augments the
  existing `p10/pass.test`.
- **FR-012**: A new fixture `test/preprocess/p10/cycle.fail.test`
  MUST exercise the cycle detection (FR-006 + FR-007). The
  CHECK line cites the FR-007-locked diagnostic string verbatim.
- **FR-013**: The unit suite at `test_unit/helper_evaluator_test/`
  MUST gain coverage for the textual-substitution behavior of
  the helper-call argument list (US1 acceptance scenario 1, at
  the unit level).
- **FR-014**: The two M1 fixtures that currently use workarounds
  for the missing textual concat feature
  (`test/preprocess/p12/pass.test` and the
  `_real(%DEPTH%)` form in `test/preprocess/p10/pass.test`) MAY
  be reverted to the canonical `<MACRO>.<digit>` form per pp.ebnf
  P5. **Optional**, but a clean-up indicator that the
  amendment has fully closed the M1 workaround surface.
- **FR-015**: Per Principle VIII TDD, fixtures MUST land BEFORE
  the implementation change. The test-author commit MUST be
  observed failing on the M1-vintage `lib/Preprocess/PPExpression.cpp`
  (which only handles sub-expression-style identifier
  resolution).

**Determinism (Principle V):**

- **FR-016**: The textual-substitution pre-pass MUST be a pure
  function of `(input expression text, macro table state)`. No
  environment-derived inputs (CWD, mtime, etc.) influence the
  substituted output.
- **FR-017**: An undefined bare identifier inside a `#define`
  body / `#if` condition is left as-is (no substitution; no
  diagnostic at substitution time). The downstream expression
  parser raises an "unknown identifier" diagnostic when it
  encounters the unsubstituted identifier as a primary expression
  with no resolvable value. This matches the M1 behavior and is
  unchanged by this feature.

### Key Entities

- **Bare-identifier substitution context**: the set of preprocessor
  contexts where bare identifiers are eligible for macro expansion.
  At M1+this feature: `#define` replacement body + `#if` condition
  expression. Excludes: NSL passthrough lines, `#include` paths
  (where filenames are quoted strings or angle-bracketed paths,
  not expressions), `#error` / `#warning` body text (none defined
  in this spec).
- **Substitution depth counter**: per-expression call-stack depth
  tracking macro expansion recursion. Bounded at 256 levels to
  detect cycles (FR-006).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The pp.ebnf P5 canonical example
  (`_int(_pow(2.0, DEPTH.0))` with `#define DEPTH 8`) produces the
  integer literal `256` end-to-end through `nslc -emit=tokens`.
  100% of the new `p05/` fixture acceptance scenarios pass.
- **SC-002**: The two M1 workaround fixtures
  (`test/preprocess/p10/pass.test` and `test/preprocess/p12/pass.test`)
  can be reverted to the canonical pp.ebnf form (`%DEPTH%.0` ↔
  `DEPTH.0` equivalence per US1 acceptance scenario 4) without
  test regressions. Either form passes; both forms equivalent.
- **SC-003**: A 3-level macro chain (`A → B → C → 8`) resolves
  end-to-end in under 1 ms on the reference Linux x86_64 host —
  i.e., recursive expansion is not a perf cliff. (Informal at this
  feature's milestone; tighter perf budgets defer to M7's audited-
  corpus measurement basis.)
- **SC-004**: A 256-deep cycle (`A → A` self-ref or any transitive
  cycle reaching the depth bound) raises the FR-007-locked
  diagnostic exactly once per cycle, and the preprocessor exits
  cleanly (exit code 1; no stack overflow / SIGSEGV / infinite
  loop).
- **SC-005**: M1's existing 113 lit + ctest fixtures continue to
  pass after this feature's implementation lands. **No
  regressions** in M1 acceptance criteria SC-001..SC-009.
- **SC-006**: The `pp.ebnf` line count stays within ±2 lines of the
  current 559 (the spec amendment in FR-001 is a clarifying
  paragraph, not a section restructure). Avoids cascading
  `docs/CLAUDE.md §5` line-reference drift.

## Assumptions

- **Scope is the M1+this-feature delta**: no new layers, no new
  libraries, no new driver flags. Implementation lives in
  `lib/Preprocess/PPExpression.cpp` (or a dedicated new file
  `lib/Preprocess/MacroSubstitution.cpp` per implementer
  preference) plus the existing `Preprocessor.cpp` that calls
  into it.
- **Adjacency rules match `%IDENT%`**: per P3's commentary
  ("splice the replacement TEXT into the surrounding identifier
  text"), bare-identifier substitution behaves identically with
  respect to adjacent characters. The `DEPTH.0` example's
  `8.0` post-splice token is the same shape as `%DEPTH%.0`'s
  post-splice token.
- **Recursion budget at 256** mirrors M1 FR-022 for consistency.
  Not user-configurable in this feature; a future
  `--max-macro-depth` flag would be a separate spec change.
- **Bare-identifier substitution does NOT happen in NSL passthrough
  lines** — preserves pp.ebnf P2 narrower-than-C semantics. Only
  `#define` body + `#if` condition contexts gain the new behavior.
- **The `%IDENT%` and bare-identifier forms are equivalent inside
  the eligible contexts** — same body text spliced, same adjacency
  rules, same recursion budget. The `%IDENT%` form is a stylistic
  convenience for hand-readability (the explicit delimiters help
  human readers) but is functionally redundant with the bare form
  inside `#define`/`#if`.
- **The cycle-detection diagnostic message** uses the FR-007-locked
  string `recursive macro expansion: <NAME>`. Picked to be terse
  and machine-parseable; matches M1's FR-037 locked-string
  discipline.
- **No spec amendment to `lang.ebnf`** is required by this feature.
  The amendment is contained to `pp.ebnf` P10.
- **No tooling-track impact** (T1–T12). Editor-grammar work, LSP
  features, lint rules — none affected. The textual concat is
  entirely in the preprocessor's expression-evaluation path,
  which doesn't surface in editor tooling.
- **Out of scope**: function-like macros (`#define F(x) ...`).
  pp.ebnf §2.2 explicitly says "object-like macros only — not
  function-like macros." This feature does not change that
  restriction.
