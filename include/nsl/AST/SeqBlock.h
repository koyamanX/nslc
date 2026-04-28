// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/SeqBlock.h — `seq { ... }` block
// (`lang.ebnf §8`; data-model §1.5). Field: `items` — sequenced
// statements (one cycle per element by S7 semantics; resolution
// is M3 Sema's). `seq` is the sequential counterpart to
// `ParallelBlock`.

#ifndef NSL_AST_SEQ_BLOCK_H
#define NSL_AST_SEQ_BLOCK_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Stmt.h"

#include <memory>
#include <utility>
#include <vector>

namespace nsl::ast {

class SeqBlock final : public Stmt {
public:
  SeqBlock(SourceRange range, std::vector<std::unique_ptr<Stmt>> items)
      : Stmt(NodeKind::NK_SeqBlock, range), items_(std::move(items)) {}

  [[nodiscard]] const std::vector<std::unique_ptr<Stmt>> &
  items() const noexcept {
    return items_;
  }

  NSL_AST_NODE_BOILERPLATE(SeqBlock)

private:
  std::vector<std::unique_ptr<Stmt>> items_;
};

} // namespace nsl::ast

#endif // NSL_AST_SEQ_BLOCK_H
