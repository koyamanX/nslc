// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/VariableDecl.h — `internal_declaration` variable
// form (`lang.ebnf §6`; data-model §1.4). Fields: `name`, optional
// `width` expression. Distinct AST node from `WireDecl` even though
// the field shape matches — Sema (M3) treats `variable` differently
// from `wire`.

#ifndef NSL_AST_VARIABLE_DECL_H
#define NSL_AST_VARIABLE_DECL_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Decl.h"
#include "nsl/AST/Expr.h"

#include <memory>
#include <utility>

namespace nsl::ast {

class VariableDecl final : public Decl {
public:
  VariableDecl(SourceRange range, Identifier name,
               std::unique_ptr<Expr> width)
      : Decl(NodeKind::NK_VariableDecl, range), name_(name),
        width_(std::move(width)) {}

  [[nodiscard]] Identifier name() const noexcept { return name_; }
  [[nodiscard]] const Expr *width() const noexcept { return width_.get(); }

  NSL_AST_NODE_BOILERPLATE(VariableDecl)

private:
  Identifier name_;
  std::unique_ptr<Expr> width_;
};

} // namespace nsl::ast

#endif // NSL_AST_VARIABLE_DECL_H
