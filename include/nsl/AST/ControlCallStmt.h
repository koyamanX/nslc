// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/ControlCallStmt.h — `control_call` statement
// form (`lang.ebnf §9`; data-model §1.5; per parser-note N6).
// Fields: `target` (a `ScopedName` such as `inst.finish`), `args`
// (zero or more argument expressions).

#ifndef NSL_AST_CONTROL_CALL_STMT_H
#define NSL_AST_CONTROL_CALL_STMT_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Expr.h"
#include "nsl/AST/Stmt.h"

#include <memory>
#include <utility>
#include <vector>

namespace nsl::ast {

class ControlCallStmt final : public Stmt {
public:
  ControlCallStmt(SourceRange range, ScopedName target,
                  std::vector<std::unique_ptr<Expr>> args)
      : Stmt(NodeKind::NK_ControlCallStmt, range),
        target_(std::move(target)), args_(std::move(args)) {}

  [[nodiscard]] const ScopedName &target() const noexcept { return target_; }
  [[nodiscard]] const std::vector<std::unique_ptr<Expr>> &
  args() const noexcept {
    return args_;
  }

  NSL_AST_NODE_BOILERPLATE(ControlCallStmt)

private:
  ScopedName target_;
  std::vector<std::unique_ptr<Expr>> args_;
};

} // namespace nsl::ast

#endif // NSL_AST_CONTROL_CALL_STMT_H
