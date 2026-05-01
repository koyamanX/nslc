// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/ASTToMLIR.h — private internal header for the AST → nsl
// MLIR dialect lowering visitor (M5, layer 8a).
//
// **Specification anchors**:
//   - `specs/008-m5-structural-passes/spec.md` FR-004, FR-005,
//     FR-006, FR-007, FR-008.
//   - `specs/008-m5-structural-passes/data-model.md` §1 — class
//     shape + invariants.
//   - `specs/008-m5-structural-passes/research.md` §4 — Clarifications
//     Q4 → Option A: single-pass walk with MLIR `SymbolTable` lazy
//     resolution.
//
// **Internal-only header.** Per `contracts/lower-api.contract.md`
// §3, the `ASTToMLIR` class itself is NOT part of the public M5
// surface. Consumers use the `nsl::lower::astToMLIR(...)` free
// function from `include/nsl/Lower/Lower.h`.

#ifndef NSL_LIB_LOWER_ASTTOMLIR_H
#define NSL_LIB_LOWER_ASTTOMLIR_H

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
/// contains one `nsl.module` per `ast::ModuleBlock`. Every emitted
/// op carries a non-`UnknownLoc` `mlir::Location` resolvable to the
/// AST `SourceRange` (FR-008).
///
/// Borrows the `MLIRContext`, the `CompilationUnit`, and the
/// `SemaResult` for its lifetime — does NOT extend their lifetime
/// past the `lower(...)` call.
class ASTToMLIR {
public:
  ASTToMLIR(mlir::MLIRContext &ctx, const sema::SemaResult &sr);

  /// Public entry point. Returns a non-null `OwningOpRef` on success
  /// and a failed `OwningOpRef` on internal invariant violation
  /// (after posting an ICE diagnostic via the active
  /// `mlir::DiagnosticEngine`).
  ///
  /// At Phase 2 (this scaffolding), the implementation walks the
  /// `CompilationUnit` and produces an empty `mlir::ModuleOp{}`.
  /// Per-AST-node `visit()` overrides land in Phase 3 (US1) per
  /// `tasks.md` T047–T056.
  mlir::OwningOpRef<mlir::ModuleOp>
  lower(const ast::CompilationUnit &cu);

private:
  mlir::MLIRContext &ctx_;
  const sema::SemaResult &sr_;
  mlir::OpBuilder builder_;
  // Future maps (populated by US1 implementation):
  //   llvm::DenseMap<const sema::Symbol*, mlir::Value> valueMap_;
  //   llvm::DenseMap<const ast::Identifier*, mlir::FlatSymbolRefAttr>
  //       symbolRefs_;
  // Both are key-lookup-only; never iterated during emission to
  // preserve Principle V determinism (research §13 audit rule).
};

} // namespace nsl::lower

#endif // NSL_LIB_LOWER_ASTTOMLIR_H
