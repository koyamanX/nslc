// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/BinaryExpr.h — binary expression
// (`lang.ebnf §11`; data-model §1.6). Closed-set `Op` covering
// every infix operator from §11. Fields: `op`, `lhs`, `rhs`.

#ifndef NSL_AST_BINARY_EXPR_H
#define NSL_AST_BINARY_EXPR_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Expr.h"

#include <memory>
#include <utility>

namespace nsl::ast {

class BinaryExpr final : public Expr {
public:
  /// Closed set of infix operators in `lang.ebnf §11`. Order is
  /// not significant for codegen — Pratt parsing assigns
  /// precedence via the `lib/Parse/PrecedenceTable.h` static
  /// table, not via enumerator value.
  enum class Op {
    // Arithmetic
    Add, Sub, Mul, Div, Mod,
    // Bitwise
    BitAnd, BitOr, BitXor,
    // Shift
    ShiftLeft, ShiftRight,
    // Relational
    Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual,
    // Logical
    LogicalAnd, LogicalOr,
  };

  BinaryExpr(SourceRange range, Op op, std::unique_ptr<Expr> lhs,
             std::unique_ptr<Expr> rhs)
      : Expr(NodeKind::NK_BinaryExpr, range), op_(op),
        lhs_(std::move(lhs)), rhs_(std::move(rhs)) {}

  [[nodiscard]] Op op() const noexcept { return op_; }
  [[nodiscard]] const Expr *lhs() const noexcept { return lhs_.get(); }
  [[nodiscard]] const Expr *rhs() const noexcept { return rhs_.get(); }

  NSL_AST_NODE_BOILERPLATE(BinaryExpr)

private:
  Op op_;
  std::unique_ptr<Expr> lhs_;
  std::unique_ptr<Expr> rhs_;
};

} // namespace nsl::ast

#endif // NSL_AST_BINARY_EXPR_H
