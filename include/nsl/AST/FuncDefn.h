// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/FuncDefn.h — `function_definition`
// (`lang.ebnf §7`; data-model §1.4). Fields: `name` (a
// `ScopedName` — single identifier OR `inst.id` per parser-note
// N7's dotted-`func` def for submodule-out), `body` (the parsed
// action statement).

#ifndef NSL_AST_FUNC_DEFN_H
#define NSL_AST_FUNC_DEFN_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Decl.h"
#include "nsl/AST/Stmt.h"

#include <memory>
#include <utility>

namespace nsl::ast {

class FuncDefn final : public Decl {
public:
  FuncDefn(SourceRange range, ScopedName name, std::unique_ptr<Stmt> body)
      : Decl(NodeKind::NK_FuncDefn, range), name_(std::move(name)),
        body_(std::move(body)) {}

  [[nodiscard]] const ScopedName &name() const noexcept { return name_; }
  [[nodiscard]] const Stmt *body() const noexcept { return body_.get(); }

  NSL_AST_NODE_BOILERPLATE(FuncDefn)

private:
  ScopedName name_;
  std::unique_ptr<Stmt> body_;
};

} // namespace nsl::ast

#endif // NSL_AST_FUNC_DEFN_H
