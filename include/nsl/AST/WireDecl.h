// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/WireDecl.h — `internal_terminal_declaration`
// (wire form; `lang.ebnf §6`; data-model §1.4). Fields: `name`,
// optional `width` expression.

#ifndef NSL_AST_WIRE_DECL_H
#define NSL_AST_WIRE_DECL_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Decl.h"
#include "nsl/AST/Expr.h"

#include <memory>
#include <utility>

namespace nsl::ast {

class WireDecl final : public Decl {
public:
  WireDecl(SourceRange range, Identifier name, std::unique_ptr<Expr> width)
      : Decl(NodeKind::NK_WireDecl, range), name_(name),
        width_(std::move(width)) {}

  [[nodiscard]] Identifier name() const noexcept { return name_; }
  [[nodiscard]] const Expr *width() const noexcept { return width_.get(); }

  NSL_AST_NODE_BOILERPLATE(WireDecl)

private:
  Identifier name_;
  std::unique_ptr<Expr> width_;
};

} // namespace nsl::ast

#endif // NSL_AST_WIRE_DECL_H
