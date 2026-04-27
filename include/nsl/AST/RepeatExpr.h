// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/RepeatExpr.h — bit-vector replication
// `{N{x}}` (`lang.ebnf §11`; data-model §1.6). Fields: `count` (the
// replication count expression — typically a `LiteralExpr`), `body`
// (the value being replicated).

#ifndef NSL_AST_REPEAT_EXPR_H
#define NSL_AST_REPEAT_EXPR_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Expr.h"

#include <memory>
#include <utility>

namespace nsl::ast {

class RepeatExpr final : public Expr {
public:
  RepeatExpr(SourceRange range, std::unique_ptr<Expr> count,
             std::unique_ptr<Expr> body)
      : Expr(NodeKind::NK_RepeatExpr, range), count_(std::move(count)),
        body_(std::move(body)) {}

  [[nodiscard]] const Expr *count() const noexcept { return count_.get(); }
  [[nodiscard]] const Expr *body() const noexcept { return body_.get(); }

  NSL_AST_NODE_BOILERPLATE(RepeatExpr)

private:
  std::unique_ptr<Expr> count_;
  std::unique_ptr<Expr> body_;
};

} // namespace nsl::ast

#endif // NSL_AST_REPEAT_EXPR_H
