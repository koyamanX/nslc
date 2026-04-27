// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/Stmt.h
//
// `Stmt` — abstract mid-level base for action statements (data-model
// §1.1, §1.5). Concrete subclasses live in their own per-kind
// headers (Principle II §3 exception): `TransferStmt`, `IfStmt`,
// `SeqBlock`, `ParallelBlock`, etc.
//
// `Stmt` carries no fields beyond `ASTNode` — every statement form
// has its own kind-specific payload.

#ifndef NSL_AST_STMT_H
#define NSL_AST_STMT_H

#include "nsl/AST/ASTNode.h"

namespace nsl::ast {

/// Abstract mid-level base for action-statement AST nodes.
class Stmt : public ASTNode {
protected:
  using ASTNode::ASTNode;
};

} // namespace nsl::ast

#endif // NSL_AST_STMT_H
