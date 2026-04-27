<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Research: Bare-Macro Textual Concatenation

**Branch**: `003-macro-textual-concat` | **Date**: 2026-04-27
**Plan**: [plan.md](./plan.md)

Each section resolves one Technical-Context decision (or one open
plan question) with **Decision / Rationale / Alternatives**. The
spec already pinned the user-visible decisions: substitution scope
(`#define` body + `#if` condition only), adjacency rules (match
`%IDENT%` per P3), recursion budget (256 levels mirroring M1
FR-022), FR-007 locked diagnostic string. This file pins the
*implementation-mechanism* decisions left as plan-phase choices.

---

## §1. Substitution algorithm: pre-tokenize textual pre-pass

**Decision**: Add a textual-substitution **pre-pass** that runs over
the raw character stream of a `#define` body / `#if` condition
**before** the expression parser tokenizes the input. The pre-pass:

1. Walks the input characters left to right.
2. At each position, attempts to lex an identifier (sequence of
   `[A-Za-z_][A-Za-z0-9_]*` characters).
3. Looks up the identifier in the `MacroTable`.
4. If defined: replaces the identifier's character span with the
   macro body's character span, then **resumes scanning at the
   beginning of the substituted text** (so recursive references
   re-trigger the lookup until either a non-macro identifier or
   the cycle-budget kicks in).
5. If undefined: leaves the identifier in place, advances past it.
6. Skips identifier scanning inside string literals (string content
   is not subject to substitution).

The output is a fully-substituted character buffer, which is then
handed to the existing `PPExpression` lexer + parser.

**Rationale**: This is the C-preprocessor model. Three converging
reasons it's the right shape here:

1. **Spec compliance**: pp.ebnf P5's canonical example
   `_int(_pow(2.0, DEPTH.0))` works only if `DEPTH` substitutes
   into the character stream as `8`, then re-tokenization picks
   up `8.0` as a single `float_literal`. A token-level model
   (substitute `DEPTH` token with the integer-token `8`, then
   parse) cannot produce `8.0` from `8` + `.0` because adjacent
   tokens aren't merged at parse time.
2. **Adjacency invariant**: pp.ebnf P3's `%IDENT%` splicing is
   already documented as "splice the replacement TEXT into the
   surrounding identifier text" — character-level. Bare-identifier
   substitution adopting the same adjacency rules makes the two
   forms semantically equivalent (per spec assumption 4).
3. **Implementation simplicity**: a character-walking pre-pass is
   ~150 lines of straightforward C++. The alternative — modifying
   the existing recursive-descent expression parser to splice
   tokens at parse time and trigger re-tokenization — is more
   invasive and harder to reason about.

**Alternatives**:

- *Token-level substitution*: rejected per the spec-compliance
  argument above. C++ macros are token-paste-only (`##` operator);
  NSL's pp.ebnf doesn't have `##` and the P5 example explicitly
  needs character-level adjacency.
- *Eager substitution at `#define` time*: substitute the body once
  when the directive is processed, then store the resolved text.
  Rejected because of recursive references with later definitions
  — `#define A B` is legal even if `B` is defined later. Lazy
  substitution at use-site preserves the existing M1 semantics.
- *Hybrid (token-level for whole-token cases, character-level for
  adjacency cases)*: rejected as adding complexity without
  benefit. Character-level handles all cases uniformly.

---

## §2. SourceLocation propagation: outermost use-site

**Decision**: Tokens emitted by the lexer over the substituted
character buffer carry a `SourceLocation` pointing at the
**outermost use-site** in the original source — i.e., the
`SourceLocation` of the **macro reference being expanded**, not the
location of the macro's body in the `#define` directive.

**Rationale**: Three reasons.

1. **C-preprocessor convention**: clang's preprocessor reports the
   use-site location for tokens produced by macro expansion, with
   "expanded from macro" notes pointing at the definition site.
   This matches user intuition: an error in `_int(_pow(2.0, DEPTH.0))`
   should cite the line containing the `_int(...)` call, not the
   line containing `#define DEPTH 8`.
2. **Diagnostic quality**: when an evaluator error fires (e.g.,
   `_log(0)` — domain error), the rendered `path:line:col` is
   most useful if it points at the call-site code the user is
   reading. The macro-definition location is secondary; we'd add
   it as a `note: expanded from macro <NAME>` follow-up note in a
   future polish PR.
3. **Existing M1 SourceManager already supports this**: the
   `SourceManager::pushIncludeFrame` / `popIncludeFrame` machinery
   from M1 Phase 5 (T068) handles the analogous case for
   `#include`. We don't need a new mechanism — we just reuse the
   `SourceRange` of the original macro-reference token (which is
   already in the input character stream's SourceLocation domain)
   on every token emitted from the post-substitution lexer pass.
   The substituted character buffer is **registered as a synthetic
   in-memory buffer** via `SourceManager::addBufferInMemory` (same
   pattern as M1's `EmitTokens.cpp` post-preprocess synthetic
   buffer); the lexer scans the synthetic buffer but emitted
   tokens get `SourceRange`s manually rewritten to point at the
   original use-site span.

**Alternatives**:

- *Substituted-text location* (i.e., tokens point into the
  synthetic in-memory buffer): rejected because users can't read
  it — the synthetic buffer has no on-disk path.
- *Macro-definition location*: rejected because it's confusing —
  the user's "current line" is the use-site, not the definition.
- *Stack of locations* (use-site + definition-site + recursion
  trail): correct in theory but complex; defer to a future polish
  PR per Track B Open Question #4 deferral pattern. M1's
  include-stack notes (T068) already establish the precedent that
  cross-context source-trail rendering is its own focused
  follow-up.

---

## §3. Re-tokenization mechanics: synthetic-buffer + lexer rerun

**Decision**: After the textual-substitution pre-pass produces the
final substituted character buffer, hand it to a **fresh lexer
instance** (`nsl::Lexer` from `lib/Lex/`) that scans the buffer
exactly as if it were freshly-read source text. The lexer's output
token stream is then handed to the `PPExpression` parser per the
existing M1 control flow.

**Rationale**:

- **Code reuse**: M1's `nsl::Lexer` already implements the full
  `lang.ebnf §13` lexical grammar including float-literal
  recognition (`8.0` ↔ `tk_float_literal` post-substitution).
  Reinventing the lexer inside `MacroExpander` would duplicate
  hundreds of lines.
- **Layer correctness**: `nsl-preprocess` already depends on
  `nsl-basic` (per layer table); adding a dependency on `nsl-lex`
  would cross a layer boundary in the wrong direction (preprocess
  is layer 2; lex is layer 3 — preprocess must NOT depend on lex
  per Principle II).
  - **Workaround**: rather than have `nsl-preprocess` link
    `nsl-lex`, we register the substituted buffer as a synthetic
    `SourceManager` buffer and let the **driver glue**
    (`lib/Driver/EmitTokens.cpp`, layer 9) run the lexer over it.
    The preprocessor's `PPExpression` keeps its existing
    hand-written lexer for the limited expression-grammar surface;
    only the *content* of the input string changes (post-textual-
    substitution).
  - This is a slight scope tightening from the original plan: the
    re-tokenization happens INSIDE `PPExpression`'s existing
    expression lexer (not the full `nsl-lex` lexer). For
    expression-grammar tokens (numbers, identifiers, operators,
    floats per pp.ebnf §3.2), this is sufficient — the
    expression lexer already recognizes float literals.
- **No new layer crossing**: the change is contained entirely
  within `lib/Preprocess/`. `MacroExpander.cpp` produces a
  substituted character buffer and hands it back to
  `PPExpression`, which lexes + parses + evaluates as today (with
  the substituted bytes as input rather than raw source bytes).

**Alternatives**:

- *Cross-layer call to nsl-lex*: rejected per the layer-boundary
  argument.
- *Lazy substitution during expression parsing* (rewrite the
  parser to splice tokens mid-parse): rejected as more invasive
  + harder to reason about + bypasses the spec's "characters
  adjoin" invariant.

---

## §4. Cycle-detection algorithm: depth-bounded recursion

**Decision**: Track substitution depth via an explicit counter
passed through `MacroExpander::expand()` recursive calls. Bound
at **256 levels** (FR-006 / matches M1 FR-022). If depth exceeds
the bound during expansion, emit the FR-007-locked diagnostic
`recursive macro expansion: <NAME>` (where `<NAME>` is the macro
that triggered the depth violation, typically the outermost name
in the cycle) and abort the current expansion (returning an
error sentinel that `PPExpression` propagates as an evaluation
error).

**Rationale**:

- **Consistent with M1 FR-022**: the include-stack also uses a
  depth-bound at 256 (`Preprocessor::Impl::IncludeStack` size
  cap). Reusing the same bound + same diagnostic style gives the
  user a consistent mental model.
- **O(1) per expansion**: no hash-keyed visited-set lookup. Faster
  + simpler than the alternative.
- **Detects ALL pathological cycles**: a 257-deep
  non-cyclic-but-deep chain (`A → B → C → ... → very_deep → 8`)
  IS treated as a cycle even if not strictly recursive. This is
  consistent with M1's interpretation of "cycle" for include
  recursion — pathological deep chains and true cycles both fail
  the user the same way. Acceptable.
- **Locked diagnostic**: FR-007's exact string is asserted at the
  fail-fixture per Constitution Principle VIII / M1 FR-037.

**Alternatives**:

- *Visited-set (per-expansion, hash-keyed by macro name)*:
  rejected for complexity. The depth-bound is uniformly
  applicable. If the user hits the 256-deep limit on a
  non-cyclic-but-deep chain, the error message is "recursive
  macro expansion" which is technically a small lie (the chain
  isn't recursive); but the practical fix (use shorter macro
  chains) is the same either way.
- *Cycle-edge detection* (track which macro is being expanded;
  re-entry to the same name = cycle): correct but requires the
  per-macro state. Deferred for the same reason as the visited-
  set: complexity vs. uniformity.

---

## §5. Constitution Check — post-design re-evaluation

After Phase 1 design (data-model.md, contracts/, quickstart.md):

| Principle | Status post-design | Notes |
|---|---|---|
| I. Spec Authoritative | ✅ | The pp.ebnf P10 amendment text is drafted in `data-model.md` §entity 1 ("substitution algorithm"). |
| II. Layered Library | ✅ | `MacroExpander.h` published at `include/nsl/Preprocess/` (matching the M1 `MacroTable.h` / `HelperEvaluator.h` precedent so the gtest suite can `#include` it directly); `.cpp` stays in `lib/Preprocess/`. No new layer crossings. Driver `tools/nslc/main.cpp` unchanged. |
| III. Stock CIRCT | ✅ | N/A. |
| IV. Source-Locating Diagnostics | ✅ | Use-site SourceLocation propagation per §2. Synthetic-buffer pattern reuses M1's `addBufferInMemory` mechanism. |
| V. Inspectable Deterministic | ✅ | Substitution pre-pass is pure function of (input text, macro table); macro-table iteration insertion-ordered (M1 FR-039 inherited). |
| VI. Layered Test Discipline | ✅ | 3 lit fixtures + 1 gtest suite per FR-010..FR-013. |
| VII. Spec ↔ Design Coupling | ✅ | pp.ebnf amendment + impl land together in this PR. |
| VIII. TDD | ✅ | Tasks plan sequences test-first per FR-015. |
| IX. CI/CD | ✅ | Reuses M0 ci.sh wiring; new gtest suite picked up via foreach. |
| Build/Code/Licensing | ✅ | C++17; SPDX on every new file. |

**Gate result post-design: PASSES.** No violations to record;
`Complexity Tracking` in plan.md remains empty.

---

## §6. Open follow-ups

These are non-blocking work items surfaced during research; none
gate this feature's acceptance:

1. **`note: expanded from macro <NAME>`** — a future polish PR
   could add per-expansion notes mirroring M1's include-stack
   notes (FR-026). Not in scope here. Track via Linear feature-
   track issue if user values it.
2. **Function-like macros** (`#define F(x) ...`) — pp.ebnf §2.2
   explicitly says "object-like macros only." Not in scope. Out
   of scope per spec Assumptions.
3. **`note: stack of macro expansions`** for the cycle diagnostic
   — could enhance the FR-007 message with a chain trace
   (`A → B → C → A`). Deferred for the same reason as #1: a
   polish PR after the basic feature lands.
