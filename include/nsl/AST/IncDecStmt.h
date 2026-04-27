// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/IncDecStmt.h — increment/decrement at statement
// position (`lang.ebnf §9`; data-model §1.5). Fields: `target`,
// `kind` (Inc/Dec), `prefix` (true for `++x`, false for `x++`).
// Distinct from `IncDecExpr` (data-model §1.6) which lives in
// expression position.

#ifndef NSL_AST_INC_DEC_STMT_H
#define NSL_AST_INC_DEC_STMT_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Expr.h"
#include "nsl/AST/Stmt.h"

#include <memory>
#include <utility>

namespace nsl::ast {

class IncDecStmt final : public Stmt {
public:
  enum class Op { Inc, Dec };

  IncDecStmt(SourceRange range, std::unique_ptr<Expr> target, Op op,
             bool prefix)
      : Stmt(NodeKind::NK_IncDecStmt, range), target_(std::move(target)),
        op_(op), prefix_(prefix) {}

  [[nodiscard]] const Expr *target() const noexcept { return target_.get(); }
  [[nodiscard]] Op op() const noexcept { return op_; }
  [[nodiscard]] bool prefix() const noexcept { return prefix_; }

  NSL_AST_NODE_BOILERPLATE(IncDecStmt)

private:
  std::unique_ptr<Expr> target_;
  Op op_;
  bool prefix_;
};

} // namespace nsl::ast

#endif // NSL_AST_INC_DEC_STMT_H
