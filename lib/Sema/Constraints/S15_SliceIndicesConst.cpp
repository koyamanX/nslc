// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S15_SliceIndicesConst.cpp - S15 checker.
// Spec: lang.ebnf:877 — bit-slice indices must be compile-time
// constants.

#include "../ConstraintCheckRegistry.h"
#include "ConstraintHelpers.h"
#include "nsl/AST/BinaryExpr.h"
#include "nsl/AST/CallExpr.h"
#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/ConcatExpr.h"
#include "nsl/AST/ConditionalExpr.h"
#include "nsl/AST/Expr.h"
#include "nsl/AST/FieldAccessExpr.h"
#include "nsl/AST/IdentifierExpr.h"
#include "nsl/AST/IncDecExpr.h"
#include "nsl/AST/LiteralExpr.h"
#include "nsl/AST/MemDecl.h"
#include "nsl/AST/RegDecl.h"
#include "nsl/AST/RepeatExpr.h"
#include "nsl/AST/SignExtendExpr.h"
#include "nsl/AST/SliceExpr.h"
#include "nsl/AST/StructCastExpr.h"
#include "nsl/AST/StructInstDecl.h"
#include "nsl/AST/TransferStmt.h"
#include "nsl/AST/UnaryExpr.h"
#include "nsl/AST/VariableDecl.h"
#include "nsl/AST/WireDecl.h"
#include "nsl/AST/ZeroExtendExpr.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Sema/SymbolTable.h"

namespace nsl::sema {
namespace {

bool isCompileTimeConst(const ast::Expr *e) noexcept {
  if (e == nullptr) {
    return true;
  }
  if (e->kind() == ast::NodeKind::NK_LiteralExpr) {
    return true;
  }
  return false;
}

/// Walk a possibly-wrapped Expr to its underlying IdentifierExpr
/// head; returns the head Identifier or empty.
ast::Identifier sliceHeadIdentifier(const ast::Expr *e) noexcept {
  while (e != nullptr) {
    if (e->kind() == ast::NodeKind::NK_IdentifierExpr) {
      const auto &n = static_cast<const ast::IdentifierExpr &>(*e);
      if (!n.name().parts.empty()) {
        return n.name().parts.front();
      }
      return ast::Identifier();
    }
    if (e->kind() == ast::NodeKind::NK_SliceExpr) {
      e = static_cast<const ast::SliceExpr &>(*e).sub();
      continue;
    }
    break;
  }
  return ast::Identifier();
}

void walkExpr(const ast::Expr *e, DiagnosticEngine &diag, SymbolTable *symbols);

void walkSliceCheck(const ast::SliceExpr &n, DiagnosticEngine &diag,
                    SymbolTable *symbols) {
  // Memory cell indexing (`mem[i]`) uses the same SliceExpr shape as
  // bit-slicing but its index is RUNTIME-dynamic by design; skip
  // when the slice head resolves to a MemSymbol.
  if (symbols != nullptr) {
    ast::Identifier const head = sliceHeadIdentifier(n.sub());
    if (!head.empty()) {
      Symbol *sym = symbols->lookup(head);
      if (sym != nullptr && sym->kind() == SymbolKind::SK_Mem) {
        return;
      }
    }
  }
  if (!isCompileTimeConst(n.hi()) || !isCompileTimeConst(n.lo())) {
    diag.report(Severity::Error, n.loc().begin(),
                "bit-slice index must be a compile-time constant (S15)");
  }
}

void walkExpr(const ast::Expr *e, DiagnosticEngine &diag,
              SymbolTable *symbols) {
  if (e == nullptr) {
    return;
  }
  switch (e->kind()) {
  case ast::NodeKind::NK_SliceExpr: {
    const auto &n = static_cast<const ast::SliceExpr &>(*e);
    walkSliceCheck(n, diag, symbols);
    walkExpr(n.sub(), diag, symbols);
    walkExpr(n.hi(), diag, symbols);
    walkExpr(n.lo(), diag, symbols);
    break;
  }
  case ast::NodeKind::NK_BinaryExpr: {
    const auto &n = static_cast<const ast::BinaryExpr &>(*e);
    walkExpr(n.lhs(), diag, symbols);
    walkExpr(n.rhs(), diag, symbols);
    break;
  }
  case ast::NodeKind::NK_UnaryExpr:
    walkExpr(static_cast<const ast::UnaryExpr &>(*e).sub(), diag, symbols);
    break;
  case ast::NodeKind::NK_ConditionalExpr: {
    const auto &n = static_cast<const ast::ConditionalExpr &>(*e);
    walkExpr(n.cond(), diag, symbols);
    walkExpr(n.thenE(), diag, symbols);
    walkExpr(n.elseE(), diag, symbols);
    break;
  }
  case ast::NodeKind::NK_ConcatExpr: {
    const auto &n = static_cast<const ast::ConcatExpr &>(*e);
    for (const auto &p : n.parts()) {
      walkExpr(p.get(), diag, symbols);
    }
    break;
  }
  case ast::NodeKind::NK_RepeatExpr: {
    const auto &n = static_cast<const ast::RepeatExpr &>(*e);
    walkExpr(n.count(), diag, symbols);
    walkExpr(n.body(), diag, symbols);
    break;
  }
  case ast::NodeKind::NK_SignExtendExpr: {
    const auto &n = static_cast<const ast::SignExtendExpr &>(*e);
    walkExpr(n.width(), diag, symbols);
    walkExpr(n.sub(), diag, symbols);
    break;
  }
  case ast::NodeKind::NK_ZeroExtendExpr: {
    const auto &n = static_cast<const ast::ZeroExtendExpr &>(*e);
    walkExpr(n.width(), diag, symbols);
    walkExpr(n.sub(), diag, symbols);
    break;
  }
  case ast::NodeKind::NK_FieldAccessExpr:
    walkExpr(static_cast<const ast::FieldAccessExpr &>(*e).obj(), diag,
             symbols);
    break;
  case ast::NodeKind::NK_CallExpr: {
    const auto &n = static_cast<const ast::CallExpr &>(*e);
    for (const auto &a : n.args()) {
      walkExpr(a.get(), diag, symbols);
    }
    break;
  }
  case ast::NodeKind::NK_StructCastExpr:
    walkExpr(static_cast<const ast::StructCastExpr &>(*e).sub(), diag, symbols);
    break;
  case ast::NodeKind::NK_IncDecExpr:
    walkExpr(static_cast<const ast::IncDecExpr &>(*e).target(), diag, symbols);
    break;
  default:
    break;
  }
}

void walkExprsInDecl(const ast::Decl &d, DiagnosticEngine &diag,
                     SymbolTable *symbols) {
  switch (d.kind()) {
  case ast::NodeKind::NK_RegDecl: {
    const auto &n = static_cast<const ast::RegDecl &>(d);
    walkExpr(n.width(), diag, symbols);
    walkExpr(n.init(), diag, symbols);
    break;
  }
  case ast::NodeKind::NK_WireDecl:
    walkExpr(static_cast<const ast::WireDecl &>(d).width(), diag, symbols);
    break;
  case ast::NodeKind::NK_VariableDecl:
    walkExpr(static_cast<const ast::VariableDecl &>(d).width(), diag, symbols);
    break;
  case ast::NodeKind::NK_MemDecl: {
    const auto &n = static_cast<const ast::MemDecl &>(d);
    walkExpr(n.depth(), diag, symbols);
    walkExpr(n.width(), diag, symbols);
    for (const auto &v : n.init()) {
      walkExpr(v.get(), diag, symbols);
    }
    break;
  }
  case ast::NodeKind::NK_StructInstDecl: {
    const auto &n = static_cast<const ast::StructInstDecl &>(d);
    walkExpr(n.arraySize(), diag, symbols);
    for (const auto &v : n.init()) {
      walkExpr(v.get(), diag, symbols);
    }
    break;
  }
  default:
    break;
  }
}

void walkExprsInStmt(const ast::Stmt &s, DiagnosticEngine &diag,
                     SymbolTable *symbols) {
  if (s.kind() == ast::NodeKind::NK_TransferStmt) {
    const auto &t = static_cast<const ast::TransferStmt &>(s);
    walkExpr(t.lhs(), diag, symbols);
    walkExpr(t.rhs(), diag, symbols);
  }
}

class S15Visitor : public ConstraintVisitor {
public:
  void run(const ConstraintContext &ctx) const override {
    if (ctx.unit == nullptr || ctx.diag == nullptr) {
      return;
    }
    detail::walkUnit(
        *ctx.unit,
        [&](const ast::Decl &d, uint32_t /*lex*/) {
          walkExprsInDecl(d, *ctx.diag, ctx.symbols);
        },
        [&](const ast::Stmt &s, uint32_t /*lex*/) {
          walkExprsInStmt(s, *ctx.diag, ctx.symbols);
        });
  }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(15, S15Visitor)
