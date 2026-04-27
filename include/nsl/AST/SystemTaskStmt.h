// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/SystemTaskStmt.h — system-task call at statement
// position (`lang.ebnf §10`; data-model §1.5; per parser-note
// N11(a)). Fields: `name` (the source-level name including the
// leading underscore — e.g., `"_display"`, `"_finish"`,
// `"_init"`, `"_delay"`), `args` (zero or more argument
// expressions).

#ifndef NSL_AST_SYSTEM_TASK_STMT_H
#define NSL_AST_SYSTEM_TASK_STMT_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Expr.h"
#include "nsl/AST/Stmt.h"

#include <memory>
#include <utility>
#include <vector>

namespace nsl::ast {

class SystemTaskStmt final : public Stmt {
public:
  SystemTaskStmt(SourceRange range, Identifier name,
                 std::vector<std::unique_ptr<Expr>> args)
      : Stmt(NodeKind::NK_SystemTaskStmt, range), name_(name),
        args_(std::move(args)) {}

  /// Includes the leading underscore (e.g., `"_display"`). Sema
  /// (M3) classifies into the closed system-task set.
  [[nodiscard]] Identifier name() const noexcept { return name_; }
  [[nodiscard]] const std::vector<std::unique_ptr<Expr>> &
  args() const noexcept {
    return args_;
  }

  NSL_AST_NODE_BOILERPLATE(SystemTaskStmt)

private:
  Identifier name_;
  std::vector<std::unique_ptr<Expr>> args_;
};

} // namespace nsl::ast

#endif // NSL_AST_SYSTEM_TASK_STMT_H
