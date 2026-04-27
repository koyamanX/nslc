// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/Expr.h
//
// `Expr` — abstract mid-level base for expressions (data-model §1.1,
// §1.6). Concrete subclasses live in their own per-kind headers
// (Principle II §3 exception): `LiteralExpr`, `BinaryExpr`,
// `UnaryExpr`, `SignExtendExpr`, `CallExpr`, etc.
//
// `Expr` carries the `TypeRef inferredType_` slot (FR-004,
// data-model §1.1): nullptr at M2; M3 Sema fills it during name-
// resolution + width-inference. The setter is intentionally NOT
// `const` — Sema mutates it post-construction.

#ifndef NSL_AST_EXPR_H
#define NSL_AST_EXPR_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Type.h"

namespace nsl::ast {

/// Abstract mid-level base for expression AST nodes.
class Expr : public ASTNode {
public:
  /// Inferred type of this expression. nullptr until M3 Sema runs.
  /// Read-only access; mutation goes through `setInferredType()` so
  /// the slot is grep-discoverable.
  [[nodiscard]] TypeRef inferredType() const noexcept {
    return inferredType_;
  }

  /// Sema's hook to fill the slot. Idempotent in practice (Sema
  /// resolves each `Expr` exactly once); but the setter does not
  /// enforce this — that's a Sema-level invariant.
  void setInferredType(TypeRef t) noexcept { inferredType_ = t; }

protected:
  using ASTNode::ASTNode;

private:
  TypeRef inferredType_ = nullptr;
};

} // namespace nsl::ast

#endif // NSL_AST_EXPR_H
