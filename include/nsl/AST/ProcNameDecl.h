// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/ProcNameDecl.h — `procedure_name_declaration`
// (`lang.ebnf §6`; data-model §1.4). Fields: `name`, `regArgs`
// (the parenthesized identifier list — empty for the bare form
// `proc_name foo;`).

#ifndef NSL_AST_PROC_NAME_DECL_H
#define NSL_AST_PROC_NAME_DECL_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Decl.h"

#include <utility>
#include <vector>

namespace nsl::ast {

class ProcNameDecl final : public Decl {
public:
  ProcNameDecl(SourceRange range, Identifier name,
               std::vector<Identifier> regArgs)
      : Decl(NodeKind::NK_ProcNameDecl, range), name_(name),
        regArgs_(std::move(regArgs)) {}

  [[nodiscard]] Identifier name() const noexcept { return name_; }
  [[nodiscard]] const std::vector<Identifier> &regArgs() const noexcept {
    return regArgs_;
  }

  NSL_AST_NODE_BOILERPLATE(ProcNameDecl)

private:
  Identifier name_;
  std::vector<Identifier> regArgs_;
};

} // namespace nsl::ast

#endif // NSL_AST_PROC_NAME_DECL_H
