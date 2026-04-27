// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/ParallelBlock.h — `par { ... }` block
// (`lang.ebnf §8`; data-model §1.5). Field: `items` — body
// statements (executed in parallel by Sema/lowering semantics; the
// AST stores them in textual order).

#ifndef NSL_AST_PARALLEL_BLOCK_H
#define NSL_AST_PARALLEL_BLOCK_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Stmt.h"

#include <memory>
#include <utility>
#include <vector>

namespace nsl::ast {

class ParallelBlock final : public Stmt {
public:
  ParallelBlock(SourceRange range, std::vector<std::unique_ptr<Stmt>> items)
      : Stmt(NodeKind::NK_ParallelBlock, range), items_(std::move(items)) {}

  [[nodiscard]] const std::vector<std::unique_ptr<Stmt>> &
  items() const noexcept {
    return items_;
  }

  NSL_AST_NODE_BOILERPLATE(ParallelBlock)

private:
  std::vector<std::unique_ptr<Stmt>> items_;
};

} // namespace nsl::ast

#endif // NSL_AST_PARALLEL_BLOCK_H
