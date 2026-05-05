<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Feature Specification: T1 — TextMate Grammar + Language Configuration for NSL

**Feature Branch**: `009-t1-textmate-grammar`
**Created**: 2026-05-04
**Status**: Draft
**Input**: User description: "T1"

> **Roadmap anchor.** Tooling-track milestone **T1**, the first
> tooling milestone, per [`README.md`](../../README.md) §Roadmap row
> T1 and [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
> §4 (Syntax Highlighter). T1 has **no compiler-track dependency** —
> it is the first tooling milestone able to run in parallel with
> compiler work, and the only tooling milestone with no LSP
> involvement (LSP work begins at T3).
>
> **In-scope deferral.** The original T1 plan included filing a PR
> against `github-linguist/linguist` for `.nsl` recognition; that
> cross-org publication step is **paused** per
> [`README.md`](../../README.md) §Roadmap deferral note. Everything
> that ships *in-tree* — the TextMate grammar JSON,
> `language-configuration.json`, and the scope-test fixture — is
> unchanged.

---

## User Scenarios & Testing *(mandatory)*

### User Story 1 — NSL author opens a file and sees structure at a glance (Priority: P1)

A hardware engineer authoring an NSL module — or a reviewer reading one on
GitHub's web view — opens a `.nsl` file in any editor that supports
TextMate grammars. Reserved keywords (`module`, `declare`, `reg`, `wire`,
`proc`, `state`, `func`, `goto`, `finish`, …), comments, string literals,
and number literals (decimal, hex `0x…`, binary `0b…`, octal `0o…`, and
the Verilog-style sized form `<width>'b…` / `'o…` / `'d…` / `'h…`
including `Z` / `X` / `U` value markers per `lang.ebnf §13`) are each
visually distinct. The reader can scan the file and recognise its
structure without invoking the compiler.

**Why this priority**: This is the entire user-visible value of T1. With
this story alone, the project gains GitHub-web syntax colouring for the
audited corpus and any author can drop the grammar folder into their
editor's extension path for instant offline highlighting. No compiler
binary is needed.

**Independent Test**: Open a representative NSL file from the audited
corpus (e.g. `rv32x_dev/main.nsl`) in VS Code with the grammar folder
loaded; observe that every reserved keyword from `lang.ebnf §15` is
coloured under the keyword scope, every number form under a numeric
scope, every comment under a comment scope, every string under a string
scope, and that no token inside a comment or string is mis-coloured as a
keyword.

**Acceptance Scenarios**:

1. **Given** an NSL file containing every reserved keyword from
   `lang.ebnf §15`, **When** opened in a TextMate-compatible viewer
   with the T1 grammar loaded, **Then** every keyword token is assigned
   to the keyword-category scope appropriate to its role
   (declaration / control / flow / modifier / storage / system).
2. **Given** an NSL line `// proc foo {` (a `//` line comment that
   contains the keyword `proc`), **When** rendered, **Then** the entire
   line carries the line-comment scope and the word `proc` does **not**
   carry the keyword scope.
3. **Given** an NSL line `reg q[8] = 8'hFF;`, **When** rendered, **Then**
   `reg` carries the storage-class scope, `8` carries the numeric scope,
   `8'hFF` carries the numeric scope (one token, not three), and `;`
   carries no special scope.
4. **Given** an NSL fragment with a block comment `/* func_in foo() */`,
   **When** rendered, **Then** the entire fragment between `/*` and `*/`
   carries the block-comment scope and `func_in` does not carry the
   storage-class scope.
5. **Given** an NSL fragment containing the string `"hello world"`,
   **When** rendered, **Then** the entire fragment carries the
   string-literal scope.

---

### User Story 2 — Editor affordances when authoring NSL in VS Code (Priority: P2)

An engineer authoring NSL in VS Code expects the same baseline editing
behaviours offered for every first-class language: typing `{` produces a
matching `}`, typing `(` produces a matching `)`, typing `"` produces a
matching `"`, the comment-toggle keystroke wraps the selection in `//` or
`/* */`, and pressing `Enter` after `{` indents one level. None of these
require the compiler.

**Why this priority**: Without language configuration, the grammar alone
colours tokens but the editor still treats NSL as plain text for
brackets, commenting, and indentation — a poor authoring experience.
With it, NSL becomes pleasant to author in VS Code on day one of the
tooling track.

**Independent Test**: In VS Code, open a fresh `.nsl` file with the T1
package loaded; type `module foo {⏎` and confirm that the cursor lands
indented inside an auto-inserted `}`; select a region and press the
comment-toggle shortcut; confirm the selection is wrapped in `//`
(line) or `/* … */` (block) appropriately; type `"a` and confirm a
closing `"` is auto-inserted.

**Acceptance Scenarios**:

1. **Given** a `.nsl` file open in VS Code, **When** the user types `{`,
   **Then** a matching `}` is auto-inserted with the cursor between them.
2. **Given** a selection inside a `.nsl` file, **When** the user invokes
   the line-comment-toggle command, **Then** each selected line is
   prefixed with `//` (or has its `//` prefix removed if the line was
   already a line comment).
3. **Given** a selection inside a `.nsl` file, **When** the user invokes
   the block-comment-toggle command, **Then** the selection is wrapped
   in `/* … */`.
4. **Given** the cursor immediately after `{` at end of line, **When**
   the user presses `Enter`, **Then** the new line is indented one
   level deeper than the line containing the `{`.

---

### User Story 3 — Preprocessor directives and macro splices are visually distinguished (Priority: P3)

NSL source uses the line-oriented preprocessor described in
`pp.ebnf §2` (`#include`, `#define`, `#undef`, `#if`, `#ifdef`,
`#ifndef`, `#else`, `#endif`, `#line`) and the `%IDENT%` macro splicing
form described in `§4`. Both should be visually distinct from
NSL-language keywords so a reader can see at a glance where the
preprocessor is active.

**Why this priority**: Preprocessor lines look NSL-ish but follow a
different grammar; mis-colouring them as NSL keywords (or worse, as
identifiers) hides which lines disappear at the preprocessor seam (P12).
Calling them out as a separate category is high value but lower priority
than baseline keyword/comment/literal colouring (P1).

**Independent Test**: Open a fixture file containing every directive
form from `pp.ebnf §2` plus one `%IDENT%` reference; confirm each
directive line carries a directive scope distinct from
`keyword.declaration` / `keyword.control`, and that `%IDENT%` carries
its own macro-reference scope.

**Acceptance Scenarios**:

1. **Given** a line `#include "foo.nsl"`, **When** rendered, **Then**
   the directive token (`#include`) carries the directive scope and the
   string `"foo.nsl"` carries the string-literal scope.
2. **Given** a fragment `reg q[%WIDTH%];`, **When** rendered, **Then**
   `%WIDTH%` carries a macro-reference scope distinct from the
   storage-class scope of `reg` and the numeric scope of any sized
   literal.
3. **Given** a line `#define WIDTH 8`, **When** rendered, **Then**
   `#define` carries the directive scope; the rest of the line is
   coloured under whatever scopes its tokens would normally carry
   (best-effort — the textmate grammar is regex-only).

---

### User Story 4 — Grammar maintainer adds or revises a keyword without breaking the highlighter (Priority: P3)

The NSL spec evolves under Constitution Principle I (monotonic
S/N/P numbering) and the `coupling-audit` rule (Principle VII): any
spec change that touches `lang.ebnf §15` (reserved keywords) must
propagate to the highlighter. The maintainer needs an automated check
that catches drift before merge.

**Why this priority**: This is the **test gate** stated in
[`README.md`](../../README.md) §Roadmap row T1: "TextMate scope tests
on a fixture file matching every keyword, number form, and string
literal." Without this gate, the grammar drifts silently from the spec
and the audited-corpus colouring degrades over time. Bundled with T1.

**Independent Test**: Extend the fixture file with one new keyword;
without updating the grammar, run the scope tests and confirm they
fail with a localised diagnostic; update the grammar to recognise the
new keyword; re-run the tests and confirm they pass.

**Acceptance Scenarios**:

1. **Given** the current grammar and the current fixture, **When** the
   scope tests run, **Then** every assertion passes.
2. **Given** a new keyword added to `lang.ebnf §15` but not to the
   grammar, **When** a corresponding fixture line is added asserting
   the new keyword's scope, **Then** the scope tests fail and identify
   the missing keyword by name.
3. **Given** a fixture file regression run inside the same CI workflow
   as compiler tests, **When** the workflow runs on a clean checkout
   that does not contain a built compiler, **Then** the scope tests
   still execute and pass — the highlighter has no compiler dependency.

---

### Edge Cases

- **Block comment containing keyword-like tokens** (`/* func */`): the
  tokens between `/*` and `*/` must not match keyword/storage scopes.
- **String literal containing keyword-like tokens** (`"reg q[8];"`):
  same — only the string scope applies.
- **Verilog-sized number with `Z`/`X`/`U` value markers**
  (`8'hFZ_3X`, `4'bx10z`): the entire token is one numeric literal;
  underscores within digits are part of the token (per `lang.ebnf
  §13`).
- **Sign-extend `#` vs `#line` directive**: both use `#`. The directive
  matches at line start (per `pp.ebnf §1` line orientation); the
  sign-extend `#` matches in expression position. TextMate is
  context-free so this disambiguation is best-effort and will be
  perfected at T8 (tree-sitter) and T4 (LSP semantic tokens). This
  best-effort behaviour is acceptable for T1.
- **Reduction-vs-bitwise `&` `|` `^` (parser note N2)**: same situation
  — TextMate cannot disambiguate; both cases match the same operator
  scope at T1. Refinement deferred to T4/T8.
- **Built-in `_`-prefix names** (`_display`, `_random`, `_time`,
  `_finish`, `_init`, `_delay`, `_readmemh`, `_readmemb`, `_stop`,
  `_monitor`, `_write`, per `lang.ebnf` parser note N11): these
  receive a dedicated system-name scope distinct from regular
  identifiers; user-defined `_x` names remain unscoped.
- **Backslash escapes inside strings** (`"a\nb"`): the backslash
  sequence carries a sub-scope distinct from the surrounding string
  scope.
- **`label` keyword (parser note N10)**: reserved but rarely used; the
  grammar must include it in the keyword set even though the audited
  corpus does not exercise it.
- **File extension `.inc`**: this extension is shared with several
  other languages (Verilog includes, Pascal, ASM macros). Associating
  it with the NSL grammar may cause false-positive activation on
  non-NSL `.inc` files in mixed repos. Acceptable for T1 because the
  grammar is conservative (no destructive transforms; just colouring),
  and the deferred Linguist PR can add the disambiguator later.

## Requirements *(mandatory)*

### Functional Requirements

#### Coverage of the NSL spec

- **FR-001**: The grammar MUST recognise and assign a keyword scope to
  every reserved keyword listed in `docs/spec/nsl_lang.ebnf §15` —
  both the Appendix-3 set and the practical-additions set in the same
  section. (As of 2026-05-04 this is **42 keywords** — 31 Appendix-3 +
  11 practical additions per `include/nsl/Lex/KeywordSet.def`; the
  count tracks the X-macro file in source-of-truth coupling.)
- **FR-002**: The grammar MUST distinguish keyword sub-categories per
  `docs/design/nsl_tooling_design.md §4.1`: declaration, control-block,
  control-flow, modifier, storage-type, support-type, support-function,
  support-variable. T1 introduces one additional sub-scope —
  `storage.modifier.direction.nsl` for `inout`/`input`/`output`
  port-direction keywords — which `nsl_tooling_design.md §4.1`
  does NOT enumerate; per `data-model.md §1.2` rationale, this
  honours FR-001 (every keyword highlighted) by giving the three
  port-direction tokens a sub-scope rather than dumping them into
  `storage.type.control.nsl`. The full T1 category mapping is
  frozen in `contracts/grammar-coverage.contract.md §1`.
- **FR-003**: The grammar MUST recognise line comments (`// …` to end
  of line) and block comments (`/* … */`, non-nestable per
  `lang.ebnf §14`) and ensure no inner token receives a non-comment
  scope.
- **FR-004**: The grammar MUST recognise string literals (`"…"` with
  backslash escapes) and ensure no inner token receives a non-string
  scope; backslash escape sequences inside strings receive a distinct
  sub-scope.
- **FR-005**: The grammar MUST recognise every numeric literal form
  defined in `lang.ebnf §13`: bare decimal, hex `0x…`, binary
  `0b…`, octal `0o…`, and Verilog-style sized literals `<width>'b…`,
  `<width>'o…`, `<width>'d…`, `<width>'h…`, including `Z`/`X`/`U`
  value markers and underscore digit separators.
- **FR-006**: The grammar MUST recognise every preprocessor directive
  defined in `pp.ebnf §2` (`#include`, `#define`, `#undef`, `#if`,
  `#ifdef`, `#ifndef`, `#else`, `#endif`, `#line`) and assign them a
  directive scope distinct from NSL-language keyword scopes.
- **FR-007**: The grammar MUST recognise the `%IDENT%` macro-reference
  form defined in `pp.ebnf §4` and assign it a macro-reference
  scope distinct from identifier and keyword scopes.
- **FR-008**: The grammar MUST recognise operators per
  `nsl_tooling_design.md §4.1`: arithmetic (`+`, `-`, `*`, `++`, `--`),
  bitwise (`&`, `|`, `^`, `~`), shift (`<<`, `>>`), comparison (`==`,
  `!=`, `<`, `<=`, `>`, `>=`), logical (`&&`, `||`, `!`), assignment
  (`=`, `:=`), and extension (`#`, `'`).
- **FR-009**: The grammar MUST recognise built-in `_`-prefix system
  names listed in `nsl_tooling_design.md §4.1` (system functions
  `_display`/`_monitor`/`_write`/`_finish`/`_stop`/`_readmemh`/
  `_readmemb`/`_init`/`_delay`; system variables `_random`/`_time`)
  and assign them a system-name scope distinct from user identifiers.
- **FR-010**: The grammar MUST recognise auto-synthesised clock and
  reset names `m_clock` and `p_reset` (per `lang.ebnf §15` and
  `nsl_tooling_design.md §4.1`) and assign them a clock/reset support
  scope.

#### Packaging and editor behaviour

- **FR-011**: The grammar MUST associate to scope name `source.nsl`
  and apply to file extensions `.nsl`, `.nslh`, and `.inc` (per
  `nsl_tooling_design.md §4.2` `fileTypes`).
- **FR-012**: The language configuration MUST declare line-comment
  marker `//` and block-comment markers `/* … */`.
- **FR-013**: The language configuration MUST declare bracket-matching
  pairs, auto-close pairs, and surround-with pairs for `()`, `[]`,
  `{}`, and `"…"`.
- **FR-014**: The language configuration MUST declare a word pattern
  matching the NSL identifier production from `lang.ebnf §13` —
  i.e. a leading letter or underscore followed by letters / digits /
  underscores.
- **FR-015**: The language configuration MUST declare an indent-rule
  pair such that opening `{` increases indent and closing `}`
  decreases indent on the same line.

#### Test gate (mandatory per README §Roadmap T1)

- **FR-016**: A scope-test fixture MUST exist that contains at least
  one occurrence of every reserved keyword from `lang.ebnf §15`,
  every numeric literal form from `§13`, every operator category from
  `nsl_tooling_design.md §4.1`, every preprocessor directive form
  from `pp.ebnf §2`, every `%IDENT%` form from `§4`, both comment
  forms, and at least one string literal with an escape sequence.
- **FR-017**: An automated scope-test runner MUST verify, for every
  expected (file, line, column) → scope assertion in the fixture,
  that the grammar assigns that scope. Test failure MUST identify the
  failing assertion by file/line/column and observed-vs-expected
  scope.
- **FR-018**: The scope-test runner MUST run in the project's existing
  CI matrix (whichever cell makes sense for a non-compiled artifact)
  and MUST fail the CI run if any assertion fails.
- **FR-019**: The scope tests MUST run on a clean checkout without a
  built NSL compiler — the grammar package has no compile-time
  dependency on `libNSLFrontend.a`, `nslc`, `nsl-opt`, or any other
  in-tree binary, satisfying Constitution Principle II's
  no-duplication rule by trivial means (no parser at all in T1).

#### Boundaries

- **FR-020**: The grammar MUST NOT attempt semantic identifier
  classification: it MUST NOT distinguish `reg` references from
  `wire` references in expression position, MUST NOT tag
  control-terminal names per `S27`, and MUST NOT track
  declaration-vs-reference contexts. Per
  `nsl_tooling_design.md §4.1`, "TextMate leaves [identifiers]
  un-scoped" — semantic identifier scopes are deferred to T4 (LSP
  `semanticTokens`) and T8 (tree-sitter).
- **FR-021**: The grammar MUST NOT attempt context-sensitive
  disambiguation that the EBNF marks as parser-note territory
  (`N5` `#` line-marker vs sign-extend; `N2` reduction vs bitwise
  `&`/`|`/`^`; `N3` `.{` two-character lookahead). Best-effort
  regex matching is acceptable; correctness is delegated to T8.
- **FR-022**: The deliverable MUST NOT include a published VS Code
  Marketplace listing, a `github-linguist/linguist` PR, or any
  cross-organisation publication step (deferred per
  [`README.md`](../../README.md) §Roadmap T1/T12 deferral note).

### Key Entities

- **TextMate grammar** — A regex-based token-classification
  description that maps surface-syntax patterns to nested scope
  names. Lives in-tree as a JSON file; consumed by VS Code,
  Sublime, Atom, GitHub web, TextMate itself, and any other
  TextMate-compatible viewer.
- **Language configuration** — Editor-behaviour metadata (comment
  delimiters, bracket pairs, auto-close pairs, surround pairs,
  word pattern, indent rules). Lives in-tree as a JSON file;
  consumed primarily by VS Code and editors that respect VS
  Code's language-configuration schema.
- **Scope-test fixture** — A representative NSL file (`.nsl`) with
  inline assertions stating, for selected character ranges, what
  scope the grammar should assign. Lives under the project's
  `test/` tree (precise location is plan-level).
- **Token category** — A grouping of related scope names
  (declaration, control-block, control-flow, modifier, storage,
  support-type, support-function, support-variable, numeric,
  string, comment, operator-arithmetic, operator-bitwise,
  operator-shift, operator-comparison, operator-logical,
  operator-assignment, operator-extension, directive,
  macro-reference). The category set is closed and matches
  `nsl_tooling_design.md §4.1`.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A reader opening any audited-corpus NSL file (the
  seven projects vendored under audited-corpus regression — `rv32x_dev`,
  `turboV`, `mmcspi`, `SDRAM_Controler`, `mips32_single_cycle`,
  `ahb_lite_nsl`, `cpu16`) in a TextMate-compatible viewer sees
  100% of reserved keywords coloured under the keyword scope, with
  zero keyword-mis-colouring inside comments or string literals.
- **SC-002**: The scope-test fixture asserts ≥ 1 occurrence per
  reserved keyword in `lang.ebnf §15` (currently **42 distinct
  keyword assertions** as of 2026-05-04; the count tracks the
  X-macro file `include/nsl/Lex/KeywordSet.def` in source-of-truth
  coupling per Principle VII), ≥ 1 occurrence per numeric form
  (≥ 8 forms: bare decimal, hex `0x…`, binary `0b…`, octal `0o…`,
  Verilog-sized for each of `b`/`o`/`d`/`h` × with-and-without
  `Z`/`X`/`U` markers), and ≥ 1 occurrence per directive form
  (9 directives) — and 100% of these assertions pass on a clean
  checkout.
- **SC-003**: When `lang.ebnf §15` gains a new reserved keyword,
  the same PR that adds the keyword adds a fixture assertion and a
  grammar entry; the scope tests pass before the PR merges. (This
  is enforceable via CI — when the spec changes without a grammar
  update, CI fails.)
- **SC-004**: A new contributor can install the T1 package locally
  in VS Code (drop folder into `~/.vscode/extensions/`) in under 60
  seconds with no compiler, no tree-sitter runtime, and no LSP
  process required.
- **SC-005**: The T1 deliverable contributes 0 bytes of compiled
  binary to the project; package size is dominated by the JSON
  files (≤ 50 KB total expected).
- **SC-006**: Running scope tests against the fixture takes under
  10 seconds on a standard CI runner — fast enough that the test
  gate may run on every PR with negligible cost.

## Assumptions

- **File-extension set**: The grammar associates with `.nsl`,
  `.nslh`, and `.inc` per the existing `fileTypes` array in
  `nsl_tooling_design.md §4.2`. The `.inc` extension is shared with
  other languages; this risk is documented in the Edge Cases
  section and accepted because TextMate colouring is non-destructive.
- **Scope-name namespace**: Scope names follow the established
  TextMate convention (e.g. `keyword.control.flow.nsl`,
  `storage.type.register.nsl`, `comment.line.double-slash.nsl`)
  and the explicit list in `nsl_tooling_design.md §4.1`.
  Implementation may extend the list with additional sub-scopes
  (e.g. `keyword.directive.preprocessor.nsl`) as long as parent
  scopes match those listed in §4.1 so that downstream consumers
  (themes, T4/T8 follow-ups) match correctly.
- **Editor-integration depth at T1**: T1 ships only the JSON files
  and a fixture. The `editors/vscode/` packaging directory layout
  per `nsl_tooling_design.md §8` is honoured, but no `.vsix`
  artefact is built and no Marketplace listing is published —
  consumption is via folder-drop at `~/.vscode/extensions/` (or
  the equivalent for other editors). Marketplace publication
  was previously planned for T12 but is deferred per
  [`README.md`](../../README.md) §Roadmap deferral note.
- **Linguist disambiguator**: GitHub's source-language statistics
  for the repository will continue to mis-classify NSL until a
  `github-linguist/linguist` PR lands. That PR is deferred per
  [`README.md`](../../README.md) §Roadmap T1 note and is **out
  of scope** for this feature. The grammar will produce correct
  colouring on GitHub's web view as soon as Linguist learns about
  `.nsl`; no further grammar work is required at that point.
- **Tree-sitter and LSP**: Semantic identifier scopes (register vs
  wire references, control-terminal name highlighting per
  `S27`, `proc_name` vs `state_name` references, etc.) are NOT
  delivered at T1. They land at T8 (tree-sitter) and T4 (LSP
  `semanticTokens`).
- **Lint, format, LSP**: All compiler-backed tooling features —
  diagnostics, hover, definition, formatter, lint rules — are
  out of scope for T1 and require the compiler stack at M3 or
  later. T1 is the only tooling milestone with no compiler
  dependency.
- **Audited-corpus access**: Acceptance scenarios reference NSL
  files from the audited corpus. As of 2026-05-04, the corpus
  is vendored at M5+ (P-VEN landing in the M-track), so the
  audited-corpus colouring claim in SC-001 may be verified on
  whichever subset has already been vendored at the time T1
  merges. If T1 merges before P-VEN, SC-001 is verified on a
  hand-written representative file equivalent to the audited
  corpus in coverage.
- **CI matrix**: T1's CI cell is added to the existing GitHub
  Actions matrix; choice between adding a dedicated job or
  extending an existing one is a plan-level decision. The
  invariant from FR-018 is that the cell must run on every PR
  and must fail the run on any scope-test failure.
