// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/SliceExpr.h — bit-slice expression `x[hi]` /
// `x[hi:lo]` (`lang.ebnf §11`; data-model §1.6). Fields: `sub`
// (the value being sliced), `hi` (upper index), optional `lo`
// (lower index — nullptr for the single-index form). Per S15
// the indices must be compile-time evaluable; that's M3 Sema's
// check, not the parser's.

#ifndef NSL_AST_SLICE_EXPR_H
#define NSL_AST_SLICE_EXPR_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Expr.h"

#include <memory>
#include <utility>

namespace nsl::ast {

class SliceExpr final : public Expr {
public:
  SliceExpr(SourceRange range, std::unique_ptr<Expr> sub,
            std::unique_ptr<Expr> hi, std::unique_ptr<Expr> lo)
      : Expr(NodeKind::NK_SliceExpr, range), sub_(std::move(sub)),
        hi_(std::move(hi)), lo_(std::move(lo)) {}

  [[nodiscard]] const Expr *sub() const noexcept { return sub_.get(); }
  [[nodiscard]] const Expr *hi() const noexcept { return hi_.get(); }
  /// nullptr for the single-index form `x[hi]`.
  [[nodiscard]] const Expr *lo() const noexcept { return lo_.get(); }

  NSL_AST_NODE_BOILERPLATE(SliceExpr)

private:
  std::unique_ptr<Expr> sub_;
  std::unique_ptr<Expr> hi_;
  std::unique_ptr<Expr> lo_;
};

} // namespace nsl::ast

#endif // NSL_AST_SLICE_EXPR_H
