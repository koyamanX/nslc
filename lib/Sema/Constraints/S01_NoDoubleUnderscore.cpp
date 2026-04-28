// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S01_NoDoubleUnderscore.cpp - S1 checker.
// Spec: lang.ebnf:830 — identifiers may not contain `__`.
//
// Walks every declaration site in the AST and emits the frozen S1
// message at every Identifier slot whose spelling contains `__`.

#include "../ConstraintCheckRegistry.h"

#include "nsl/AST/AltBlock.h"
#include "nsl/AST/AnyBlock.h"
#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/ControlCallStmt.h"
#include "nsl/AST/DeclareBlock.h"
#include "nsl/AST/Expr.h"
#include "nsl/AST/FirstStateDecl.h"
#include "nsl/AST/ForBlock.h"
#include "nsl/AST/FuncDefn.h"
#include "nsl/AST/FuncSelfDecl.h"
#include "nsl/AST/IfStmt.h"
#include "nsl/AST/InitBlockStmt.h"
#include "nsl/AST/IntegerDecl.h"
#include "nsl/AST/LabeledStmt.h"
#include "nsl/AST/MemDecl.h"
#include "nsl/AST/ModuleBlock.h"
#include "nsl/AST/NodeKind.h"
#include "nsl/AST/ParallelBlock.h"
#include "nsl/AST/PortDecl.h"
#include "nsl/AST/ProcDefn.h"
#include "nsl/AST/ProcNameDecl.h"
#include "nsl/AST/RegDecl.h"
#include "nsl/AST/SeqBlock.h"
#include "nsl/AST/StateDefn.h"
#include "nsl/AST/StateNameDecl.h"
#include "nsl/AST/Stmt.h"
#include "nsl/AST/StructDecl.h"
#include "nsl/AST/StructInstDecl.h"
#include "nsl/AST/StructuralGenerate.h"
#include "nsl/AST/SubmoduleDecl.h"
#include "nsl/AST/TopLevelParamDecl.h"
#include "nsl/AST/VariableDecl.h"
#include "nsl/AST/WhileBlock.h"
#include "nsl/AST/WireDecl.h"
#include "nsl/Basic/Diagnostic.h"

#include "llvm/ADT/StringRef.h"

namespace nsl::sema {
namespace {

bool containsDoubleUnderscore(llvm::StringRef s) noexcept {
  for (size_t i = 0; i + 1 < s.size(); ++i) {
    if (s[i] == '_' && s[i + 1] == '_') {
      return true;
    }
  }
  return false;
}

void emitIfBad(DiagnosticEngine &diag, ast::Identifier name,
               SourceRange where) {
  if (containsDoubleUnderscore(name)) {
    diag.report(Severity::Error, where.begin(),
                "identifier may not contain '__' (S1)");
  }
}

void checkStmt(const ast::Stmt &s, DiagnosticEngine &diag);

void checkDecl(const ast::Decl &d, DiagnosticEngine &diag) {
  switch (d.kind()) {
  case ast::NodeKind::NK_TopLevelParamDecl: {
    const auto &n = static_cast<const ast::TopLevelParamDecl &>(d);
    emitIfBad(diag, n.name(), n.loc());
    break;
  }
  case ast::NodeKind::NK_StructDecl: {
    const auto &n = static_cast<const ast::StructDecl &>(d);
    emitIfBad(diag, n.name(), n.loc());
    for (const auto &m : n.members()) {
      emitIfBad(diag, m.name, n.loc());
    }
    break;
  }
  case ast::NodeKind::NK_DeclareBlock: {
    const auto &n = static_cast<const ast::DeclareBlock &>(d);
    emitIfBad(diag, n.name(), n.loc());
    for (const auto &p : n.headerParams()) {
      if (p) {
        checkDecl(*p, diag);
      }
    }
    for (const auto &p : n.ports()) {
      if (p) {
        checkDecl(*p, diag);
      }
    }
    break;
  }
  case ast::NodeKind::NK_PortDecl: {
    const auto &n = static_cast<const ast::PortDecl &>(d);
    emitIfBad(diag, n.name(), n.loc());
    break;
  }
  case ast::NodeKind::NK_ModuleBlock: {
    const auto &n = static_cast<const ast::ModuleBlock &>(d);
    emitIfBad(diag, n.name(), n.loc());
    for (const auto &i : n.internals()) {
      if (i) {
        checkDecl(*i, diag);
      }
    }
    for (const auto &f : n.funcs()) {
      if (f) {
        checkDecl(*f, diag);
      }
    }
    for (const auto &p : n.procs()) {
      if (p) {
        checkDecl(*p, diag);
      }
    }
    for (const auto &a : n.actions()) {
      if (a) {
        checkStmt(*a, diag);
      }
    }
    break;
  }
  case ast::NodeKind::NK_RegDecl: {
    const auto &n = static_cast<const ast::RegDecl &>(d);
    emitIfBad(diag, n.name(), n.loc());
    break;
  }
  case ast::NodeKind::NK_WireDecl: {
    const auto &n = static_cast<const ast::WireDecl &>(d);
    emitIfBad(diag, n.name(), n.loc());
    break;
  }
  case ast::NodeKind::NK_VariableDecl: {
    const auto &n = static_cast<const ast::VariableDecl &>(d);
    emitIfBad(diag, n.name(), n.loc());
    break;
  }
  case ast::NodeKind::NK_IntegerDecl: {
    const auto &n = static_cast<const ast::IntegerDecl &>(d);
    emitIfBad(diag, n.name(), n.loc());
    break;
  }
  case ast::NodeKind::NK_MemDecl: {
    const auto &n = static_cast<const ast::MemDecl &>(d);
    emitIfBad(diag, n.name(), n.loc());
    break;
  }
  case ast::NodeKind::NK_FuncSelfDecl: {
    const auto &n = static_cast<const ast::FuncSelfDecl &>(d);
    emitIfBad(diag, n.name(), n.loc());
    break;
  }
  case ast::NodeKind::NK_ProcNameDecl: {
    const auto &n = static_cast<const ast::ProcNameDecl &>(d);
    emitIfBad(diag, n.name(), n.loc());
    break;
  }
  case ast::NodeKind::NK_StateNameDecl: {
    const auto &n = static_cast<const ast::StateNameDecl &>(d);
    for (auto nm : n.names()) {
      emitIfBad(diag, nm, n.loc());
    }
    break;
  }
  case ast::NodeKind::NK_FirstStateDecl:
    break;
  case ast::NodeKind::NK_SubmoduleDecl: {
    const auto &n = static_cast<const ast::SubmoduleDecl &>(d);
    for (const auto &inst : n.instances()) {
      emitIfBad(diag, inst.name, n.loc());
    }
    break;
  }
  case ast::NodeKind::NK_StructInstDecl: {
    const auto &n = static_cast<const ast::StructInstDecl &>(d);
    emitIfBad(diag, n.instanceName(), n.loc());
    break;
  }
  case ast::NodeKind::NK_FuncDefn: {
    const auto &n = static_cast<const ast::FuncDefn &>(d);
    if (!n.name().parts.empty()) {
      emitIfBad(diag, n.name().parts.back(), n.loc());
    }
    if (n.body()) {
      checkStmt(*n.body(), diag);
    }
    break;
  }
  case ast::NodeKind::NK_ProcDefn: {
    const auto &n = static_cast<const ast::ProcDefn &>(d);
    emitIfBad(diag, n.name(), n.loc());
    if (n.body()) {
      checkStmt(*n.body(), diag);
    }
    break;
  }
  case ast::NodeKind::NK_StateDefn: {
    const auto &n = static_cast<const ast::StateDefn &>(d);
    emitIfBad(diag, n.name(), n.loc());
    if (n.body()) {
      checkStmt(*n.body(), diag);
    }
    break;
  }
  default:
    break;
  }
}

void checkStmt(const ast::Stmt &s, DiagnosticEngine &diag) {
  switch (s.kind()) {
  case ast::NodeKind::NK_LabeledStmt: {
    const auto &n = static_cast<const ast::LabeledStmt &>(s);
    emitIfBad(diag, n.label(), n.loc());
    if (n.body()) {
      checkStmt(*n.body(), diag);
    }
    break;
  }
  case ast::NodeKind::NK_SeqBlock: {
    const auto &n = static_cast<const ast::SeqBlock &>(s);
    for (const auto &it : n.items()) {
      if (it) {
        checkStmt(*it, diag);
      }
    }
    break;
  }
  case ast::NodeKind::NK_ParallelBlock: {
    const auto &n = static_cast<const ast::ParallelBlock &>(s);
    for (const auto &it : n.items()) {
      if (it) {
        checkStmt(*it, diag);
      }
    }
    break;
  }
  case ast::NodeKind::NK_AltBlock: {
    const auto &n = static_cast<const ast::AltBlock &>(s);
    for (const auto &c : n.cases()) {
      if (c.body) {
        checkStmt(*c.body, diag);
      }
    }
    if (n.elseCase()) {
      checkStmt(*n.elseCase(), diag);
    }
    break;
  }
  case ast::NodeKind::NK_AnyBlock: {
    const auto &n = static_cast<const ast::AnyBlock &>(s);
    for (const auto &c : n.cases()) {
      if (c.body) {
        checkStmt(*c.body, diag);
      }
    }
    if (n.elseCase()) {
      checkStmt(*n.elseCase(), diag);
    }
    break;
  }
  case ast::NodeKind::NK_WhileBlock: {
    const auto &n = static_cast<const ast::WhileBlock &>(s);
    for (const auto &it : n.items()) {
      if (it) {
        checkStmt(*it, diag);
      }
    }
    break;
  }
  case ast::NodeKind::NK_ForBlock: {
    const auto &n = static_cast<const ast::ForBlock &>(s);
    for (const auto &it : n.items()) {
      if (it) {
        checkStmt(*it, diag);
      }
    }
    break;
  }
  case ast::NodeKind::NK_IfStmt: {
    const auto &n = static_cast<const ast::IfStmt &>(s);
    if (n.thenBr()) {
      checkStmt(*n.thenBr(), diag);
    }
    if (n.elseBr()) {
      checkStmt(*n.elseBr(), diag);
    }
    break;
  }
  case ast::NodeKind::NK_InitBlockStmt: {
    const auto &n = static_cast<const ast::InitBlockStmt &>(s);
    for (const auto &it : n.items()) {
      if (it) {
        checkStmt(*it, diag);
      }
    }
    break;
  }
  case ast::NodeKind::NK_StructuralGenerate: {
    const auto &n = static_cast<const ast::StructuralGenerate &>(s);
    emitIfBad(diag, n.init(), n.loc());
    if (n.body()) {
      checkStmt(*n.body(), diag);
    }
    break;
  }
  default:
    break;
  }
}

class S01Visitor : public ConstraintVisitor {
public:
  void run(const ConstraintContext &ctx) const override {
    if (ctx.unit == nullptr || ctx.diag == nullptr) {
      return;
    }
    for (const auto &item : ctx.unit->items()) {
      if (item) {
        checkDecl(*item, *ctx.diag);
      }
    }
  }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(1, S01Visitor)
