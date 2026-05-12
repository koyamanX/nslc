// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/ASTToMLIR.h — private internal header for the AST → nsl
// MLIR dialect lowering visitor (M5, layer 8a).
//
// **Specification anchors**:
//   - `specs/008-m5-structural-passes/spec.md` FR-004, FR-005,
//     FR-006, FR-007, FR-008.
//   - `specs/008-m5-structural-passes/data-model.md` §1.
//   - `specs/008-m5-structural-passes/research.md` §4 — Q4 → Option A:
//     single-pass walk with MLIR `SymbolTable` lazy resolution.

#ifndef NSL_LIB_LOWER_ASTTOMLIR_H
#define NSL_LIB_LOWER_ASTTOMLIR_H

#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/OwningOpRef.h"
#include "mlir/IR/Types.h"
#include "mlir/IR/Value.h"
#include "nsl/AST/ASTVisitor.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSet.h"

namespace nsl::ast {
class CompilationUnit;
class Expr;
class ModuleBlock;
class PortDecl;
class ProcDefn;
class Stmt;
} // namespace nsl::ast

namespace nsl::sema {
struct SemaResult;
} // namespace nsl::sema

namespace nsl::lower {

/// Single-pass AST → `nsl` MLIR dialect lowering visitor.
///
/// Walks an `ast::CompilationUnit` exactly once (Q4 → Option A) and
/// produces an `mlir::OwningOpRef<mlir::ModuleOp>` whose body
/// contains one `nsl.module` per `ast::ModuleBlock`.
class ASTToMLIR : public ast::ASTVisitor {
public:
  ASTToMLIR(mlir::MLIRContext &ctx, const sema::SemaResult &sr);
  ~ASTToMLIR() override;

  /// Public entry point per FR-005. Walks `cu` and returns the
  /// resulting top-level `mlir::ModuleOp`.
  mlir::OwningOpRef<mlir::ModuleOp> lower(const ast::CompilationUnit &cu);

  // Declare all visit() overrides via the X-macro. Each
  // implementation lives in `ASTToMLIR.cpp`. At Phase 3 (US1)
  // implementation is incremental: visit(CompilationUnit) and
  // visit(ModuleBlock) emit real ops; the remainder are no-op stubs
  // turning GREEN incrementally as US1 sub-tasks complete (T047–T056
  // in tasks.md).
#define NSL_NODE_KIND(EnumName, BaseClass)                                     \
  void visit(const ast::EnumName &node) override;
#include "nsl/AST/NodeKind.def"
#undef NSL_NODE_KIND

private:
  /// Lower an action-body `Stmt` into the current insertion point's
  /// block. If `body` is a top-level `ast::ParallelBlock`, its
  /// `decls()` + `items()` are recursed directly (no wrapping
  /// `nsl.parallel` is emitted) — matching the M4 dialect's
  /// flattened proc/state/func body shape (see
  /// `test/Dialect/atomic/finish_roundtrip.mlir`). Any other Stmt
  /// kind dispatches normally via `accept(*this)`.
  void lowerActionBody(const ast::Stmt *body);

  /// Lower an expression-position `ast::Expr` to an `mlir::Value`,
  /// emitting any necessary expression-tree ops at the current
  /// insertion point. Returns a null `mlir::Value` on unresolved /
  /// unsupported shapes (Phase 3 quietly soft-fails per FR-010 — Sema
  /// would have caught the bad shape upstream, so this only fires on
  /// future-feature gaps). Phase 3 lowers `LiteralExpr` (Decimal) and
  /// `IdentifierExpr` (via `nameTable_`); richer expression coverage
  /// (BinaryExpr / UnaryExpr / Conditional / Slice / Concat / etc.)
  /// lands incrementally per T055.
  ///
  /// `typeHint` (optional) carries a target `!nsl.bits<N>` width for
  /// width-inference at literal-lowering sites. When an unsized
  /// `LiteralExpr` (e.g., `0`, `42`) reaches a context with a known
  /// target type (e.g., the RHS of `reg[8] q := 0`, the RHS of an
  /// arithmetic `nsl.add %a, %b` whose LHS is `!nsl.bits<8>`), the
  /// hint widens the resulting `nsl.constant` so trait-driven
  /// `SameOperandsAndResultType` / `SameTypeOperands` verifiers
  /// accept the op. Sized literals (`2'd0`, `4'b0000`, `8'h2A`) carry
  /// their own width in the spelling and ignore the hint. Non-literal
  /// recursions (e.g., `BinaryExpr` arguments) propagate the hint
  /// through to inner literals; arithmetic/bitwise ops use LHS type
  /// as RHS hint so `(a + 1)` widens `1` to `a`'s type. Comparison ops
  /// also use LHS-as-RHS-hint (result is fixed at `!nsl.bits<1>`,
  /// independent of the hint). Returning early when LHS is null
  /// avoids leaking RHS-side `nsl.constant` ops into the IR.
  mlir::Value lowerExpr(const ast::Expr *expr, mlir::Type typeHint = nullptr);

  mlir::MLIRContext &ctx_;
  const sema::SemaResult &sr_;
  mlir::OpBuilder builder_;
  /// Top-level `mlir::ModuleOp` produced by `lower(...)`. Set on
  /// entry to `visit(CompilationUnit)`; child `nsl.module` ops are
  /// inserted into its body.
  mlir::ModuleOp top_module_;

  // TRANSITIONAL (option (d) per offload 2026-05-01): name-string-
  // keyed scope dictionary while M3 Sema is stub-only. Replace with
  // `llvm::DenseMap<const sema::Symbol *, mlir::Value> valueMap_`
  // once M3 lands and `IdentifierExpr::resolvedSym()` is available
  // per Q4 → Option A. See research.md §15 (M4 amendment) + the
  // four-way decision recorded in commit ceea300's body.
  //
  // Ordering rule (Constitution Principle V — determinism): this map
  // is for LOOKUP only; never iterate it for emission ordering.
  llvm::StringMap<mlir::Value> nameTable_;

  /// One field of an emitted `nsl.struct` — the field's NSL-spec name
  /// + its emitted `mlir::Type`. The type is recorded so
  /// `lowerExpr(FieldAccessExpr)` and `lowerExpr(StructCastExpr)` can
  /// pass the exact same `mlir::Type` instance to `nsl.field`'s
  /// `result` operand — `FieldOp::verify()` requires pointer-equal
  /// type match against the corresponding `nsl.field_decl`'s
  /// `fieldType` attribute (per `lib/Dialect/NSL/IR/NSLOps.cpp:861`).
  struct StructField {
    llvm::StringRef name;
    mlir::Type type;
  };

  /// TRANSITIONAL (option (d) per offload 2026-05-01): name-string-
  /// keyed struct catalog while M3 Sema is stub-only. Maps struct
  /// name → ordered field list (in source-declaration order, which
  /// is also MSB-first per S18 — `lang.ebnf:889`). Populated by
  /// `visit(StructDecl)`. Replace with `sema::TypeSystem::structFor`
  /// once M3 lands.
  ///
  /// Ordering rule (Constitution Principle V — determinism): this
  /// map is for LOOKUP only; never iterate the outer map for
  /// emission ordering. The inner `SmallVector` IS iterated, but
  /// only by index — that's deterministic.
  llvm::StringMap<llvm::SmallVector<StructField, 4>> structTable_;

  /// TRANSITIONAL (offload 2026-04-30 Commit 5 / T045): name-keyed
  /// catalog of control terminals whose `nsl.fire_probe @<name>`
  /// targets are valid per the M4 op verifier. Populated by
  /// `visit(PortDecl)` for `Direction::FuncIn` / `Direction::FuncOut`
  /// and by `visit(FuncSelfDecl)` (M4-valid subset since Phase 3),
  /// AND post-merge M4-amendment 2026-05-02 (#5) by
  /// `visit(ProcNameDecl)` / `visit(StateNameDecl)` for the
  /// proc_name / state_name S27 tap targets (the dialect verifier
  /// for `nsl.fire_probe` was extended to accept sibling
  /// `nsl.proc` and (within an enclosing proc body) sibling
  /// `nsl.state` symbols). Lookup-only; never iterated for
  /// emission ordering (Constitution Principle V).
  ///
  /// Note: NSL S27 (`lang.ebnf:965`) classifies a broader set as
  /// "control-terminal-tap" (`func_in`, `func_out`, `func_self`,
  /// `proc_name`, `state_name`). All five are now legal targets
  /// post-amendment-#5; the table is the union of all five
  /// surfaces. State_name is per-proc-scope per S11 — the table
  /// is module-flat, which is conservative (collisions across
  /// procs would be flagged as the same target; if S11's per-
  /// proc scoping later surfaces as a sema issue, the table can
  /// grow a per-proc dimension).
  llvm::StringSet<> controlTable_;

  /// TRANSITIONAL (offload 2026-04-30 follow-on / T045): scoped
  /// pointers to the AST `ModuleBlock` / `ProcDefn` whose visit is
  /// currently in flight. Used by `visit(ProcNameDecl)` /
  /// `visit(StateNameDecl)` to scan for matching `ProcDefn` /
  /// `StateDefn` bodies BEFORE registering the name in
  /// `controlTable_` — proc_name / state_name without a matching
  /// body would emit a verifier-rejected fire_probe (per S27 +
  /// post-merge M4-amendment 2026-05-02 #5; the `nsl.fire_probe`
  /// verifier requires the target to resolve to a sibling Symbol).
  /// Set on entry to `visit(ModuleBlock)` / `visit(ProcDefn)`,
  /// restored on exit via local `RestoreOnExit` guard.
  const ast::ModuleBlock *currentModule_ = nullptr;
  const ast::ProcDefn *currentProc_ = nullptr;

  /// Name-keyed catalog of control-terminal `PortDecl`s collected
  /// from `visit(DeclareBlock)` and consumed by `visit(ModuleBlock)`.
  ///
  /// Background (post-merge M4-amendment 2026-05-05 #9 — `nsl.declare`
  /// + port-info ops). The NSL `declare M { ... }` block carries BOTH
  /// data terminals (input/output/inout) AND control terminals
  /// (func_in/func_out/func_self) per `lang.ebnf §4`. Post-amendment
  /// the data terminals lower to `nsl.input_port`/`nsl.output_port`/
  /// `nsl.inout_port` ops INSIDE the `nsl.declare` body; control
  /// terminals continue to lower to `nsl.func_in`/`nsl.func_out`/
  /// `nsl.func_self` ops INSIDE the paired `nsl.module` body
  /// (those ops carry `HasParent<"ModuleOp">`). Since NSL grammar
  /// places `declare M` before `module M` in source order, the
  /// declare visit fires first; this table parks the control-terminal
  /// AST nodes until the matching `visit(ModuleBlock)` runs and can
  /// open the module's region as the insertion point.
  ///
  /// Ordering rule (Constitution Principle V — determinism): this
  /// map is for LOOKUP only by module name; the inner vector
  /// preserves source order so control terminals materialize in
  /// the same sequence they appear in the NSL source.
  llvm::StringMap<llvm::SmallVector<const ast::PortDecl *, 4>>
      pendingControlTerminals_;

  /// TRANSITIONAL (offload 2026-04-30 Commit 1 / T071): name-keyed
  /// catalog of `nsl.param_int`-style top-level integer parameters
  /// emitted by `visit(TopLevelParamDecl)`. Consumed by
  /// `visit(StructuralGenerate)` so a `generate(i = 0; i < N; ...)`
  /// whose `cond` is an `IdentifierExpr` referencing a param resolves
  /// to a literal I64Attr `upper` bound at MLIR-emit time. This is
  /// the eager-resolve seam — at M5's frozen dialect surface no
  /// `nsl::*` op carries a `FlatSymbolRefAttr` slot pointing at a
  /// param symbol (`nsl.structural_generate.{lower,upper,step}` are
  /// I64Attrs), so deferred resolution via `NSLResolveParamsPass` has
  /// no operand-side substitution target. The pass remains a
  /// registered slot that walks defensively over `FlatSymbolRefAttr`
  /// uses but does nothing on pure-NSL inputs at M5. M7 (Verilog
  /// emission) will consume `nsl.param_int` ops directly when
  /// generating `param_int` instance arguments to `nsl.submodule`.
  ///
  /// String params are NOT tracked here because at M5 they have no
  /// expression-position consumer — only `param_int` is referenceable
  /// in a `generate` bound (S10 requires integer).
  ///
  /// Ordering rule (Constitution Principle V — determinism): this
  /// map is for LOOKUP only; never iterate it for emission ordering.
  llvm::StringMap<int64_t> paramTable_;
};

} // namespace nsl::lower

#endif // NSL_LIB_LOWER_ASTTOMLIR_H
