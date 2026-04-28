// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/StructCastExpr.h — `struct_cast` expression
// (`lang.ebnf §11`; data-model §1.6). Fields: `typeName` (the
// target struct type), `sub` (the value being cast), `memberPath`
// (dotted member names — empty for whole-struct casts).

#ifndef NSL_AST_STRUCT_CAST_EXPR_H
#define NSL_AST_STRUCT_CAST_EXPR_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Expr.h"

#include <memory>
#include <utility>
#include <vector>

namespace nsl::ast {

class StructCastExpr final : public Expr {
public:
  StructCastExpr(SourceRange range, Identifier typeName,
                 std::unique_ptr<Expr> sub,
                 std::vector<Identifier> memberPath)
      : Expr(NodeKind::NK_StructCastExpr, range), typeName_(typeName),
        sub_(std::move(sub)), memberPath_(std::move(memberPath)) {}

  [[nodiscard]] Identifier typeName() const noexcept { return typeName_; }
  [[nodiscard]] const Expr *sub() const noexcept { return sub_.get(); }
  [[nodiscard]] const std::vector<Identifier> &memberPath() const noexcept {
    return memberPath_;
  }

  NSL_AST_NODE_BOILERPLATE(StructCastExpr)

private:
  Identifier typeName_;
  std::unique_ptr<Expr> sub_;
  std::vector<Identifier> memberPath_;
};

} // namespace nsl::ast

#endif // NSL_AST_STRUCT_CAST_EXPR_H
