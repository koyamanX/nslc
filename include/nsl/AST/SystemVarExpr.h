// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/SystemVarExpr.h — `_random` / `_time` (no-parens
// system-variable form; `lang.ebnf §11`; data-model §1.6; per
// parser-note N11 (b)). Field: `kind` ∈ {Random, Time}.
// Parenthesized `_time()` etc. land as `SystemTaskStmt` at
// statement position; the no-parens form lives in expression
// position as this node.

#ifndef NSL_AST_SYSTEM_VAR_EXPR_H
#define NSL_AST_SYSTEM_VAR_EXPR_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Expr.h"

namespace nsl::ast {

class SystemVarExpr final : public Expr {
public:
  enum class Var { Random, Time };

  SystemVarExpr(SourceRange range, Var var)
      : Expr(NodeKind::NK_SystemVarExpr, range), var_(var) {}

  [[nodiscard]] Var var() const noexcept { return var_; }

  NSL_AST_NODE_BOILERPLATE(SystemVarExpr)

private:
  Var var_;
};

} // namespace nsl::ast

#endif // NSL_AST_SYSTEM_VAR_EXPR_H
