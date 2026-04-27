<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Research: M1 — Lex + Preprocess (with Diagnostic Plumbing)

**Branch**: `002-m1-lex-preprocess` | **Date**: 2026-04-27
**Plan**: [plan.md](./plan.md)

Each section resolves one Technical Context decision (or one open
plan question) with **Decision / Rationale / Alternatives**. The spec
already pinned the user-visible decisions via `/speckit-clarify`
(helper-eval locus = M1; JSON-diagnostic depth = smoke-only;
`nslc -emit=tokens` in scope); this file pins the
implementation-mechanism decisions.

---

## §1. Lexer implementation strategy: hand-written

**Decision**: **Hand-written recursive-descent lexer** in C++17, with
explicit `peek()` / `advance()` / `skipWhitespace()` primitives over a
`StringRef` view of the buffer.

**Rationale**: Three converging reasons.

1. **LLVM/CIRCT convention.** Clang's lexer is hand-written, so is
   MLIR's tokenizer. Following the convention keeps M1 readable to
   anyone who has seen the upstream codebases (Constitution Build/
   Code/Licensing "LLVM/CIRCT conventions for naming, brace style,
   header guards, and file headers").
2. **N5 disambiguation is context-sensitive.** The `#` token is the
   line-marker form at start-of-line, the sign-extend operator in
   expression position. A flex/re2c-generated DFA cannot capture
   "start-of-line" cleanly without action code that ends up
   resembling hand-written logic anyway; the tooling overhead is not
   recouped.
3. **N11 `_`-prefix classification** picks one of three classes
   based on the identifier's text. A small `StringMap<TokenKind>`
   built once at lexer construction is direct and deterministic;
   generator-emitted alternatives add a build-time dependency
   without simplifying the lookup.

**Alternatives considered**:
- *flex (or re2c) generator*: rejected per the N5 / N11 rationale; also
  pulls in a build-time tool we don't otherwise need at M1.
- *PEG library (cpp-peglib, etc.)*: would force N5 into a custom
  parser-action, removing the LLVM-convention alignment. Rejected.
- *Reuse `mlir::AsmParser`'s tokenizer*: the surface is too narrow
  (designed for the textual MLIR form) and rebinding it to NSL would
  be more code than a fresh lexer. Rejected.

---

## §2. Preprocessor architecture: line-oriented scanner with include-stack and bounded recursion

**Decision**: A **line-oriented scanner** that reads input
line-by-line, classifies each line as directive vs passthrough per
**P1**, and dispatches to per-directive handlers. Include resolution
pushes a new `IncludeFrame { FileID, line_cursor, post_include_emit_marker }`
onto an `IncludeStack`; the bounded-recursion cycle guard from
**FR-022** is the stack's depth limit (256 frames). Output is a
**lazy token stream** — the preprocessor emits tokens on demand to
its consumer (the lexer in `lib/Driver/EmitTokens.cpp`, or the future
M2 parser).

The directive grammar is parsed with a small dedicated parser
(`lib/Preprocess/DirectiveParser.cpp`) — separate from the
`PPExpression` parser because the directive forms (`#include "f"`,
`#define X v`, `#if expr`, …) are line-oriented and do not need the
expression-grammar precedence machinery. The expression parser
(`lib/Preprocess/PPExpression.cpp`) implements `pp.ebnf §3` exactly.

**Rationale**:
- **P1 line-orientation** is a hard preprocessor invariant — running
  the same lexer over directive-bearing lines and over NSL-content
  lines would conflate two grammars that the spec explicitly
  separates ("the NSL parser, in contrast, is free-form").
- **Include-stack model** matches clang/cpp's, supports the P13
  emit-`#line`-on-enter / emit-`#line`-on-leave invariant naturally
  (each frame knows its own resume location), and gives the include-
  stack diagnostic notes (FR-026) a structural source of truth.
- **Bounded recursion at 256** beats hash-keyed cycle detection: it's
  O(1) per include, deterministic across runs, and robust against
  pathological inputs (a 1024-deep include chain that doesn't loop
  is still rejected — that is intentional and matches the spec).
- **Lazy emission** lets the M2 parser drive the preprocessor
  on-demand without buffering the entire token stream — necessary
  for the LSP at T3 but cheap to ship at M1.

**Alternatives**:
- *Two-pass preprocessor* (first pass collects all tokens to a
  vector, second pass consumed by lexer): simpler memory model but
  burns memory on long inputs and doesn't compose with the future
  T3 LSP. Rejected.
- *Hash-keyed cycle detection (visited-FileID set)*: handles legitimate
  N-level non-cyclic includes but fails the "1024-deep non-cyclic"
  pathological case. Bounded-depth is simpler and matches the spec's
  "include cycle detected" language (the 1024-deep case is
  arguably a cycle equivalent). Rejected.

---

## §3. SourceManager / SourceLocation design — own implementation, not `llvm::SourceMgr`

**Decision**: **Own `nsl::basic::SourceManager`** modeled on clang's
`clang::SourceManager` rather than reusing `llvm::SourceMgr`.
`SourceLocation` is an opaque 32-bit handle (24 bits offset, 8 bits
`FileID` index — extensible). `SourceRange` is a pair. The
`SourceManager` maintains:

1. A vector `Buffer[FileID]` holding the raw bytes per loaded file.
2. A per-`FileID` **line-offset table** built lazily on first
   `(file, line, col)` query.
3. A per-`FileID` **`#line` adjustment table** (an
   ordered list of `{ origin_offset, virtual_filename, virtual_line }`
   entries, populated by the preprocessor when it sees a `#line`).
   Lookups bisect by `origin_offset` to find the active virtual
   mapping for a given physical offset.

**Rationale**:
- **`llvm::SourceMgr` is too narrow.** It supports diagnostics in
  text form but lacks the `#line` virtual-file machinery NSL
  requires (Principle IV). Bolting that onto `llvm::SourceMgr` would
  be invasive.
- **`clang::SourceManager` is a known-good blueprint** but is sized
  for a multi-million-LOC C++ codebase with macro expansion
  history; its complexity isn't paid back at NSL scale. We take its
  *shape* (FileID + offset; loaded buffers; expansion-vs-spelling
  separation) and ship a fraction of the code.
- **Opaque 32-bit handle** beats a `(FileID, offset)` struct for
  cache locality (Tokens are bulk artifacts; halving their size
  matters for `nslc -emit=tokens` output throughput) and matches
  clang convention. The 24/8 split gives 16 MiB per file × 256 files
  — comfortably above any plausible NSL input.
- **Lazy line-offset table** avoids paying the line-scan cost at
  load time when most callers only ask about a small subset of
  files. Built on first query, cached thereafter.

**Alternatives**:
- *Reuse `llvm::SourceMgr`*: rejected per `#line` gap.
- *`(FileID, offset)` as a 64-bit struct*: passable but doubles
  Token size unnecessarily. Rejected.
- *Per-line offset stored eagerly at file load*: simpler but pays
  for unused state. Rejected as gratuitous.

---

## §4. Determinism strategy for token output and macro iteration

**Decision**: Three rules, all enforced at the type-system level.

1. **Macro table type is `llvm::MapVector<StringRef, MacroDef>`** —
   insertion-ordered iteration; lookup remains O(1) average via the
   internal `DenseMap`. Iteration order is deterministic and
   reproducible across builds.
2. **Token stream is a `std::vector<Token>` (or, in lazy form, a
   pull-model iterator)** — sequential by construction.
3. **Diagnostic emission is single-threaded** (FR-024: only the
   `DiagnosticEngine` writes diagnostics) and the engine's internal
   buffer is `std::vector<Diagnostic>`, sorted at render time by
   `(SourceLocation, severity)` to give a deterministic output
   ordering even if multiple diagnostics are emitted in the same
   pass.

No `std::unordered_map`, no `llvm::DenseMap` direct iteration
exposed to output code; all such structures are wrapped behind
explicit insertion-order or sort-on-render facades.

**Rationale**: Principle V's "no hash-map iteration order" is the
reason. `llvm::MapVector` exists in upstream LLVM specifically for
this case (deterministic ordered map with hash-map lookup) — it's
the LLVM-convention choice. Sorting diagnostics at render time
catches the "two passes raised diagnostics in different orders due
to early-out optimization" failure mode that pure
emission-ordering would miss.

**Alternatives**:
- *`std::map<std::string, MacroDef>`*: deterministic but expensive
  (string keys, tree walk per lookup); inflates the macro-expansion
  inner loop unnecessarily. Rejected.
- *`std::unordered_map` with a sort pass at output*: doubles work
  for output and leaves the inner loop's iteration order
  unstable, which is a footgun for any future code that walks the
  macro table during expansion. Rejected.

---

## §5. Helper evaluator: numeric model and coercion

**Decision**: The 22-helper closed-set evaluator uses a tagged
`PPValue` of `std::variant<int64_t, long double>`. Coercion rules:

- `_int(<arg>)` truncates a real argument toward zero to `int64_t`.
- `_real(<arg>)` widens an integer argument to `long double` (per
  pp.ebnf §3 commentary on `_real`).
- All trigonometric / logarithmic / exponential / `_sqrt` /
  `_floor` / `_ceil` / `_round` helpers operate on `long double`
  and return `long double` — integer arguments are widened first.
- `_pow(b, e)` uses `std::powl` on `long double`. Integer overflow
  of the input is detected (return value is real anyway).
- `_abs` / `_min` / `_max` preserve the kind: integer inputs return
  integer; mixed inputs widen to real.
- A real result emitted into the output stream is rendered as a
  decimal with 17 significant digits (`long double` precision —
  enough to round-trip `double`); an integer result is rendered as
  a base-10 signed decimal.

`#if` conditions evaluate to a `PPValue`; the truth test is "non-zero
integer or non-zero real" per **P4**.

**Rationale**: `long double` (80-bit on x86-64) gives more precision
than `double` for trig/log helpers, matching the typical NSL author
expectation that `_sin(pi/2)` is exactly `1.0`. The kind-preserving
behavior of `_abs`/`_min`/`_max` matches the spec ("`_abs`, `_min`,
`_max` preserve their argument's numeric kind" per spec FR-017
final clause). Rendering an integer with no decimal is required by
**P5** ("the integer literal '256'") so the lexer downstream sees a
syntactically valid token.

**Alternatives**:
- *`double` instead of `long double`*: half the precision; users
  with deep helper math chains would see surprising rounding. Use
  `long double`.
- *Arbitrary-precision (e.g., GMP)*: overkill for hardware-config
  expressions where 80-bit suffices; adds a dependency. Rejected.
- *Integer-only at preprocess time, real-eval deferred*: violates
  the /speckit-clarify Q1 resolution. Rejected.

---

## §6. Helper closed-set source-of-truth: X-macro `.def` file

**Decision**: `include/nsl/Basic/HelperSet.def` is an X-macro file
listing the 22 helpers in `pp.ebnf §3` order:

```cpp
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// HELPER(name, arity, returns_real)
HELPER(_int,   1, false)
HELPER(_real,  1, true)
HELPER(_pow,   2, true)
HELPER(_sqrt,  1, true)
HELPER(_sin,   1, true)
HELPER(_cos,   1, true)
HELPER(_tan,   1, true)
HELPER(_asin,  1, true)
HELPER(_acos,  1, true)
HELPER(_atan,  1, true)
HELPER(_sinh,  1, true)
HELPER(_cosh,  1, true)
HELPER(_tanh,  1, true)
HELPER(_log,   1, true)
HELPER(_log10, 1, true)
HELPER(_exp,   1, true)
HELPER(_floor, 1, true)
HELPER(_ceil,  1, true)
HELPER(_round, 1, true)
HELPER(_abs,   1, false)  // kind-preserving — `false` = "not unconditionally real"
HELPER(_min,   2, false)
HELPER(_max,   2, false)
```

Consumers `#define HELPER(...)` to whatever they need:
- `lib/Preprocess/HelperEvaluator.cpp` builds a dispatch table.
- `lib/Preprocess/DirectiveParser.cpp` builds the recognizer.
- `test_unit/helper_evaluator_test/` iterates over them to author
  one fixture per helper at TDD setup.
- A future `nsl-fmt` (T2) reuses the file for formatting decisions.

**Rationale**: X-macro is the LLVM-standard pattern for closed
enumerations with multiple consumers (see `clang/include/clang/Basic/
DiagnosticIDs.h`, MLIR's `Dialect.def` patterns). It guarantees
single-source-of-truth: a future spec amendment adding a 23rd helper
is a one-line addition that propagates to every consumer
automatically. Matches FR-017's "single source-of-truth header
included from both `lib/Preprocess/` and `test/preprocess/`".

**Alternatives**:
- *Hardcode the list in three places*: violates DRY and FR-017.
  Rejected.
- *JSON/YAML manifest with code-generation*: adds a build-time
  generator and breaks SPDX-on-every-source-file (the generated
  file would need a header). X-macro avoids the generator.
  Rejected.
- *constexpr `std::array<HelperInfo, 22>`*: works for the dispatch
  table but doesn't compose with the recognizer's `StringMap`
  initialization the same way. X-macro feeds both naturally.
  Rejected.

---

## §7. TokenKind enum granularity

**Decision**: `TokenKind` is a single `enum class` with one
enumerator per **distinct lexical class**:

- **One enumerator per reserved keyword** (`tk_module`, `tk_proc`,
  `tk_state`, `tk_func`, `tk_func_in`, `tk_func_out`, `tk_par`,
  `tk_alt`, `tk_any`, `tk_seq`, `tk_if`, `tk_else`, …). The full
  list comes from `lang.ebnf` §15 and is enumerated via
  `KeywordSet.def` (a sibling X-macro to `HelperSet.def`).
- **One enumerator per `_`-prefix class** (`tk_system_task`,
  `tk_system_function`, `tk_unused_underscore`).
- **One enumerator per numeric base** (`tk_decimal_lit`,
  `tk_hex_lit`, `tk_binary_lit`, `tk_octal_lit`). The Z/X/U digit
  classification is carried as a `Token` *attribute*, not a kind —
  because all four base/digit combinations are still "a numeric
  literal" semantically.
- **One enumerator per punctuation/operator** (`tk_lparen`,
  `tk_rparen`, `tk_lbrace`, `tk_lbracket`, `tk_assign`,
  `tk_assign_seq`, `tk_at`, `tk_hash_sign_extend`, …).
- `tk_identifier`, `tk_string_lit`, `tk_eof`, `tk_unknown`.
- `tk_line_directive` — the `#line` marker that survives the
  preprocessor → lexer seam (per **P13**); carried through the lex
  stream so the M2 parser can see and discard it.

**Rationale**: Per-keyword enumerators give the parser at M2 a
trivial `switch` on `Token::kind` for keyword dispatch and let
syntax-error messages name the keyword exactly. Per-base numeric
enumerators are similarly cheap and let the parser pick a base-aware
`Constant` representation later. Carrying the Z/X/U classification
as an attribute (rather than a kind) avoids enumerator-count
explosion: `4 bases × 4 digit-forms = 16` enumerators would conflate
"X content" with "kind" when the *only* downstream consumer is the
NSL constant-folder which already needs to introspect the digit
content anyway.

**Alternatives**:
- *Single `tk_keyword` with a sub-enumerator*: forces every
  parser-side dispatch to do two-level switching. Rejected.
- *`tk_number` only with all base info as attribute*: arguably
  cleaner but loses the "`0x` form vs `0b` form" distinction at the
  switch-level, which is visible enough in error messages to matter.
  Rejected.
- *Per-base × per-digit-form (16 numeric kinds)*: rejected per the
  rationale above.

---

## §8. Test corpus organization and generation

**Decision**: Five fixture trees, each with one **template** and a
small **generator** script that emits the per-instance fixture
files.

1. **`test/lex/keywords/`** — `gen_keyword_fixtures.py` reads
   `lang.ebnf` §15 (lines 783–824) and emits one `.test` per keyword
   using a template:
   ```
   // RUN: %nslc -emit=tokens %s | FileCheck %s
   {{KEYWORD}}
   // CHECK: TOKEN tk_{{KEYWORD}} "{{KEYWORD}}" <{{LOC}}>
   ```
2. **`test/lex/numbers/`** — generated 4 (bases) × 4 (digit forms,
   incl. plain) = 16 fixtures + boundary cases (max width, leading
   underscore, etc.) hand-authored.
3. **`test/lex/n11/`** — 3 hand-authored fixtures (one per
   `_`-prefix class).
4. **`test/lex/strings/`** + **`test/lex/comments/`** + **`test/lex/n5/`** —
   each holds 2–6 hand-authored fixtures.
5. **`test/preprocess/p01/` … `test/preprocess/p13/`** — 13
   directories, each with a hand-authored `pass.test` and (where the
   note has a violation case) a `fail.test`. The `fail.test` cites
   the exact diagnostic string per FR-037.
6. **`test/preprocess/line/`** — the `#line` round-trip golden
   (FR-035): three input variants → assert canonical variant 1 or 2
   in output + post-`#line` virtual coordinates on subsequent tokens.
7. **`test/preprocess/include-stack/`** — the multi-file diagnostic
   golden (FR-026, SC-006): three-file include chain with an error
   in the innermost file → assert path + include-trace notes.
8. **`test/Driver/emit-tokens.test`** — the `-emit=tokens` smoke
   golden.

The optional generator script `scripts/gen_keyword_fixtures.py`
exists so a future spec amendment to `lang.ebnf` §15 (adding /
removing a keyword) regenerates the keyword corpus automatically.
Generated files carry the SPDX header and a "Generated from
`lang.ebnf` §15 — DO NOT EDIT" marker; the generator script is
itself version-controlled.

**Rationale**: Per-Pn directories make the failure surface
self-documenting (a red CI cell names the offending Pn directly).
The generator avoids 70 hand-typed near-identical files for the
keyword grid, reducing copy-paste drift and matching FR-031's "one
fixture per reserved keyword" without forcing a future amendment to
spam the test directory by hand. Hand-authored fixtures elsewhere
because the variations are too irregular for a uniform template.

**Alternatives**:
- *One huge `keywords.test` with all keywords in one file*: failure
  isolation is lost — a single bad keyword fails the whole
  fixture and the diagnostic doesn't name which one. Rejected.
- *Generate everything (numbers, strings, …) via Python*: makes the
  fixture corpus opaque to a reviewer who doesn't run the
  generator. Reserved for the keyword case where the count is
  large and the variation truly is uniform.
- *gtest-only (no lit fixtures for lex)*: violates the spirit of
  Principle VI "lit + FileCheck for the goldens" — token streams
  are textual goldens by nature.

---

## §9. Diagnostic JSON schema (smoke-only at M1, full schema deferred to T3)

**Decision**: At M1 the `--diagnostic-format=json` output is one
JSON object per diagnostic, one diagnostic per line (NDJSON), with
the following minimal fields:

```json
{"path":"...", "line":N, "col":N, "severity":"error|warning|note", "message":"..."}
```

Include-stack notes appear as separate NDJSON lines with
`"severity":"note"` and a `"included_from":{"path":..., "line":...}`
field. **No schema file, no schema version, no per-diagnostic-kind
discriminator at M1** — those land at T3 against a real LSP
consumer per /speckit-clarify Q2.

The smoke test parses the NDJSON, asserts every line is valid JSON,
and asserts each parsed object has the five mandatory fields. No
content equality is asserted — that's reserved for T3 schema work.

**Rationale**: Lock the *shape* (NDJSON, five mandatory fields)
without locking the *schema* — gives T3 freedom to extend with
schema versioning, diagnostic kind enums, fixit hints, etc.,
without breaking M1 tests. NDJSON is preferred over a JSON array
because it streams (matches the LSP's
`window/showMessage` push-style consumption pattern) and is
trivial to grep / diff.

**Alternatives**:
- *Lock the full schema at M1*: violates the /speckit-clarify Q2
  resolution and risks rework. Rejected.
- *Pretty-printed JSON array at M1*: doesn't stream, and the LSP
  use case at T3 wants newline-delimited. Rejected.
- *No JSON path at M1*: violates FR-027. Rejected.

---

## §10. Helper evaluator error model

**Decision**: Three error categories at the helper-evaluator level,
each producing a diagnostic at the helper-call's source location:

1. **Domain error** (`_log(0)`, `_sqrt(-1)`, `_asin(2)`): emit
   `error: helper '_NAME' domain error: <reason>` and produce a
   `PPValue` of integer `0` for fail-soft continuation.
2. **Overflow** (`_pow(2, 1024)`): emit `warning: helper '_pow'
   result exceeds long-double range`; the resulting infinity flows
   through subsequent computations until it reaches a context that
   coerces to integer (where it becomes another error).
3. **Arity mismatch** detected by the parser (`_min(1)` —
   one argument when two are required): emit `error: helper
   '_NAME' expects N arguments, got M` and abort the current
   `#define` body's evaluation (the macro is left undefined; a
   future use site re-emits the original error).

**Rationale**: Domain errors are bugs in the user's source and must
surface at the original line; overflows are typically deliberate
("infinity until I hit `_int`") and warning-not-error matches
floating-point convention; arity mismatches are static and caught
at parse time, before evaluation, so they short-circuit cleanly.
Failsoft `0` for domain errors prevents cascade failures (a single
bad helper call would otherwise abort `#define` evaluation entirely
and produce N follow-on errors). The "kind-preserving" return value
discipline of `_abs`/`_min`/`_max` is preserved in the failsoft
path (integer `0` is a safe default kind).

**Alternatives**:
- *Hard-fail on every domain error*: cascade failures; reviewer
  can't diff one error vs. ten. Rejected.
- *Domain errors as warnings*: too lenient; `_log(0)` should not
  silently produce a useful number. Rejected.

---

## §11. Constitution Check — post-design re-evaluation

After Phase 1 design (data-model.md, contracts/, quickstart.md):

| Principle | Status post-design | Notes |
|---|---|---|
| I. Spec Authoritative | ✅ | No `Sn`/`Nn`/`Pn` introduced or renumbered. |
| II. Layered Library | ✅ | Three new libraries added via M0's `add_nsl_library`; layer table preserved. `tools/nslc/main.cpp` ≤ 60 lines confirmed in design (the ~10-line delta lives in a single switch case). |
| III. Stock CIRCT | ✅ | M1 introduces no CIRCT-adjacent code. |
| IV. Source-Locating Diagnostics | ✅ | SourceManager design (research §3) keeps physical and virtual location separate; FR-027 JSON path lives in `Diagnostic.h`. |
| V. Inspectable Deterministic | ✅ | Determinism strategy in research §4 is type-system-enforced; `-emit=tokens` is the new stage flag (research §6 attribute classification keeps Token sized for cache locality). |
| VI. Layered Test Discipline | ✅ | Test corpus organization in research §8 satisfies "one fixture per Pn / per keyword / per number form" without violating Principle VI's lit+FileCheck mandate. |
| VII. Spec ↔ Design Coupling | ✅ | One follow-up coupling fix (CLAUDE.md §1 row) deliberately scoped to a separate PR per Q1. |
| VIII. TDD | ✅ | Tasks plan will sequence test-first per FR-036. |
| IX. CI/CD | ✅ | Stages 3 + 4 of the M0 pipeline gain real content; stages 5/6 remain wired-but-empty. |
| Build/Code/Licensing | ✅ | C++17 across; SPDX header on every new file. |

**Gate result post-design: PASSES.** No violations to record;
`Complexity Tracking` in plan.md remains empty.

---

## §12. Open follow-ups (recorded for tasks.md and post-merge)

These are non-blocking work items surfaced during research; none
gate M1's acceptance:

1. **CLAUDE.md §1 helper-eval row coupling-fix** — one-line
   amendment to the "Compile-time helpers" row to disentangle
   "preprocessor evaluator (M1)" from "Sema NSL-language constant
   evaluator (M3)". Scoped to a separate PR per /speckit-clarify Q1.
   `nsl-coupling-audit` will flag this on the M1 PR; address in PR
   review by referencing the follow-up issue.
2. **Per-line throughput SLO for the lexer** — deferred per spec
   "Performance: Outstanding"; revisit at M7 when the audited
   corpus gives a measurement basis.
3. **JSON schema lock for diagnostics** — deferred to T3 per
   /speckit-clarify Q2.
4. **`KeywordSet.def`** sibling X-macro to `HelperSet.def` is also
   single-source-of-truth for the §15 keyword grid; a future spec
   change adding/removing a keyword regenerates the lexer
   recognizer + the test corpus from one source. Implement as a
   tasks.md item.
5. **`lang.ebnf §15` lines 822–824 helper-list staleness** — surfaced
   during T004/T005 implementation. The §15 parenthetical "Lexically-
   reserved (prefix '_') system names — Preprocessor-only" list is
   inconsistent with `pp.ebnf §3` lines 282–288 (the canonical helper
   grammar production): §15 includes `_atan2`, `_fabs`, `_fmod` but
   misses `_round`, `_abs`, `_min`, `_max`. Per parser-note **N11**
   (lang.ebnf:1101), preprocessor helpers are formally defined in
   `pp.ebnf §3.1`, so `pp.ebnf` wins. The §15 comment list should be
   either deleted (preferred) or synced to pp.ebnf §3. Principle-VII
   coupling-fix follow-up scoped to a separate PR alongside the
   CLAUDE.md §1 helper-eval row fix from /speckit-clarify Q1.
