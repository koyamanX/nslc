// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/RegDecl.h — `register_declaration`
// (`lang.ebnf §6`; data-model §1.4). Fields: `name`, optional
// `width` expression, optional `init` expression. The `init` slot
// is populated iff the source had `= <expr>`.

#ifndef NSL_AST_REG_DECL_H
#define NSL_AST_REG_DECL_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Decl.h"
#include "nsl/AST/Expr.h"

#include <memory>
#include <utility>

namespace nsl::ast {

class RegDecl final : public Decl {
public:
  RegDecl(SourceRange range, Identifier name, std::unique_ptr<Expr> width,
          std::unique_ptr<Expr> init)
      : Decl(NodeKind::NK_RegDecl, range), name_(name),
        width_(std::move(width)), init_(std::move(init)) {}

  [[nodiscard]] Identifier name() const noexcept { return name_; }
  [[nodiscard]] const Expr *width() const noexcept { return width_.get(); }
  [[nodiscard]] const Expr *init() const noexcept { return init_.get(); }

  NSL_AST_NODE_BOILERPLATE(RegDecl)

private:
  Identifier name_;
  std::unique_ptr<Expr> width_;
  std::unique_ptr<Expr> init_;
};

} // namespace nsl::ast

#endif // NSL_AST_REG_DECL_H
