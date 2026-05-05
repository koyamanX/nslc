<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Feature Specification: T8 — Tree-sitter Grammar + Highlight Queries + VS Code WASM Consumer

**Feature Branch**: `010-t8-tree-sitter-grammar`
**Created**: 2026-05-05
**Status**: Draft
**Input**: User description: "T8"

## Clarifications

### Session 2026-05-05

- Q: Tree-sitter CLI version pinning strategy → A: Pin a specific minor version (e.g. `0.22.x`) in CI + a project-level config file; bump deliberately.
- Q: `tree-sitter-nsl.wasm` artefact commit policy → A: Don't commit to the repo; CI builds the WASM and uploads it as a workflow artefact, and tagged releases attach it. Local contributors build via `tree-sitter build-wasm`.
- Q: Capture-name granularity delivered by T8 in `queries/highlights.scm` → A: The `nsl_tooling_design.md §4.3` set, plus the eight sub-captures required to make Acceptance Scenarios AS1.1–AS1.5 pass: `@variable.register`, `@variable.wire`, `@variable.memory`, `@function.proc`, `@function.func`, `@function.call.proc`, `@function.call.func`, `@label.state`.
- Q: Smoke-fixture source when P-VEN (audited corpus vendoring at M7) has not yet landed at T8 merge → A: Run the smoke gate against the existing in-tree `examples/01_*.nsl`–`examples/20_simulation_tb.nsl` corpus (20 hand-written files curated for production coverage); add the audited corpus to the same gate once P-VEN lands.

> **Roadmap anchor.** Tooling-track milestone **T8**, per
> [`README.md`](../../README.md) §Roadmap row T8 and
> [`docs/design/nsl_tooling_design.md`](../../docs/design/nsl_tooling_design.md)
> §4.3 (Tree-sitter Grammar Skeleton). T8 builds on **T1**
> (TextMate scope alignment) and is the second of the project's
> two-tier highlighter strategy described in
> `nsl_tooling_design.md §4`: T1 ships a fast regex-based grammar
> for any TextMate-compatible viewer; T8 ships a
> **context-aware, semantic** parser that distinguishes
> identifier *contexts* (`reg` vs `wire` vs `proc_name` vs
> `func_in` references), tags control-terminal names per `S27`,
> handles `%IDENT%` macro splice sites, and resolves the parser-note
> ambiguities (`N5`, `N2`, `N3`) that T1 explicitly handled
> best-effort only.
>
> **Constitution Principle II (no duplication).** The tree-sitter
> grammar is an independent JS-based parser written for the
> highlighter tier; it does **not** reuse `libNSLFrontend.a`
> because tree-sitter requires its own grammar input format
> (`grammar.js`). This is the same scoped exception that applies
> to T1's TextMate grammar — both are "parallel parsers" needed
> for the highlighter tier specifically. The LSP, formatter, and
> linter (T3+/T2/T6) all reuse the front-end library and remain
> the canonical Sema source.
>
> **In-scope deferral.** Per
> [`README.md`](../../README.md) §Roadmap T1/T12 deferral note,
> all cross-organisation publication steps are paused for the
> project's current phase. Specifically: no VS Code Marketplace
> listing, no `nvim-treesitter` upstream PR, no
> `tree-sitter/tree-sitter-nsl` separate-repo carve-out. Everything
> that ships *in-tree* — `grammar.js`, `queries/highlights.scm`,
> `editors/vscode/` extension shell, generated `parser.c`, the
> WASM build pipeline, the smoke and golden tests — is unchanged.

---

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Semantic identifier scopes distinguish reg / wire / proc / state / func references (Priority: P1)

A hardware engineer reading an NSL module — locally in a tree-sitter-aware
editor (VS Code with the T8 extension shell, Neovim with native
tree-sitter, Emacs with `tree-sitter.el`) — sees register references,
wire references, procedure-name references, state-name references, and
function-name references each in a visually distinct colour. Where T1's
TextMate grammar left identifiers un-scoped (per
`nsl_tooling_design.md §4.1` "Identifiers (refined by tree-sitter / LSP;
TextMate leaves them un-scoped)"), T8 colours them according to their
declaration kind, derived from a real parse tree.

**Why this priority**: This is the **headline value** T8 delivers over
T1. Without tree-sitter, an NSL reader without an LSP server running
sees a sea of unscoped identifiers; with tree-sitter, the structure of
a module is legible at a glance — registers stand out from wires,
state-machine state names stand out from function calls, control-terminal
names used as 1-bit values (per constraint `S27`) are tagged as such.
This is the same value prop that lets a developer navigate a C++ file
in VS Code without clangd running.

**Independent Test**: Open a representative NSL file containing
declarations of all five identifier kinds — `reg q[8];`, `wire w;`,
`proc foo {…}`, `state idle;`, `func bar() …` — plus a use of each
in expression position. Confirm that the parse tree from
`grammar.js` correctly classifies each name node, and that
`queries/highlights.scm` assigns the corresponding capture
(`@variable.register`, `@variable.wire`, `@function.proc`, `@label.state`,
`@function.func` or equivalent tree-sitter convention names per
`nsl_tooling_design.md §4.3`). Verify that capture assignments are
identical for declaration sites and reference sites of the same
identifier.

**Acceptance Scenarios**:

1. **Given** an NSL module declaring `reg counter[8];` and using
   `counter` in an expression, **When** the file is parsed by the T8
   tree-sitter grammar, **Then** the declaration-site `counter` node
   AND every reference-site `counter` node carry the same
   register-variable capture under the highlight query, distinct from
   the wire-variable capture.
2. **Given** an NSL module declaring `wire w;` and using `w` in an
   expression, **When** parsed, **Then** the declaration- and
   reference-site `w` nodes carry a wire-variable capture distinct
   from the register-variable capture.
3. **Given** an NSL module declaring `proc compute {…}` and a control
   call `compute();`, **When** parsed, **Then** the procedure-name
   declaration site carries a procedure-definition capture; the
   call site carries a procedure-call capture; both are distinct
   from function and state captures.
4. **Given** an NSL module declaring `state idle;` and a transition
   `goto idle;`, **When** parsed, **Then** the state-name declaration
   site carries a state-label capture; the `goto` target carries a
   state-reference capture; both are distinct from procedure captures.
5. **Given** an NSL module declaring `func_in start();` and using
   `start` in an expression position (per `S27` constructive constraint
   — control-terminal name as 1-bit value), **When** parsed, **Then**
   the expression-position `start` carries a control-terminal-value
   capture distinct from a regular function-call capture.
6. **Given** an NSL fragment containing `reg q[%WIDTH%];`, **When**
   parsed, **Then** the `%WIDTH%` token carries a macro-reference
   capture, and the surrounding declaration is parsed as a register
   declaration with a macro-spliced width (parse must not fail merely
   because a macro is unexpanded — see Edge Cases).

---

### User Story 2 — Tree-sitter parses the audited corpus without error and the highlight queries pass a golden test (Priority: P2)

The T8 grammar must accept every NSL file in the audited corpus
(`rv32x_dev`, `turboV`, `mmcspi`, `SDRAM_Controler`, `mips32_single_cycle`,
`ahb_lite_nsl`, `cpu16` per `README.md` §Roadmap row M7 / `P-VEN`)
without producing an `(ERROR)` node, and the corresponding highlight
queries must produce the expected capture set for a curated golden
fixture file. This is the **test gate** stated in
[`README.md`](../../README.md) §Roadmap row T8: *"Tree-sitter parse
tree on the audited corpus matches expected structure (smoke);
highlight-query golden test."*

**Why this priority**: Without a real-world parse-success gate, the
grammar drifts away from the NSL the audited corpus actually uses; the
P1 colouring claim becomes vacuous on real files. Without a golden
highlight test, scope assignments drift from `nsl_tooling_design.md §4.3`
and the user-visible colouring degrades silently. Bundled as one P2
story because both gates land in the same CI cell.

**Independent Test**: Run `tree-sitter parse` over every `.nsl` /
`.nslh` / `.inc` file in the audited corpus; confirm zero `ERROR` nodes
and zero `MISSING` nodes. Run the golden highlight test (one
representative fixture file with inline scope assertions per
`tree-sitter test` convention); confirm every assertion passes.

**Acceptance Scenarios**:

1. **Given** the audited corpus vendored under `test/audited/<project>/`,
   **When** `tree-sitter parse --quiet` runs over every NSL file,
   **Then** the exit code is 0 and the report contains no `ERROR`
   or `MISSING` nodes.
2. **Given** the golden highlight fixture, **When** the
   `tree-sitter test` (or `tree-sitter highlight --test`) command runs,
   **Then** every inline `// <- @capture` (or equivalent
   tree-sitter-test syntax) assertion passes.
3. **Given** a regression — a grammar change that breaks parsing of a
   single audited file — **When** the smoke test runs in CI, **Then**
   the run fails and the failure output names the file and the byte
   offset of the `(ERROR)` node.
4. **Given** a clean-checkout CI run that does not have the M-track
   compiler binary built, **When** the T8 cell runs, **Then** the
   tests still execute and pass — the tree-sitter grammar has no
   compile-time dependency on `libNSLFrontend.a`, `nslc`, or any
   in-tree binary, satisfying the "T1 (TextMate scope alignment)"
   dependency stated in `README.md §Roadmap` only at the level of
   shared scope-name vocabulary, not at the level of build artefacts.

---

### User Story 3 — VS Code extension shell loads the WASM build of the grammar and applies highlight queries (Priority: P3)

An engineer authoring NSL in VS Code installs the T8 extension shell
(folder-drop into `~/.vscode/extensions/`, equivalent to T1's install
model) and gets context-aware tree-sitter colouring on top of the T1
TextMate base layer. The T1 layer continues to provide instant
colouring while the file is being typed; the T8 tree-sitter layer
refines identifier captures once the parse settles. This is the
in-tree consumer surface called out in
[`README.md`](../../README.md) §Roadmap row T8 ("VS Code extension
shell consuming the WASM tree-sitter build").

**Why this priority**: The grammar and queries are useful to
Neovim / Emacs users via their native tree-sitter integrations
(those land in **T11**). For VS Code users — the project's
primary editor target per `nsl_tooling_design.md §4.4` — there is
no built-in NSL tree-sitter consumer; T8 provides the minimum
extension shell so that the tree-sitter colouring is actually
visible in VS Code at T8 (rather than waiting until T11). P3
priority because the value-prop work is in P1 (semantic scopes)
and P2 (test gate); P3 packages it into the editor.

**Independent Test**: With a clean VS Code instance and the T8
extension folder installed under `~/.vscode/extensions/`, open a
fixture NSL file and visually confirm that:
(a) reserved keywords retain the colour assigned by T1 (no
regression), and (b) register references colour distinctly from
wire references — a colour distinction that T1 alone cannot
produce. Reload the window with VS Code's developer tools open and
confirm no extension-host errors related to loading the WASM grammar.

**Acceptance Scenarios**:

1. **Given** the T8 extension shell is installed (folder-drop),
   **When** a `.nsl` file is opened in VS Code, **Then** the
   tree-sitter parser loads the WASM grammar without throwing an
   extension-host error, and the highlight-query captures are
   applied as theme tokens.
2. **Given** the same file open in VS Code with T8 installed and
   T1 also installed, **When** the file is rendered, **Then** the
   T1 TextMate layer provides the base keyword/comment/literal
   colouring (no regression), and the T8 layer overrides identifier
   captures with the semantic register/wire/proc/state/func scopes.
3. **Given** the T8 extension shell is uninstalled, **When** the
   same file is opened, **Then** colouring degrades gracefully to
   T1 levels (registers, wires, procs all un-scoped — but the
   editor is still usable).
4. **Given** the WASM build pipeline runs in CI, **When** the build
   completes, **Then** the artefact `tree-sitter-nsl.wasm` is
   produced reproducibly (byte-identical on repeated builds with
   identical inputs) per Constitution Principle V.

---

### Edge Cases

- **`%IDENT%` macro splice in a position where the surrounding NSL
  production normally requires a non-macro token** (e.g.
  `reg q[%WIDTH%];`, `func %name%() …`, `module %M% { … }`):
  tree-sitter must accept the splice without producing an `(ERROR)`
  node. The grammar models `%IDENT%` as an *escape hatch* permitted
  wherever an identifier or expression is expected, so live editing
  pre-preprocessing succeeds. The capture assigns macro-reference
  scope to the `%…%` token itself; the surrounding production
  resolves once the editor or compiler expands the macro
  (post-T8 — out of scope here).
- **`#` line-marker (`#line`) vs sign-extend `#expr`** (parser
  note `N5`): tree-sitter parses `#line`-form directives as
  preprocessor-directive nodes at line start (per `pp.ebnf §2.4`)
  and `#expr` as a sign-extend operator in expression position;
  the disambiguation that T1 marked best-effort
  (`spec/T1` Edge Cases) is correct in T8.
- **Reduction vs bitwise `&`/`|`/`^`** (parser note `N2`):
  tree-sitter parses these by *position in the production*
  (operator vs unary reduction) rather than by regex shape;
  T1's best-effort behaviour is upgraded to correct here.
- **`.{` two-character lookahead** (parser note `N3`): the
  tree-sitter grammar consumes `.{ … }` as a single
  field-aggregate construct where `nsl_lang.ebnf` requires it,
  using tree-sitter's lookahead facility natively.
- **Block comment containing a `//` substring**: parsed as a single
  `block_comment` extra-token node (per `nsl_lang.ebnf §14`,
  block comments are non-nestable but the inner `//` is not a line
  comment; tree-sitter handles via the standard `extras` mechanism).
- **String literal containing a closing-`"` escape** (`"a\"b"`):
  parsed as one `string_literal` node; the `\"` substring carries
  an escape-sequence sub-capture distinct from the surrounding
  string capture.
- **File-extension overlap with non-NSL `.inc`** (Verilog includes,
  Pascal, ASM macros): the VS Code extension declares the same
  `fileTypes` set as T1 (`.nsl`, `.nslh`, `.inc`) so that T1 and T8
  activate together; the same false-positive risk applies and is
  accepted on the same grounds as T1 (highlighting is non-destructive,
  and the deferred Linguist PR can disambiguate later).
- **An audited-corpus file that uses an NSL construct not covered by
  the design-doc skeleton in `nsl_tooling_design.md §4.3`** (the
  skeleton is illustrative, not exhaustive): the grammar must still
  parse it. The design-doc reference is the *starting* shape of
  `grammar.js`; the EBNF in `docs/spec/nsl_lang.ebnf` is the
  canonical input.
- **VS Code WASM-loader API instability**: the tree-sitter VS Code
  consumption pattern uses `web-tree-sitter` and a `.wasm` build of
  the grammar; the API surface here is small and stable, but the
  fixture for User Story 3 is intentionally a *smoke* test (extension
  loads without error) rather than a pixel-perfect-colour test, to
  isolate the deliverable from upstream API churn.
- **Tree-sitter parser regeneration drift**: `grammar.js` is the
  source; `parser.c` is regenerated by `tree-sitter generate`. CI
  must verify that regeneration produces no diff against the
  committed `parser.c` (analogous to T1's grammar-from-`KeywordSet.def`
  regenerate-and-diff gate).
- **Audited corpus not yet vendored when T8 merges**: P-VEN lands at
  M7 (per `README.md` §Roadmap); T8 may merge before that. In that
  case the smoke test runs against a hand-written representative
  fixture equivalent to the audited corpus in production coverage,
  with a marker comment indicating that the fixture is to be replaced
  by audited-corpus iteration once P-VEN lands. (Same precedent set
  by T1's SC-001 Assumption.)

## Requirements *(mandatory)*

### Functional Requirements

#### Grammar coverage (the parser side)

- **FR-001**: The tree-sitter grammar (`grammar.js`) MUST accept
  every production from `docs/spec/nsl_lang.ebnf` §§1–11
  (compilation unit, struct, top-level params, declare, module,
  internal-structure elements, func/proc/state, action statements,
  atomic actions, system tasks, expressions). Coverage is verified
  against the audited corpus per FR-014.
- **FR-002**: The grammar MUST consume preprocessor directives
  (`pp.ebnf §2`: `#include`, `#define`, `#undef`, `#ifdef`, `#ifndef`,
  `#if`, `#else`, `#endif`, `#line`) as top-level
  `preprocessor_directive` nodes — `nsl_tooling_design.md §4.3`'s
  illustrative `_top_level_item` choice — and MUST NOT require
  preprocessor expansion to have run before parsing. Live editing
  works on unprocessed source.
- **FR-003**: The grammar MUST accept `%IDENT%` macro-reference
  tokens (`pp.ebnf §4`) wherever an identifier or expression is
  expected, by modelling `%IDENT%` as a recognised alternative in
  the affected productions. The grammar MUST NOT produce `(ERROR)`
  nodes solely because a macro is unexpanded.
- **FR-004**: The grammar MUST recognise the same lexical tokens
  T1 covers (per `nsl_lang.ebnf §13`): bare decimal, hex `0x…`,
  binary `0b…`, octal `0o…`, Verilog-sized literals
  `<width>'b…/'o…/'d…/'h…` with `Z`/`X`/`U` markers and underscore
  digit separators; identifiers per the §13 production; strings with
  backslash escapes per §13.
- **FR-005**: The grammar MUST recognise the same reserved keyword
  set T1 covers, sourced from `include/nsl/Lex/KeywordSet.def`
  (the established single-source-of-truth per Constitution
  Principle VII). When the X-macro file gains a keyword, the
  tree-sitter grammar's keyword set MUST update in the same change.
- **FR-006**: The grammar MUST disambiguate parser notes `N2`
  (reduction vs bitwise `&`/`|`/`^`), `N3` (`.{` two-char lookahead),
  `N5` (`#` line-marker vs sign-extend), and `N6`
  (proc-instance method access) using tree-sitter's native parser
  facilities (precedence, lookahead, distinct rule paths) — not as
  best-effort regex matching. T1 explicitly defers these to T8;
  T8 fulfils them.

#### Highlight queries (the colouring side)

- **FR-007**: A `queries/highlights.scm` file MUST exist and MUST
  assign **at minimum** the following capture set — the
  `nsl_tooling_design.md §4.3` base set plus the eight sub-captures
  needed to satisfy User Story 1 acceptance scenarios. Implementation
  MAY add further sub-captures provided their parents remain in this
  set (per the capture-name-namespace Assumption).

  Base set (per §4.3): `@keyword`, `@keyword.control`,
  `@keyword.control.flow`, `@keyword.modifier`, `@type.builtin`,
  `@keyword.storage`, `@type` (for module/declare/struct names),
  `@function.call` (for control calls in general),
  `@constant.macro` (for `%IDENT%`), plus
  `@number` / `@string` / `@comment` literal/comment captures.

  Sub-captures required by FR-008 / FR-009 / FR-010 / US1:
  `@variable.register` (for `reg` declaration and reference sites),
  `@variable.wire` (for `wire` declaration and reference sites),
  `@variable.memory` (for `mem` declaration and reference sites),
  `@function.proc` (for `proc` definition sites),
  `@function.func` (for `func`/`function` definition sites),
  `@function.call.proc` (for procedure invocation),
  `@function.call.func` (for function call),
  `@label.state` (for `state_name` declaration sites and `goto`
  targets).
- **FR-008**: Highlight queries MUST distinguish `reg` references
  from `wire` references in expression position — the deferral T1
  recorded in its FR-020 — by emitting `@variable.register` for
  the former and `@variable.wire` for the latter (per FR-007's
  required sub-capture set), leveraging the tree-sitter parse
  tree's binding resolution where T1's regex could not.
- **FR-009**: Highlight queries MUST tag control-terminal names
  used in expression position per constraint `S27` (constructive)
  with a capture distinct from regular identifier references.
  T8 uses a dedicated sub-capture for this case (the precise name
  is plan-level — e.g. `@variable.builtin.terminal` — but it
  MUST be distinct from the eight sub-captures listed in FR-007).
- **FR-010**: Highlight queries MUST tag `proc_name` references in
  control-call position with `@function.call.proc` distinct from
  `func` references in function-call position which use
  `@function.call.func`, per `nsl_lang.ebnf §6` and
  `nsl_tooling_design.md §4.3`.
- **FR-011**: Capture names in `queries/highlights.scm` MUST follow
  the tree-sitter convention shown in `nsl_tooling_design.md §4.3`
  (e.g. `@keyword.storage`, `@variable.register` if used,
  `@function.proc`); editor consumers map these conventional names
  to theme colours via their own theme infrastructure.
- **FR-012**: Capture names MUST be aligned with T1's TextMate
  scope vocabulary at the *category* level — when T1 uses
  `keyword.control.flow.nsl` and tree-sitter uses
  `@keyword.control.flow`, they refer to the same conceptual
  category — so that the two-tier highlighter strategy in
  `nsl_tooling_design.md §4` produces consistent colouring when
  both layers are active and the same theme is used.

#### VS Code extension shell

- **FR-013**: An `editors/vscode/` extension shell MUST consume the
  WASM build of the grammar (`tree-sitter-nsl.wasm`) via the
  `web-tree-sitter` library and apply
  `queries/highlights.scm` to each open `.nsl` / `.nslh` / `.inc`
  buffer. The extension MUST coexist with the T1 TextMate
  contribution: T1 provides the base layer; T8 overrides
  identifier captures.

#### Test gate (the README §Roadmap T8 row)

- **FR-014**: A smoke-parse test MUST run `tree-sitter parse` over
  the **smoke-fixture set** and MUST fail the run if any file
  produces an `(ERROR)` or `(MISSING)` node. The smoke-fixture
  set is defined as follows:
  - **At T8 merge** (if P-VEN has not yet landed): the existing
    in-tree `examples/01_*.nsl`–`examples/20_simulation_tb.nsl`
    corpus (20 hand-written files curated for production coverage
    by an earlier milestone). This satisfies FR-014 and SC-002 in
    the M5-current world without authoring a new T8-specific
    fixture.
  - **After P-VEN lands** (M7): the audited corpus
    (`rv32x_dev`, `turboV`, `mmcspi`, `SDRAM_Controler`,
    `mips32_single_cycle`, `ahb_lite_nsl`, `cpu16`, vendored
    under `test/audited/<project>/`) is **added** to the same
    smoke gate. The `examples/*.nsl` files remain in the gate
    so that the gate continues to validate construct-level
    coverage even if the audited corpus underuses some
    construct.
- **FR-015**: A highlight-query golden test MUST exist that
  exercises every capture name listed in
  `queries/highlights.scm` and verifies — via inline
  `tree-sitter test`-style assertions or equivalent — that the
  captures produced match the expected capture for selected
  byte ranges.
- **FR-016**: Both tests MUST run in CI on every PR and MUST fail
  the run if any assertion fails. They MUST run on a clean
  checkout without a built NSL compiler — the tree-sitter grammar
  has no compile-time dependency on `libNSLFrontend.a`, `nslc`,
  `nsl-opt`, or any in-tree binary, satisfying Constitution
  Principle II's no-duplication rule by the same scoped
  exception that applies to T1: the highlighter tier is
  permitted a parallel parser because tree-sitter's input format
  is `grammar.js`, not C++.
- **FR-017**: A regenerate-and-diff CI gate MUST verify that
  running `tree-sitter generate` from the committed `grammar.js`
  produces no diff against the committed `parser.c` (and any
  other generated artefacts that are committed). Mirror the
  grammar-from-`KeywordSet.def` precedent set by T1's
  `gen_textmate_grammar.py` in `scripts/`.

#### Boundaries

- **FR-018**: The tree-sitter grammar MUST NOT replace any
  semantic functionality that the LSP `semanticTokens` request
  (T4) or LSP diagnostics (T3) provide. Where tree-sitter and
  the LSP both produce a capture for the same token, the LSP's
  answer wins (per the precedence in
  `nsl_tooling_design.md §4` "The LSP's `semanticTokens`
  response is a third layer — the most accurate, since it has
  the full symbol table — and overrides the other two where
  available").
- **FR-019**: The deliverable MUST NOT include lint, formatter,
  or LSP functionality — those are T2/T3+/T6 and use the
  compiler front-end library, not tree-sitter.
- **FR-020**: The deliverable MUST NOT include Neovim, Emacs,
  Sublime, or other-editor integrations — those are **T11**.
  T8 ships only the **VS Code** consumer (per
  `README.md` §Roadmap row T8) plus the editor-agnostic
  artefacts (`grammar.js`, `queries/highlights.scm`,
  `parser.c`, WASM build) that **T11** will then wire to
  Neovim/Emacs without further grammar work.
- **FR-021**: The deliverable MUST NOT include a published VS Code
  Marketplace listing, an upstream `nvim-treesitter` PR, or any
  cross-organisation publication step (deferred per
  [`README.md`](../../README.md) §Roadmap T1/T12 deferral note).
  Folder-drop install under `~/.vscode/extensions/` is the
  intended consumption model at T8.

### Key Entities

- **Tree-sitter grammar source** (`grammar.js`) — A JavaScript
  description of NSL's grammar productions, parsed by the
  tree-sitter CLI to produce a generated C parser. Lives in-tree.
- **Generated parser** (`parser.c`, possibly `grammar.json`,
  `node-types.json`) — Output of `tree-sitter generate`.
  Committed for ease of consumption, gated by a
  regenerate-and-diff CI check (FR-017).
- **WASM artefact** (`tree-sitter-nsl.wasm`) — Output of
  `tree-sitter build-wasm`. Built reproducibly in CI;
  consumption-time availability described in Assumptions.
- **Highlight queries** (`queries/highlights.scm`) — A
  tree-sitter query file in S-expression syntax that maps
  parse-tree shapes to capture names. Consumed by every
  tree-sitter-aware editor.
- **VS Code extension shell** — A minimal extension under
  `editors/vscode/` (extending the directory T1 already
  established) that activates on the `.nsl` / `.nslh` / `.inc`
  language ID, loads the WASM grammar via `web-tree-sitter`,
  and registers a tree-sitter highlight provider applying
  `queries/highlights.scm`.
- **Smoke-parse test** — A CI step that runs the tree-sitter
  CLI's `parse` command over every NSL file in the audited
  corpus (or a representative-equivalent fixture, per the
  Assumptions) and fails on any `(ERROR)` or `(MISSING)` node.
- **Highlight-query golden test** — A CI step that runs
  `tree-sitter test` (or equivalent) over a fixture file with
  inline capture assertions and fails on any mismatch.
- **Capture name** — A tree-sitter convention name (e.g.
  `@keyword.storage`, `@function.call`) assigned by
  `queries/highlights.scm` and mapped to theme colours by
  consumer editors. The set is closed and aligned with
  T1's TextMate categories at the conceptual level
  (FR-012).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A reader opening any file in the smoke-fixture set
  (per FR-014 — at T8 merge: the 20 in-tree `examples/*.nsl`
  files; after P-VEN: also the seven audited-corpus projects) in
  VS Code with the T8 extension shell installed sees register,
  wire, procedure-name, state-name, and function-name references
  each in **distinct** colour captures (5 categories visually
  distinct), with **zero** mis-colouring inside comments, strings,
  or `%IDENT%` macro splice sites.
- **SC-002**: `tree-sitter parse` produces **0** `(ERROR)` and
  **0** `(MISSING)` nodes across **100%** of files in the
  smoke-fixture set defined by FR-014.
- **SC-003**: The `queries/highlights.scm` golden test asserts
  ≥ 1 occurrence per capture name in the **required minimum set**
  defined in FR-007 — i.e. the `nsl_tooling_design.md §4.3` base
  captures plus the eight sub-captures (`@variable.register`,
  `@variable.wire`, `@variable.memory`, `@function.proc`,
  `@function.func`, `@function.call.proc`, `@function.call.func`,
  `@label.state`) — for a total of **≥ 17 distinct capture
  assertions**, and **100%** of these assertions pass on a clean
  checkout.
- **SC-004**: When `nsl_lang.ebnf` gains a new production or
  `pp.ebnf` gains a new directive, the same PR that updates the
  spec adds a tree-sitter rule, regenerates `parser.c`, adds a
  golden-test assertion, and updates the smoke-fixture if
  applicable; the T8 CI cell passes before the PR merges.
- **SC-005**: The `tree-sitter generate` regenerate-and-diff
  CI gate (FR-017) catches any uncommitted regeneration of
  `parser.c` and fails the run with a localised diagnostic
  pointing at the affected rule in `grammar.js`.
- **SC-006**: Running the smoke-parse test against every
  audited-corpus file plus the golden highlight test takes
  **under 60 seconds** on a standard CI runner.
- **SC-007**: A new contributor can install the T8 extension
  shell locally in VS Code (folder-drop into
  `~/.vscode/extensions/`) in **under 5 minutes**, with no
  compiler binary, no LSP process, and no Marketplace listing
  required, by either: (a) downloading the
  `tree-sitter-nsl.wasm` workflow artefact from the latest green
  CI run (or from a tagged release if one exists), or (b) running
  `tree-sitter build-wasm` locally with the pinned CLI version.
  See WASM-artefact-lifecycle Assumption.
- **SC-008**: The `tree-sitter-nsl.wasm` artefact built in CI
  is **byte-identical** between two consecutive CI runs on the
  same commit (Constitution Principle V — determinism).

## Assumptions

- **Build system for the grammar**: T8 uses the standard
  tree-sitter CLI toolchain (`tree-sitter generate`,
  `tree-sitter parse`, `tree-sitter test`, `tree-sitter build-wasm`).
  Dependency on a Node.js + tree-sitter-CLI toolchain is contained
  to the T8 CI cell and to contributors running grammar tests
  locally; the rest of the project (M-track, other T-track
  cells) remains free of this dependency.
  The toolchain version is **pinned to a specific minor version**
  (e.g. `tree-sitter-cli@0.22.x`) recorded in a project-level
  config file (the canonical location is plan-level — for example
  `package.json`'s `devDependencies`, or a dedicated
  `.tree-sitter-version` file). The CI cell installs exactly this
  pinned version; bumping is a deliberate PR (analogous to the
  M-track's deliberate LLVM/MLIR bumps via the dev-container).
  This pinning is what makes FR-017 (regenerate-and-diff) and
  SC-008 (byte-identical WASM) evaluable.
- **WASM artefact lifecycle**: The build pipeline produces
  `tree-sitter-nsl.wasm` in CI but **does not commit** the
  artefact to the repo (preserves the no-binary norm
  established by T1 SC-005). CI uploads it as a **GitHub Actions
  workflow artefact** so that any green CI run produces a
  downloadable WASM bundle; a tagged release additionally
  attaches the artefact to the release page (matching the
  deferred-publication policy — binary available alongside
  source releases, but no Marketplace push). Local contributors
  who want the WASM without going through CI run
  `tree-sitter build-wasm` once with the pinned CLI version.
  This commits-no-binary stance keeps the regenerate-and-diff
  gate (FR-017) and SC-008 byte-identity check pure CI-side
  guarantees rather than working-tree assertions.
- **Audited-corpus availability**: Per `README.md` §Roadmap, the
  audited corpus lands via `P-VEN` on or before M7. T8 may merge
  before M7. The smoke-fixture set is therefore
  *time-conditional* (per FR-014 / Clarifications session
  2026-05-05 Q4 → Option C): at T8 merge it is the in-tree
  `examples/01_*.nsl`–`examples/20_simulation_tb.nsl` corpus (20
  hand-written files curated for production coverage); once
  P-VEN lands, the audited corpus is **added** to the gate
  alongside `examples/*.nsl`. Both SC-001 and SC-002 evaluate
  against whichever set is current at the time the gate runs.
- **Capture-name namespace**: Tree-sitter capture names follow
  the convention shown in `nsl_tooling_design.md §4.3` (e.g.
  `@keyword.storage`, `@function.call`, `@constant.macro`). T8's
  delivered set extends §4.3 with eight specific sub-captures
  (per FR-007 / Clarifications session 2026-05-05 Q3 → Option B):
  `@variable.register`, `@variable.wire`, `@variable.memory`,
  `@function.proc`, `@function.func`, `@function.call.proc`,
  `@function.call.func`, `@label.state` — these are required to
  realise FR-008 / US1's headline value-prop. Implementations
  may add further sub-captures provided their parents remain in
  the §4.3 set so that downstream consumer themes match correctly.
  Maximally-granular sub-captures for every Symbol kind in
  `nsl_compiler_design.md §6` (e.g. fine-grained control-terminal
  classes) are deferred to T4 (LSP `semanticTokens`) and T11
  per the three-tier precedence in `nsl_tooling_design.md §4`.
- **VS Code extension packaging at T8**: T8 ships only the
  extension folder under `editors/vscode/` plus a generated WASM
  artefact (lifecycle per the WASM Assumption above). No `.vsix`
  is built and no Marketplace listing is published — consumption
  is via folder-drop. Marketplace publication was previously
  planned for T12 but is deferred per
  [`README.md`](../../README.md) §Roadmap deferral note.
- **Other-editor integrations**: Neovim (`nvim-treesitter`
  upstream), Emacs (`tree-sitter.el`), Sublime — these all
  consume the same `grammar.js` + `queries/highlights.scm`
  artefacts that T8 produces, but the editor-specific wiring
  lands at **T11** (per `README.md` §Roadmap row T11 dependency
  on T8). T8 produces the artefacts; T11 wires them.
- **LSP `semanticTokens` integration**: The LSP layer (T4) will
  *override* the tree-sitter colouring for any token whose
  Sema-aware classification differs from tree-sitter's
  parse-tree-only classification (per `nsl_tooling_design.md §4`
  three-tier precedence). T8's queries do not need to anticipate
  the LSP override surface; they aim for parse-tree correctness.
- **Constitution Principle II scope of exception**: The
  highlighter tier (T1 + T8) is the *only* tier in
  `nsl_tooling_design.md` that re-implements parsing outside
  `libNSLFrontend.a`. Lint (T6/T7), formatter (T2/T5), and LSP
  (T3+) remain pinned to the C++ front-end library. Adding a
  new tooling tier outside the highlighter MUST NOT introduce a
  third parser; that would violate Principle II without a scoped
  exception.
- **Linguist disambiguator for `.nsl`**: GitHub will continue to
  mis-classify `.nsl` files until the `github-linguist/linguist`
  PR (deferred per
  [`README.md`](../../README.md) §Roadmap T1 note) lands. Once
  it does, GitHub's web view will use T1's TextMate grammar (not
  tree-sitter — GitHub's UI does not run tree-sitter for
  highlighter purposes, only for code-navigation features in
  selected languages). T8 produces no GitHub-web colouring
  improvements; that path is owned by T1 + the deferred PR.
- **CI matrix cell for T8**: T8's CI cell is a Node.js cell with
  the tree-sitter CLI installed. Choice between adding a new job
  or extending an existing tooling-cell is a plan-level decision.
  The invariant from FR-016 is that the cell runs on every PR
  and fails the run on any smoke-parse error or
  golden-highlight assertion failure. The cell does not
  pre-build the M-track compiler.
