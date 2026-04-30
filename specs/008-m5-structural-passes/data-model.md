<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Data Model: M5 — `nsl-lower` part 1

**Branch**: `008-m5-structural-passes` | **Date**: 2026-04-30
**Plan**: [plan.md](./plan.md)

This document catalogs the entities introduced or extended at M5:
their fields, relationships, lifecycle, and the validation rules
they impose. M5 is a compiler-engineering milestone — most
"entities" are C++ classes, MLIR ops (M4-frozen, referenced here),
or AST node kinds (M2-frozen, referenced here). The fixture-corpus
shape under `test/Lower/` is also documented as a data shape
because FR-027 / FR-028 / FR-029 / FR-030 mandate its layout.

---

## 1. `nsl::lower::ASTToMLIR` (visitor class)

**Layer**: 8a (private, lib-internal — declared in
`lib/Lower/ASTToMLIR.h`).

**Class shape** (frozen by FR-004 + FR-005):

```cpp
namespace nsl::lower {

class ASTToMLIR : public ast::ConstASTVisitor {
public:
    ASTToMLIR(mlir::MLIRContext& ctx, const sema::SemaResult& sr);

    // Public entry point (FR-005).
    mlir::OwningOpRef<mlir::ModuleOp> lower(const ast::CompilationUnit& cu);

private:
    // ~30 visit() overrides per FR-006 (see contracts/lower-api.contract.md).
    void visit(const ast::ModuleBlock&) override;
    void visit(const ast::ProcDefn&)    override;
    // ... etc, every concrete AST node kind

    // Expression-position lowering (FR-007).
    mlir::Value lowerExpr(const ast::Expr&);

    mlir::MLIRContext& ctx_;
    const sema::SemaResult& sr_;
    mlir::OpBuilder builder_;
    llvm::DenseMap<const sema::Symbol*, mlir::Value> valueMap_;
    llvm::DenseMap<const ast::Identifier*, mlir::FlatSymbolRefAttr> symbolRefs_;
};

// Public umbrella export.
mlir::OwningOpRef<mlir::ModuleOp>
astToMLIR(mlir::MLIRContext& ctx, const ast::CompilationUnit& cu, const sema::SemaResult& sr);

} // namespace nsl::lower
```

**Key invariants** (FR-008–010, research §4 + §13):

- Every emitted op carries `mlir::Location` resolvable to AST `SourceRange`.
- Single-pass walk in source order; cross-region `FlatSymbolRefAttr`
  lazy-resolved by MLIR's `SymbolTable` at verifier time.
- `valueMap_` is keyed on `Symbol*` for **lookup only** — never iterated
  for emission ordering (would leak pointer order, violate Principle V).
- `symbolRefs_` is similarly key-lookup-only.
- Both maps are reset / discarded at the end of `lower(...)`.

**Lifecycle**:

```text
[Compilation::lowerToNSL] → [ASTToMLIR ctor] → [walk(cu)] → [return ModuleOp] → [ASTToMLIR dtor]
                                ↑                                                ↑
                                |                                                |
                                +-------- DiagnosticBridge active ----------+
```

**Failure mode**: Internal-error path per FR-010 emits a `Severity::Error`
diagnostic with `mlir::Location::UnknownLoc` (since the failing AST
node lacks a meaningful `SourceRange` association) plus the AST
node-kind name in the message. `Compilation::lowerToNSL` returns
nullptr. Driver halts, prints diagnostics, exits non-zero.

---

## 2. The six pass classes

All six derive from `mlir::OperationPass<nsl::dialect::ModuleOp>`
and follow the pattern in research §11. Frozen by FR-011 + FR-012.

| # | Class | CLI flag | Purpose | Body delivers at M5? |
|---|---|---|---|---|
| 1 | `NSLResolveParamsPass` | `-nsl-resolve-params` | Substitute `nsl.param_int` / `nsl.param_str` references with constants from M3 Sema parameter map | Yes (FR-013) |
| 2 | `NSLExpandGeneratePass` | `-nsl-expand-generate` | Unroll each `nsl.structural_generate` into N inline copies of body; substitute loop variable | Yes (FR-014) |
| 3 | `NSLExpandVariablesPass` | `-nsl-expand-variables` | Convert `nsl.variable` to SSA chain of `nsl.wire` + `nsl.transfer`; per-field for struct-typed | Yes (FR-015) |
| 4 | `NSLExplodeSubmodArrayPass` | `-nsl-explode-submod-array` | Replace array-form `nsl.submodule` (`SUB[3]`) with N independent ops + rewrite references | Yes (FR-016) |
| 5 | `NSLInlineInternalFuncPass` | `-nsl-inline-internal-func` | Inline `func_self` at single call site (optional perf pass) | **No-op slot** (FR-017, research §3) |
| 6 | `NSLCheckSemanticsPass` | `-nsl-check-semantics` | Detect `%IDENT%` residue via regex; re-check 6 post-expansion-sensitive `Sn` | Yes (FR-018, research §1+§5) |

**Pipeline ordering** (frozen by FR-012, illustrated):

```text
nsl::ModuleOp (post-AST→nsl)
   │
   ▼  1. NSLResolveParamsPass  ──▶ all param_int/param_str now constants
   │
   ▼  2. NSLExpandGeneratePass ──▶ zero nsl.structural_generate; loop vars substituted
   │
   ▼  3. NSLExpandVariablesPass ──▶ zero nsl.variable; SSA chains built
   │
   ▼  4. NSLExplodeSubmodArrayPass ──▶ zero array-form nsl.submodule
   │
   ▼  5. NSLInlineInternalFuncPass ──▶ no-op at M5 (slot reserved)
   │
   ▼  6. NSLCheckSemanticsPass ──▶ residue + Sn re-check; emits diagnostics if any
   │
   ▼ (success) → driver prints with default printer
   ▼ (failure) → driver emits diagnostics, exits non-zero
```

**Public construction API** (re-exported through `Lower.h`):

```cpp
namespace nsl::lower {
  std::unique_ptr<mlir::Pass> createNSLResolveParamsPass();
  std::unique_ptr<mlir::Pass> createNSLExpandGeneratePass();
  std::unique_ptr<mlir::Pass> createNSLExpandVariablesPass();
  std::unique_ptr<mlir::Pass> createNSLExplodeSubmodArrayPass();
  std::unique_ptr<mlir::Pass> createNSLInlineInternalFuncPass();
  std::unique_ptr<mlir::Pass> createNSLCheckSemanticsPass();

  void registerNSLLowerPasses();  // calls mlir::registerPass<X>() for each
}
```

---

## 3. `%IDENT%` residue (NOT a typed entity)

Per Clarifications Q1 → Option B (research §1), residue is **not**
a dialect-level entity — it is a runtime regex match over
`mlir::StringAttr` values on already-existing `nsl::*` ops. The
M4 dialect surface gains zero new ops, zero new attribute types.

**Detection regex** (frozen by `contracts/residue-detection.contract.md`):

```text
%[A-Za-z_][A-Za-z0-9_]*%
```

**Carrier ops in M4 dialect** (string-attrs scanned):

| `nsl::*` op | String-attr fields scanned |
|---|---|
| `nsl.module` | `sym_name` |
| `nsl.reg` | `sym_name`, `n` (textual identifier) |
| `nsl.wire` | `sym_name`, `n` |
| `nsl.mem` | `sym_name`, `n` |
| `nsl.func` | `sym_name` |
| `nsl.proc` | `sym_name` |
| `nsl.state` | `sym_name` |
| `nsl.first_state` | `sym_name` (the `@s` reference) |
| `nsl.field_decl` | `sym_name` |
| `nsl.submodule` | `sym_name`, `n` |
| `nsl.struct` | `sym_name` |
| (any future op carrying user-provided strings) | (added by amendment) |

**Diagnostic format** (FR-018):

```text
<file>:<line>:<col>: error: unresolved macro splice '%<IDENT>%' after structural expansion
```

---

## 4. `Compilation` driver class — M5 body fills

The class itself is M4-frozen
(see [`specs/007-m4-mlir-dialect/contracts/dialect-api.contract.md`](../007-m4-mlir-dialect/contracts/dialect-api.contract.md)).
M5 fills two member-function bodies whose **signatures are unchanged**:

```cpp
mlir::OwningOpRef<mlir::ModuleOp>
Compilation::lowerToNSL(const ast::CompilationUnit& cu, const sema::SemaResult& sr) {
    // [M5] body — was stub at M4
    if (sr.hasErrors()) return {};
    DiagnosticBridge bridge(diag_, mlirCtx_);
    return nsl::lower::astToMLIR(mlirCtx_, cu, sr);
}

mlir::LogicalResult Compilation::runNSLPasses(mlir::ModuleOp module) {
    // [M5] body — was stub at M4
    DiagnosticBridge bridge(diag_, mlirCtx_);
    mlir::PassManager pm(&mlirCtx_, mlir::ModuleOp::getOperationName());
    pm.addPass(nsl::lower::createNSLResolveParamsPass());
    pm.addPass(nsl::lower::createNSLExpandGeneratePass());
    pm.addPass(nsl::lower::createNSLExpandVariablesPass());
    pm.addPass(nsl::lower::createNSLExplodeSubmodArrayPass());
    pm.addPass(nsl::lower::createNSLInlineInternalFuncPass());
    pm.addPass(nsl::lower::createNSLCheckSemanticsPass());
    return pm.run(module);
}
```

`Compilation::run()` delegates to these in the existing M4 dispatch
chain (whose enum-switch on `EmitKind` was frozen at M3 and stub-bodied
at M4 for the post-Sema arms). M5 wires the `EmitKind::NSLMLIR` arm:

```cpp
int Compilation::run() {
    auto ppSrc = preprocess();
    if (diag_.hasErrors()) return printDiagsAndExitNonZero();
    if (opts_.emit == EmitKind::Tokens) return emitTokens(ppSrc);

    auto cu = parse(ppSrc);
    if (diag_.hasErrors()) return printDiagsAndExitNonZero();
    if (opts_.emit == EmitKind::AST) return emitAST(*cu);

    auto sr = sema(*cu);
    if (diag_.hasErrors()) return printDiagsAndExitNonZero();

    // [M5] from here on
    auto module = lowerToNSL(*cu, sr);
    if (!module) return printDiagsAndExitNonZero();
    if (mlir::failed(runNSLPasses(*module))) return printDiagsAndExitNonZero();
    if (opts_.emit == EmitKind::NSLMLIR) return emitNSLMLIR(*module);

    // [M6+] forward-looking
    return diag_.emitNotImplemented(opts_.emit);
}
```

---

## 5. `EmitKind::NSLMLIR` — driver flag value

Frozen at M3 in the enum declaration; M5 wires the body.

| Flag | `EmitKind` | Halts pipeline after | Output |
|---|---|---|---|
| `-emit=tokens` | `Tokens` | M1 lex | token stream |
| `-emit=ast` | `AST` | M2 parse + M3 sema | AST snapshot |
| `-emit=mlir` | **`NSLMLIR`** | **M5 `runNSLPasses`** | **`mlir::ModuleOp` default-printed** (Q2 → Option A) |
| `-emit=hw` | `CIRCT` | (M6 — not delivered) | (stubbed: not-implemented diag) |
| `-emit=hw` later refinement | `HW` | (M6+) | (stubbed) |
| `-emit=verilog` | `Verilog` | (M7) | (stubbed) |

Output destination: stdout if `-o` absent, otherwise the named file.
Stdin-piped input via `nslc -emit=mlir -` is supported (FR-024).

---

## 6. `DiagnosticBridge` (private internal helper)

**Location**: `lib/Lower/Pass/Common/DiagnosticBridge.{h,cpp}` (research §6 + §12).

```cpp
namespace nsl::lower {

class DiagnosticBridge {
public:
    DiagnosticBridge(basic::DiagnosticEngine& sink, mlir::MLIRContext& ctx);
    ~DiagnosticBridge();

    DiagnosticBridge(const DiagnosticBridge&) = delete;
    DiagnosticBridge& operator=(const DiagnosticBridge&) = delete;

private:
    basic::DiagnosticEngine& sink_;
    mlir::ScopedDiagnosticHandler handler_;  // owns the underlying handler-id
};

} // namespace nsl::lower
```

**Lifecycle**: RAII — installation on construction (registers a
diagnostic handler with the supplied `MLIRContext`), tear-down on
destruction (un-installs the handler). Construction sites:
top of `Compilation::lowerToNSL`, top of `Compilation::runNSLPasses`.

**Translation rules**:

| MLIR `DiagnosticSeverity` | Project `basic::Severity` | Notes |
|---|---|---|
| `Note` | `Note` | secondary annotations |
| `Warning` | `Warning` | informational, does not block |
| `Error` | `Error` | sets `hasErrors()`, blocks pipeline |
| `Remark` | `Note` | demoted (project has no `Remark` tier) |

`mlir::Location` → `basic::SourceRange` translation walks the
location attribute, prefers `FileLineColLoc`, falls back to
`FusedLoc`'s child locations (deepest first).

---

## 7. Test fixture data shape

### 7.1 Per-AST-node fixtures (FR-027 — US1 coverage)

Path: `test/Lower/<category>/<node>_emit_mlir.nsl` (input) +
`<node>_emit_mlir.mlir.expected` (golden).

`<category>` is the AST node-kind family:

| `<category>` | AST node kinds covered |
|---|---|
| `module` | `ModuleBlock`, `DeclareBlock` |
| `decl` | `RegDecl`, `WireDecl`, `MemDecl`, `FuncDefn`, `ProcDefn`, `StateDefn`, `FirstStateDecl` |
| `action` | `ParallelBlock`, `AltBlock`, `AnyBlock`, `SeqBlock`, `WhileBlock`, `ForBlock` (×2 forms), `IfStmt` |
| `stmt` | `TransferStmt` (×2 modes), `ControlCallStmt`, `BareFinishStmt`, `SystemTaskStmt` (×4 sim tasks) |
| `expr` | `BinaryExpr` (×operator), `UnaryExpr`, `LiteralExpr`, `IdentifierExpr`, `ConditionalExpr`, `SliceExpr`, `ConcatExpr`, `StructCastExpr`, `FieldAccessExpr` |
| `marker` | control-name-as-1-bit-value (lowers to `nsl.fire_probe`) |

Fixture cardinality: ≥30 distinct files (count varies with
`<operator>` + `<sim-task>` axes; estimate 40–50 fixtures total).

### 7.2 Per-pass fixtures (FR-028 — pass-standalone coverage)

Path: `test/Lower/passes/<pass-flag>/<scenario>.mlir` (input) +
`<scenario>.expected.mlir` (golden) consumed via
`nsl-opt -<pass-flag>`.

Per-pass scenario axes:

| Pass flag | Scenarios |
|---|---|
| `nsl-resolve-params` | literal-param, multi-param, param-in-generate-bound |
| `nsl-expand-generate` | literal-bound, param-bound, nested, body-references-multiple-positions, zero-bound, one-bound |
| `nsl-expand-variables` | scalar-single, scalar-chain-of-3, partial-assignment-S12, struct-typed, cross-scope |
| `nsl-explode-submod-array` | size-3, size-1, size-0 |
| `nsl-inline-internal-func` | noop-roundtrip (single fixture per research §3) |
| `nsl-check-semantics` | residue-typo, residue-undefined, S15-post-expansion, S16-violation, S25-replicated-collision, clean-baseline |

### 7.3 M3-corpus extension fixtures (FR-030 — Sema-clean lowering)

Path: `test/Lower/m3_corpus/<sn>/<case>.expected.mlir` mirrored from
`test/sema/<sn>/<case>.nsl` (the M3 pass-case fixture). One golden
per ~34 M3 pass-case input. CI runs each through
`nslc -emit=mlir input.nsl | diff - <case>.expected.mlir`.

### 7.4 Determinism gate fixtures (FR-029, US5)

Path: `test/Lower/determinism/` plus a CI matrix job that
materializes two build trees in distinct host paths and `diff -q`
every output across the two builds. No per-test goldens; pass/fail
is the diff result.

### 7.5 Fixture-existence enforcement (CI guard)

Per FR-027 last sentence, a CI check
(`scripts/audit_lower_fixtures.sh`) enumerates the visitor's
`visit(...)` overrides via grep on `lib/Lower/ASTToMLIR.cpp` and
asserts a paired fixture exists under `test/Lower/`. Missing
fixture = CI block.

---

## 8. Relationships to upstream entities (M3 + M4)

```text
M3 Sema:
  sema::SemaResult → consumed by Compilation::lowerToNSL
  sema::Symbol     → key in ASTToMLIR::valueMap_
  sema::TypeSystem → query for op-result types in lowerExpr()
                     (via sr_.typeOf(expr) lookup)

M2 AST:
  ast::CompilationUnit → input to ASTToMLIR::lower()
  ast::*Block / *Decl / *Stmt / *Expr → visit() targets

M4 Dialect (M4-frozen, NOT amended at M5):
  nsl::dialect::ModuleOp / RegOp / WireOp / etc. → visitor builds these
  nsl::dialect::structural_generate / variable / submodule → pass inputs
  nsl::dialect::*Op verifiers → run during MLIR's verify-each at end of pass pipeline

M1 Preprocessor (M1-frozen, NOT amended at M5):
  Preserves %IDENT% as literal substring in AST node names; M5
  detects via regex post-expansion
```

---

## 9. State transitions: pass-pipeline IR shapes

| Stage | Pre-stage IR property | Post-stage IR property |
|---|---|---|
| AST→nsl visitor output | (no IR before) | `nsl::ModuleOp` containing all `nsl.module @M`s; may contain `param_int`, `structural_generate`, `variable`, array-form `submodule`, `%IDENT%` substrings |
| After resolve-params | param refs may exist | zero `param_int` / `param_str` operand refs |
| After expand-generate | `structural_generate` may exist | zero `structural_generate`; loop-var splices substituted |
| After expand-variables | `variable` may exist | zero `variable`; SSA chains built |
| After explode-submod-array | array-form `submodule` may exist | zero array-form `submodule` |
| After inline-internal-func | (no change at M5; slot present) | byte-identical to input |
| After check-semantics | `%IDENT%` residue may exist | residue → diagnostics; sensitive-`Sn` violations → diagnostics; success → IR unchanged |
| Driver emit | (post-pass IR) | Default-printed text on stdout / file |

Each row in this table is the post-condition contract for one pass;
violation of a row's "post-stage" property is a CI-blocking bug.
