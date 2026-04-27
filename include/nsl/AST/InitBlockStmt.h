// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/InitBlockStmt.h — `_init { ... }` block
// (`lang.ebnf §10`; data-model §1.5). Field: `items` — sequenced
// statements run at simulation reset.

#ifndef NSL_AST_INIT_BLOCK_STMT_H
#define NSL_AST_INIT_BLOCK_STMT_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Stmt.h"

#include <memory>
#include <utility>
#include <vector>

namespace nsl::ast {

class InitBlockStmt final : public Stmt {
public:
  InitBlockStmt(SourceRange range, std::vector<std::unique_ptr<Stmt>> items)
      : Stmt(NodeKind::NK_InitBlockStmt, range), items_(std::move(items)) {}

  [[nodiscard]] const std::vector<std::unique_ptr<Stmt>> &
  items() const noexcept {
    return items_;
  }

  NSL_AST_NODE_BOILERPLATE(InitBlockStmt)

private:
  std::vector<std::unique_ptr<Stmt>> items_;
};

} // namespace nsl::ast

#endif // NSL_AST_INIT_BLOCK_STMT_H
