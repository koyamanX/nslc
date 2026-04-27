// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/LabeledStmt.h — `labeled_statement`
// (`lang.ebnf §9`; data-model §1.5). Fields: `label` (the textual
// identifier — see N10 caveat about the reserved word `label` as
// an identifier; this AST field is the user-supplied name), `body`.

#ifndef NSL_AST_LABELED_STMT_H
#define NSL_AST_LABELED_STMT_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Stmt.h"

#include <memory>
#include <utility>

namespace nsl::ast {

class LabeledStmt final : public Stmt {
public:
  LabeledStmt(SourceRange range, Identifier label, std::unique_ptr<Stmt> body)
      : Stmt(NodeKind::NK_LabeledStmt, range), label_(label),
        body_(std::move(body)) {}

  [[nodiscard]] Identifier label() const noexcept { return label_; }
  [[nodiscard]] const Stmt *body() const noexcept { return body_.get(); }

  NSL_AST_NODE_BOILERPLATE(LabeledStmt)

private:
  Identifier label_;
  std::unique_ptr<Stmt> body_;
};

} // namespace nsl::ast

#endif // NSL_AST_LABELED_STMT_H
