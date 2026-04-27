// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/BareFinishStmt.h — bare `finish;` form
// (`lang.ebnf §9`; data-model §1.5). No fields beyond `ASTNode`.

#ifndef NSL_AST_BARE_FINISH_STMT_H
#define NSL_AST_BARE_FINISH_STMT_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Stmt.h"

namespace nsl::ast {

class BareFinishStmt final : public Stmt {
public:
  explicit BareFinishStmt(SourceRange range)
      : Stmt(NodeKind::NK_BareFinishStmt, range) {}

  NSL_AST_NODE_BOILERPLATE(BareFinishStmt)
};

} // namespace nsl::ast

#endif // NSL_AST_BARE_FINISH_STMT_H
