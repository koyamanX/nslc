// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/UnaryExpr.h — unary expression
// (`lang.ebnf §11`; data-model §1.6). Includes negation, bitwise
// NOT, logical NOT, and the N2 reduction-prefix forms (`&` `|`
// `^` with no left operand). Sign-extend `#` per N5 has its own
// AST node (`SignExtendExpr`). Fields: `op`, `sub`.

#ifndef NSL_AST_UNARY_EXPR_H
#define NSL_AST_UNARY_EXPR_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Expr.h"

#include <memory>
#include <utility>

namespace nsl::ast {

class UnaryExpr final : public Expr {
public:
  /// Closed set of unary-prefix operators in the NSL grammar.
  enum class Op {
    Neg,        ///< `-x`
    Plus,       ///< `+x`
    BitNot,     ///< `~x`
    LogicalNot, ///< `!x`
    ReduceAnd,  ///< `&x`  (N2 reduction)
    ReduceOr,   ///< `|x`  (N2 reduction)
    ReduceXor,  ///< `^x`  (N2 reduction)
  };

  UnaryExpr(SourceRange range, Op op, std::unique_ptr<Expr> sub)
      : Expr(NodeKind::NK_UnaryExpr, range), op_(op), sub_(std::move(sub)) {}

  [[nodiscard]] Op op() const noexcept { return op_; }
  [[nodiscard]] const Expr *sub() const noexcept { return sub_.get(); }

  NSL_AST_NODE_BOILERPLATE(UnaryExpr)

private:
  Op op_;
  std::unique_ptr<Expr> sub_;
};

} // namespace nsl::ast

#endif // NSL_AST_UNARY_EXPR_H
