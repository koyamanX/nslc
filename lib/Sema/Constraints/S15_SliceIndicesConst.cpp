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

void walkExpr(const ast::Expr *e, DiagnosticEngine &diag);

void walkSliceCheck(const ast::SliceExpr &n, DiagnosticEngine &diag) {
  if (!isCompileTimeConst(n.hi()) || !isCompileTimeConst(n.lo())) {
    diag.report(Severity::Error, n.loc().begin(),
                "bit-slice index must be a compile-time constant (S15)");
  }
}

void walkExpr(const ast::Expr *e, DiagnosticEngine &diag) {
  if (e == nullptr) {
    return;
  }
  switch (e->kind()) {
  case ast::NodeKind::NK_SliceExpr: {
    const auto &n = static_cast<const ast::SliceExpr &>(*e);
    walkSliceCheck(n, diag);
    walkExpr(n.sub(), diag);
    walkExpr(n.hi(), diag);
    walkExpr(n.lo(), diag);
    break;
  }
  case ast::NodeKind::NK_BinaryExpr: {
    const auto &n = static_cast<const ast::BinaryExpr &>(*e);
    walkExpr(n.lhs(), diag);
    walkExpr(n.rhs(), diag);
    break;
  }
  case ast::NodeKind::NK_UnaryExpr:
    walkExpr(static_cast<const ast::UnaryExpr &>(*e).sub(), diag);
    break;
  case ast::NodeKind::NK_ConditionalExpr: {
    const auto &n = static_cast<const ast::ConditionalExpr &>(*e);
    walkExpr(n.cond(), diag);
    walkExpr(n.thenE(), diag);
    walkExpr(n.elseE(), diag);
    break;
  }
  case ast::NodeKind::NK_ConcatExpr: {
    const auto &n = static_cast<const ast::ConcatExpr &>(*e);
    for (const auto &p : n.parts()) {
      walkExpr(p.get(), diag);
    }
    break;
  }
  case ast::NodeKind::NK_RepeatExpr: {
    const auto &n = static_cast<const ast::RepeatExpr &>(*e);
    walkExpr(n.count(), diag);
    walkExpr(n.body(), diag);
    break;
  }
  case ast::NodeKind::NK_SignExtendExpr: {
    const auto &n = static_cast<const ast::SignExtendExpr &>(*e);
    walkExpr(n.width(), diag);
    walkExpr(n.sub(), diag);
    break;
  }
  case ast::NodeKind::NK_ZeroExtendExpr: {
    const auto &n = static_cast<const ast::ZeroExtendExpr &>(*e);
    walkExpr(n.width(), diag);
    walkExpr(n.sub(), diag);
    break;
  }
  case ast::NodeKind::NK_FieldAccessExpr:
    walkExpr(static_cast<const ast::FieldAccessExpr &>(*e).obj(), diag);
    break;
  case ast::NodeKind::NK_CallExpr: {
    const auto &n = static_cast<const ast::CallExpr &>(*e);
    for (const auto &a : n.args()) {
      walkExpr(a.get(), diag);
    }
    break;
  }
  case ast::NodeKind::NK_StructCastExpr:
    walkExpr(static_cast<const ast::StructCastExpr &>(*e).sub(), diag);
    break;
  case ast::NodeKind::NK_IncDecExpr:
    walkExpr(static_cast<const ast::IncDecExpr &>(*e).target(), diag);
    break;
  default:
    break;
  }
}

void walkExprsInDecl(const ast::Decl &d, DiagnosticEngine &diag) {
  switch (d.kind()) {
  case ast::NodeKind::NK_RegDecl: {
    const auto &n = static_cast<const ast::RegDecl &>(d);
    walkExpr(n.width(), diag);
    walkExpr(n.init(), diag);
    break;
  }
  case ast::NodeKind::NK_WireDecl:
    walkExpr(static_cast<const ast::WireDecl &>(d).width(), diag);
    break;
  case ast::NodeKind::NK_VariableDecl:
    walkExpr(static_cast<const ast::VariableDecl &>(d).width(), diag);
    break;
  case ast::NodeKind::NK_MemDecl: {
    const auto &n = static_cast<const ast::MemDecl &>(d);
    walkExpr(n.depth(), diag);
    walkExpr(n.width(), diag);
    for (const auto &v : n.init()) {
      walkExpr(v.get(), diag);
    }
    break;
  }
  case ast::NodeKind::NK_StructInstDecl: {
    const auto &n = static_cast<const ast::StructInstDecl &>(d);
    walkExpr(n.arraySize(), diag);
    for (const auto &v : n.init()) {
      walkExpr(v.get(), diag);
    }
    break;
  }
  default:
    break;
  }
}

void walkExprsInStmt(const ast::Stmt &s, DiagnosticEngine &diag) {
  if (s.kind() == ast::NodeKind::NK_TransferStmt) {
    const auto &t = static_cast<const ast::TransferStmt &>(s);
    walkExpr(t.lhs(), diag);
    walkExpr(t.rhs(), diag);
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
          walkExprsInDecl(d, *ctx.diag);
        },
        [&](const ast::Stmt &s, uint32_t /*lex*/) {
          walkExprsInStmt(s, *ctx.diag);
        });
  }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(15, S15Visitor)
