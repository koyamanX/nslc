// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/ConditionalExpr.h — `if (c) a else b`
// expression-position form (`lang.ebnf §11`; data-model §1.6;
// per parser-note N1 — at expression position the form is
// `ConditionalExpr`; at statement position the form is `IfStmt`).
// Fields: `cond`, `thenE`, `elseE`. The `?:` ternary maps here too.

#ifndef NSL_AST_CONDITIONAL_EXPR_H
#define NSL_AST_CONDITIONAL_EXPR_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Expr.h"

#include <memory>
#include <utility>

namespace nsl::ast {

class ConditionalExpr final : public Expr {
public:
  ConditionalExpr(SourceRange range, std::unique_ptr<Expr> cond,
                  std::unique_ptr<Expr> thenE, std::unique_ptr<Expr> elseE)
      : Expr(NodeKind::NK_ConditionalExpr, range), cond_(std::move(cond)),
        thenE_(std::move(thenE)), elseE_(std::move(elseE)) {}

  [[nodiscard]] const Expr *cond() const noexcept { return cond_.get(); }
  [[nodiscard]] const Expr *thenE() const noexcept { return thenE_.get(); }
  [[nodiscard]] const Expr *elseE() const noexcept { return elseE_.get(); }

  NSL_AST_NODE_BOILERPLATE(ConditionalExpr)

private:
  std::unique_ptr<Expr> cond_;
  std::unique_ptr<Expr> thenE_;
  std::unique_ptr<Expr> elseE_;
};

} // namespace nsl::ast

#endif // NSL_AST_CONDITIONAL_EXPR_H
