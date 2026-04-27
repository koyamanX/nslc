<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Feature Specification: M2 — Parser + AST (with `-emit=ast`)

**Feature Branch**: `005-m2-parser`
**Created**: 2026-04-27
**Status**: Draft
**Input**: User description: "M2"

> **Scope interpretation.** "M2" maps to the **M2** row of
> [`README.md`](../../README.md) §Roadmap, which delivers the next two
> compiler-track libraries: `nsl-parse` (4) + `nsl-ast` (5). The same
> row defines the milestone's test gate ("AST-snapshot tests covering
> every grammar production from `docs/spec/nsl_lang.ebnf §§1–11`;
> parser-note `N1`–`N14` disambiguation tests pass") and its
> constitutional anchors (VI parser tests; VII spec coupling). The
> NSL-feature → milestone roll-up in [`CLAUDE.md`](../../CLAUDE.md) §1
> confirms which language-spec rows land here. Per
> [`README.md`](../../README.md) §Usage "Status by milestone" — "real
> `-emit=` flags arrive incrementally from M1 onward; `-emit=ast`
> lands at M2" — this spec includes the `nslc -emit=ast` driver
> increment alongside the libraries themselves.

## Clarifications

### Session 2026-04-27

- Q: Parser error-recovery tolerance at M2 — does the parser ship
  (A) full multi-error recovery with statement-level recovery
  sets at every grammar rule, (B) single-error bail with
  recovery skeleton stubbed and multi-error fixtures deferred,
  or (C) top-level / module-item recovery only with
  statement-level deferred? → A: **Full multi-error recovery at
  M2 (Option A).** This is the clangd / rust-analyzer / gopls
  industry pattern and is what the T3+ LSP track will need for
  `publishDiagnostics` (all errors per save, not first only) and
  for partial-AST navigation under in-flight edits (hover /
  definition / completion past an earlier syntax error).
  Recovery touches every `parseFoo()` site, so retrofitting it
  after M2 would require re-walking every grammar rule —
  cheaper to land it once at M2. The CST layer (per
  `nsl_tooling_design.md §2`) absorbs the *fully-in-flight*
  edit case at T-track, but the AST parser still feeds Sema
  diagnostics, hover-types, and `publishDiagnostics`, so AST
  recovery is what the LSP user actually feels.
- Q: `-emit=ast` output format depth at M2 — does M2 ship
  (A) text-only S-expression-style dump à la clang's
  `-ast-dump`, (B) text default + smoke-JSON
  (`--emit-format=json` wired with a parse-only smoke test;
  schema lock deferred), or (C) text + JSON with locked schema
  and per-node-kind round-trip goldens at M2? → A: **Text-only
  at M2 (Option A).** Parallels M1's deliberate
  schema-deferral pattern (M1 spec session 2026-04-27 Q2): the
  AST JSON schema is best designed against the actual LSP
  consumer (T3), not in a vacuum. The text dump is debug-grade
  and is allowed to drift across M-track milestones — M3 adds
  Sema-resolved types, M5 adds expansion residue, etc. — with
  the `-emit=ast` golden re-cut in the same patch as each
  format-bumping change. Wiring smoke-JSON now would commit a
  partial schema before its consumer can inform it; better to
  add JSON in T3's planning artifact when the LSP team has
  decided whether AST-as-JSON belongs in the LSP protocol or
  only as a debug surface.

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Parse any NSL compilation unit into a typed, source-locating AST (Priority: P1)

A contributor pipes a preprocessed NSL token stream (the M1 output) through the
parser and receives back an in-memory AST whose root is a
`CompilationUnit` and whose nodes cover every production in
[`docs/spec/nsl_lang.ebnf`](../../docs/spec/nsl_lang.ebnf) §§1–11
(compilation unit, struct types, top-level params, `declare`,
`module`, internal-structure elements, `func`/`proc`/`state`
definitions, `par`/`alt`/`any`/`seq`/`if`/`for`/`while`/`generate`
action statements, atomic actions including transfers and control
calls, system tasks `_display` / `_finish` / `_init` / `_delay` / …,
and the full expression grammar including sign-extend `#`,
zero-extend `'`, slice, concat, conditional). Every AST node carries
a `SourceRange` whose endpoints round-trip to the original (post-
`#line`) virtual file coordinates supplied by `nsl-basic`'s
`SourceManager`.

**Why this priority**: M3 sema, M4 dialect, M5 lowering, M6 CIRCT
lowering, M7 end-to-end, M8 formal, and M9 release all consume the
AST. Without a parser there is no AST; without an AST nothing else
lands. M2 is the first milestone that produces a *structured*
artifact — M1's tokens are a flat stream — and the parser is the
load-bearing half of M2's two-deliverable pair (the other being the
AST type definitions in `nsl-ast`).

**Independent Test**: Build the project, run the per-production
AST-snapshot fixtures (one input per grammar production listed in
[`docs/spec/nsl_lang.ebnf`](../../docs/spec/nsl_lang.ebnf) §§1–11).
Assert each fixture parses without error and produces an AST
matching the committed snapshot. Run `nslc -emit=ast fixture.nsl`
on a representative input and observe the same AST printed
deterministically across two runs. Does not depend on US2 (which
covers parser-note disambiguation) or US3 (diagnostics-on-error) —
this story is the well-formed-input path only.

**Acceptance Scenarios**:

1. **Given** a fixture exercising every `top_level_item`
   alternative from `nsl_lang.ebnf §1` (struct declaration,
   `declare` block, `module` block, top-level parameter,
   `line_marker` survival), **When** the parser runs, **Then** the
   resulting `CompilationUnit::items` vector contains one node per
   surviving top-level item in declaration order, and a
   `line_marker` is consumed (no AST node) but updates the
   `SourceManager` cursor for subsequent nodes.
2. **Given** a fixture defining a `module` with each
   `internal_declaration` form from `nsl_lang.ebnf §6` (`wire`,
   `reg`, `func_self`, submodule, `proc_name`, `state_name`,
   `first_state`, `mem`, struct instance, `integer`, `variable`),
   **When** the parser runs, **Then** the corresponding
   `*Decl` nodes appear under `ModuleBlock` with the
   `SourceRange`, identifier name, and per-form fields
   (`width`, `init`, `depth`, `args`, …) populated as documented
   in [`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md)
   §5 lines 299–615.
3. **Given** a fixture defining a `module` containing `func` /
   `proc` / `state` definitions (`nsl_lang.ebnf §7`), **When** the
   parser runs, **Then** each definition appears under
   `ModuleBlock` as a `FuncDefn` / `ProcDefn` / `StateDefn` whose
   `body` is the parsed action statement.
4. **Given** a fixture exercising every action-statement form
   (`nsl_lang.ebnf §8`: `par {…}`, `alt {…}`, `any {…}`, `seq {…}`,
   `if`, `for`, `while`, `generate`, plus the atomic actions of §9:
   transfer, increment/decrement, control-call, `finish`, system
   task, `return`, labeled, `goto`, `_init` block, `_delay`),
   **When** the parser runs, **Then** the resulting statement tree
   contains one node per form with the correct child-statement
   nesting and the correct `SourceRange` per node.
5. **Given** a fixture exercising every expression form
   (`nsl_lang.ebnf §11`: literal, identifier, system variable,
   unary, binary, conditional, concat, repeat, sign-extend `#`,
   zero-extend `'`, slice, field access, call, struct cast,
   inc/dec), **When** the parser runs, **Then** the resulting
   `Expr` tree matches the operator-precedence ordering documented
   for the production and each `SourceRange` round-trips to the
   input bytes.
6. **Given** any fixture above, **When** the contributor runs
   `nslc -emit=ast fixture.nsl` twice on the same input under the
   same flags, **Then** the stdout is byte-identical between the
   two invocations (Principle V determinism).

---

### User Story 2 — Parser-note disambiguation across the grammar's known ambiguities (Priority: P1)

A contributor parses NSL source that exercises each *parsing-level*
disambiguation rule documented in
[`docs/spec/nsl_lang.ebnf`](../../docs/spec/nsl_lang.ebnf) §§N1–N14
(specifically, the parser notes whose disambiguation is observable
at the AST level: `N1` `if` statement-vs-expression, `N2` `&` `|`
`^` reduction-vs-bitwise, `N3` `.{` two-character lookahead, `N5`
`#` sign-extend operator post-preprocess, `N6` proc-instance method
access, `N7` dotted `func` def for submodule out, `N10` `label`
reserved-but-warned, `N11` `_`-prefix system-task vs system-variable
position, `N14` `line_marker` consumed-not-emitted) and observes the
parser produces the expected AST node kinds. (Notes `N4`, `N8`,
`N9`, `N12`, `N13` were moved to
[`docs/spec/nsl_pp.ebnf`](../../docs/spec/nsl_pp.ebnf) and are
covered by M1 — see Assumptions.)

**Why this priority**: The M2 row's named test gate is
*"parser-note `N1`–`N14` disambiguation tests pass."* These
disambiguations are why a hand-written recursive-descent parser is
necessary at all (cf. an LL(1) generator would not handle N1, N3,
or N5 without lookahead). Getting them wrong means every later
layer (M3 sema, lowering, …) consumes a wrong tree. Shares P1 with
US1 because both are explicit M2 acceptance gates.

**Independent Test**: For each parser-note that has a *parsing*
observable, ship a passing fixture pair: (i) the construct in the
ambiguous position whose intended interpretation A is selected
(asserted via AST-snapshot equality), and (ii) the same lexical
form in the alternative position whose interpretation B is
selected. Where applicable, ship a failing fixture asserting the
diagnostic for the disambiguation-violation case (e.g., `if` as
expression at statement position with no surrounding context that
permits expression parse). Does not depend on US1's full grammar
coverage — only on the parser's ability to construct *some* AST
for the disambiguation fixtures.

**Acceptance Scenarios**:

1. **Given** an input `if (c) a = b; else a = 0;` at statement
   position, **When** the parser runs, **Then** the AST contains
   an `IfStmt` (statement form) — not a `ConditionalExpr`
   (expression form) wrapped in a `TransferStmt`. (`N1`)
2. **Given** an input `wire q = if (c) a else b;` at expression
   position (RHS of transfer), **When** the parser runs, **Then**
   the AST contains a `ConditionalExpr` — not an `IfStmt`. (`N1`)
3. **Given** an input using `&x` in an expression with no left
   operand, **When** the parser runs, **Then** the AST contains a
   `UnaryExpr{op=ReduceAnd}` — not a `BinaryExpr`. Conversely
   `a & b` produces `BinaryExpr{op=BitAnd}`. (`N2`)
4. **Given** an input using `.{a, b, c} = x;` (concat-LHS form),
   **When** the parser runs, **Then** the AST's `TransferStmt::lhs`
   is a `ConcatExpr` over `IdentifierExpr` parts — not a
   `FieldAccessExpr` chain. (`N3`)
5. **Given** an input using `8 # sig` (sign-extend in expression
   position), **When** the parser runs, **Then** the AST contains
   a `SignExtendExpr{width=8, sub=sig}` — not a stray `line_marker`
   token. (`N5`)
6. **Given** an input `inst.finish();` where `inst` is a
   submodule/proc instance, **When** the parser runs, **Then** the
   AST contains a `ControlCallStmt` whose target is the
   scoped-identifier `inst.finish`. (`N6`)
7. **Given** an input `func ic.ready { … }` at module-item
   position, **When** the parser runs, **Then** the AST contains a
   `FuncDefn` whose `name` is the dotted scoped name `ic.ready`
   (per `N7`).
8. **Given** an input using the `label` keyword as an identifier
   in user code, **When** the parser runs, **Then** a warning
   diagnostic is emitted naming `N10` and the identifier is still
   accepted. (`N10`)
9. **Given** an input invoking `_display(arg)` at statement
   position vs. `_random` at expression position (`N11` (a)/(b)),
   **When** the parser runs, **Then** the former parses as a
   `SystemTaskStmt` and the latter as a `SystemVarExpr`.
10. **Given** an input containing a `#line 100 "F"` directive
    surviving the M1 preprocessor seam, **When** the parser
    runs, **Then** no AST node corresponds to the directive
    (the `line_marker` is consumed) AND every subsequent AST
    node's `SourceRange` reports `F:100:…` for the next line.
    (`N14`)

---

### User Story 3 — Source-locating diagnostics with parse-error recovery (Priority: P1)

A contributor running the parser on input that contains a syntax
error sees a single-line diagnostic of the form
`path:line:col: error: <message>` (per FR-025 in M1's spec, re-used
by `nsl-parse`), where the location is the offending token's start.
The parser THEN recovers (via "skip-to-recovery-token" sets
documented per top-level/statement/declaration position) and
continues parsing so that *multiple* independent syntax errors in
one source file are all reported in a single run, rather than the
parser bailing on the first error.

**Why this priority**: Constitution Principle IV ("Diagnostics
First") and the M1 invariant that every diagnostic crosses through
the `DiagnosticEngine` apply unchanged at M2 — no new infrastructure,
just new raise sites in the parser. Recovery is what makes the
diagnostic engine useful in practice: a parser that bails on first
error is worse for the user than one that reports five errors at
once. Shares P1 with US1/US2 because the M2 acceptance gate
("AST-snapshot tests covering every grammar production") implicitly
requires the parser not to crash on malformed inputs encountered
during fuzz-testing.

**Independent Test**: (a) Provoke a known syntax error (e.g.,
missing semicolon at a known position) and assert
`F:L:C: error: <message>`. (b) Provoke two independent syntax
errors in the same fixture (e.g., missing `}` on a `module` and
malformed expression in a different `module`) and assert *both*
are reported in a single parser run. (c) Provoke a recoverable
error inside a `module` body and assert that subsequent
`module_item`s after the recovery point still appear in the AST
under that `ModuleBlock` (i.e., the parser did not lose the rest
of the module). Does not depend on US1's full grammar coverage or
US2's parser-note correctness — only on the recovery sets being
populated and the diagnostic-emission sites being wired.

**Acceptance Scenarios**:

1. **Given** an input missing a `;` at the end of a
   `register_declaration`, **When** the parser runs, **Then** the
   diagnostic cites the byte offset of the next token's start
   (where the parser expected `;`) and contains a non-empty
   message naming the missing token (e.g., "expected ';'").
2. **Given** an input with two independent syntax errors in
   separate top-level items, **When** the parser runs, **Then**
   both diagnostics are emitted in source order, the parser exits
   non-zero, AND no AST is emitted on stdout (FR-029-style "no
   partial output" parallel to M1's `-emit=tokens`).
3. **Given** an input containing a malformed expression inside a
   `module`'s `seq` block, **When** the parser runs, **Then** the
   diagnostic is emitted, the parser recovers at the `;` or the
   matching `}`, and the rest of the `module`'s `module_item`s
   are still parsed and appear in the AST. Recovery is the
   default behavior (per Clarifications Q1) — there is no
   "fail-fast" toggle.
4. **Given** any successful pass through parse, **When** the
   diagnostic engine is queried, **Then** its diagnostic buffer
   is empty and the run reports success.
5. **Given** any error raised by the parser, **When** the
   diagnostic is rendered, **Then** the message text is non-empty
   and references the offending construct or expected token by
   name (e.g., "expected ';' after register declaration", not
   "parse error").

---

### Edge Cases

- An empty compilation unit (zero top-level items). The parser
  MUST produce a `CompilationUnit` with `items` empty and exit
  successfully.
- A `compilation_unit` consisting only of `line_marker`
  directives (M1 emits these around `#include` boundaries even
  for empty includes). The parser MUST consume them all without
  producing AST nodes; the resulting `CompilationUnit::items` is
  empty.
- A `module` whose body contains only `line_marker`s (e.g., a
  module whose sole `#include` was empty). Same as above: the
  `ModuleBlock` is constructed but its `internals` / `actions` /
  `funcs` / `procs` vectors are empty.
- A deeply nested expression (e.g., 256 levels of parenthesized
  binary operators). The parser MUST NOT stack-overflow; it MAY
  cap recursion depth and emit a "expression nesting too deep"
  diagnostic at a documented bound, but the bound MUST NOT fire
  on any of the seven audited NSL projects' actual sources. A
  bound of 1024 is acceptable as an initial value.
- The `function` keyword used in place of `func` (per `S26`
  parser-equivalence rule). The parser at M2 MUST accept both
  spellings — the canonicalization warning ("prefer `func`") is
  Sema's responsibility at M3, not the parser's. (See Assumptions.)
- A struct-instance declaration whose initializer is a
  parenthesized list with too many or too few values for the
  struct's member count. At M2 the parser accepts the list
  syntactically; the per-member arity check is Sema's at M3.
- A `for`-loop syntactic shape that uses commas instead of
  semicolons (or vice versa) in a way the EBNF disallows. The
  parser MUST emit a syntax-error diagnostic and recover at the
  matching `}`.
- A `#line` `LINENUM` value of `0` (P13 explicit-permission case
  preserved by M1). The parser's `SourceManager` cursor update
  MUST set the next AST-node's reported line to `1`, not `0`.
- A `#line` directive in *every* item-list position (`top_level_item`,
  `declare_item`, `module_item`, parallel-block item, seq-block
  item, per `N14`). The parser MUST consume each one identically:
  pop, update cursor, continue.
- `nslc -emit=ast` invoked on a file that fails preprocess or lex
  (M1 layers). The driver MUST exit non-zero and MUST NOT emit
  any AST to stdout — the failure is reported via the M1
  diagnostic surface; M2 adds nothing.
- `nslc -emit=ast` invoked on a file that fails parse. Exit
  non-zero; emit no AST on stdout (FR-029-style "no partial
  output"); diagnostics on stderr.

## Requirements *(mandatory)*

### Functional Requirements

**Library deliverables (M2 layers per [`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md) §3, lines 138–148):**

- **FR-001**: The build MUST produce the static library `nsl-ast` at the
  layer-4 position, exposing public headers `nsl/AST/*.h` (one per
  concrete AST node kind, plus `nsl/AST/ASTNode.h` for the base and
  `nsl/AST/ASTVisitor.h` for the visitor). The "per-node-kind
  headers under one directory" exception is explicitly allowed by
  Constitution Principle II.
- **FR-002**: The build MUST produce the static library `nsl-parse` at
  the layer-5 position, exposing the public header
  `nsl/Parse/Parser.h`, depending on `nsl-lex` and `nsl-ast` (per
  the §3 dependency table).
- **FR-003**: Both libraries MUST be declared via the `add_nsl_library`
  macro from M0; their inter-layer dependencies MUST be expressed
  exclusively via that macro's `DEPENDS` argument. `nsl-parse` MUST
  NOT depend on `nsl-sema`, `nsl-dialect`, `nsl-lower`, or
  `nsl-driver` (Principle II layered structure: parser does not
  consume sema or anything below it).
- **FR-004**: The `nsl-ast` library MUST define one C++ class per AST
  node kind enumerated in
  [`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md)
  §5 lines 299–682, with the fields documented there. The base
  `ASTNode` MUST carry a `SourceRange` and a `kind()` enum, and the
  abstract `Decl` / `Stmt` / `Expr` mid-level bases MUST exist.
  `Expr` MUST carry a `TypeRef inferredType()` slot writable by
  Sema at M3.
- **FR-005**: AST nodes MUST be allocated as `std::unique_ptr<T>` with
  a polymorphic visitor (`ASTVisitor`) defined in `nsl-ast`. The
  visitor's per-node-kind methods MUST cover every concrete node
  enumerated in §5 — a missing override is a compile-time error
  (e.g., via pure-virtual or `[[nodiscard]] = delete`-style
  enforcement).

**Parser functional requirements (per [`docs/spec/nsl_lang.ebnf`](../../docs/spec/nsl_lang.ebnf) §§1–11 and parser notes N1–N14):**

- **FR-006**: The parser MUST accept every well-formed compilation
  unit reachable by `nsl_lang.ebnf` §1's `compilation_unit`
  production and produce a `CompilationUnit` AST whose `items`
  vector contains one node per surviving `top_level_item` in
  declaration order. `line_marker` items MUST be consumed (no
  AST node) but MUST update the `SourceManager` cursor.
- **FR-007**: The parser MUST accept every `struct_declaration`
  form per §3 and produce a `StructDecl` whose `members` are
  `StructMember` records carrying optional width expressions.
- **FR-008**: The parser MUST accept every `top_level_parameter`
  form per §3.1 (`param_int` with constant-expression initializer;
  `param_str` with string-literal initializer) and produce a
  `TopLevelParamDecl` carrying the kind and parsed initializer
  expression.
- **FR-009**: The parser MUST accept every `declare_block` form
  per §4 — including the optional name, optional `interface` /
  `simulation` modifier, and the full `declare_item` set
  (parameter declaration, data-terminal declaration with `input`
  / `output` / `inout` direction, control-terminal declaration
  with `func_in` / `func_out` direction, optional dummy-arg list,
  optional return-value-terminal `: identifier`, and `line_marker`
  survival). The result is a `DeclareBlock` whose
  `headerParams` and `ports` vectors are populated.
- **FR-010**: The parser MUST accept every `module_block` form
  per §5 — and dispatch each `module_item` into the matching
  `ModuleBlock` sub-vector (`internals`, `actions`, `funcs`,
  `procs`) per
  [`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md)
  §5 lines 339–344.
- **FR-011**: The parser MUST accept every `internal_declaration`
  form per §6: `wire`, `reg` (with optional width and optional
  initializer per `register_declarator`), `func_self` (with
  optional dummy-arg list and optional return-value terminal),
  submodule (with optional array size and optional parenthesized
  parameter-assignment list), `proc_name` (both parenthesized
  and bare forms — see the §6 grammar comment), `state_name`,
  `first_state`, `mem`, struct-instance declaration, `integer`,
  and `variable`. Each form lowers to its corresponding
  `*Decl` AST node (per §5 lines 359–411).
- **FR-012**: The parser MUST accept every `function_definition`
  / `procedure_definition` / `state_definition` form per §7 and
  produce a `FuncDefn` / `ProcDefn` / `StateDefn` whose `body`
  is the parsed action statement. The dotted-`func` form
  (`N7`: `func ic.ready { … }`) MUST be parsed correctly with
  `name` set to the scoped identifier.
- **FR-013**: The parser MUST accept every action-statement form
  per §§8–9: `par {…}`, `alt {…}`, `any {…}`, `seq {…}`,
  `if`-statement (per `N1`), `for`, `while`, `generate`,
  transfer (`=` and `:=`), increment/decrement, control-call
  (per `N6`), `finish`, system task (per `N11` (a)),
  `return`, empty (`;`), labeled, `goto`, `_init` block,
  `_delay`. Each form lowers to its matching `*Stmt` AST node
  (per §5 lines 437–527).
- **FR-014**: The parser MUST accept every expression form per
  §11 with the operator precedence documented in the EBNF: literal,
  identifier (per `N11` (b) for `_random` / `_time`), unary
  (including reduction `&` `|` `^` per `N2`, sign-extend `#` per
  `N5`, zero-extend `'`), binary, conditional (per `N1`), concat
  (including the `.{…}` LHS form per `N3`), repeat, slice, field
  access, call, struct cast, increment/decrement.
  `constant_expression` and `width_expression` (§12) reuse the
  same `Expr` tree.
- **FR-015**: The parser MUST consume every `line_marker` per `N14`
  in every item-list position (top_level_item, declare_item,
  module_item, parallel-block item, seq-block item) by popping
  the marker, calling the `SourceManager` cursor-update
  primitive, and continuing parse. No AST node is produced for
  the marker.
- **FR-016**: The parser MUST accept the `function` keyword as an
  alias for `func` at every grammar position where `func` is
  allowed (per `S26` parser-equivalence). The Sema-time
  canonicalization warning ("prefer `func`") is M3's
  responsibility, not M2's.
- **FR-017**: The parser MUST emit a warning diagnostic when the
  reserved keyword `label` appears as a user identifier (per
  `N10`), and accept the identifier. The lookup-and-rewrite is
  Sema's; the parser only flags the lexical occurrence.
- **FR-018**: Every AST node MUST carry a `SourceRange` whose
  start and end byte-offsets correspond to the full extent of the
  parsed construct (first-token start to last-token end), in the
  *post-`#line`* virtual coordinates supplied by the
  `SourceManager`.

**Diagnostic and recovery requirements (per Constitution Principle IV and M1's diagnostic plumbing):**

- **FR-019**: The parser MUST emit every diagnostic exclusively via
  the `DiagnosticEngine` introduced at M1 (FR-024 of the M1 spec).
  Direct writes to `stderr` from inside `nsl-parse` are forbidden.
  Every diagnostic MUST render in the canonical form
  `<path>:<line>:<col>: <severity>: <message>` (FR-025 of M1).
- **FR-020**: The parser MUST recover from syntax errors using a
  documented recovery-set strategy: at each grammar position, the
  parser knows a small set of "safe" tokens (typically `;`, `}`,
  the next item-list start keyword) at which to resume. After
  emitting the diagnostic and skipping forward to the recovery
  token, the parser continues — so multiple independent syntax
  errors in a single source file are all reported in a single
  run.
- **FR-021**: The recovery sets MUST be documented per grammar
  rule in `nsl-parse`'s implementation (a comment block at each
  recovery site naming the recovery tokens and the expected
  resume position). The documentation lives next to the code,
  not in a separate file. Recovery MUST cover every
  `parseFoo()` site at every level (top-level / declare-item /
  module-item / parallel-block-item / seq-block-item / statement
  / expression) — single-error bail is not acceptable, and
  partial recovery (e.g., top-level only) is not acceptable
  (per Clarifications session 2026-04-27 Q1 → Option A). The
  multi-error fixture corpus under `test/parse/recovery/` MUST
  cover at minimum: (a) two independent errors in separate
  top-level items, (b) two independent errors in separate
  `module_item`s within one `module`, (c) an error inside a
  `seq` block followed by a well-formed `module_item` after the
  block. Each fixture asserts BOTH diagnostics are emitted in
  source order in a single parser run.

**Driver / CLI surface (per [`README.md`](../../README.md) §Usage and the M1 spec note that "real `-emit=` flags arrive incrementally from M1 onward; `-emit=ast` lands at M2"):**

- **FR-022**: The `nslc` driver MUST accept `-emit=ast` and, on
  successful preprocess + lex + parse, print the parsed AST to
  stdout as a text-only S-expression-style dump in the spirit
  of clang's `-ast-dump` (per Clarifications session
  2026-04-27 Q2 → Option A). The per-node line carries the
  node kind, the `SourceRange` (post-`#line` virtual
  coordinates), and the kind-specific fields enumerated in
  [`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md)
  §5 (e.g., `IdentifierExpr` carries the resolved name text;
  `LiteralExpr` carries the literal kind and value). Indentation
  reflects child-of relationships. The format is frozen
  byte-exactly by an authoritative golden-test fixture under
  `test/parse/emit-ast/` for M2; later milestones MAY re-cut the
  golden in the same patch as a format-bumping change (M3
  resolved types; M5 expansion residue; etc.). On failure
  (preprocess, lex, or parse), exit non-zero with no AST
  output on stdout. JSON output is NOT in M2 scope — schema
  design defers to the T-track LSP consumer.
- **FR-023**: The `nslc` driver MUST inherit the M1 flag set
  (`-I <dir>`, `-D NAME=value`, `NSL_INCLUDE` environment
  variable, `--diagnostic-format=json`) without modification.
  M2 adds only the `-emit=ast` flag — no `--emit-format=*` flag
  is added at M2 (per Clarifications Q2).
- **FR-024**: The `nslc -emit=ast` code path MUST live behind a
  thin wrapper inside `nsl-driver` (per the M0/M1 invariant that
  the driver remains ≤60 lines plus per-`-emit=*` glue). The
  wrapper invokes `Compilation::preprocess()` →
  `Compilation::parse()` and prints `CompilationUnit*` via the
  AST-printer entry point in `nsl-ast`'s public header.

**Test gates (per Constitution Principles VI and VIII):**

- **FR-025**: The repository MUST carry one passing AST-snapshot
  fixture per *grammar production* in
  [`docs/spec/nsl_lang.ebnf`](../../docs/spec/nsl_lang.ebnf)
  §§1–11 under `test/parse/grammar/<production>/pass.nsl` plus
  `<production>/pass.ast`. Coverage is enumerated by the spec's
  EBNF rule names (cross-checked against the AST node kinds in
  [`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md)
  §5 lines 622–639). Productions that have no observable AST
  effect (e.g., `line_marker` consumption) ship as
  documented-elsewhere fixtures rather than per-production
  snapshots.
- **FR-026**: For every parser-note `Nn` whose disambiguation is
  observable at the AST level (`N1`, `N2`, `N3`, `N5`, `N6`,
  `N7`, `N10`, `N11`, `N14`), the repository MUST carry one
  passing fixture pair (interpretation A vs interpretation B as
  enumerated in US2's acceptance scenarios) under
  `test/parse/notes/n<NN>/`. Notes whose only observable is at
  Sema time (none, at M2 — all Sn checks are M3) are not in
  M2's scope. Notes moved to `nsl_pp.ebnf` (`N4`, `N8`, `N9`,
  `N12`, `N13`) are covered by M1 and not re-tested at M2.
- **FR-027**: For every parser-note that has a *failing* shape
  (e.g., `N10` warning case: the `label` reserved word as a user
  identifier; `N14` `#line` with a malformed line number),
  the repository MUST carry one fail-case fixture and assert
  the diagnostic string per the Principle VIII rule that
  diagnostic-bearing rules test the diagnostic text.
- **FR-028**: The repository MUST carry a `-emit=ast` golden test
  whose output format is frozen byte-exactly. Adding a new AST
  node kind is a coordinated change: bump the golden, update
  the per-grammar-production fixture set (FR-025), and update
  the parser's recovery-set documentation if applicable.
- **FR-029**: Every fixture MUST be authored before its driving
  implementation (Principle VIII TDD); the test commit MUST be
  observed failing prior to the implementation commit being
  accepted. The TDD evidence path (failing-CI link in PR
  description) is the standard mechanism — no separate audit log
  is required.

**Determinism (Constitution Principle V):**

- **FR-030**: The AST emitted by `nsl-parse` MUST be a pure
  function of (input token stream, CLI flag list). No
  environment-derived inputs (CWD, mtime, locale, hostname,
  env vars other than `NSL_INCLUDE`) MAY influence the AST or
  its serialized form. Two `nslc -emit=ast` invocations on the
  same input under the same flag list MUST produce
  byte-identical stdout.
- **FR-031**: AST-node memory addresses MUST NOT leak into the
  serialized output (no `0x7fff…` raw-pointer prints). Any
  cross-reference between AST nodes (e.g., a `goto`'s target,
  if represented as a pointer) MUST serialize via a stable
  identifier (the target's `SourceRange` start, or a
  zero-based monotonic node index).
- **FR-032**: All collection types whose iteration order is
  part of the serialized AST MUST be deterministic
  (insertion-ordered or sorted) — no unordered_map iteration in
  serialization.

### Key Entities

- **`Parser`**: stateful recursive-descent parser over a `Lexer`;
  produces a `std::unique_ptr<CompilationUnit>` on success or
  null on fatal-after-recovery-exhaustion. Owns the recovery-
  set bookkeeping. Public type in `nsl-parse`.
- **`CompilationUnit`**: AST root node. Field: `items
  std::vector<std::unique_ptr<TopLevelItem>>`. Per
  [`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md)
  §5 line 327.
- **`Decl` / `Stmt` / `Expr`**: abstract mid-level base classes,
  per §5 lines 311–325. Every concrete AST node kind inherits
  from exactly one of these.
- **`ASTVisitor`**: polymorphic visitor base, per FR-005. One
  `visit(T&)` per concrete node kind enumerated in §5 lines
  622–639; missing overrides are compile-time errors.
- **`TypeRef`**: forward-declared in `nsl-ast`'s `Expr`; its
  filled-in form is M3's responsibility. At M2 the slot exists
  and reads as "unresolved" / nullptr.
- **`SourceRange`** (re-used from `nsl-basic` per M1): every AST
  node carries one. The AST printer's per-node "loc" line uses
  the post-`#line` virtual coordinates.
- **`AST printer`**: a free function in `nsl-ast` that walks a
  `CompilationUnit` and emits the `-emit=ast` text-only
  S-expression-style representation (per FR-022). Lives in
  `nsl-ast`, NOT in `nsl-parse`, so future Sema-resolved
  printing can extend it without breaking the parser-only path.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: `nslc -emit=ast fixture.nsl` produces a
  `CompilationUnit` AST covering 100% of the grammar productions
  enumerated in
  [`docs/spec/nsl_lang.ebnf`](../../docs/spec/nsl_lang.ebnf)
  §§1–11 for the per-production fixtures shipped under
  `test/parse/grammar/`. (Coverage measured by per-fixture pass
  rate against the grammar-production enumeration, not line
  coverage of the parser source.)
- **SC-002**: For every parser-note `Nn` whose disambiguation
  has an AST-level observable, the shipping fixture pair
  (interpretation A + interpretation B) compiles on a green CI
  run; the fail-case fixtures (where applicable) emit the
  expected diagnostic text. 0 of the in-scope N-notes are
  unenforced.
- **SC-003**: The `-emit=ast` golden test passes byte-exactly on
  consecutive runs of the same input under the same flag list
  (Principle V; FR-030).
- **SC-004**: A diagnostic emitted by the parser matches the
  regex `^[^:]+:\d+:\d+: (error|warning|note): .+$` (FR-019;
  re-statement of M1's SC-004 for M2 raise sites). 0% deviation
  tolerated.
- **SC-005**: A reviewer opening a red CI run from a parser
  regression can identify the failing grammar production or
  parser-note within 10 seconds, without reading raw
  parser-internal log scrollback (re-statement of M1 SC-008 for
  M2 fixtures).
- **SC-006**: Adding a new grammar production (a hypothetical
  spec change in a later release) requires editing exactly one
  new fixture directory under `test/parse/grammar/`, one new
  AST-node header under `include/nsl/AST/`, one row in
  [`CLAUDE.md`](../../CLAUDE.md) §1's table, and the parser
  itself — no edit to the M2 milestone scaffolding (Principle II
  layer extensibility, applied to the parser test corpus).
- **SC-007**: Two consecutive `nslc -emit=ast` invocations on the
  same input under the same flag list produce byte-identical
  stdout — across both supported build types (Debug and Release)
  and both supported compilers (gcc and clang) (Principle V;
  FR-030; matches M0/M1's reproducibility gates).
- **SC-008**: 100% of files newly added under `lib/AST/`,
  `lib/Parse/`, `include/nsl/AST/`, `include/nsl/Parse/`, and
  `test/parse/` carry the `Apache-2.0 WITH LLVM-exception` SPDX
  header (M0 FR-010 hygiene re-stated for the M2 file set).
- **SC-009**: The `nsl-parse` library's only build-time
  dependencies are `nsl-lex`, `nsl-ast`, and `nsl-basic` (the
  layered structure of Principle II). A CI guard MUST verify
  this — no link-time edge from `nsl-parse` to `nsl-sema` or any
  later layer.

## Assumptions

- **Scope is the M2 row of `README.md` §Roadmap, plus the
  `nslc -emit=ast` driver flag and the inheritance from M1's
  `-I` / `-D` / `NSL_INCLUDE` / `--diagnostic-format=json`
  plumbing.** The driver flag is included because (a)
  `README.md` §Usage shows it as the user-facing surface for the
  parsed AST and (b) the M1 spec stated explicitly "real
  `-emit=` flags arrive incrementally from M1 onward". Without
  it the M2 deliverable is library-only and cannot be exercised
  end-to-end through `nslc`.
- **The parser-note set covered at M2 is the *parsing-observable*
  subset.** Per
  [`docs/spec/nsl_lang.ebnf`](../../docs/spec/nsl_lang.ebnf)
  N4/N8/N9/N12/N13, those notes were moved to
  [`docs/spec/nsl_pp.ebnf`](../../docs/spec/nsl_pp.ebnf) and are
  M1's responsibility. The notes covered at M2 are N1, N2, N3,
  N5, N6, N7, N10, N11, N14. The general policy from
  [`CLAUDE.md`](../../CLAUDE.md) §1's row "Parser notes N1–N14 →
  M2 (most); M3 (S/N interactions)" is interpreted as
  "M2 covers the *parsing* disambiguation; M3 covers any
  S/N interaction (e.g., `S26` `func ≡ function`
  canonicalization warning, which is Sema's, even though the
  *parser* must accept both spellings — see FR-016)."
- **Semantic constraints `S1`–`S29` are M3, not M2.** The
  parser at M2 accepts grammar-conformant input even when it
  violates a Sema constraint. Examples: `S1` (`__` in
  identifiers — accepted by the lexer at M1, parser at M2 sees
  it as an identifier token, Sema rejects at M3); `S15` (bit-
  slice indices must be compile-time — parser accepts any
  expression, Sema enforces at M3); `S22` (`return`-width must
  match — parser builds the `ReturnStmt`, Sema checks).
- **The error-recovery framework is parser-internal and
  lives in `nsl-parse`'s implementation.** It does not require
  a new public API; it is driven by per-rule recovery-token
  sets coded next to each `parseFoo()` method. The depth is
  full multi-error recovery at every grammar level (per
  Clarifications session 2026-04-27 Q1 → Option A); see FR-021
  for the multi-error fixture corpus this implies.
- **The `nsl-ast` library's "per-node-kind headers under one
  directory" pattern is constitutional.** Principle II
  explicitly carves this out — `nsl-ast` is the one library
  exempt from the "single public header per library" rule
  because its surface is a class hierarchy that benefits from
  per-kind separation. Cross-cutting infrastructure
  (`ASTNode`, `ASTVisitor`, the `NodeKind` enum, the `TypeRef`
  forward declaration, the AST-printer entry point) lives in a
  small set of "umbrella" headers under the same directory.
- **The AST output format frozen at M2 is allowed to change in
  later milestones** as the AST gains Sema-resolved types
  (M3), structural-expansion residue (M5), and so on. Each
  format-bumping change re-cuts the `-emit=ast` golden in the
  same patch as the source change. The M2 golden is the
  *initial* freeze, not the final.
- **Audited-project ingestion (`P-VEN`) and golden VCDs
  (`P-VCD`) are out of scope** — they gate M7. CI's end-to-end
  stage and formal stage remain in the "wired-but-empty" state
  established at M0.
- **Tooling track** (T1–T12) is out of scope. T2 (formatter)
  and T3 (LSP skeleton) gate on M3 (Sema), not M2 — M2's AST
  alone is insufficient for either, but it is the structured
  artifact T-track will eventually consume via Sema.
- **The `nsl-sema`, `nsl-dialect`, `nsl-lower`, `nsl-driver`
  libraries** are present as empty layer-skeletons from M0 (with
  the minor M1 amendment for `-emit=tokens` glue) and are not
  modified by M2 except for the minimal `nsl-driver` glue
  needed to expose `-emit=ast` (a single new code path in
  `tools/nslc/main.cpp`'s otherwise-≤60-line driver and a thin
  `nsl-driver` library function invoking lex+preprocess+parse
  and printing the AST).
- **Reference host, build matrix, CI infrastructure, SPDX
  hygiene, `add_nsl_library`, and the `DiagnosticEngine` /
  `SourceManager` plumbing** are M0/M1 deliverables and are
  taken as given. This spec inherits and does not re-justify
  them.
