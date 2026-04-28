// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/DelayTaskStmt.h — `_delay(N)` system task at
// statement position (`lang.ebnf §10`; data-model §1.5). Field:
// `count` (the delay-count expression).

#ifndef NSL_AST_DELAY_TASK_STMT_H
#define NSL_AST_DELAY_TASK_STMT_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Expr.h"
#include "nsl/AST/Stmt.h"

#include <memory>
#include <utility>

namespace nsl::ast {

class DelayTaskStmt final : public Stmt {
public:
  DelayTaskStmt(SourceRange range, std::unique_ptr<Expr> count)
      : Stmt(NodeKind::NK_DelayTaskStmt, range), count_(std::move(count)) {}

  [[nodiscard]] const Expr *count() const noexcept { return count_.get(); }

  NSL_AST_NODE_BOILERPLATE(DelayTaskStmt)

private:
  std::unique_ptr<Expr> count_;
};

} // namespace nsl::ast

#endif // NSL_AST_DELAY_TASK_STMT_H
