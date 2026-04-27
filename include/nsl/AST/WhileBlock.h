// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/WhileBlock.h — `while (cond) { ... }` block
// (`lang.ebnf §8`; data-model §1.5). Fields: `cond`, `items` —
// sequenced statements forming the loop body.

#ifndef NSL_AST_WHILE_BLOCK_H
#define NSL_AST_WHILE_BLOCK_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Expr.h"
#include "nsl/AST/Stmt.h"

#include <memory>
#include <utility>
#include <vector>

namespace nsl::ast {

class WhileBlock final : public Stmt {
public:
  WhileBlock(SourceRange range, std::unique_ptr<Expr> cond,
             std::vector<std::unique_ptr<Stmt>> items)
      : Stmt(NodeKind::NK_WhileBlock, range), cond_(std::move(cond)),
        items_(std::move(items)) {}

  [[nodiscard]] const Expr *cond() const noexcept { return cond_.get(); }
  [[nodiscard]] const std::vector<std::unique_ptr<Stmt>> &
  items() const noexcept {
    return items_;
  }

  NSL_AST_NODE_BOILERPLATE(WhileBlock)

private:
  std::unique_ptr<Expr> cond_;
  std::vector<std::unique_ptr<Stmt>> items_;
};

} // namespace nsl::ast

#endif // NSL_AST_WHILE_BLOCK_H
