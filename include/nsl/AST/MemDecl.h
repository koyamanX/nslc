// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/MemDecl.h — `memory_declaration`
// (`lang.ebnf §6`; data-model §1.4). Fields: `name`, `depth`,
// `width`, optional `init` (parenthesized list of initial values
// stored as one Expr per element).

#ifndef NSL_AST_MEM_DECL_H
#define NSL_AST_MEM_DECL_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Decl.h"
#include "nsl/AST/Expr.h"

#include <memory>
#include <utility>
#include <vector>

namespace nsl::ast {

class MemDecl final : public Decl {
public:
  MemDecl(SourceRange range, Identifier name, std::unique_ptr<Expr> depth,
          std::unique_ptr<Expr> width, std::vector<std::unique_ptr<Expr>> init)
      : Decl(NodeKind::NK_MemDecl, range), name_(name),
        depth_(std::move(depth)), width_(std::move(width)),
        init_(std::move(init)) {}

  [[nodiscard]] Identifier name() const noexcept { return name_; }
  [[nodiscard]] const Expr *depth() const noexcept { return depth_.get(); }
  [[nodiscard]] const Expr *width() const noexcept { return width_.get(); }
  [[nodiscard]] const std::vector<std::unique_ptr<Expr>> &
  init() const noexcept {
    return init_;
  }

  NSL_AST_NODE_BOILERPLATE(MemDecl)

private:
  Identifier name_;
  std::unique_ptr<Expr> depth_;
  std::unique_ptr<Expr> width_;
  std::vector<std::unique_ptr<Expr>> init_;
};

} // namespace nsl::ast

#endif // NSL_AST_MEM_DECL_H
