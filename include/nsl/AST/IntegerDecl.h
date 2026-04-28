// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/IntegerDecl.h — `internal_declaration` integer
// form (`lang.ebnf §6`; data-model §1.4). Field: `name`. Width is
// implicit (per the EBNF) — a host-int sized counter, distinct
// from `wire`/`variable`/`reg`.

#ifndef NSL_AST_INTEGER_DECL_H
#define NSL_AST_INTEGER_DECL_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Decl.h"

namespace nsl::ast {

class IntegerDecl final : public Decl {
public:
  IntegerDecl(SourceRange range, Identifier name)
      : Decl(NodeKind::NK_IntegerDecl, range), name_(name) {}

  [[nodiscard]] Identifier name() const noexcept { return name_; }

  NSL_AST_NODE_BOILERPLATE(IntegerDecl)

private:
  Identifier name_;
};

} // namespace nsl::ast

#endif // NSL_AST_INTEGER_DECL_H
