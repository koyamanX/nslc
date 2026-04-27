// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/ZeroExtendExpr.h — `W ' value` zero-extend
// expression (`lang.ebnf §11`; data-model §1.6). Same shape as
// `SignExtendExpr` but distinct AST node — the lowering and Sema
// (M3) treat them differently. Fields: `width`, `sub`.

#ifndef NSL_AST_ZERO_EXTEND_EXPR_H
#define NSL_AST_ZERO_EXTEND_EXPR_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Expr.h"

#include <memory>
#include <utility>

namespace nsl::ast {

class ZeroExtendExpr final : public Expr {
public:
  ZeroExtendExpr(SourceRange range, std::unique_ptr<Expr> width,
                 std::unique_ptr<Expr> sub)
      : Expr(NodeKind::NK_ZeroExtendExpr, range), width_(std::move(width)),
        sub_(std::move(sub)) {}

  [[nodiscard]] const Expr *width() const noexcept { return width_.get(); }
  [[nodiscard]] const Expr *sub() const noexcept { return sub_.get(); }

  NSL_AST_NODE_BOILERPLATE(ZeroExtendExpr)

private:
  std::unique_ptr<Expr> width_;
  std::unique_ptr<Expr> sub_;
};

} // namespace nsl::ast

#endif // NSL_AST_ZERO_EXTEND_EXPR_H
