// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/StateNameDecl.h — `state_name_declaration`
// (`lang.ebnf §6`; data-model §1.4). Field: `names` (declaration
// order list of identifiers — `state_name s1, s2, s3;` produces
// one node with three names).

#ifndef NSL_AST_STATE_NAME_DECL_H
#define NSL_AST_STATE_NAME_DECL_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Decl.h"

#include <utility>
#include <vector>

namespace nsl::ast {

class StateNameDecl final : public Decl {
public:
  StateNameDecl(SourceRange range, std::vector<Identifier> names)
      : Decl(NodeKind::NK_StateNameDecl, range), names_(std::move(names)) {}

  [[nodiscard]] const std::vector<Identifier> &names() const noexcept {
    return names_;
  }

  NSL_AST_NODE_BOILERPLATE(StateNameDecl)

private:
  std::vector<Identifier> names_;
};

} // namespace nsl::ast

#endif // NSL_AST_STATE_NAME_DECL_H
