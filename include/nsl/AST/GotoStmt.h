// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/GotoStmt.h — `goto_statement`
// (`lang.ebnf §9`; data-model §1.5). Field: `target` (the
// referenced label name; resolution is M3 Sema's responsibility —
// at M2 only the textual `Identifier` is stored, so the AST never
// carries a raw `LabeledStmt*` pointer per data-model §6).

#ifndef NSL_AST_GOTO_STMT_H
#define NSL_AST_GOTO_STMT_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Stmt.h"

namespace nsl::ast {

class GotoStmt final : public Stmt {
public:
  GotoStmt(SourceRange range, Identifier target)
      : Stmt(NodeKind::NK_GotoStmt, range), target_(target) {}

  [[nodiscard]] Identifier target() const noexcept { return target_; }

  NSL_AST_NODE_BOILERPLATE(GotoStmt)

private:
  Identifier target_;
};

} // namespace nsl::ast

#endif // NSL_AST_GOTO_STMT_H
