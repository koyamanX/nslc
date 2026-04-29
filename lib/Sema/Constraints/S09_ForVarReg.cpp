// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S09_ForVarReg.cpp - S9 checker.
// Spec: lang.ebnf:857 — for-loop variable must be a `reg` identifier.

#include "../ConstraintCheckRegistry.h"
#include "ConstraintHelpers.h"
#include "nsl/AST/ForBlock.h"
#include "nsl/AST/IdentifierExpr.h"
#include "nsl/AST/IncDecStmt.h"
#include "nsl/AST/TransferStmt.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Sema/SymbolTable.h"

namespace nsl::sema {
namespace {

ast::Identifier headIdentifier(const ast::Expr *e) noexcept {
  if (e == nullptr) {
    return ast::Identifier();
  }
  if (e->kind() == ast::NodeKind::NK_IdentifierExpr) {
    const auto &n = static_cast<const ast::IdentifierExpr &>(*e);
    if (!n.name().parts.empty()) {
      return n.name().parts.front();
    }
  }
  return ast::Identifier();
}

ast::Identifier loopVarFromInit(const ast::Stmt *init) noexcept {
  if (init == nullptr) {
    return ast::Identifier();
  }
  if (init->kind() == ast::NodeKind::NK_TransferStmt) {
    const auto &t = static_cast<const ast::TransferStmt &>(*init);
    return headIdentifier(t.lhs());
  }
  if (init->kind() == ast::NodeKind::NK_IncDecStmt) {
    const auto &t = static_cast<const ast::IncDecStmt &>(*init);
    return headIdentifier(t.target());
  }
  return ast::Identifier();
}

class S09Visitor : public ConstraintVisitor {
public:
  void run(const ConstraintContext &ctx) const override {
    if (ctx.unit == nullptr || ctx.diag == nullptr || ctx.symbols == nullptr) {
      return;
    }
    detail::walkUnit(
        *ctx.unit, /*dcb=*/nullptr, [&](const ast::Stmt &s, uint32_t /*lex*/) {
          if (s.kind() != ast::NodeKind::NK_ForBlock) {
            return;
          }
          const auto &fb = static_cast<const ast::ForBlock &>(s);
          ast::Identifier name = loopVarFromInit(fb.form().init.get());
          if (name.empty()) {
            return;
          }
          Symbol *sym = ctx.symbols->lookup(name);
          if (sym != nullptr && sym->kind() != SymbolKind::SK_Reg) {
            ctx.diag->report(
                Severity::Error, fb.loc().begin(),
                "for-loop variable must be a 'reg' identifier (S9)");
          }
        });
  }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(9, S09Visitor)
