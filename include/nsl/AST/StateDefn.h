// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/StateDefn.h — `state_definition`
// (`lang.ebnf §7`; data-model §1.4). Fields: `name`, `body`.

#ifndef NSL_AST_STATE_DEFN_H
#define NSL_AST_STATE_DEFN_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Decl.h"
#include "nsl/AST/Stmt.h"

#include <memory>
#include <utility>

namespace nsl::ast {

class StateDefn final : public Decl {
public:
  StateDefn(SourceRange range, Identifier name, std::unique_ptr<Stmt> body)
      : Decl(NodeKind::NK_StateDefn, range), name_(name),
        body_(std::move(body)) {}

  [[nodiscard]] Identifier name() const noexcept { return name_; }
  [[nodiscard]] const Stmt *body() const noexcept { return body_.get(); }

  NSL_AST_NODE_BOILERPLATE(StateDefn)

private:
  Identifier name_;
  std::unique_ptr<Stmt> body_;
};

} // namespace nsl::ast

#endif // NSL_AST_STATE_DEFN_H
