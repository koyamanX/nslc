<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Research: M2 — Parser + AST (with `-emit=ast`)

**Branch**: `005-m2-parser` | **Date**: 2026-04-27
**Plan**: [plan.md](./plan.md)

Each section resolves one Technical Context decision (or one open
plan question) with **Decision / Rationale / Alternatives**. The
spec already pinned the user-visible decisions via
`/speckit-clarify` (full multi-error recovery; text-only AST dump);
this file pins the implementation-mechanism decisions.

---

## §1. Parser implementation strategy: hand-written recursive descent

**Decision**: **Hand-written recursive-descent parser** in C++17,
with explicit `peek()` / `consume()` / `expect(TokenKind)` primitives
over the M1 `Lexer`'s pull-model interface. Each grammar non-terminal
in `lang.ebnf §§1–11` corresponds to one `parseFoo()` member function
returning `std::unique_ptr<T>` for some AST node `T`.

**Rationale**: Four converging reasons.

1. **LLVM/CIRCT convention.** Clang's parser is hand-written; MLIR's
   `mlir::AsmParser` is hand-written. Following the convention keeps
   M2 readable to anyone who has seen the upstream codebases
   (Constitution Build/Code/Licensing "LLVM/CIRCT conventions for
   naming, brace style, header guards, and file headers"). It also
   matches M1's hand-written lexer (research §1 of the M1 plan).
2. **Parser-note disambiguation is context-sensitive.** N1 (`if`
   statement-vs-expression) requires the parser to know whether it
   is at statement position or expression position — information
   trivially carried by the call stack of a recursive-descent parser
   but awkward to encode in a generator's grammar. Same for N3
   (`.{` two-character lookahead — handled in the lexer per N3, but
   the parser-side consumer must know it is an LHS-concat) and N6
   (`inst.finish()` recognition based on `inst` being a proc-instance
   in the enclosing scope — though *resolution* of the scope is
   M3's, the *syntactic* disambiguation needs lookahead).
3. **Full multi-error recovery (Q1) is naturally expressed in
   recursive descent.** Recovery sets are per-rule `constexpr`
   bitsets (or sorted `TokenKind` lists); the `skipUntil(set)`
   primitive walks the lexer forward until it sees a member of the
   set. Generator-emitted parsers either lack first-class recovery
   (yacc/bison emit one `error` token and bail) or force a
   per-rule action language (ANTLR's `recover()` callbacks).
4. **No build-time generator.** A hand-written parser adds zero
   tooling dependencies. Flex/Bison would pull in a configure-time
   tool we don't otherwise need.

**Alternatives considered**:
- *Bison / yacc generator*: rejected per recovery rationale (yacc's
  one-token error recovery is incompatible with FR-021's per-rule
  recovery sets); also forces an LR(1) grammar, and the NSL grammar
  has known LR(1) ambiguities (N1, N3) that would need precedence
  hacks.
- *ANTLR4 (C++ runtime)*: rejected for build-time-tool dependency
  and licensing surface.
- *PEG library (cpp-peglib, lexy)*: would compile-time evaluate the
  grammar — attractive — but the recovery story is weaker than
  hand-written, and N1's statement-vs-expression disambiguation
  is awkward in declarative PEG.
- *Combinator library (e.g., Boost.Spirit X3)*: rejected for
  compile-time blowup and the same N1 disambiguation concern.

---

## §2. Expression-grammar parsing: Pratt-style precedence climbing

**Decision**: A **Pratt parser** (top-down operator-precedence) for
`lang.ebnf §11` expressions, sharing the same `Parser` object and
recursive-descent style as the rest. Operator-precedence levels and
left/right associativities are encoded in a small `static constexpr`
table indexed by `TokenKind` (one entry per operator listed in
`lang.ebnf §11`); `parseExpr(precedenceFloor)` consumes operators
until it sees one whose precedence is below the floor.

**Rationale**:
- **NSL has ~17 binary precedence levels** (per `lang.ebnf §11`
  including arithmetic, shift, relational, bitwise, logical,
  conditional, concat). Encoding these as nested
  `parseAdditive() / parseMultiplicative() / …` recursive functions
  produces deep call stacks and ~17 near-identical helper bodies;
  Pratt collapses them into one loop and one precedence table.
- **N1's `if`-as-expression form** needs precedence parsing too
  (`return if(c) a else b;` per N1 last bullet) — Pratt handles
  prefix-keyword expressions naturally via "null denotation"
  (nud) entries.
- **N2's reduction-vs-bitwise** (`&x` unary vs `a & b` binary) is
  exactly Pratt's prefix-vs-infix dispatch — `&` has both a `nud`
  (reduction operator) and a `led` (left denotation = bitwise
  binary) entry, with the `nud` invoked when no left operand is
  on the stack.
- **N5's sign-extend `#`** is a prefix operator in expression
  position post-preprocess (the line-marker form is consumed by
  M1) — same Pratt dispatch as N2.

**Alternatives considered**:
- *Pure recursive descent with one parse function per precedence
  level*: rejected for code-volume cost (~17 near-identical helpers)
  and the N2/N5 prefix-vs-infix duplication this would force.
- *Shunting-yard*: rejected — produces a postfix sequence the AST
  builder would have to re-tree; Pratt builds the AST directly.
- *Compile-time precedence table generator*: out of scope for M2;
  the static `constexpr` table is small enough to maintain by hand.

---

## §3. Recovery-set design: per-rule `constexpr` token sets + `skipUntil()` primitive

**Decision**: Each `parseFoo()` function declares a local `constexpr
TokenSet recoverSet` (a small bitset over `TokenKind`) naming the
tokens at which to resume if a syntax error is detected inside the
rule. Recovery is performed by a single `Parser::skipUntil(TokenSet)`
primitive that advances the lexer until `peek().kind() ∈ set` or EOF.
The top-level `parseCompilationUnit()` always recovers at the next
top-level keyword (`struct`, `declare`, `module`, `param_int`,
`param_str`); per-item-list rules (`parseDeclareItem()`,
`parseModuleItem()`, `parseSeqItem()`) recover at `;` or the matching
`}`; per-statement rules recover at `;` or the next statement-start
keyword. A documenting comment block at each recovery site names the
tokens and the expected resume position (FR-021).

**Rationale**:
- **Q1 (full recovery)** demands recovery at every grammar level.
  Per-rule constexpr sets keep the recovery story local to each
  rule — the cost of adding recovery to a new rule is one set
  declaration plus one `skipUntil(set)` call in the catch path.
- **Determinism (FR-030, Principle V)**: a `constexpr` bitset has
  no environment dependency. Two builds produce identical recovery
  behavior given identical input.
- **Documentation (FR-021)**: the comment block at each recovery
  site doubles as the per-rule recovery-set documentation. No
  separate "recovery design doc" needs to be maintained — the code
  *is* the doc.
- **Clangd/rust-analyzer alignment** (per Q1 rationale): both use
  per-rule recovery sets in the same shape (clang's
  `RecoveryConsumer`, rust-analyzer's `recover_until`).

**Alternatives considered**:
- *Single project-wide recovery set*: rejected — different rules
  need different recovery tokens (a missing `;` inside a `seq`
  block should NOT resume at the next `module` keyword).
- *Exception-based recovery*: rejected — exceptions in C++ have
  ABI cost; LLVM/CIRCT convention forbids them in hot paths
  (and the Constitution defers to LLVM/CIRCT convention).
- *Fail-fast then re-parse with skip-ahead*: rejected — produces
  worse error messages and quadratic worst-case time.

---

## §4. AST node ownership: `std::unique_ptr<T>` with polymorphic visitor

**Decision**: AST nodes are heap-allocated via `std::make_unique<T>(...)`.
Children are owned by parents through `std::unique_ptr<Child>` data
members. The visitor (`ASTVisitor` in `nsl/AST/ASTVisitor.h`) is a
classic double-dispatch base class with one pure-virtual
`visit(T&)` per concrete node kind — adding a new node forces every
existing visitor to either implement or `default`-route it
(compile-time exhaustiveness check via `[[noreturn]] = 0` on the
base, with each derived visitor explicitly opting in or out per
method). No `shared_ptr`, no arena — direct ownership only at M2.
The future Sema / lowering layers MAY introduce an arena allocator
in a follow-up if measurement justifies it.

**Rationale**:
- **`nsl_compiler_design.md` §5.x** (lines 617–682) shows the
  `std::unique_ptr` ownership pattern with the visitor double-
  dispatch — this decision is implementing the design doc verbatim,
  not redesigning it.
- **Determinism (FR-031)**: `unique_ptr` is straightforward to walk
  in a fixed traversal order — no GC, no shared ownership cycles
  that would force topological-sort serialization.
- **No-pointer-prints invariant (FR-031)**: cross-references between
  AST nodes (e.g., a `goto`'s target if represented as a node
  pointer) serialize via the target's `SourceRange::start` byte
  offset, not via raw pointer. The printer never `<<` a pointer.
- **Visitor exhaustiveness**: a missing `visit(T&)` override on a
  concrete visitor produces a link-time error (the `= 0` on the
  base resolves to "missing override" at link of the derived
  vtable). This catches "added a new node kind, forgot to teach
  the printer about it" at compile/link, not at runtime.

**Alternatives considered**:
- *Bump-pointer arena (`llvm::BumpPtrAllocator`)*: defer to a
  future milestone if measurement shows AST-allocation hot spot.
  M2 is correctness-first; arena is a possible follow-up.
- *Discriminated union of all node kinds (`std::variant`)*: would
  require the visitor to be a single `std::visit` call rather than
  a class hierarchy — workable, but the design doc commits to the
  class-hierarchy form, and `std::variant` over ~50 alternatives
  produces large codegen.
- *Boost.Variant / `mpark::variant`*: extra dependency for no
  upside over `std::variant` (which we ruled out anyway).

---

## §5. AST-printer format: text-only S-expression with one node per line

**Decision**: The `-emit=ast` printer emits one node per line with
`(` opening per node and `)` closing matching the visit-time stack;
indentation is two spaces per parent. Each node-line carries:
**`(NodeKind  loc=path:line:col-line:col  field1=value1  field2=value2  …)`**
followed by child nodes on subsequent indented lines. The format is
human-readable and grep-friendly. No JSON at M2 (per Q2). Format is
frozen byte-exactly by the `test/Driver/emit-ast.test` golden;
M3/M5/M7 may re-cut the golden in the same patch as a node-kind
addition (Assumptions in spec).

Example shape:

```text
(CompilationUnit  loc=foo.nsl:1:1-12:1
  (ModuleBlock  loc=foo.nsl:1:1-12:1  name=hello
    (RegDecl  loc=foo.nsl:2:3-2:18  name=q  width=
      (LiteralExpr  loc=foo.nsl:2:9-2:10  kind=Decimal  value=8))))
```

**Rationale**:
- **Per Q2 → Option A**: text-only S-expression-style dump à la
  clang's `-ast-dump`. Clang's format is the reference.
- **Determinism (FR-030, FR-032)**: every collection iterated in
  the printer is `std::vector<std::unique_ptr<T>>` (insertion-
  ordered) or a sorted-by-key map. No `std::unordered_map`
  iteration anywhere on the print path.
- **Source-locating (Principle IV, FR-018)**: every node line shows
  its `SourceRange` in `path:line:col-line:col` form (post-`#line`
  virtual coordinates). A future LSP consumer can extract these by
  regex if needed, but the canonical mechanism is the in-memory
  `SourceRange` accessor — the `-emit=ast` text is for humans and
  CI goldens.
- **Format-bumping discipline**: the M2 golden is the *initial*
  freeze; M3 will add `(type=...)` annotations on `Expr` nodes
  (Sema-resolved types), at which point the golden is re-cut in
  the same patch. The format is documented as **schema-unstable
  across M-track milestones** in the contract.

**Alternatives considered**:
- *YAML*: heavier syntax for marginal gain; YAML parser would be a
  new dep at consumer-side. Rejected.
- *S-expression with M-expression-style dotted fields
  (`(NodeKind . loc field1 . value1 …)`)*: syntactically heavier
  with no benefit; rejected.
- *Indent-only (no parens)*: ambiguity around where a multi-line
  `value` ends; rejected.

---

## §6. NodeKind enum source-of-truth: X-macro `.def` file

**Decision**: The `NodeKind` enum is defined by a single X-macro file,
`include/nsl/AST/NodeKind.def`, listing one `NSL_NODE_KIND(Name)`
entry per concrete AST node kind. `NodeKind.h` includes it once with
a definition that builds the enum body; `lib/AST/NodeKindNames.cpp`
includes it again with a different definition that builds the
enum-to-string table for the printer; future code-generated visitors
include it again to enumerate `visit(T&)` methods. This is the same
pattern M1 used for the helper closed-set
(`include/nsl/Basic/HelperSet.def`).

**Rationale**:
- **Single source of truth.** Adding a new AST node kind requires
  exactly one `NSL_NODE_KIND` line; the enum, the printer's
  enum-to-string table, and (in future) the generated visitor
  scaffolding all update automatically. No "forgot to add to the
  enum-string table" bugs.
- **Determinism**: the X-macro expansion order is the source order
  of the `.def` file — a textual artifact in the repo, identical
  across builds.
- **LLVM convention**: `llvm/IR/Instruction.def`, `clang/AST/
  StmtNodes.def`, and `clang/Basic/TokenKinds.def` all use this
  pattern. We are following it.

**Alternatives considered**:
- *Hand-maintained enum + parallel string table*: rejected for the
  parallel-table-drift risk (M1 specifically chose `.def` for the
  helper set to avoid this).
- *TableGen-generated enum*: TableGen is heavier than the simple
  `.def` pattern needs; reserved for the dialect work at M4.
- *Reflection (C++26 `std::meta`)*: not in C++17; out of scope.

---

## §7. ASTVisitor exhaustiveness enforcement

**Decision**: The base `ASTVisitor` declares one **pure-virtual**
`visit(T&)` per concrete node kind enumerated in `NodeKind.def`. A
derived visitor that fails to override a method produces a link-time
error (the vtable references an undefined symbol). The base also
provides an optional `visitDefault(ASTNode&)` template hook that a
derived class can opt into via `using ASTVisitor::visitDefault;`
plus per-method `visit(Foo&) override { visitDefault(node); }` —
this lets visitors that want a default route declare it explicitly
once, rather than being silently forgiven.

**Rationale**:
- **FR-005**: "missing override is a compile-time error". Pure
  virtuals achieve this at link time; CRTP-based static dispatch
  would achieve it at compile time but breaks the polymorphic-
  visitor pattern from the design doc (§5.x).
- **Future-safety**: when M3 sema adds a new visitor (a name-
  resolution walker, say), it inherits the same exhaustiveness
  discipline — adding a node kind in M5 or M7 then forces every
  existing visitor (printer, sema, lowering) to be updated.
- **Explicit-default opt-in**: a visitor that genuinely doesn't
  care about most node kinds (e.g., a search-for-`identifier`
  walker) can write `using ASTVisitor::visitDefault;` to opt
  into a no-op default — but it's an *explicit* opt-in, not a
  silent one. This is the same trade-off `clang::RecursiveASTVisitor`
  makes via its `WalkUpFromX` hooks.

**Alternatives considered**:
- *CRTP visitor (`ASTVisitor<Derived>`)*: faster (static dispatch),
  but breaks runtime polymorphism — the printer can't accept any
  visitor; future dynamic-dispatch use cases (LSP querying a
  node by `kind()`) would be awkward. Rejected for M2; may
  revisit at M3 if profiling justifies.
- *`std::visit` over a `std::variant<...>` of all 50+ kinds*:
  see §4 for why we ruled this out.

---

## §8. Cross-reference serialization in the AST printer (FR-031)

**Decision**: AST nodes that reference other AST nodes by pointer
(at M2: `goto`'s target, if represented as `LabeledStmt*`; the
field-access `Expr` referencing its base; the `func ic.ready` form
referencing its submodule instance) are printed with the **target's
`SourceRange::start`** as the stable identifier, formatted as
`ref=path:line:col`. M2 does *not* yet introduce a monotonic
node-index counter — `SourceRange` is sufficient because every
node's location is unique within a compilation unit (a node is at
most one place in the input).

**Rationale**:
- **FR-031**: "no `0x7fff…` raw-pointer prints". `SourceRange`-
  based references satisfy this directly.
- **Stability across builds (FR-030)**: `SourceRange` is a pure
  function of input bytes (post-`#line` adjustment from M1).
  Two builds produce identical references.
- **Future-extensibility**: if M3 or later introduces synthetic
  AST nodes that don't have a unique `SourceRange` (e.g., compiler-
  generated initializers), the format gets a `synth_id=N` extension
  in the same patch as the new node kind — backward-compatible.

**Alternatives considered**:
- *Monotonic node index assigned at parse time*: introduces a
  global counter that complicates parallel-parse work in T3 LSP;
  rejected for M2.
- *Pointer-as-hex string*: violates FR-031 explicitly. Rejected.

---

## §9. Test corpus organization

**Decision**: Test fixtures are organized by *artifact under test*
rather than by *feature*:

- `test/parse/grammar/<production-name>/`: one fixture per named
  EBNF production in `lang.ebnf §§1–11`, with `pass.nsl` (input)
  and `pass.ast` (expected `-emit=ast` output, asserted via
  FileCheck).
- `test/parse/notes/n<NN>/`: one or two `pass-A.nsl` /
  `pass-B.nsl` fixtures per parsing-observable parser-note
  (interpretation A vs interpretation B per US2 acceptance
  scenarios), plus a `fail.nsl` where the note has a violation
  case (N10 warning, N14 malformed).
- `test/parse/recovery/`: multi-error fixtures meeting FR-021's
  corpus-minimum (two-top-level-errors, two-module-item-errors,
  in-`seq`-error followed by well-formed item).
- `test/Driver/emit-ast.test`: the format golden — one fixture
  whose `-emit=ast` output is byte-frozen.
- `test_unit/`: gtest unit suites (visitor exhaustiveness,
  printer determinism, recovery-set bookkeeping).

A small Python generator
(`scripts/gen_grammar_fixtures.py`, optional) emits per-production
fixture stubs from a manually-curated list of EBNF productions; the
list is checked in next to the script so it's reviewable.

**Rationale**:
- **Per FR-025/FR-026/FR-027**: the spec mandates this directory
  structure verbatim.
- **Discoverability (SC-005)**: a reviewer opening a red CI run
  knows immediately whether the failing fixture is a per-
  production test, a parser-note test, a recovery test, or a
  format-golden — the path tells them.
- **Pattern continuity**: matches M1's `test/lex/keywords/`,
  `test/lex/numbers/`, etc. A returning contributor doesn't have
  to re-learn the layout.

**Alternatives considered**:
- *By feature (e.g., `test/parse/structs/`, `test/parse/modules/`)*:
  rejected — features cross-cut productions, leading to
  "where does this fixture go?" arguments.
- *Single flat `test/parse/`*: rejected for SC-005.

---

## §10. Diagnostic-text-assertion mechanics for parser fail-cases

**Decision**: Parser fail-case fixtures use the same
`// CHECK: <message>` lit + FileCheck pattern M1 established
(M1 spec FR-037). The `<message>` text is the *exact* string
emitted by the parser, including the `path:line:col: error: `
prefix. Where the parser uses a parameterized message
(e.g., `expected ';' after register declaration`), the fixture
asserts the full parameterized form. This makes silent
weakening of a diagnostic ("expected ';'" → "syntax error")
detectable by the test suite per Principle VIII rule for
diagnostic-bearing constraints.

**Rationale**:
- **Constitution Principle VIII**: "diagnostic-bearing rules test
  the diagnostic text". The parser-note fail-cases (N10
  `label`-keyword-as-identifier; N14 malformed line directive)
  are diagnostic-bearing.
- **No regex matching on diagnostic text**: keeps the test set
  brittle-by-design — a deliberate diagnostic-message change
  forces a fixture update, which is a feature, not a bug.

**Alternatives considered**:
- *Regex / FileCheck `CHECK-DAG` matching*: rejected — too lenient.
- *Diagnostic-ID-based assertion (e.g., `// EXPECT: parse_err_42`)*:
  would require introducing a stable diagnostic-ID system at M2;
  M1 didn't, and bolting it on now is out of scope. Reserved for
  a future cross-cutting diagnostic refactor.

---

## §11. Layering CI guard extension

**Decision**: M0 shipped `scripts/check_layering.py` (or equivalent;
verify exact path during implementation) that asserts no library
declares a `DEPENDS` edge violating Principle II's downward-only
flow. M2 extends this to specifically forbid any link-time edge
from `nsl-parse` to `nsl-sema`, `nsl-dialect`, `nsl-lower`, or
`nsl-driver` — and from `nsl-ast` to `nsl-parse` or anything below.
The check runs in CI's static-checks stage and locally via
`scripts/ci.sh static-checks`.

**Rationale**:
- **SC-009**: "A CI guard MUST verify this — no link-time edge
  from `nsl-parse` to `nsl-sema` or any later layer." Direct
  spec mandate.
- **Drift prevention**: as M3+ libraries land, the temptation to
  reach "down" from a parser into sema for a quick name-resolve
  call grows. The CI guard makes this temptation visible at PR
  time.

**Alternatives considered**:
- *Manual review-only enforcement*: rejected — Principle VII's
  "by policy, not by hope" applies here too.
- *CMake-level enforcement only*: works for `target_link_libraries`
  edges but doesn't catch direct `#include` of a private header.
  The Python script greps both `target_link_libraries` and
  `#include "nsl/<lib>/..."` for a stronger check.

---

## §12. Constitution Check — post-design re-evaluation

After Phase 1's data-model.md, contracts/, and quickstart.md are
authored, the Constitution Check (plan.md §Constitution Check) is
re-evaluated. **Result: still PASSES.** Specifically:

- **Principle II** (layered): Phase 1's data-model documents per-
  node-kind headers under `include/nsl/AST/` per the Principle II
  §3 exception. No new layer is introduced.
- **Principle V** (deterministic): Phase 1's
  `ast-stability.contract.md` formalizes the deterministic-printer
  invariants from FR-030 / FR-031 / FR-032 — every contract item
  is traceable to a constitutional principle.
- **Principle VI** (test discipline): Phase 1's quickstart
  walks the contributor through `nslc -emit=ast fixture.nsl`
  end-to-end — the exact path the test fixtures exercise.
- **Principle VII** (spec/design coupling): Phase 1's
  `data-model.md` pulls AST-node-class definitions directly from
  `nsl_compiler_design.md` §5 lines 299–682 — no design-doc edit
  is implied. If implementation reveals a §5 inaccuracy, the
  same patch updates §5 (Principle VII binding).
- **Principle VIII** (TDD): Phase 1's contracts will be the
  test-author's guide to writing the failing fixtures *before* the
  implementations land — this is the test-first artifact.

No violations to record in `plan.md` §Complexity Tracking.

---

## §13. Open follow-ups (recorded for tasks.md and post-merge)

These are *not* M2 blockers; they are deliberate deferrals or
future patches recorded so they don't get lost.

1. **AST-arena allocator.** Defer to a future milestone if
   profiling shows AST allocation is a hot spot. M2 ships
   direct `std::unique_ptr` ownership.
2. **AST-as-JSON output.** Deferred to T-track per Q2. The T3
   plan/tasks-to-issues for the LSP track will revisit this with
   the concrete LSP consumer in mind.
3. **Stable diagnostic IDs.** Not introduced at M2 (per §10
   alternatives). A future cross-cutting refactor MAY add
   `diag::parse_expected_semi` constants; if so, the fail-case
   fixtures gain a `// EXPECT-ID:` annotation in the same patch.
4. **CST-with-trivia.** The lossless CST infrastructure for the
   LSP / formatter (per `nsl_tooling_design.md §2`) is not built
   at M2. The AST is *not* lossless — comments and exact
   whitespace are not preserved. T-track plans the CST as a
   parallel walker over the same `Lexer` token stream.
5. **`docs/CLAUDE.md` line-range refresh.** If implementation
   shifts any line ranges in `docs/spec/nsl_lang.ebnf` or
   `docs/design/nsl_compiler_design.md` (it shouldn't, since
   M2 implements both verbatim), the same patch updates
   `docs/CLAUDE.md` §§4–7 per Principle VII.
