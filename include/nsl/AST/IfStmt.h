// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/IfStmt.h — statement-position `if/else`
// (`lang.ebnf §8`; data-model §1.5; per parser-note N1 — at
// statement position the form is `IfStmt`, at expression position
// the form is `ConditionalExpr`). Fields: `cond`, `thenBr`,
// optional `elseBr`.

#ifndef NSL_AST_IF_STMT_H
#define NSL_AST_IF_STMT_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Expr.h"
#include "nsl/AST/Stmt.h"

#include <memory>
#include <utility>

namespace nsl::ast {

class IfStmt final : public Stmt {
public:
  IfStmt(SourceRange range, std::unique_ptr<Expr> cond,
         std::unique_ptr<Stmt> thenBr, std::unique_ptr<Stmt> elseBr)
      : Stmt(NodeKind::NK_IfStmt, range), cond_(std::move(cond)),
        thenBr_(std::move(thenBr)), elseBr_(std::move(elseBr)) {}

  [[nodiscard]] const Expr *cond() const noexcept { return cond_.get(); }
  [[nodiscard]] const Stmt *thenBr() const noexcept { return thenBr_.get(); }
  /// nullptr when no `else` arm is present.
  [[nodiscard]] const Stmt *elseBr() const noexcept { return elseBr_.get(); }

  NSL_AST_NODE_BOILERPLATE(IfStmt)

private:
  std::unique_ptr<Expr> cond_;
  std::unique_ptr<Stmt> thenBr_;
  std::unique_ptr<Stmt> elseBr_;
};

} // namespace nsl::ast

#endif // NSL_AST_IF_STMT_H
