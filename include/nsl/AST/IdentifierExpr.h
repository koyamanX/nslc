// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/IdentifierExpr.h — `identifier` expression
// (`lang.ebnf §11`; data-model §1.6). Field: `name` (a
// `ScopedName` — supports `inst.field` form for parser-note N6
// scope traversal). The `Symbol* resolvedSym` slot mentioned in
// the data-model is M3 Sema's responsibility — at M2 only the
// textual `ScopedName` is stored.

#ifndef NSL_AST_IDENTIFIER_EXPR_H
#define NSL_AST_IDENTIFIER_EXPR_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Expr.h"

#include <utility>

namespace nsl::ast {

class IdentifierExpr final : public Expr {
public:
  IdentifierExpr(SourceRange range, ScopedName name)
      : Expr(NodeKind::NK_IdentifierExpr, range), name_(std::move(name)) {}

  [[nodiscard]] const ScopedName &name() const noexcept { return name_; }

  NSL_AST_NODE_BOILERPLATE(IdentifierExpr)

private:
  ScopedName name_;
};

} // namespace nsl::ast

#endif // NSL_AST_IDENTIFIER_EXPR_H
