// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/AltBlock.h — `alt { cond : stmt; ... }` block
// (`lang.ebnf §8`; data-model §1.5). Fields: `cases` (one entry
// per condition arm — guard expression + body statement), optional
// `elseCase` (the trailing `else: ...` arm). `alt` semantics
// (S13: priority-ordered selection) is M3 Sema's responsibility.

#ifndef NSL_AST_ALT_BLOCK_H
#define NSL_AST_ALT_BLOCK_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Expr.h"
#include "nsl/AST/Stmt.h"

#include <memory>
#include <utility>
#include <vector>

namespace nsl::ast {

/// One arm of an `alt` or `any` block: a guard + body. Used by
/// `AltBlock` and `AnyBlock`.
struct CondCase {
  std::unique_ptr<Expr> cond;
  std::unique_ptr<Stmt> body;
};

class AltBlock final : public Stmt {
public:
  AltBlock(SourceRange range, std::vector<CondCase> cases,
           std::unique_ptr<Stmt> elseCase)
      : Stmt(NodeKind::NK_AltBlock, range), cases_(std::move(cases)),
        elseCase_(std::move(elseCase)) {}

  [[nodiscard]] const std::vector<CondCase> &cases() const noexcept {
    return cases_;
  }
  /// nullptr when no `else:` arm is present.
  [[nodiscard]] const Stmt *elseCase() const noexcept {
    return elseCase_.get();
  }

  NSL_AST_NODE_BOILERPLATE(AltBlock)

private:
  std::vector<CondCase> cases_;
  std::unique_ptr<Stmt> elseCase_;
};

} // namespace nsl::ast

#endif // NSL_AST_ALT_BLOCK_H
