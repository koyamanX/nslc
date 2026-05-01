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

#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/OwningOpRef.h"

namespace nsl::ast {
class CompilationUnit;
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
  mlir::MLIRContext &ctx_;
  const sema::SemaResult &sr_;
  mlir::OpBuilder builder_;
  /// Top-level `mlir::ModuleOp` produced by `lower(...)`. Set on
  /// entry to `visit(CompilationUnit)`; child `nsl.module` ops are
  /// inserted into its body.
  mlir::ModuleOp top_module_;
};

} // namespace nsl::lower

#endif // NSL_LIB_LOWER_ASTTOMLIR_H
