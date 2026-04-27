// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/CallExpr.h — `function_call` expression
// (`lang.ebnf §11`; data-model §1.6). Fields: `target` (a
// `ScopedName` — supports `inst.func` form per N7), `args`
// (zero or more argument expressions).

#ifndef NSL_AST_CALL_EXPR_H
#define NSL_AST_CALL_EXPR_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Expr.h"

#include <memory>
#include <utility>
#include <vector>

namespace nsl::ast {

class CallExpr final : public Expr {
public:
  CallExpr(SourceRange range, ScopedName target,
           std::vector<std::unique_ptr<Expr>> args)
      : Expr(NodeKind::NK_CallExpr, range), target_(std::move(target)),
        args_(std::move(args)) {}

  [[nodiscard]] const ScopedName &target() const noexcept { return target_; }
  [[nodiscard]] const std::vector<std::unique_ptr<Expr>> &
  args() const noexcept {
    return args_;
  }

  NSL_AST_NODE_BOILERPLATE(CallExpr)

private:
  ScopedName target_;
  std::vector<std::unique_ptr<Expr>> args_;
};

} // namespace nsl::ast

#endif // NSL_AST_CALL_EXPR_H
