// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/ForBlock.h — `for (init; cond; step) { ... }`
// block (`lang.ebnf §8`; data-model §1.5). Fields: `form` — the
// three-clause tuple captured per-clause as parsed AST nodes —
// and `items` for the loop body.

#ifndef NSL_AST_FOR_BLOCK_H
#define NSL_AST_FOR_BLOCK_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Expr.h"
#include "nsl/AST/Stmt.h"

#include <memory>
#include <utility>
#include <vector>

namespace nsl::ast {

/// The `(init; cond; step)` tuple of a `for` block. Each clause
/// MAY be omitted in source — modeled as nullptr when absent. The
/// parser is responsible for the EBNF-required commas vs
/// semicolons (Edge Cases — `for`-loop comma/semicolon shape).
struct ForForm {
  std::unique_ptr<Stmt> init; ///< `IncDecStmt`/`TransferStmt`/...
  std::unique_ptr<Expr> cond; ///< the loop guard expression
  std::unique_ptr<Stmt> step; ///< the per-iteration update statement
};

class ForBlock final : public Stmt {
public:
  ForBlock(SourceRange range, ForForm form,
           std::vector<std::unique_ptr<Stmt>> items)
      : Stmt(NodeKind::NK_ForBlock, range), form_(std::move(form)),
        items_(std::move(items)) {}

  [[nodiscard]] const ForForm &form() const noexcept { return form_; }
  [[nodiscard]] const std::vector<std::unique_ptr<Stmt>> &
  items() const noexcept {
    return items_;
  }

  NSL_AST_NODE_BOILERPLATE(ForBlock)

private:
  ForForm form_;
  std::vector<std::unique_ptr<Stmt>> items_;
};

} // namespace nsl::ast

#endif // NSL_AST_FOR_BLOCK_H
