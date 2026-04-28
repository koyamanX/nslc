<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Data Model: M2 — Parser + AST

**Branch**: `005-m2-parser` | **Date**: 2026-04-27
**Plan**: [plan.md](./plan.md)

This file enumerates the *types* M2 introduces to the codebase —
the AST node hierarchy and the parser's internal state. Definitions
mirror
[`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md)
§5 (lines 299–682) verbatim per Principle VII; this file is the
plan-level summary, not a re-derivation.

The post-condition for M2 is: every entity below has a header in
`include/nsl/AST/` (or `include/nsl/Parse/` for parser entities), a
test in `test/parse/` or `test_unit/`, and (where applicable) a
`-emit=ast` printer line.

---

## 1. AST entities

### 1.1 Base classes (`include/nsl/AST/`)

| Type | Header | Fields | Relationships |
|---|---|---|---|
| `ASTNode` | `ASTNode.h` | `NodeKind kind_`; `SourceRange loc_` | abstract base; every concrete node inherits transitively |
| `Decl` | `Decl.h` | `Identifier name` (most kinds) | abstract mid-base; declares "name + body" things |
| `Stmt` | `Stmt.h` | (none beyond `ASTNode`) | abstract mid-base; action statements |
| `Expr` | `Expr.h` | `TypeRef inferredType_` (filled by Sema at M3, nullptr at M2) | abstract mid-base; expression-tree leaves and interior nodes |
| `ASTVisitor` | `ASTVisitor.h` | one pure-virtual `visit(T&)` per concrete node kind | polymorphic visitor; missing override = link error |

The `NodeKind` enum (`NodeKind.h`) is generated from
`NodeKind.def` (X-macro source-of-truth — research §6).

`TypeRef` (`Type.h`) is forward-declared at M2; concrete `Type`
subclasses land at M3 (Sema).

### 1.2 Top-level (`compilation_unit` per `lang.ebnf §§1–3.1`)

| Type | Header | Fields | EBNF anchor |
|---|---|---|---|
| `CompilationUnit` | `CompilationUnit.h` | `std::vector<std::unique_ptr<TopLevelItem>> items` | §1 `compilation_unit` |
| `StructDecl` | `StructDecl.h` | `Identifier name`; `std::vector<StructMember> members` | §3 `struct_declaration` |
| `TopLevelParamDecl` | `TopLevelParamDecl.h` | `enum ParamKind {Int, Str} kind`; `Identifier name`; `std::unique_ptr<Expr> init` | §3.1 `top_level_parameter` |

`TopLevelItem` is an alias / `std::variant` of `StructDecl`,
`DeclareBlock`, `ModuleBlock`, `TopLevelParamDecl` plus
`line_marker`-consumed-no-AST (the marker doesn't get a node;
see FR-015).

### 1.3 Declare block (`lang.ebnf §4`)

| Type | Header | Fields | EBNF anchor |
|---|---|---|---|
| `DeclareBlock` | `DeclareBlock.h` | optional `Identifier name`; optional `Modifier modifier ∈ {None, Interface, Simulation}`; `std::vector<std::unique_ptr<Decl>> headerParams`; `std::vector<std::unique_ptr<PortDecl>> ports` | §4 `declare_block` |
| `PortDecl` | `PortDecl.h` | `enum Direction {Input, Output, Inout} direction`; `Identifier name`; optional `std::unique_ptr<Expr> width` | §4 `data_terminal_declaration` |
| `HeaderParamDecl` | (under `Decl.h` as nested or separate) | `enum ParamKind` (matches `param_int`/`param_str`); `Identifier name` | §4 `parameter_declaration` |
| `ControlTerminalDecl` | (separate header, sub-grouped under `PortDecl.h` if inheritance flat) | `enum CtrlDirection {FuncIn, FuncOut} direction`; `Identifier name`; optional `std::vector<Identifier> dummyArgs`; optional `Identifier returnTerminal` | §4 `control_terminal_declaration` |

### 1.4 Module block (`lang.ebnf §§5–7`)

| Type | Header | Fields | EBNF anchor |
|---|---|---|---|
| `ModuleBlock` | `ModuleBlock.h` | `Identifier name`; vectors of `internals`, `actions`, `funcs`, `procs` (typed per design §5) | §5 `module_block` |
| `RegDecl` | `RegDecl.h` | `Identifier name`; optional `std::unique_ptr<Expr> width`; optional `std::unique_ptr<Expr> init` | §6 `register_declaration` |
| `WireDecl` | `WireDecl.h` | `Identifier name`; optional `std::unique_ptr<Expr> width` | §6 `internal_terminal_declaration` |
| `VariableDecl` | `VariableDecl.h` | `Identifier name`; optional `std::unique_ptr<Expr> width` | §6 (variable form) |
| `IntegerDecl` | `IntegerDecl.h` | `Identifier name` | §6 (integer form) |
| `MemDecl` | `MemDecl.h` | `Identifier name`; `std::unique_ptr<Expr> depth`; `std::unique_ptr<Expr> width`; optional `InitList init` | §6 `memory_declaration` |
| `FuncSelfDecl` | `FuncSelfDecl.h` | `Identifier name`; optional `std::vector<Identifier> dummyArgs`; optional `Identifier returnTerminal` | §6 `control_internal_declaration` |
| `ProcNameDecl` | `ProcNameDecl.h` | `Identifier name`; `std::vector<Identifier> regArgs` | §6 `procedure_name_declaration` |
| `StateNameDecl` | `StateNameDecl.h` | `std::vector<Identifier> names` | §6 `state_name_declaration` |
| `FirstStateDecl` | `FirstStateDecl.h` | `Identifier target` | §6 `first_state_declaration` |
| `SubmoduleDecl` | `SubmoduleDecl.h` | `Identifier templateName`; `std::vector<Instance> instances`; `std::vector<ParamAssign> paramAssigns` | §6 `submodule_declaration` |
| `StructInstDecl` | `StructInstDecl.h` | `Identifier typeName`; `enum {Reg, Wire} kind`; optional `std::unique_ptr<Expr> arraySize`; optional `InitValue init` | §6 `struct_instance_declaration` |
| `FuncDefn` | `FuncDefn.h` | `ScopedName name` (single ID, or `submodule.id` per N7); `std::unique_ptr<Stmt> body` | §7 `function_definition` |
| `ProcDefn` | `ProcDefn.h` | `Identifier name`; `std::unique_ptr<Stmt> body` | §7 `procedure_definition` |
| `StateDefn` | `StateDefn.h` | `Identifier name`; `std::unique_ptr<Stmt> body` | §7 `state_definition` |

### 1.5 Action statements (`lang.ebnf §§8–9`)

| Type | Header | Fields | EBNF anchor |
|---|---|---|---|
| `ParallelBlock` | `ParallelBlock.h` | `std::vector<std::unique_ptr<Stmt>> items` | §8 `par_block` |
| `AltBlock` | `AltBlock.h` | `std::vector<CondCase> cases`; optional `std::unique_ptr<Stmt> elseCase` | §8 `alt_block` |
| `AnyBlock` | `AnyBlock.h` | `std::vector<CondCase> cases`; optional `std::unique_ptr<Stmt> elseCase` | §8 `any_block` |
| `SeqBlock` | `SeqBlock.h` | `std::vector<std::unique_ptr<Stmt>> items` | §8 `seq_block` |
| `WhileBlock` | `WhileBlock.h` | `std::unique_ptr<Expr> cond`; `std::vector<std::unique_ptr<Stmt>> items` | §8 `while_block` |
| `ForBlock` | `ForBlock.h` | `ForForm form`; `std::vector<std::unique_ptr<Stmt>> items` | §8 `for_block` |
| `IfStmt` | `IfStmt.h` | `std::unique_ptr<Expr> cond`; `std::unique_ptr<Stmt> thenBr`; optional `std::unique_ptr<Stmt> elseBr` | §8 `conditional_if_statement` (per N1) |
| `StructuralGenerate` | `StructuralGenerate.h` | `Identifier init`; `std::unique_ptr<Expr> cond`; `std::unique_ptr<Expr> step`; `std::unique_ptr<Stmt> body` | §8 `generate_block` |
| `TransferStmt` | `TransferStmt.h` | `enum {WireEq, RegColonEq} op`; `std::unique_ptr<Expr> lhs`; `std::unique_ptr<Expr> rhs` | §9 `transfer_statement` |
| `IncDecStmt` | `IncDecStmt.h` | `std::unique_ptr<Expr> target`; `enum {Inc, Dec} kind`; `bool prefix` | §9 (inc/dec) |
| `ControlCallStmt` | `ControlCallStmt.h` | `ScopedExpr target`; `std::vector<std::unique_ptr<Expr>> args` | §9 `control_call` (per N6) |
| `BareFinishStmt` | `BareFinishStmt.h` | (none) | §9 `bare_finish` |
| `SystemTaskStmt` | `SystemTaskStmt.h` | `SystemTaskKind name`; `std::vector<std::unique_ptr<Expr>> args` | §9 (system task — per N11(a)) |
| `ReturnStmt` | `ReturnStmt.h` | optional `std::unique_ptr<Expr> value` | §9 `return_statement` |
| `EmptyStmt` | `EmptyStmt.h` | (none) | §9 `;` |
| `LabeledStmt` | `LabeledStmt.h` | `Identifier label`; `std::unique_ptr<Stmt> body` | §9 `labeled_statement` |
| `GotoStmt` | `GotoStmt.h` | `Identifier target` | §9 `goto_statement` |
| `InitBlockStmt` | `InitBlockStmt.h` | `std::vector<std::unique_ptr<Stmt>> items` | §10 `_init` block |
| `DelayTaskStmt` | `DelayTaskStmt.h` | `std::unique_ptr<Expr> count` | §10 `_delay(n)` |

### 1.6 Expressions (`lang.ebnf §11`)

| Type | Header | Fields | EBNF anchor / parser-note |
|---|---|---|---|
| `LiteralExpr` | `LiteralExpr.h` | `LiteralKind kind`; `Value value` | §11 `literal` |
| `IdentifierExpr` | `IdentifierExpr.h` | `ScopedName name`; `Symbol* resolvedSym` (nullptr at M2) | §11 `identifier` |
| `SystemVarExpr` | `SystemVarExpr.h` | `enum {Random, Time} kind` | §11 (per N11 (b)) |
| `UnaryExpr` | `UnaryExpr.h` | `UnaryOp op`; `std::unique_ptr<Expr> sub` | §11 (per N2 reduction; N5 sign-extend) |
| `BinaryExpr` | `BinaryExpr.h` | `BinaryOp op`; `std::unique_ptr<Expr> lhs, rhs` | §11 (per N2 bitwise) |
| `ConditionalExpr` | `ConditionalExpr.h` | `std::unique_ptr<Expr> cond, thenE, elseE` | §11 (per N1 expression form) |
| `ConcatExpr` | `ConcatExpr.h` | `std::vector<std::unique_ptr<Expr>> parts` | §11 (incl. `.{...}` LHS form per N3) |
| `RepeatExpr` | `RepeatExpr.h` | `std::unique_ptr<Expr> count, body` | §11 `repeat_expression` |
| `SignExtendExpr` | `SignExtendExpr.h` | `std::unique_ptr<Expr> width, sub` | §11 `sign_extend` (N5) |
| `ZeroExtendExpr` | `ZeroExtendExpr.h` | `std::unique_ptr<Expr> width, sub` | §11 `zero_extend` |
| `SliceExpr` | `SliceExpr.h` | `std::unique_ptr<Expr> sub, hi`; optional `std::unique_ptr<Expr> lo` | §11 `slice_expression` |
| `FieldAccessExpr` | `FieldAccessExpr.h` | `std::unique_ptr<Expr> obj`; `Identifier field` | §11 `field_access` |
| `CallExpr` | `CallExpr.h` | `ScopedExpr target`; `std::vector<std::unique_ptr<Expr>> args` | §11 `function_call` |
| `StructCastExpr` | `StructCastExpr.h` | `Identifier typeName`; `std::unique_ptr<Expr> sub`; `std::vector<Identifier> memberPath` | §11 `struct_cast` |
| `IncDecExpr` | `IncDecExpr.h` | `std::unique_ptr<Expr> target`; `enum {Inc, Dec} kind`; `bool prefix` | §11 (inc/dec in expr position) |

---

## 2. Parser entities (`include/nsl/Parse/` + `lib/Parse/`)

| Type | Header | Fields | Purpose |
|---|---|---|---|
| `Parser` | `Parser.h` | `Lexer& lex`; `DiagnosticEngine& diag`; recovery-set bookkeeping | recursive-descent driver; one `parseFoo()` per non-terminal |
| `TokenSet` | `lib/Parse/Recovery.cpp` (private) | `constexpr` bitset over `TokenKind` | recovery-token sets per FR-021 |
| `RecoveryGuard` | `lib/Parse/Recovery.cpp` (private RAII helper) | `Parser&`; `TokenSet recoverSet` | RAII helper that pushes a recovery scope and pops on dtor |

Public API:

```cpp
namespace nsl::parse {
  std::unique_ptr<ast::CompilationUnit>
  parseCompilationUnit(lex::Lexer& lex, basic::DiagnosticEngine& diag);
}
```

Returns nullptr if and only if recovery exhausts (rare — typically
only on a corrupted token stream from an upstream lex/preprocess
crash).

---

## 3. AST printer entity (`include/nsl/AST/Printer.h`)

| Type | Header | Fields | Purpose |
|---|---|---|---|
| `print(const CompilationUnit&, llvm::raw_ostream&)` | `Printer.h` | (free function) | text-only S-expression-style dump per research §5 |

Internal printer state (a stateful walker class) lives in
`lib/AST/Printer.cpp`; it is not part of the public API.

---

## 4. Validation rules (parser-level, distinct from Sema's M3 work)

These checks are M2's responsibility (lexical-syntactic, not
semantic):

| Rule | Anchor | M2 enforcement |
|---|---|---|
| Every `*Decl` carries a non-empty `Identifier name` (where applicable) | EBNF terminal | parser refuses to construct without an identifier |
| `RegDecl::init` is present iff `=` was parsed | `register_declarator` | parser sets the optional iff the production matched |
| `FuncDefn::name` is single-ID OR `inst.id` form | N7 | parser's `parseFuncDefn()` calls `parseScopedName()` which encodes the dot |
| `IfStmt` is a Stmt-tree node (not Expr-tree) | N1 statement-position | parser dispatches to `parseStmt()` from statement-position contexts only |
| `ConditionalExpr` is an Expr-tree node | N1 expression-position | parser dispatches to `parseExpr()` from expression-position contexts only |
| `ConcatExpr` LHS form (`.{a, b, c} = x;`) | N3 | parser recognizes `.` `{` lookahead via the lexer's `LDotBrace` token |
| Reduction `&` `\|` `^` is `UnaryExpr`, binary forms are `BinaryExpr` | N2 | Pratt nud/led dispatch (research §2) |
| `#` in expression position is `SignExtendExpr` | N5 | Pratt nud entry for `#` |
| `_random` / `_time` (no parens) is `SystemVarExpr` | N11 (b) | parser dispatches identifier classification by lookahead at `(` |
| `_display(...)` / `_finish(...)` etc. is `SystemTaskStmt` | N11 (a) | parser dispatches identifier classification by statement position |
| `label` reserved-keyword-as-identifier emits a warning | N10 | parser raises `warning` diagnostic when identifier-position consumes `label` token |
| `#line` directives are consumed (no AST node) and update `SourceManager` | N14, FR-015 | parser's per-item-list dispatch checks for `LineMarker` kind first |

Sema-level checks (S1–S29) are explicitly **out of M2 scope** —
they land at M3.

---

## 5. State transitions

The parser is largely stateless beyond the `Lexer` cursor and the
recovery-scope stack. The only "state machine" is the recovery
scope:

```
Idle ────[error detected]───▶ InRecovery
   ▲                                │
   │     [skipUntil() finds         │
   │      a recovery token]         │
   └────────────────────────────────┘
```

Recovery scopes nest: an inner rule's `RecoveryGuard` pushes its
recovery set on top of the outer scope's; on inner scope exit
(success OR failure), the outer set is what `skipUntil()`
ultimately resumes at. This is the standard "merged recovery
sets" pattern from compiler textbooks (Aho et al., chapter 4)
and is what clangd uses.

---

## 6. Cross-references between AST nodes

Per research §8, M2 introduces no pointer-based cross-references
that require stable IDs. The only inter-node references are:

- `LabeledStmt` ↔ `GotoStmt` (the goto target by name; resolution
  is M3's, M2 just stores the textual `Identifier`)
- `SubmoduleDecl` ↔ `FuncDefn` (the dotted-`func` form references
  the submodule instance by name; same resolution pattern)
- `ProcNameDecl` ↔ `StateDefn` (proc-scoped state names; same)

All three reference targets by `Identifier` (a `StringRef` over
the source buffer), not by `ASTNode*`. This is what enables
FR-031 trivially: the printer never holds a pointer to print.

---

## 7. Per-node-kind header layout (Principle II §3 exception)

The Constitution Principle II exception for `nsl-ast` allows
per-node-kind headers under `include/nsl/AST/`. M2 exercises this
exception for the first time. The pattern:

- One header per concrete `Decl` / `Stmt` / `Expr` subclass.
- One header per umbrella primitive: `ASTNode.h`, `ASTVisitor.h`,
  `NodeKind.h`, `NodeKind.def`, `Type.h`, `Decl.h`, `Stmt.h`,
  `Expr.h`, `Printer.h`.
- Private headers (e.g., printer-internal walker class,
  recovery-set bitset definitions) stay in `lib/AST/` /
  `lib/Parse/` per the same Principle II rule.

Total: ~50 per-node headers + ~9 umbrella headers = ~59 headers
under `include/nsl/AST/`. Each is ~30–60 lines (class definition +
SPDX + inclusion guard).
