// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/StructDecl.h — `struct_declaration`
// (`lang.ebnf §3`; data-model §1.2). Fields: `name` (the struct
// type name) and `members` (one entry per declared field; each
// carries an identifier and an optional width expression).

#ifndef NSL_AST_STRUCT_DECL_H
#define NSL_AST_STRUCT_DECL_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Decl.h"
#include "nsl/AST/Expr.h"

#include <memory>
#include <utility>
#include <vector>

namespace nsl::ast {

/// One field of a struct: identifier + optional width.
struct StructMember {
  Identifier name;
  std::unique_ptr<Expr> width; ///< nullptr when unspecified
};

class StructDecl final : public Decl {
public:
  StructDecl(SourceRange range, Identifier name,
             std::vector<StructMember> members)
      : Decl(NodeKind::NK_StructDecl, range), name_(name),
        members_(std::move(members)) {}

  [[nodiscard]] Identifier name() const noexcept { return name_; }
  [[nodiscard]] const std::vector<StructMember> &members() const noexcept {
    return members_;
  }

  NSL_AST_NODE_BOILERPLATE(StructDecl)

private:
  Identifier name_;
  std::vector<StructMember> members_;
};

} // namespace nsl::ast

#endif // NSL_AST_STRUCT_DECL_H
