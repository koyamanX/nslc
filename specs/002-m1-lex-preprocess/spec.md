<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Feature Specification: M1 — Lex + Preprocess (with Diagnostic Plumbing)

**Feature Branch**: `002-m1-lex-preprocess`
**Created**: 2026-04-27
**Status**: Draft
**Input**: User description: "Implement Project milestone M1"

> **Scope interpretation.** "Project milestone M1" maps to **M1** in
> [`README.md`](../../README.md) §Roadmap, which delivers the first
> three compiler-track libraries: `nsl-basic` (1) +
> `nsl-preprocess` (2) + `nsl-lex` (3). The same row defines the
> milestone's test gate ("Lexer tests on token streams (Z/X/U digits,
> `_`-prefix, `%IDENT%` macros, all reserved keywords); preprocessor
> tests covering directive set; `#line` round-trip golden") and its
> constitutional anchors (IV diagnostics; VI lexer + preprocessor
> tests). The NSL-feature → milestone roll-up in
> [`CLAUDE.md`](../../CLAUDE.md) §1 confirms which language-spec rows
> land here. Per [`README.md`](../../README.md) §Roadmap, "real
> `-emit=` flags arrive incrementally from M1 onward" — this spec
> includes the `nslc -emit=tokens` CLI surface as the M1 increment.

## Clarifications

### Session 2026-04-27

- Q: Compile-time helper evaluation — does the preprocessor
  evaluate the 22 helpers (`_int`/`_real`/`_pow`/`_sin`/…) at M1
  per pp.ebnf P5/P6/P7/P12, or does evaluation defer to Sema (M3)
  per CLAUDE.md §1's "M1 (parse); M3 (eval)" row? → A: **Preprocessor
  evaluates at M1** (Option A). The pp.ebnf clauses are load-bearing:
  without preprocess-time eval, `#if` cannot select branches and
  P5/P6/P7/P12 are unenforceable, leaving the directive set
  non-functional. CLAUDE.md §1's "M3 (eval)" row is interpreted as
  NSL-language Sema constant evaluation (S15 bit-slice indices etc.)
  — a different evaluator from the preprocessor's. CLAUDE.md §1 gets
  a Principle-VII coupling-fix patch in a follow-up PR (not in this
  feature) to disentangle the two evaluators.
- Q: JSON-mode diagnostic output — at M1, is it (A) wired behind
  `--diagnostic-format=json` with a parse-only smoke test (schema +
  consumer deferred to T3), (B) exercised end-to-end at M1 with a
  versioned schema + per-kind round-trip goldens + a partial T3 LSP
  harness, or (C) deferred entirely to T3 (no JSON path at M1)? →
  A: **Smoke-only JSON at M1** (Option A). M1 ships
  `--diagnostic-format=json` plus a parse-only smoke test; schema
  lock, schema versioning, per-diagnostic-kind round-trip goldens,
  and LSP consumer wiring all defer to T3. The schema is best
  designed against a real LSP consumer, so locking it in a vacuum
  at M1 risks rework.
- Q: Is the `nslc -emit=tokens` driver flag (plus `-I` / `-D` /
  `NSL_INCLUDE` plumbing) in M1 scope, or library-only-at-M1 with
  the driver flag deferred to M2 / M1.5? → A: **Include
  `nslc -emit=tokens` at M1** (Option A). FR-029 + FR-030 stand.
  This honors the M0 spec's binding handoff "real `-emit=` flags
  arrive incrementally from M1 onward." `nsl-driver` gains a thin
  `EmitTokens` code path; `tools/nslc/main.cpp` gains `-emit=tokens`
  argument handling — both kept minimal so the driver remains
  ≤60 lines per Principle II.

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Lex any NSL source file into a deterministic, source-locating token stream (Priority: P1)

A contributor pipes an NSL source file (post-preprocess, or a
preprocess-free fixture) through the lexer and receives back a token
stream covering every lexical construct in
[`docs/spec/nsl_lang.ebnf`](../../docs/spec/nsl_lang.ebnf) §§13–15:
identifiers, decimal / hex / binary / octal numeric literals
(including the `Z` / `X` / `U` "value" digits), string literals, the
full reserved-keyword set, `_`-prefix system names per parser-note
**N11**, and whitespace / comment skipping. Every emitted token
carries a `SourceRange` whose endpoints round-trip to the original
file byte offsets.

**Why this priority**: Every subsequent compiler milestone (M2 parser,
M3 sema, M4 dialect, …, M9 release) consumes tokens. Without a lexer
no parser exists; without a parser nothing else lands. M1 is the
first milestone that produces a structured artifact a downstream
layer can read, and the lexer is the closer-to-the-source half of
M1's two-deliverable pair (the other being the preprocessor in US2).

**Independent Test**: Build the project, run the per-keyword and
per-number-form lexer fixtures (one input per item in
`nsl_lang.ebnf §15` plus one per row of the §13 numeric grid: base ×
{plain, Z, X, U}). Assert each fixture produces the expected token
stream and that every emitted token's `SourceRange` matches the
known-correct byte offsets in the input. Run `nslc -emit=tokens
fixture.nsl` on a representative input and observe the same token
stream printed deterministically. Does not depend on US2 (the lexer
operates on raw bytes; preprocessor output is just one possible
input).

**Acceptance Scenarios**:

1. **Given** a fixture file containing every reserved keyword from
   `nsl_lang.ebnf §15`, **When** the lexer runs, **Then** each
   keyword is emitted as the corresponding token kind and no keyword
   is misclassified as an identifier.
2. **Given** a fixture file containing the cross product of {decimal,
   hex, binary, octal} × {plain digits, `Z`, `X`, `U`} numeric
   forms, **When** the lexer runs, **Then** each form is recognized
   as a numeric literal token with its category preserved and its
   source range exact.
3. **Given** an identifier whose name begins with one of the three
   `_`-prefix classes from parser-note **N11** (`_int`, `_display`,
   `_unused`-style), **When** the lexer runs, **Then** the token
   kind reflects the N11 classification and the source range
   matches.
4. **Given** input containing both the `#` line-marker form and the
   `#`-as-sign-extend expression form (parser-note **N5**), **When**
   the lexer runs, **Then** the `#` is classified per the N5
   disambiguation rule and the test fixture for both forms passes.
5. **Given** a fixture mixing single-line `//`, block `/* … */`
   comments, and whitespace (`nsl_lang.ebnf §14`), **When** the
   lexer runs, **Then** comments and whitespace are skipped from the
   token stream but their byte spans do not corrupt the source
   ranges of adjacent tokens.
6. **Given** a successful build, **When** the contributor runs `nslc
   -emit=tokens fixture.nsl` twice on the same input, **Then** the
   stdout is byte-identical between the two invocations (Principle
   V determinism).

---

### User Story 2 — Preprocess any NSL source file (full directive set + `%IDENT%` splicing + helper evaluation) (Priority: P1)

A contributor preprocesses an NSL source file containing the full
preprocessor directive set defined in
[`docs/spec/nsl_pp.ebnf`](../../docs/spec/nsl_pp.ebnf): `#include`
in both quote and angle forms (P8), `#define` / `#undef` with
compile-time math in replacement bodies, `#ifdef` / `#ifndef` /
`#if` / `#else` / `#endif` with arbitrary nesting (P9), `%IDENT%`
splicing inside identifiers (P3), and `#line` directives (P13).
The output is a token stream containing only NSL tokens plus
canonical-form `#line` markers (P12 boundary) — every other
directive, every `%IDENT%` reference, every `_int` / `_pow` / `_sin`
helper call, and every float literal has been resolved before
crossing the preprocessor → lexer seam.

**Why this priority**: Every audited NSL project in the verified
corpus (`rv32x_dev`, `turboV`, `mmcspi`, `SDRAM_Controler`,
`mips32_single_cycle`, `ahb_lite_nsl`, `cpu16`) uses `#include` and
`#define` heavily. Without the preprocessor no real-world NSL
compiles, so M2 (parser) cannot exercise its acceptance gate
(grammar-production AST snapshots) on anything that resembles
production NSL. US2 is the other half of M1's two-deliverable pair
and shares P1 with US1.

**Independent Test**: For every preprocessor note **P1–P13**, run
the corresponding fixture pair (one passing input that exercises
the note, one failing input that violates it where applicable per
Constitution Principle VI's pass+fail rule). Assert the passing
fixture produces the expected normalized token stream, and the
failing fixture produces a diagnostic citing the offending line and
the relevant `Pn`. Run a `#line` round-trip golden test (the
milestone's named test gate): preprocess input containing a
`#line` directive in each of the three variants from
`nsl_pp.ebnf §2.4`, assert the output stream contains a single
canonical (variant 1 or 2) `#line` per input directive, and assert
the location of every emitted token after the directive matches
the post-`#line` coordinates. Does not depend on US1's full
keyword/numeric coverage — it depends only on the preprocessor's
ability to emit *some* token stream and on the diagnostic engine
from US3.

**Acceptance Scenarios**:

1. **Given** a fixture exercising `#include "file"` (quote form,
   P8), **When** the preprocessor runs with the documented `-I`
   include-search path list, **Then** the included file's
   preprocessed contents are inserted at the directive site,
   bracketed by canonical `#line` markers per P13.
2. **Given** a fixture exercising `#include <file>` (angle form,
   P8), **When** the preprocessor runs with `NSL_INCLUDE`
   environment variable set, **Then** the file is found via
   `NSL_INCLUDE` (and not via `-I`) and inserted as in scenario 1.
3. **Given** a fixture defining `#define MEMDEPTH _int(_pow(2.0,
   DEPTH.0))` followed by `#define DEPTH 8` (P5 + P10 ordering),
   **When** the preprocessor expands `%MEMDEPTH%` at a use site,
   **Then** the emitted token is the integer literal `256` (the
   evaluated result), not the unexpanded helper call.
4. **Given** a fixture nesting `#ifdef A` … `#if B+1` … `#else` …
   `#endif` … `#endif` (P9), **When** the preprocessor runs with
   `A` defined and `B` defined as `0`, **Then** the `#else` branch
   of the inner `#if B+1` is selected, and a malformed nesting
   (e.g., extra `#endif`) raises a diagnostic per P9.
5. **Given** a fixture using `reg buf_%W% [W];` with `#define W 8`
   (P3), **When** the preprocessor runs, **Then** the output
   contains the single identifier token `buf_8` (splice into
   identifier text) followed by `[`, `W` (bare identifier
   passthrough — see P3's commentary), `]`, `;`.
6. **Given** a fixture invoking a compile-time helper outside a
   `#define` body or an `#if` condition (P6 violation), **When**
   the preprocessor runs, **Then** the unexpanded helper call
   triggers a "compile-time helper outside #define / #if" error
   diagnostic at preprocess time, and no helper call survives
   into the output stream (P12).
7. **Given** a fixture whose `#define` body produces a float
   literal that is never reduced via `_int(...)` before
   substitution (P7 violation), **When** the preprocessor runs,
   **Then** a "float crossed the preprocessor seam" diagnostic is
   raised before the lexer sees the float.
8. **Given** an input file containing a `#line 100 "synth.v"`
   variant 1 directive followed by ordinary NSL source on the
   next line, **When** the preprocessor runs, **Then** the output
   token stream contains a canonical `#line 100 "synth.v"`
   directive followed by tokens whose `SourceRange` reports
   `synth.v:100:…` for the next input line (P13 round-trip).
9. **Given** an input file using a `#line` variant 3 form (the
   "anything-else" variant), **When** the preprocessor runs,
   **Then** the variant 3 form is normalized to variant 1 or 2 in
   the output (P13 emitter rule "always emit the canonical form,
   never variant 3").
10. **Given** any input file with `#include "f"`, **When** the
    preprocessor runs, **Then** the very first directive emitted
    inside the expanded include text is `#line 1 "f"` and the
    very first directive emitted on return to the outer file is
    a `#line` re-establishing the outer file's location (P13
    "implementation guidance" bullets).

---

### User Story 3 — Source-locating diagnostics for every lex / preprocess error (Priority: P1)

A contributor running the lexer or preprocessor on input that
contains an error sees a single-line diagnostic of the form
`path:line:col: severity: message` where `path` is the originating
file (resolved through `#line` adjustments), `line` and `col` are
1-based, and `severity` is one of `error` / `warning` / `note`. For
errors raised inside an `#include`'d file, the diagnostic includes
an include-stack trace naming each ancestor file and line.

**Why this priority**: Constitution Principle IV ("Diagnostics
First") gates this milestone explicitly — the README's M1 row cites
"IV (diagnostics)" as the constitutional anchor alongside VI. The
`DiagnosticEngine` (per
[`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md)
§12, lines 1161–1195) is the cross-cutting plumbing that every
later layer (parser, sema, MLIR pass, CIRCT pass, ExportVerilog) is
required to use. Getting it wrong at M1 means every later
diagnostic is wrong; rebuilding it later is far more expensive than
landing it correctly now. Shares P1 with US1 and US2 because the
M1 acceptance gate and the milestone's constitutional anchors both
depend on it.

**Independent Test**: Provoke a known-bad input at a known
position and assert the diagnostic format. Specifically: (a) a
lexer fixture with an unterminated string literal at byte offset
`X`, line `L`, column `C` of fixture file `F`; assert the
diagnostic equals `F:L:C: error: <message>`. (b) A preprocessor
fixture `outer.nsl` `#include`s `inner.nsl`; an undefined-macro
splice `%UNDEF%` exists at `inner.nsl:5:10`; assert the diagnostic
cites `inner.nsl:5:10:` and includes an `included from outer.nsl:N`
note. (c) A fixture sets `#line 1000 "synth.nsl"` then introduces
an error two lines later; assert the diagnostic cites
`synth.nsl:1002:…`. Does not depend on US1's full keyword coverage
or US2's helper-evaluation correctness — only on the engine being
wired and the relevant raise sites in lex / preprocess being
reached.

**Acceptance Scenarios**:

1. **Given** a fixture containing an unterminated string literal,
   **When** the lexer runs, **Then** the diagnostic is emitted
   with `path:line:col:` matching the byte offset of the opening
   `"` of the unterminated literal.
2. **Given** an `#include` chain `a.nsl` → `b.nsl` → `c.nsl` and
   an error in `c.nsl`, **When** the preprocessor runs on `a.nsl`,
   **Then** the diagnostic cites `c.nsl:<line>:<col>:` *and*
   contains an include-stack note listing `b.nsl` and `a.nsl`
   with their respective include sites.
3. **Given** a fixture whose `#line N "alias.nsl"` directive
   precedes a malformed token on the next line, **When** lex /
   preprocess runs, **Then** the diagnostic's `path` is
   `alias.nsl` and the `line` is `N+1` (per P13 line-counter
   semantics).
4. **Given** any successful pass through lex + preprocess, **When**
   the diagnostic engine is queried, **Then** its diagnostic
   buffer is empty and the run reports success.
5. **Given** any error raised by the lexer or preprocessor,
   **When** the diagnostic is rendered, **Then** the message text
   is non-empty and references the offending construct by name
   (e.g., "unterminated string literal", not "lex error").

---

### Edge Cases

- A single-character file consisting only of `#`. The lexer's N5
  disambiguation rule must classify it deterministically and not
  hang or produce inconsistent tokens between runs.
- A `#define` body whose final reduction (per P10 step 3) produces a
  real value but the macro is never substituted into a context
  requiring an integer. The float must NOT cross the preprocessor
  seam (P7); the preprocessor either emits the float as a literal
  (if the use site is itself inside another `#define` body / `#if`
  condition) or raises a P7 diagnostic at the use site.
- A `#line LINENUM=0` directive (explicitly permitted per P13). The
  next line's reported line number must be `1`.
- An `#include` cycle (`a.nsl` includes `b.nsl` which includes
  `a.nsl` again). Pp.ebnf does not specify cycle handling; the
  preprocessor MUST detect and reject it with a clear "include
  cycle detected" diagnostic rather than recursing until stack
  overflow. A bounded include depth (say, 256) is acceptable as the
  detection mechanism; the limit MUST be documented and is not
  user-configurable at M1.
- A keyword that appears as part of a `_`-prefix system name (e.g.,
  `_init` versus `_init_block` — treat both as single identifiers
  per `nsl_lang.ebnf §13` lexical rules; the keyword `init` does
  not "leak through" the underscore prefix).
- A `#define` whose body is empty (legal per pp.ebnf §2.2). A
  `%MACRO%` reference to it must produce no tokens at the use site.
- A mismatched `#endif` (no matching opener), or end-of-file before
  `#endif` (per P9). Both are preprocessor errors with file:line:col
  diagnostics.
- A `%IDENT%` reference to an undefined macro (per P3 last
  sentence). Preprocessor error.
- A compile-time helper in `_init`-prefix collision with NSL system
  task names (`_init` is both an NSL system task and a possible
  helper-naming pattern). The preprocessor's helper grammar (per
  pp.ebnf §3) is closed: only `_int`, `_pow`, `_sin`, `_cos`,
  `_exp`, `_log`, `_sqrt`, `_tan` are recognized as helpers. A
  `_init(…)` inside a `#define` body is a P6 violation (helper not
  in the closed set), not a system-task call.
- `nslc -emit=tokens` invoked on a file that fails preprocess. The
  run MUST exit non-zero and MUST NOT emit any tokens to stdout
  (Principle V "no partial output" intent — even if not strictly a
  determinism issue, leaving partial output confuses CI consumers).

## Requirements *(mandatory)*

### Functional Requirements

**Library deliverables (M1 layers per `nsl_compiler_design.md` §3, lines 138–141):**

- **FR-001**: The build MUST produce the static library `nsl-basic`
  at the layer-1 position, exposing public headers `Basic/
  SourceLocation.h` and `Basic/Diagnostic.h` (per the §3 table). The
  library MUST contain `SourceLocation`, `SourceRange`, `FileID`,
  `SourceManager`, and `DiagnosticEngine` types as described in
  `nsl_compiler_design.md` §12.
- **FR-002**: The build MUST produce the static library
  `nsl-preprocess` at the layer-2 position, exposing the public
  header `Preprocess/Preprocessor.h`, depending only on `nsl-basic`.
- **FR-003**: The build MUST produce the static library `nsl-lex`
  at the layer-3 position, exposing the public headers `Lex/
  Lexer.h` and `Lex/Token.h`, depending only on `nsl-basic`.
- **FR-004**: All three libraries MUST be declared via the
  `add_nsl_library` macro from M0 (Constitution Principle II layer
  enforcement); their inter-layer dependencies MUST be expressed
  exclusively via that macro's `DEPENDS` argument.

**Lexer functional requirements (per `nsl_lang.ebnf` §§13–15, parser notes N5 and N11):**

- **FR-005**: The lexer MUST recognize every reserved keyword listed
  in `nsl_lang.ebnf §15` (lines 783–824) as a distinct token kind
  (not as an identifier). The test gate (FR-029) requires one
  fixture per keyword.
- **FR-006**: The lexer MUST recognize numeric literals in all
  combinations of base × digit form per `nsl_lang.ebnf §13`:
  decimal, hex (`0x` / `0X`), binary (`0b` / `0B`), octal
  (`0o` / `0O`), each in plain digits and with the "value" digits
  `Z`, `X`, `U` mixed in. Each combination produces a single
  numeric-literal token.
- **FR-007**: The lexer MUST classify `_`-prefix names per parser
  note **N11** (`nsl_lang.ebnf` lines 1083–1105) into the three
  classes (system task, system function, reserved-but-unused) and
  emit the corresponding token kind.
- **FR-008**: The lexer MUST recognize string literals (with the
  escape-sequence subset documented in `nsl_lang.ebnf §13`) and
  emit them as a single string-literal token.
- **FR-009**: The lexer MUST recognize identifiers per
  `nsl_lang.ebnf §13` lexical rules. The semantic constraint S1
  (no `__` in identifiers) is enforced at Sema time (M3), NOT at
  M1 — at the lexer level `__foo` is still emitted as an identifier
  token.
- **FR-010**: The lexer MUST skip whitespace (spaces, tabs,
  newlines) and comments (single-line `//` and block `/* … */` per
  `nsl_lang.ebnf §14`) without affecting the byte-offset accuracy
  of the source ranges of surrounding tokens.
- **FR-011**: The lexer MUST disambiguate the `#` token per parser
  note **N5** (`nsl_lang.ebnf` lines 1035–1049): `#` at start-of-
  line followed by a decimal integer is the line-marker form; `#`
  in expression position is the sign-extend operator. The lexer is
  the layer where this disambiguation lives (per parser-note
  classification "M2 (incl. N5 `#` line-marker disambiguation)" in
  `CLAUDE.md` §1).
- **FR-012**: Every token emitted by the lexer MUST carry a
  `SourceRange` whose start and end byte offsets round-trip exactly
  to the original source file (or, for tokens after a `#line`
  adjustment, to the post-`#line` virtual file coordinates).

**Preprocessor functional requirements (per `nsl_pp.ebnf` and notes P1–P13):**

- **FR-013**: The preprocessor MUST resolve `#include "file"`
  (quote form) by searching the configured include-search list
  (the `-I` flag list, plus the directory of the including file),
  and `#include <file>` (angle form) by searching the
  `NSL_INCLUDE` environment variable's path list. Both forms
  textually insert the included file's preprocessed token stream.
  (Per **P8**.)
- **FR-014**: The preprocessor MUST process `#define <ident> <body>`
  by storing the macro's name and unexpanded body in the macro
  table; expansion happens at use sites per **P10**. `#undef
  <ident>` removes the entry. Per `nsl_pp.ebnf §2.2`.
- **FR-015**: The preprocessor MUST evaluate `#if <expr>` per **P4**
  by parsing the expression per `nsl_pp.ebnf §3` (compile-time
  expression sub-grammar) and producing an integer result; non-zero
  selects the "then" region. `#ifdef <ident>` and `#ifndef <ident>`
  test macro-table presence. `#else` always pairs with the most
  recent unmatched opener (P9); `#endif` closes it. Nesting is
  unbounded.
- **FR-016**: The preprocessor MUST recognize `%IDENT%` references
  inside identifier text (per **P3**), look up the inner identifier
  in the macro table, splice the replacement TEXT (not the value —
  splicing is textual), and emit the surrounding identifier as ONE
  identifier token. An undefined `%IDENT%` is a preprocessor error.
- **FR-017**: The preprocessor MUST evaluate compile-time helper
  calls inside `#define` replacement bodies and `#if` conditions.
  The closed set is defined by `nsl_pp.ebnf §3` (lines 282–288) and
  contains 22 helpers: `_int`, `_real`, `_pow`, `_sqrt`, `_sin`,
  `_cos`, `_tan`, `_asin`, `_acos`, `_atan`, `_sinh`, `_cosh`,
  `_tanh`, `_log`, `_log10`, `_exp`, `_floor`, `_ceil`, `_round`,
  `_abs`, `_min`, `_max`. The order of expansion within a `#define`
  body is fixed by **P10**: (1) `%IDENT%` splices first; (2) helper
  calls left-to-right; (3) operator-precedence reduction. Helper
  return types per **P5**: `_int` and `_real` produce
  integer/real-coerced results respectively; trigonometric,
  logarithmic, exponential, and rounding helpers return real;
  `_abs`, `_min`, `_max` preserve their argument's numeric kind.
- **FR-018**: The preprocessor MUST reject any compile-time helper
  call that survives outside a `#define` body or an `#if` condition
  (per **P6**) with a clear diagnostic at preprocess time, and
  MUST NOT permit a helper call to cross the preprocessor →
  lexer seam (P12).
- **FR-019**: The preprocessor MUST reject any float literal that
  would cross the preprocessor → lexer seam (per **P7** — floats
  are preprocess-time only). Float values must be reduced to
  integers via `_int(…)` before substitution into the output
  stream.
- **FR-020**: The preprocessor MUST act as both consumer and
  emitter for `#line` directives per **P13**:
  - **Consumer**: parse the directive (per `nsl_pp.ebnf §2.4`),
    macro-expand the variant-3 "anything-else" form, and reset its
    own `(file, line)` cursor so subsequent diagnostics for input
    lines cite the new location.
  - **Emitter**: re-emit the directive into its output stream in
    the canonical two-token form `#line <pp_decimal_integer> [
    <string_literal> ]` (variant 1 or 2 only — never variant 3).
  - At the start of each `#include`'d file's expanded content,
    emit `#line 1 "filename"`. At return-from-include, emit a
    `#line` re-establishing the outer file's location.
  - A `#line LINENUM=0` directive is permitted; the next input
    line is numbered `1`.
- **FR-021**: The preprocessor MUST guarantee the **P12 boundary
  property**: after preprocessing, the output token stream contains
  only NSL tokens (per `nsl_lang.ebnf` lexical grammar) plus
  well-formed `#line` directives. Any surviving `#include`,
  `#define`, `#undef`, `#ifdef`, `#ifndef`, `#if`, `#else`,
  `#endif`, `%IDENT%` reference, helper call, or float literal is a
  preprocessor bug AND MUST be detected by an internal post-pass
  assertion that fires under non-NDEBUG builds (the future M5
  `%IDENT%`-residue check is dialect-level; the M1 check is
  preprocessor-internal and happens at the seam).
- **FR-022**: The preprocessor MUST detect `#include` cycles (a
  file transitively including itself) and reject with a "`#include`
  cycle detected: <path-trace>" diagnostic. A bounded include
  recursion depth of 256 levels is the detection mechanism;
  exceeding it is treated as a cycle. The limit MUST be documented
  in the public API of `Preprocess/Preprocessor.h` and is not
  user-configurable at M1.
- **FR-023**: The preprocessor MUST reject mismatched conditional
  directives per **P9**: an `#endif` without a matching opener, or
  end-of-file before a matching `#endif`, are both errors with
  source-locating diagnostics.

**Diagnostic plumbing (per `nsl_compiler_design.md` §12 and Constitution Principle IV):**

- **FR-024**: The `DiagnosticEngine` MUST be the sole diagnostic
  emitter for both `nsl-lex` and `nsl-preprocess` (per design §12,
  lines 1163–1195). Direct writes to `stderr` from inside either
  library are forbidden.
- **FR-025**: Every diagnostic MUST be rendered in the canonical
  form `<path>:<line>:<col>: <severity>: <message>` where `path`
  is the file as currently understood by the `SourceManager`
  (i.e., post-`#line` adjustment), `line` and `col` are 1-based,
  and `severity` ∈ {`error`, `warning`, `note`}.
- **FR-026**: For diagnostics raised inside an `#include`'d file,
  the engine MUST emit one trailing `note` per ancestor file in the
  include stack, citing each ancestor's `path:line` of the
  `#include` directive.
- **FR-027**: The `DiagnosticEngine` MUST support a JSON-output
  mode (per design §12 future-LSP usage in nsl_compiler_design.md
  line 1300). At M1 the depth is **smoke-only** (per Clarifications
  session 2026-04-27 Q2): wire `nslc --diagnostic-format=json` so
  the same diagnostics that print as text under the canonical
  format also emit as JSON; ship one smoke test asserting the JSON
  parses. Schema lock, schema versioning, per-diagnostic-kind
  round-trip goldens, and a real LSP consumer all defer to T3.
- **FR-028**: A successful run (no errors raised) MUST leave the
  diagnostic buffer empty and MUST exit zero from `nslc`.

**Driver / CLI surface (per `README.md` §Usage and the M0 spec note that "real `-emit=` flags arrive incrementally from M1 onward"):**

- **FR-029**: The `nslc` driver MUST accept `-emit=tokens` and, on
  success, print the post-preprocess token stream to stdout in a
  documented, deterministic format (one token per line, with kind,
  spelling, and source range; format frozen by an authoritative
  golden test fixture under `test/lex/`). On failure, exit
  non-zero with no token output.
- **FR-030**: The `nslc` driver MUST accept the include-path flags
  documented in `README.md` §Usage: `-I <dir>` (quote-form
  `#include` search path, repeatable), `-D NAME=value` (predefine
  a preprocessor macro, repeatable), and the `NSL_INCLUDE`
  environment variable (angle-form `#include` search path).

**Test gates (per Constitution Principles VI and VIII):**

- **FR-031**: The repository MUST carry one passing fixture per
  reserved keyword (`nsl_lang.ebnf §15`, lines 783–824) under
  `test/lex/keywords/`. Each fixture exercises exactly one keyword
  in a minimal valid context.
- **FR-032**: The repository MUST carry one passing fixture per
  numeric form (the cross product enumerated in FR-006) under
  `test/lex/numbers/`.
- **FR-033**: The repository MUST carry one passing fixture per
  `_`-prefix N11 class under `test/lex/n11/`.
- **FR-034**: The repository MUST carry one passing fixture +
  one failing fixture (where the rule has a violation case) per
  preprocessor note **P1** through **P13** under `test/preprocess/
  pNN/`. Notes whose rule is not directly violatable (e.g., P12 is
  an aggregate seam invariant, not a single rule) MAY ship pass-
  only or be exercised indirectly via P-notes that cover specific
  violations (P6 covers helpers crossing the seam; P7 covers
  floats crossing the seam — both contribute to P12).
- **FR-035**: The repository MUST carry a `#line` round-trip golden
  test under `test/preprocess/line/` that exercises all three
  variants on input and asserts the output contains canonical
  variant 1 or 2 only, and that subsequent token source ranges
  reflect the post-`#line` coordinates.
- **FR-036**: Every fixture MUST be authored before its driving
  implementation (Principle VIII TDD); the test commit MUST be
  observed failing prior to the implementation commit being
  accepted. The TDD evidence path (failing-CI link in PR
  description) is the standard mechanism — no separate audit log
  is required.
- **FR-037**: For every fixture covering a semantic rule that the
  spec phrases as a constraint with a diagnostic message
  (preprocessor: P3 undefined macro, P6 helper outside `#define`,
  P7 float at seam, P9 mismatched conditional; lexer: unterminated
  string), the failing fixture MUST assert the diagnostic
  string per the Principle VIII rule that diagnostic-bearing rules
  test the diagnostic text.

**Determinism (Constitution Principle V):**

- **FR-038**: The token stream emitted by `nsl-lex` MUST be a pure
  function of (input bytes, CLI flag list). No environment-derived
  inputs (CWD, mtime, locale, hostname, env vars other than
  `NSL_INCLUDE`) MAY influence the output. Two `nslc -emit=tokens`
  invocations on the same input under the same flag list MUST
  produce byte-identical stdout.
- **FR-039**: The preprocessor's macro-table iteration order MUST
  NOT influence the order of tokens emitted to the output stream
  (no hash-map-iteration-derived order). All collection types whose
  iteration order is part of an output MUST be deterministic
  (insertion-ordered or sorted).

### Key Entities

- **`SourceLocation`**: a `(FileID, offset)` pair; the smallest
  unit of source attribution. Public type in `nsl-basic`.
- **`SourceRange`**: a `(SourceLocation start, SourceLocation end)`
  pair. Every token, every diagnostic, and (per `nsl_compiler_
  design.md` §5 line 645) every AST node carries one.
- **`SourceManager`**: owns the in-memory `Buffer` per `FileID`,
  resolves `(file, line, col)` ↔ byte-offset queries, and tracks
  `#line` adjustments such that a single `SourceLocation` can
  resolve to either the *physical* file:line:col or the *logical*
  (post-`#line`) file:line:col.
- **`DiagnosticEngine`**: owns the diagnostic buffer; renders
  diagnostics in canonical text or JSON form; supports `FixItHint`
  hooks for later milestones.
- **`Token`**: a `(kind, SourceRange, spelling)` record. The
  `kind` enum covers identifiers, all reserved-keyword kinds,
  numeric literals (with sub-kind for base × digit-form), string
  literals, all punctuation operators, and the `#line` marker
  passed through from the preprocessor.
- **`Lexer`**: stateful scanner over a single `Buffer`; emits
  `Token`s on demand (pull model) so the M2 parser can drive it.
- **`Preprocessor`**: owns the macro table, the include-search
  path lists, the conditional-stack (for `#ifdef` nesting), and a
  cursor over the input file (or stack of files, for include
  recursion). Produces a token stream; that stream is the input
  to `Lexer` in the M1 pipeline.
- **`Macro`**: `(name, body, defining_location)` record. Body is
  stored unexpanded per P10.
- **`IncludeSearchPath`**: ordered list of directories;
  configurable via `-I` (quote form) and `NSL_INCLUDE` (angle
  form); read at preprocessor construction.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: `nslc -emit=tokens fixture.nsl` produces a token
  stream covering 100% of the lexical surface defined in
  `nsl_lang.ebnf §§13–15` for the keyword/numeric/`_`-prefix/
  string fixtures shipped under `test/lex/`. (Coverage measured
  by per-fixture pass rate, not line coverage of the lexer
  source.)
- **SC-002**: For each preprocessor note **P1–P13**, the
  shipping fixture pair (pass + fail where applicable) compiles
  on a green CI run. 0 of 13 P-notes are unenforced.
- **SC-003**: The `#line` round-trip golden test passes: every
  input variant 3 directive normalizes to variant 1 or 2 in the
  output, every input variant 1 / 2 round-trips byte-identical,
  and every token after a `#line` reports source coordinates
  matching the post-`#line` virtual file.
- **SC-004**: Every diagnostic emitted by the lexer or
  preprocessor (across the entire fixture corpus) matches the
  regex `^[^:]+:\d+:\d+: (error|warning|note): .+$` (FR-025
  format). 0% deviation tolerated.
- **SC-005**: Two consecutive `nslc -emit=tokens` invocations on
  the same input + flag list produce byte-identical stdout
  (Principle V; FR-038).
- **SC-006**: A diagnostic raised inside an `#include`'d file
  resolves correctly to the inner-file `path:line:col` AND
  carries one `note` per ancestor file in the include stack —
  100% of the include-stack diagnostic fixtures under
  `test/preprocess/include-stack/`.
- **SC-007**: Adding a 14th preprocessor note (`P14`,
  hypothetically, in a later spec change) requires editing
  exactly one new fixture directory plus one row in
  `CLAUDE.md` §1's table — no edit to the M1 milestone
  scaffolding (Principle II layer extensibility, applied to the
  test corpus).
- **SC-008**: A reviewer opening a red CI run from a lex /
  preprocess regression can identify the failing fixture and
  the offending input line within 10 seconds, without reading
  raw lexer-internal log scrollback (re-statement of the M0
  SC-007 invariant for M1 fixtures).
- **SC-009**: 100% of files newly added under `lib/Basic/`,
  `lib/Lex/`, `lib/Preprocess/`, `include/nsl/Basic/`,
  `include/nsl/Lex/`, `include/nsl/Preprocess/`, and
  `test/lex/`, `test/preprocess/` carry the
  `Apache-2.0 WITH LLVM-exception` SPDX header (M0 FR-010 hygiene
  re-stated for the M1 file set).

## Assumptions

- **Scope is the M1 row of `README.md` §Roadmap, plus the
  `nslc -emit=tokens` driver flag and the `-I` / `-D` /
  `NSL_INCLUDE` plumbing it depends on.** Confirmed via
  /speckit-clarify session 2026-04-27 Q3 (Option A). The driver
  flag is included because (a) `README.md` §Usage shows it as the
  user-facing surface for the post-preprocess token stream and
  (b) the M0 spec stated explicitly "real `-emit=` flags arrive
  incrementally from M1 onward" (M0 spec.md Assumptions
  paragraph). Without it the M1 deliverable is library-only and
  cannot be exercised end-to-end through `nslc`.
- **Compile-time helper evaluation lives in M1, not M3.**
  Confirmed via /speckit-clarify session 2026-04-27 Q1 (Option A).
  The preprocessor's helper evaluator is mandated by P5 (helpers
  return integers/reals — i.e., they compute a value), P6 (helpers
  must not survive outside `#define`/`#if`), and P12 (the seam
  carries only NSL tokens + `#line` — no helpers). CLAUDE.md §1's
  "Compile-time helpers `_int`/`_pow`/`_sin`/… → M1 (parse); M3
  (eval)" row is interpreted as referring to the *NSL-language
  proper* constant evaluator (e.g., the S15 bit-slice-index
  constant evaluator, which lives in Sema at M3) — a different
  evaluator from the preprocessor's. CLAUDE.md §1 receives a
  Principle-VII coupling-fix patch in a follow-up PR (not in this
  feature) to disentangle the two evaluators.
- **NSL semantic constraint S1 (`__` forbidden in identifiers,
  `nsl_lang.ebnf:830`) is enforced at Sema (M3), not at the
  lexer.** The lexer at M1 emits `__foo` as an identifier token;
  Sema rejects it later. This matches the general layering
  policy that constraints `Sn` are Sema's responsibility (per
  CLAUDE.md §1 row "Semantic constraints S1–S29 → M3").
- **Reference host, build matrix, CI infrastructure, SPDX
  hygiene, and `add_nsl_library` are M0 deliverables and are
  taken as given.** This spec inherits and does not re-justify
  the four-build matrix, the six-stage CI pipeline, the
  `add_nsl_library` macro, the SPDX-presence script, or the
  local-reproduction entry point `./scripts/ci.sh`.
- **The `#line` round-trip golden lives at the lex+preprocess
  pair level, not at the lexer alone.** The named test gate in
  README §Roadmap M1 row ("`#line` round-trip golden") is
  interpreted as the test that exercises the preprocessor's
  consumer-and-emitter semantics (P13) and the lexer's
  source-location reset behavior together — because that
  end-to-end behavior is what M2's parser and every later layer
  depend on.
- **The `M2 (incl. N5 ...)` annotation in CLAUDE.md §1's parser-
  notes row is interpreted as "N5 disambiguation is *consumed*
  at M2, but the LEXER token-classification machinery for `#`
  lands at M1."** The lexer must classify `#` correctly when it
  emits a token; the parser at M2 then consumes the
  classification. If this interpretation is wrong, the spec
  needs a CLAUDE.md §1 amendment to disentangle "classification"
  vs. "consumption" responsibilities.
- **The compile-time-helper closed set is the 22 identifiers
  enumerated in `nsl_pp.ebnf §3` lines 282–288** (`_int`, `_real`,
  `_pow`, `_sqrt`, `_sin`, `_cos`, `_tan`, `_asin`, `_acos`,
  `_atan`, `_sinh`, `_cosh`, `_tanh`, `_log`, `_log10`, `_exp`,
  `_floor`, `_ceil`, `_round`, `_abs`, `_min`, `_max`). Any other
  `_<name>(...)` in a `#define` body or `#if` condition is a P6
  violation. The M1 implementation MUST express this closed set
  via a single source-of-truth header included from both
  `lib/Preprocess/` and `test/preprocess/`, so that any future
  EBNF-driven expansion of the set is a one-line spec patch.
- **JSON-mode diagnostic output** (FR-027) is wired but not
  exercised end-to-end by an LSP consumer at M1 (per
  Clarifications session 2026-04-27 Q2 → Option A). M1 ships
  JSON output behind `nslc --diagnostic-format=json` and a smoke
  test asserting the JSON parses; schema lock, schema versioning,
  per-diagnostic-kind round-trip goldens, and a real LSP consumer
  all defer to T3.
- **Audited-project ingestion (`P-VEN`) and golden VCDs
  (`P-VCD`) are out of scope** — they gate M7. CI's end-to-end
  stage and formal stage remain in the "wired-but-empty" state
  established at M0.
- **Tooling track** (T1–T12) is out of scope. T1 (TextMate
  grammar) MAY benefit from the keyword set landing in M1, but
  T1 itself is not a M1 deliverable.
- **`nsl-ast`, `nsl-parse`, `nsl-sema`, `nsl-dialect`,
  `nsl-lower`, `nsl-driver` libraries** are present as empty
  layer-skeletons from M0 and are not modified by M1 except for
  the minimal `nsl-driver` glue needed to expose `-emit=tokens`
  (a single new code path in `tools/nslc/main.cpp`'s otherwise-
  ≤60-line driver and a thin `nsl-driver` library function
  invoking lex+preprocess and printing tokens).
