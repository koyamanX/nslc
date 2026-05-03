// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/EmptyStmt.h — bare `;` placeholder statement
// (`lang.ebnf §9`; data-model §1.5). No fields beyond `ASTNode`.

#ifndef NSL_AST_EMPTY_STMT_H
#define NSL_AST_EMPTY_STMT_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Stmt.h"

namespace nsl::ast {

class EmptyStmt final : public Stmt {
public:
  explicit EmptyStmt(SourceRange range) : Stmt(NodeKind::NK_EmptyStmt, range) {}

  NSL_AST_NODE_BOILERPLATE(EmptyStmt)
};

} // namespace nsl::ast

#endif // NSL_AST_EMPTY_STMT_H
