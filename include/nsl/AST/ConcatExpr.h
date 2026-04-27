// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/ConcatExpr.h — bit-vector concatenation
// `{a, b, c}` and the LHS-form `.{a, b, c} = x;`
// (`lang.ebnf §11`; data-model §1.6; per parser-note N3 for the
// dot-prefix LHS form). Field: `parts` (declaration order — same
// shape for both RHS and LHS forms; the LHS-form discriminator
// lives in the enclosing `TransferStmt`).

#ifndef NSL_AST_CONCAT_EXPR_H
#define NSL_AST_CONCAT_EXPR_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Expr.h"

#include <memory>
#include <utility>
#include <vector>

namespace nsl::ast {

class ConcatExpr final : public Expr {
public:
  ConcatExpr(SourceRange range, std::vector<std::unique_ptr<Expr>> parts)
      : Expr(NodeKind::NK_ConcatExpr, range), parts_(std::move(parts)) {}

  [[nodiscard]] const std::vector<std::unique_ptr<Expr>> &
  parts() const noexcept {
    return parts_;
  }

  NSL_AST_NODE_BOILERPLATE(ConcatExpr)

private:
  std::vector<std::unique_ptr<Expr>> parts_;
};

} // namespace nsl::ast

#endif // NSL_AST_CONCAT_EXPR_H
