// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/IncDecExpr.h — increment/decrement at expression
// position (`lang.ebnf §11`; data-model §1.6). Fields: `target`,
// `op` (Inc/Dec), `prefix` (true for `++x`, false for `x++`).
// Distinct from `IncDecStmt` (data-model §1.5) — that's the
// statement-position form. The structural shape is identical;
// keeping them as distinct AST node kinds avoids a cross-grammar
// position discriminator inside one node.

#ifndef NSL_AST_INC_DEC_EXPR_H
#define NSL_AST_INC_DEC_EXPR_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Expr.h"

#include <memory>
#include <utility>

namespace nsl::ast {

class IncDecExpr final : public Expr {
public:
  enum class Op { Inc, Dec };

  IncDecExpr(SourceRange range, std::unique_ptr<Expr> target, Op op,
             bool prefix)
      : Expr(NodeKind::NK_IncDecExpr, range), target_(std::move(target)),
        op_(op), prefix_(prefix) {}

  [[nodiscard]] const Expr *target() const noexcept { return target_.get(); }
  [[nodiscard]] Op op() const noexcept { return op_; }
  [[nodiscard]] bool prefix() const noexcept { return prefix_; }

  NSL_AST_NODE_BOILERPLATE(IncDecExpr)

private:
  std::unique_ptr<Expr> target_;
  Op op_;
  bool prefix_;
};

} // namespace nsl::ast

#endif // NSL_AST_INC_DEC_EXPR_H
