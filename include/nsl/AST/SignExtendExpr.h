// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/SignExtendExpr.h — `W # value` sign-extend
// expression (`lang.ebnf §11`; data-model §1.6; per parser-note
// N5 — `#` in expression position post-preprocess; the line-marker
// `#line` form is consumed at the M1/M2 seam and never reaches an
// AST node). Fields: `width` (the resulting bit-width expression),
// `sub` (the value being extended).

#ifndef NSL_AST_SIGN_EXTEND_EXPR_H
#define NSL_AST_SIGN_EXTEND_EXPR_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Expr.h"

#include <memory>
#include <utility>

namespace nsl::ast {

class SignExtendExpr final : public Expr {
public:
  SignExtendExpr(SourceRange range, std::unique_ptr<Expr> width,
                 std::unique_ptr<Expr> sub)
      : Expr(NodeKind::NK_SignExtendExpr, range), width_(std::move(width)),
        sub_(std::move(sub)) {}

  [[nodiscard]] const Expr *width() const noexcept { return width_.get(); }
  [[nodiscard]] const Expr *sub() const noexcept { return sub_.get(); }

  NSL_AST_NODE_BOILERPLATE(SignExtendExpr)

private:
  std::unique_ptr<Expr> width_;
  std::unique_ptr<Expr> sub_;
};

} // namespace nsl::ast

#endif // NSL_AST_SIGN_EXTEND_EXPR_H
