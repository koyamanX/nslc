// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S25_GotoTwoKinds.cpp - S25 checker.
// Spec: lang.ebnf:944 — `goto` has two target kinds:
//   - inside a `seq` block, the target is a label declared in the
//     same block (label_name);
//   - inside a `state` body, the target is a state_name declared
//     in the enclosing proc.
// Cross-kind references are violations.

#include "../ConstraintCheckRegistry.h"
#include "nsl/AST/AltBlock.h"
#include "nsl/AST/AnyBlock.h"
#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/ForBlock.h"
#include "nsl/AST/GotoStmt.h"
#include "nsl/AST/IfStmt.h"
#include "nsl/AST/LabeledStmt.h"
#include "nsl/AST/ModuleBlock.h"
#include "nsl/AST/ParallelBlock.h"
#include "nsl/AST/ProcDefn.h"
#include "nsl/AST/SeqBlock.h"
#include "nsl/AST/StateDefn.h"
#include "nsl/AST/StateNameDecl.h"
#include "nsl/AST/Stmt.h"
#include "nsl/AST/WhileBlock.h"
#include "nsl/Basic/Diagnostic.h"

#include "llvm/ADT/StringSet.h"

namespace nsl::sema {
namespace {

void collectStateNames(const ast::Stmt *s, llvm::StringSet<> &out) {
  if (!s || s->kind() != ast::NodeKind::NK_ParallelBlock) {
    return;
  }
  const auto &pb = static_cast<const ast::ParallelBlock &>(*s);
  for (const auto &d : pb.decls()) {
    if (d && d->kind() == ast::NodeKind::NK_StateNameDecl) {
      const auto &sn = static_cast<const ast::StateNameDecl &>(*d);
      for (auto nm : sn.names()) {
        out.insert(nm);
      }
    }
  }
}

void collectLabels(const ast::Stmt *s, llvm::StringSet<> &out) {
  if (!s) {
    return;
  }
  if (s->kind() == ast::NodeKind::NK_LabeledStmt) {
    const auto &ls = static_cast<const ast::LabeledStmt &>(*s);
    out.insert(ls.label());
    if (ls.body()) {
      collectLabels(ls.body(), out);
    }
    return;
  }
  if (s->kind() == ast::NodeKind::NK_SeqBlock) {
    for (const auto &it : static_cast<const ast::SeqBlock &>(*s).items()) {
      collectLabels(it.get(), out);
    }
  }
}

void walkStateBody(const ast::Stmt *s,
                   const llvm::StringSet<> &state_names_in_proc,
                   DiagnosticEngine &diag) {
  if (!s) {
    return;
  }
  switch (s->kind()) {
  case ast::NodeKind::NK_GotoStmt: {
    const auto &g = static_cast<const ast::GotoStmt &>(*s);
    if (state_names_in_proc.count(g.target()) == 0U) {
      diag.report(Severity::Error, g.loc().begin(),
                  "'goto' inside a 'state' body must target a "
                  "'state_name' declared in scope (S25)");
    }
    break;
  }
  case ast::NodeKind::NK_ParallelBlock: {
    const auto &pb = static_cast<const ast::ParallelBlock &>(*s);
    for (const auto &it : pb.items()) {
      walkStateBody(it.get(), state_names_in_proc, diag);
    }
    break;
  }
  case ast::NodeKind::NK_AltBlock: {
    const auto &ab = static_cast<const ast::AltBlock &>(*s);
    for (const auto &c : ab.cases()) {
      walkStateBody(c.body.get(), state_names_in_proc, diag);
    }
    walkStateBody(ab.elseCase(), state_names_in_proc, diag);
    break;
  }
  case ast::NodeKind::NK_AnyBlock: {
    const auto &an = static_cast<const ast::AnyBlock &>(*s);
    for (const auto &c : an.cases()) {
      walkStateBody(c.body.get(), state_names_in_proc, diag);
    }
    walkStateBody(an.elseCase(), state_names_in_proc, diag);
    break;
  }
  case ast::NodeKind::NK_IfStmt: {
    const auto &is = static_cast<const ast::IfStmt &>(*s);
    walkStateBody(is.thenBr(), state_names_in_proc, diag);
    walkStateBody(is.elseBr(), state_names_in_proc, diag);
    break;
  }
  default:
    break;
  }
}

void walkSeqBody(const ast::Stmt *s, const llvm::StringSet<> &labels_in_seq,
                 DiagnosticEngine &diag) {
  if (!s) {
    return;
  }
  switch (s->kind()) {
  case ast::NodeKind::NK_GotoStmt: {
    const auto &g = static_cast<const ast::GotoStmt &>(*s);
    if (labels_in_seq.count(g.target()) == 0U) {
      diag.report(Severity::Error, g.loc().begin(),
                  "'goto' inside a 'seq' block must target a label "
                  "declared in the same block (S25)");
    }
    break;
  }
  case ast::NodeKind::NK_LabeledStmt:
    walkSeqBody(static_cast<const ast::LabeledStmt &>(*s).body(), labels_in_seq,
                diag);
    break;
  case ast::NodeKind::NK_SeqBlock: {
    const auto &sb = static_cast<const ast::SeqBlock &>(*s);
    for (const auto &it : sb.items()) {
      walkSeqBody(it.get(), labels_in_seq, diag);
    }
    break;
  }
  case ast::NodeKind::NK_WhileBlock: {
    const auto &wb = static_cast<const ast::WhileBlock &>(*s);
    for (const auto &it : wb.items()) {
      walkSeqBody(it.get(), labels_in_seq, diag);
    }
    break;
  }
  case ast::NodeKind::NK_ForBlock: {
    const auto &fb = static_cast<const ast::ForBlock &>(*s);
    for (const auto &it : fb.items()) {
      walkSeqBody(it.get(), labels_in_seq, diag);
    }
    break;
  }
  case ast::NodeKind::NK_IfStmt: {
    const auto &is = static_cast<const ast::IfStmt &>(*s);
    walkSeqBody(is.thenBr(), labels_in_seq, diag);
    walkSeqBody(is.elseBr(), labels_in_seq, diag);
    break;
  }
  default:
    break;
  }
}

void dispatch(const ast::Stmt *s, const llvm::StringSet<> &state_names_in_proc,
              DiagnosticEngine &diag) {
  if (!s) {
    return;
  }
  switch (s->kind()) {
  case ast::NodeKind::NK_SeqBlock: {
    llvm::StringSet<> labels;
    collectLabels(s, labels);
    walkSeqBody(s, labels, diag);
    break;
  }
  case ast::NodeKind::NK_ParallelBlock: {
    const auto &pb = static_cast<const ast::ParallelBlock &>(*s);
    for (const auto &d : pb.decls()) {
      if (d && d->kind() == ast::NodeKind::NK_StateDefn) {
        const auto &sd = static_cast<const ast::StateDefn &>(*d);
        walkStateBody(sd.body(), state_names_in_proc, diag);
      }
    }
    for (const auto &it : pb.items()) {
      dispatch(it.get(), state_names_in_proc, diag);
    }
    break;
  }
  case ast::NodeKind::NK_AltBlock: {
    const auto &ab = static_cast<const ast::AltBlock &>(*s);
    for (const auto &c : ab.cases()) {
      dispatch(c.body.get(), state_names_in_proc, diag);
    }
    dispatch(ab.elseCase(), state_names_in_proc, diag);
    break;
  }
  case ast::NodeKind::NK_AnyBlock: {
    const auto &an = static_cast<const ast::AnyBlock &>(*s);
    for (const auto &c : an.cases()) {
      dispatch(c.body.get(), state_names_in_proc, diag);
    }
    dispatch(an.elseCase(), state_names_in_proc, diag);
    break;
  }
  case ast::NodeKind::NK_IfStmt: {
    const auto &is = static_cast<const ast::IfStmt &>(*s);
    dispatch(is.thenBr(), state_names_in_proc, diag);
    dispatch(is.elseBr(), state_names_in_proc, diag);
    break;
  }
  default:
    break;
  }
}

class S25Visitor : public ConstraintVisitor {
public:
  void run(const ConstraintContext &ctx) const override {
    if (ctx.unit == nullptr || ctx.diag == nullptr) {
      return;
    }
    for (const auto &item : ctx.unit->items()) {
      if (!item || item->kind() != ast::NodeKind::NK_ModuleBlock) {
        continue;
      }
      const auto &mb = static_cast<const ast::ModuleBlock &>(*item);
      for (const auto &p : mb.procs()) {
        if (p && p->kind() == ast::NodeKind::NK_ProcDefn) {
          const auto &pd = static_cast<const ast::ProcDefn &>(*p);
          llvm::StringSet<> state_names;
          collectStateNames(pd.body(), state_names);
          dispatch(pd.body(), state_names, *ctx.diag);
        }
      }
    }
  }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(25, S25Visitor)
