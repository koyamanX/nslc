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

#include "nsl/AST/ASTVisitor.h"

#include "llvm/ADT/StringMap.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/OwningOpRef.h"
#include "mlir/IR/Value.h"

namespace nsl::ast {
class CompilationUnit;
class Expr;
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
  mlir::Value lowerExpr(const ast::Expr *expr);

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
};

} // namespace nsl::lower

#endif // NSL_LIB_LOWER_ASTTOMLIR_H
