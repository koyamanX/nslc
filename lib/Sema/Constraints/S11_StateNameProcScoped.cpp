// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S11_StateNameProcScoped.cpp - S11 checker.
// Spec: lang.ebnf:863 — a `state_name` declared inside a `proc` is
// scoped to that proc only. Referencing it from outside the
// declaring proc (e.g. from a sibling func body, from another
// proc, or from module top-level) violates S11.
//
// Implementation: pass 1 walks every ProcDefn and indexes the
// state_names declared inside its body's ParallelBlock decls()
// keyed by name -> declaring-proc-name. Pass 2 walks the
// CompilationUnit's IdentifierExpr forest, tracking the
// "currently inside which proc" identity; on each use, looks up
// the name in the index, and emits S11 if the use site's
// enclosing proc differs from the declaring proc (or no enclosing
// proc at all).

#include "../ConstraintCheckRegistry.h"
#include "nsl/AST/AltBlock.h"
#include "nsl/AST/AnyBlock.h"
#include "nsl/AST/BinaryExpr.h"
#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/ConcatExpr.h"
#include "nsl/AST/ConditionalExpr.h"
#include "nsl/AST/Expr.h"
#include "nsl/AST/FuncDefn.h"
#include "nsl/AST/IdentifierExpr.h"
#include "nsl/AST/IfStmt.h"
#include "nsl/AST/ModuleBlock.h"
#include "nsl/AST/ParallelBlock.h"
#include "nsl/AST/ProcDefn.h"
#include "nsl/AST/SeqBlock.h"
#include "nsl/AST/SliceExpr.h"
#include "nsl/AST/StateDefn.h"
#include "nsl/AST/StateNameDecl.h"
#include "nsl/AST/Stmt.h"
#include "nsl/AST/TransferStmt.h"
#include "nsl/AST/UnaryExpr.h"
#include "nsl/Basic/Diagnostic.h"

#include "llvm/ADT/StringMap.h"

#include <string>

namespace nsl::sema {
namespace {

using StateNameOwnerMap = llvm::StringMap<std::string>;

void collectStateNamesInProcBody(const ast::Stmt *s,
                                 const std::string &owner_proc,
                                 StateNameOwnerMap &out) {
  if (!s) {
    return;
  }
  if (s->kind() == ast::NodeKind::NK_ParallelBlock) {
    const auto &pb = static_cast<const ast::ParallelBlock &>(*s);
    for (const auto &d : pb.decls()) {
      if (!d) {
        continue;
      }
      if (d->kind() == ast::NodeKind::NK_StateNameDecl) {
        const auto &sn = static_cast<const ast::StateNameDecl &>(*d);
        for (auto nm : sn.names()) {
          out[nm] = owner_proc;
        }
      }
    }
    for (const auto &it : pb.items()) {
      collectStateNamesInProcBody(it.get(), owner_proc, out);
    }
  }
}

void buildIndex(const ast::CompilationUnit &cu, StateNameOwnerMap &out) {
  for (const auto &item : cu.items()) {
    if (!item || item->kind() != ast::NodeKind::NK_ModuleBlock) {
      continue;
    }
    const auto &mb = static_cast<const ast::ModuleBlock &>(*item);
    for (const auto &p : mb.procs()) {
      if (!p || p->kind() != ast::NodeKind::NK_ProcDefn) {
        continue;
      }
      const auto &pd = static_cast<const ast::ProcDefn &>(*p);
      collectStateNamesInProcBody(pd.body(), std::string(pd.name()), out);
    }
  }
}

void walkExpr(const ast::Expr *e, const std::string &enclosing_proc,
              const StateNameOwnerMap &index, DiagnosticEngine &diag) {
  if (!e) {
    return;
  }
  switch (e->kind()) {
  case ast::NodeKind::NK_IdentifierExpr: {
    const auto &n = static_cast<const ast::IdentifierExpr &>(*e);
    if (n.name().parts.empty()) {
      return;
    }
    auto it = index.find(n.name().parts.front());
    if (it == index.end()) {
      return;
    }
    const std::string &declaring_proc = it->second;
    if (declaring_proc != enclosing_proc) {
      diag.report(Severity::Error, n.loc().begin(),
                  "'state_name' is scoped to its enclosing 'proc' and "
                  "is not visible here (S11)");
    }
    break;
  }
  case ast::NodeKind::NK_BinaryExpr: {
    const auto &n = static_cast<const ast::BinaryExpr &>(*e);
    walkExpr(n.lhs(), enclosing_proc, index, diag);
    walkExpr(n.rhs(), enclosing_proc, index, diag);
    break;
  }
  case ast::NodeKind::NK_UnaryExpr:
    walkExpr(static_cast<const ast::UnaryExpr &>(*e).sub(), enclosing_proc,
             index, diag);
    break;
  case ast::NodeKind::NK_ConditionalExpr: {
    const auto &n = static_cast<const ast::ConditionalExpr &>(*e);
    walkExpr(n.cond(), enclosing_proc, index, diag);
    walkExpr(n.thenE(), enclosing_proc, index, diag);
    walkExpr(n.elseE(), enclosing_proc, index, diag);
    break;
  }
  case ast::NodeKind::NK_SliceExpr: {
    const auto &n = static_cast<const ast::SliceExpr &>(*e);
    walkExpr(n.sub(), enclosing_proc, index, diag);
    walkExpr(n.hi(), enclosing_proc, index, diag);
    walkExpr(n.lo(), enclosing_proc, index, diag);
    break;
  }
  case ast::NodeKind::NK_ConcatExpr: {
    const auto &n = static_cast<const ast::ConcatExpr &>(*e);
    for (const auto &p : n.parts()) {
      walkExpr(p.get(), enclosing_proc, index, diag);
    }
    break;
  }
  default:
    break;
  }
}

void walkStmt(const ast::Stmt *s, const std::string &enclosing_proc,
              const StateNameOwnerMap &index, DiagnosticEngine &diag) {
  if (!s) {
    return;
  }
  switch (s->kind()) {
  case ast::NodeKind::NK_TransferStmt: {
    const auto &t = static_cast<const ast::TransferStmt &>(*s);
    walkExpr(t.lhs(), enclosing_proc, index, diag);
    walkExpr(t.rhs(), enclosing_proc, index, diag);
    break;
  }
  case ast::NodeKind::NK_ParallelBlock: {
    const auto &pb = static_cast<const ast::ParallelBlock &>(*s);
    for (const auto &it : pb.items()) {
      walkStmt(it.get(), enclosing_proc, index, diag);
    }
    break;
  }
  case ast::NodeKind::NK_SeqBlock: {
    const auto &sb = static_cast<const ast::SeqBlock &>(*s);
    for (const auto &it : sb.items()) {
      walkStmt(it.get(), enclosing_proc, index, diag);
    }
    break;
  }
  case ast::NodeKind::NK_AltBlock: {
    const auto &ab = static_cast<const ast::AltBlock &>(*s);
    for (const auto &c : ab.cases()) {
      walkExpr(c.cond.get(), enclosing_proc, index, diag);
      walkStmt(c.body.get(), enclosing_proc, index, diag);
    }
    walkStmt(ab.elseCase(), enclosing_proc, index, diag);
    break;
  }
  case ast::NodeKind::NK_AnyBlock: {
    const auto &an = static_cast<const ast::AnyBlock &>(*s);
    for (const auto &c : an.cases()) {
      walkExpr(c.cond.get(), enclosing_proc, index, diag);
      walkStmt(c.body.get(), enclosing_proc, index, diag);
    }
    walkStmt(an.elseCase(), enclosing_proc, index, diag);
    break;
  }
  case ast::NodeKind::NK_IfStmt: {
    const auto &is = static_cast<const ast::IfStmt &>(*s);
    walkExpr(is.cond(), enclosing_proc, index, diag);
    walkStmt(is.thenBr(), enclosing_proc, index, diag);
    walkStmt(is.elseBr(), enclosing_proc, index, diag);
    break;
  }
  default:
    break;
  }
}

class S11Visitor : public ConstraintVisitor {
public:
  void run(const ConstraintContext &ctx) const override {
    if (ctx.unit == nullptr || ctx.diag == nullptr) {
      return;
    }
    StateNameOwnerMap index;
    buildIndex(*ctx.unit, index);
    if (index.empty()) {
      return;
    }
    for (const auto &item : ctx.unit->items()) {
      if (!item || item->kind() != ast::NodeKind::NK_ModuleBlock) {
        continue;
      }
      const auto &mb = static_cast<const ast::ModuleBlock &>(*item);
      for (const auto &f : mb.funcs()) {
        if (f && f->kind() == ast::NodeKind::NK_FuncDefn) {
          const auto &fd = static_cast<const ast::FuncDefn &>(*f);
          // FuncDefn's name is a ScopedName; its parts.back() is the
          // simple name. Funcs are not procs; enclosing_proc is empty.
          walkStmt(fd.body(), std::string(), index, *ctx.diag);
        }
      }
      for (const auto &p : mb.procs()) {
        if (p && p->kind() == ast::NodeKind::NK_ProcDefn) {
          const auto &pd = static_cast<const ast::ProcDefn &>(*p);
          walkStmt(pd.body(), std::string(pd.name()), index, *ctx.diag);
        }
      }
      for (const auto &a : mb.actions()) {
        walkStmt(a.get(), std::string(), index, *ctx.diag);
      }
    }
  }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(11, S11Visitor)
