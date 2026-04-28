// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/TransferStmt.h — `transfer_statement`
// (`lang.ebnf §9`; data-model §1.5). Fields: `op` (`WireEq` for
// `=`, `RegColonEq` for `:=` per S3 sequential assignment), `lhs`,
// `rhs`.

#ifndef NSL_AST_TRANSFER_STMT_H
#define NSL_AST_TRANSFER_STMT_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Expr.h"
#include "nsl/AST/Stmt.h"

#include <memory>
#include <utility>

namespace nsl::ast {

class TransferStmt final : public Stmt {
public:
  enum class Op { WireEq, RegColonEq };

  TransferStmt(SourceRange range, Op op, std::unique_ptr<Expr> lhs,
               std::unique_ptr<Expr> rhs)
      : Stmt(NodeKind::NK_TransferStmt, range), op_(op),
        lhs_(std::move(lhs)), rhs_(std::move(rhs)) {}

  [[nodiscard]] Op op() const noexcept { return op_; }
  [[nodiscard]] const Expr *lhs() const noexcept { return lhs_.get(); }
  [[nodiscard]] const Expr *rhs() const noexcept { return rhs_.get(); }

  NSL_AST_NODE_BOILERPLATE(TransferStmt)

private:
  Op op_;
  std::unique_ptr<Expr> lhs_;
  std::unique_ptr<Expr> rhs_;
};

} // namespace nsl::ast

#endif // NSL_AST_TRANSFER_STMT_H
