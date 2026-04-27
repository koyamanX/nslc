// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/AnyBlock.h — `any { cond : stmt; ... }` block
// (`lang.ebnf §8`; data-model §1.5). Same shape as `AltBlock` but
// distinct AST node — `any` semantics (parallel evaluation, no
// priority order) differs from `alt` and Sema (M3) treats them
// separately. The shared `CondCase` arm type lives in `AltBlock.h`.

#ifndef NSL_AST_ANY_BLOCK_H
#define NSL_AST_ANY_BLOCK_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/AltBlock.h" // for CondCase
#include "nsl/AST/Stmt.h"

#include <memory>
#include <utility>
#include <vector>

namespace nsl::ast {

class AnyBlock final : public Stmt {
public:
  AnyBlock(SourceRange range, std::vector<CondCase> cases,
           std::unique_ptr<Stmt> elseCase)
      : Stmt(NodeKind::NK_AnyBlock, range), cases_(std::move(cases)),
        elseCase_(std::move(elseCase)) {}

  [[nodiscard]] const std::vector<CondCase> &cases() const noexcept {
    return cases_;
  }
  /// nullptr when no `else:` arm is present.
  [[nodiscard]] const Stmt *elseCase() const noexcept {
    return elseCase_.get();
  }

  NSL_AST_NODE_BOILERPLATE(AnyBlock)

private:
  std::vector<CondCase> cases_;
  std::unique_ptr<Stmt> elseCase_;
};

} // namespace nsl::ast

#endif // NSL_AST_ANY_BLOCK_H
