// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/FieldAccessExpr.h — `obj.field`
// (`lang.ebnf §11`; data-model §1.6). Fields: `obj` (the
// containing struct/instance), `field` (the textual field name).

#ifndef NSL_AST_FIELD_ACCESS_EXPR_H
#define NSL_AST_FIELD_ACCESS_EXPR_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Expr.h"

#include <memory>
#include <utility>

namespace nsl::ast {

class FieldAccessExpr final : public Expr {
public:
  FieldAccessExpr(SourceRange range, std::unique_ptr<Expr> obj,
                  Identifier field)
      : Expr(NodeKind::NK_FieldAccessExpr, range), obj_(std::move(obj)),
        field_(field) {}

  [[nodiscard]] const Expr *obj() const noexcept { return obj_.get(); }
  [[nodiscard]] Identifier field() const noexcept { return field_; }

  NSL_AST_NODE_BOILERPLATE(FieldAccessExpr)

private:
  std::unique_ptr<Expr> obj_;
  Identifier field_;
};

} // namespace nsl::ast

#endif // NSL_AST_FIELD_ACCESS_EXPR_H
