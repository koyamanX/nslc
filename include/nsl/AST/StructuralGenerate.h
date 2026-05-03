// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/StructuralGenerate.h — `generate (init; cond;
// step) { ... }` block (`lang.ebnf §8`; data-model §1.5). Fields:
// `init` (the loop-variable name), `cond`, `step`, `body` —
// expanded at M5 (structural-expansion pass). At M2 the block is
// preserved as-written.

#ifndef NSL_AST_STRUCTURAL_GENERATE_H
#define NSL_AST_STRUCTURAL_GENERATE_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Expr.h"
#include "nsl/AST/Stmt.h"

#include <memory>
#include <utility>

namespace nsl::ast {

class StructuralGenerate final : public Stmt {
public:
  StructuralGenerate(SourceRange range, Identifier init,
                     std::unique_ptr<Expr> cond, std::unique_ptr<Expr> step,
                     std::unique_ptr<Stmt> body)
      : Stmt(NodeKind::NK_StructuralGenerate, range), init_(init),
        cond_(std::move(cond)), step_(std::move(step)), body_(std::move(body)) {
  }

  StructuralGenerate(SourceRange range, Identifier init,
                     std::unique_ptr<Expr> initValue,
                     std::unique_ptr<Expr> cond, std::unique_ptr<Expr> step,
                     std::unique_ptr<Stmt> body)
      : Stmt(NodeKind::NK_StructuralGenerate, range), init_(init),
        initValue_(std::move(initValue)), cond_(std::move(cond)),
        step_(std::move(step)), body_(std::move(body)) {}

  [[nodiscard]] Identifier init() const noexcept { return init_; }
  /// The initializer expression `<id> = <expr>` (e.g. `i = 0`).
  /// `M2` parsed-and-discarded this; M3 retains it for later
  /// structural-expansion passes (M5).
  [[nodiscard]] const Expr *initValue() const noexcept {
    return initValue_.get();
  }
  [[nodiscard]] const Expr *cond() const noexcept { return cond_.get(); }
  [[nodiscard]] const Expr *step() const noexcept { return step_.get(); }
  [[nodiscard]] const Stmt *body() const noexcept { return body_.get(); }

  NSL_AST_NODE_BOILERPLATE(StructuralGenerate)

private:
  Identifier init_;
  std::unique_ptr<Expr> initValue_;
  std::unique_ptr<Expr> cond_;
  std::unique_ptr<Expr> step_;
  std::unique_ptr<Stmt> body_;
};

} // namespace nsl::ast

#endif // NSL_AST_STRUCTURAL_GENERATE_H
