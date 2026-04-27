// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/FirstStateDecl.h — `first_state_declaration`
// (`lang.ebnf §6`; data-model §1.4). Field: `target` — the state
// name designated as the FSM's initial state.

#ifndef NSL_AST_FIRST_STATE_DECL_H
#define NSL_AST_FIRST_STATE_DECL_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Decl.h"

namespace nsl::ast {

class FirstStateDecl final : public Decl {
public:
  FirstStateDecl(SourceRange range, Identifier target)
      : Decl(NodeKind::NK_FirstStateDecl, range), target_(target) {}

  [[nodiscard]] Identifier target() const noexcept { return target_; }

  NSL_AST_NODE_BOILERPLATE(FirstStateDecl)

private:
  Identifier target_;
};

} // namespace nsl::ast

#endif // NSL_AST_FIRST_STATE_DECL_H
