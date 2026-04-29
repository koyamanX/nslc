// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S12_PartialLHSVariableOnly.cpp - S12 checker.
// Spec: lang.ebnf:866 — partial assignment is permitted only on
// `variable` identifiers.

#include "../ConstraintCheckRegistry.h"
#include "ConstraintHelpers.h"
#include "nsl/AST/IdentifierExpr.h"
#include "nsl/AST/SliceExpr.h"
#include "nsl/AST/TransferStmt.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Sema/SymbolTable.h"

namespace nsl::sema {
namespace {

ast::Identifier underlyingHead(const ast::Expr *e) noexcept {
  while (e != nullptr) {
    if (e->kind() == ast::NodeKind::NK_IdentifierExpr) {
      const auto &n = static_cast<const ast::IdentifierExpr &>(*e);
      if (!n.name().parts.empty()) {
        return n.name().parts.front();
      }
      return ast::Identifier();
    }
    if (e->kind() == ast::NodeKind::NK_SliceExpr) {
      const auto &n = static_cast<const ast::SliceExpr &>(*e);
      e = n.sub();
      continue;
    }
    return ast::Identifier();
  }
  return ast::Identifier();
}

class S12Visitor : public ConstraintVisitor {
public:
  void run(const ConstraintContext &ctx) const override {
    if (ctx.unit == nullptr || ctx.diag == nullptr || ctx.symbols == nullptr) {
      return;
    }
    detail::walkUnit(
        *ctx.unit, /*dcb=*/nullptr, [&](const ast::Stmt &s, uint32_t /*lex*/) {
          if (s.kind() != ast::NodeKind::NK_TransferStmt) {
            return;
          }
          const auto &t = static_cast<const ast::TransferStmt &>(s);
          if (t.lhs() == nullptr) {
            return;
          }
          // S12 fires only on PARTIAL assignment shapes (LHS is a
          // slice / concat / field-access). A bare IdentifierExpr
          // LHS is whole-assignment and not in scope.
          if (t.lhs()->kind() != ast::NodeKind::NK_SliceExpr) {
            return;
          }
          ast::Identifier head = underlyingHead(t.lhs());
          if (head.empty()) {
            return;
          }
          Symbol *sym = ctx.symbols->lookup(head);
          if (sym == nullptr) {
            return;
          }
          // Memory cell indexing (`mem[i] := val`) is NOT a partial
          // assignment — it's a whole-cell write to the addressed
          // element. The S12 partial-assign-restricted-to-variable
          // rule applies only to bit-slice-on-non-mem LHS.
          if (sym->kind() == SymbolKind::SK_Mem) {
            return;
          }
          if (sym->kind() != SymbolKind::SK_Variable) {
            ctx.diag->report(
                Severity::Error, t.loc().begin(),
                "partial assignment is permitted only on 'variable' "
                "identifiers (S12)");
          }
        });
  }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(12, S12Visitor)
