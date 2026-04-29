// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/ParallelBlock.h — `par { ... }` block
// (`lang.ebnf §8`; data-model §1.5). Field: `items` — body
// statements (executed in parallel by Sema/lowering semantics; the
// AST stores them in textual order).

#ifndef NSL_AST_PARALLEL_BLOCK_H
#define NSL_AST_PARALLEL_BLOCK_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Decl.h"
#include "nsl/AST/Stmt.h"

#include <memory>
#include <utility>
#include <vector>

namespace nsl::ast {

class ParallelBlock final : public Stmt {
public:
  ParallelBlock(SourceRange range, std::vector<std::unique_ptr<Stmt>> items)
      : Stmt(NodeKind::NK_ParallelBlock, range), items_(std::move(items)) {}

  ParallelBlock(SourceRange range, std::vector<std::unique_ptr<Stmt>> items,
                std::vector<std::unique_ptr<Decl>> decls)
      : Stmt(NodeKind::NK_ParallelBlock, range), items_(std::move(items)),
        decls_(std::move(decls)) {}

  [[nodiscard]] const std::vector<std::unique_ptr<Stmt>> &
  items() const noexcept {
    return items_;
  }

  /// Internal-declarations (`state_name`, `first_state`, `state` defn,
  /// `wire`, `reg`, `mem`, `proc_name`, `func_self`, etc.) accepted
  /// inside this parallel block per `lang.ebnf §8`'s
  /// `parallel_block_item ::= internal_declaration | action_statement
  /// | line_marker`. Stored separately from `items_` because Decls
  /// are not Stmts in the AST hierarchy.
  [[nodiscard]] const std::vector<std::unique_ptr<Decl>> &
  decls() const noexcept {
    return decls_;
  }

  NSL_AST_NODE_BOILERPLATE(ParallelBlock)

private:
  std::vector<std::unique_ptr<Stmt>> items_;
  std::vector<std::unique_ptr<Decl>> decls_;
};

} // namespace nsl::ast

#endif // NSL_AST_PARALLEL_BLOCK_H
