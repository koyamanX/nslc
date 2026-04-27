// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/ReturnStmt.h — `return_statement`
// (`lang.ebnf §9`; data-model §1.5). Field: optional `value`.

#ifndef NSL_AST_RETURN_STMT_H
#define NSL_AST_RETURN_STMT_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Expr.h"
#include "nsl/AST/Stmt.h"

#include <memory>
#include <utility>

namespace nsl::ast {

class ReturnStmt final : public Stmt {
public:
  ReturnStmt(SourceRange range, std::unique_ptr<Expr> value)
      : Stmt(NodeKind::NK_ReturnStmt, range), value_(std::move(value)) {}

  /// nullptr for the bare `return;` form.
  [[nodiscard]] const Expr *value() const noexcept { return value_.get(); }

  NSL_AST_NODE_BOILERPLATE(ReturnStmt)

private:
  std::unique_ptr<Expr> value_;
};

} // namespace nsl::ast

#endif // NSL_AST_RETURN_STMT_H
