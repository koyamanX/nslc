// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/FuncSelfDecl.h — `control_internal_declaration`
// (func_self form; `lang.ebnf §6`; data-model §1.4). Fields:
// `name`, optional `dummyArgs` (parenthesized identifier list),
// optional `returnTerminal` (`: identifier`).

#ifndef NSL_AST_FUNC_SELF_DECL_H
#define NSL_AST_FUNC_SELF_DECL_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Decl.h"

#include <utility>
#include <vector>

namespace nsl::ast {

class FuncSelfDecl final : public Decl {
public:
  FuncSelfDecl(SourceRange range, Identifier name,
               std::vector<Identifier> dummyArgs,
               Identifier returnTerminal)
      : Decl(NodeKind::NK_FuncSelfDecl, range), name_(name),
        dummyArgs_(std::move(dummyArgs)),
        returnTerminal_(returnTerminal) {}

  [[nodiscard]] Identifier name() const noexcept { return name_; }
  [[nodiscard]] const std::vector<Identifier> &dummyArgs() const noexcept {
    return dummyArgs_;
  }
  /// Empty `StringRef` when no return-terminal annotation was given.
  [[nodiscard]] Identifier returnTerminal() const noexcept {
    return returnTerminal_;
  }

  NSL_AST_NODE_BOILERPLATE(FuncSelfDecl)

private:
  Identifier name_;
  std::vector<Identifier> dummyArgs_;
  Identifier returnTerminal_;
};

} // namespace nsl::ast

#endif // NSL_AST_FUNC_SELF_DECL_H
