// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S03_AssignmentLHSKind.cpp - S3 checker.
// Spec: lang.ebnf:835 — `=` targets wire/output/inout/variable/
// integer; `:=` targets reg (or struct-instance-reg).
//
// The frozen-message strings + FixItHint shapes are documented in
// `specs/006-m3-sema/contracts/diagnostic-string.contract.md`.

#include "../ConstraintCheckRegistry.h"
#include "ConstraintHelpers.h"
#include "nsl/AST/IdentifierExpr.h"
#include "nsl/AST/SliceExpr.h"
#include "nsl/AST/TransferStmt.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Sema/SymbolTable.h"

namespace nsl::sema {
namespace {

/// Walk down a possibly-wrapped LHS expression (SliceExpr,
/// FieldAccessExpr, etc.) to the underlying IdentifierExpr's head.
/// Returns the head Identifier, or empty if the LHS shape doesn't
/// terminate in one (e.g., concat).
ast::Identifier headIdentifier(const ast::Expr *e) noexcept {
  while (e != nullptr) {
    switch (e->kind()) {
    case ast::NodeKind::NK_IdentifierExpr: {
      const auto &n = static_cast<const ast::IdentifierExpr &>(*e);
      if (!n.name().parts.empty()) {
        return n.name().parts.front();
      }
      return ast::Identifier();
    }
    case ast::NodeKind::NK_SliceExpr: {
      const auto &n = static_cast<const ast::SliceExpr &>(*e);
      e = n.sub();
      break;
    }
    default:
      return ast::Identifier();
    }
  }
  return ast::Identifier();
}

class S03Visitor : public ConstraintVisitor {
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
          ast::Identifier head = headIdentifier(t.lhs());
          if (head.empty()) {
            return;
          }
          Symbol *sym = ctx.symbols->lookup(head);
          if (sym == nullptr) {
            return; // unresolved — no-cascade per FR-017
          }
          SymbolKind k = sym->kind();
          // `=` (WireEq) targets wire/output/inout/variable/integer.
          // `:=` (RegColonEq) targets reg (or struct-instance which
          // is modeled as a RegSymbol per ResolutionPass).
          if (t.op() == ast::TransferStmt::Op::WireEq) {
            if (k == SymbolKind::SK_Reg) {
              auto b = ctx.diag->report(
                  Severity::Error, t.loc().begin(),
                  "'=' targets a wire, output, inout, variable, or "
                  "integer; use ':=' for reg (S3)");
              b.addFixIt(t.loc(), ":=");
            }
          } else if (t.op() == ast::TransferStmt::Op::RegColonEq) {
            bool reg_target = (k == SymbolKind::SK_Reg);
            if (!reg_target) {
              auto b = ctx.diag->report(
                  Severity::Error, t.loc().begin(),
                  "':=' targets a reg or struct-instance-reg; use "
                  "'=' for wire/output/inout/variable/integer (S3)");
              b.addFixIt(t.loc(), "=");
            }
          }
        });
  }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(3, S03Visitor)
