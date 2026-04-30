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
#include "nsl/AST/Decl.h"
#include "nsl/AST/Stmt.h"

#include <memory>
#include <utility>
#include <vector>

namespace nsl::ast {

class SeqBlock final : public Stmt {
public:
  SeqBlock(SourceRange range, std::vector<std::unique_ptr<Stmt>> items)
      : Stmt(NodeKind::NK_SeqBlock, range), items_(std::move(items)) {}

  SeqBlock(SourceRange range, std::vector<std::unique_ptr<Stmt>> items,
           std::vector<std::unique_ptr<Decl>> decls)
      : Stmt(NodeKind::NK_SeqBlock, range), items_(std::move(items)),
        decls_(std::move(decls)) {}

  [[nodiscard]] const std::vector<std::unique_ptr<Stmt>> &
  items() const noexcept {
    return items_;
  }

  /// Internal-declarations (`reg`, `wire`, `mem`, `variable`,
  /// `integer`, `proc_name`, `state_name`, `first_state`,
  /// `func_self`, `label_name`) parsed inside this seq block per
  /// `lang.ebnf §8` `seq_block_item`. Stored separately from
  /// `items_` because Decls are not Stmts.
  [[nodiscard]] const std::vector<std::unique_ptr<Decl>> &
  decls() const noexcept {
    return decls_;
  }

  NSL_AST_NODE_BOILERPLATE(SeqBlock)

private:
  std::vector<std::unique_ptr<Stmt>> items_;
  std::vector<std::unique_ptr<Decl>> decls_;
};

} // namespace nsl::ast

#endif // NSL_AST_SEQ_BLOCK_H
