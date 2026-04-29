// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S04_FuncDummyArgDirs.cpp - S4 checker.
// Spec: lang.ebnf:838 — dummy args of func_in must be `input`;
// of func_out must be `output`; of func_self must be `wire`.

#include "../ConstraintCheckRegistry.h"
#include "ConstraintHelpers.h"
#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/DeclareBlock.h"
#include "nsl/AST/FuncSelfDecl.h"
#include "nsl/AST/PortDecl.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Sema/SymbolTable.h"

#include "llvm/ADT/DenseMap.h"

namespace nsl::sema {
namespace {

class S04Visitor : public ConstraintVisitor {
public:
  void run(const ConstraintContext &ctx) const override {
    if (ctx.unit == nullptr || ctx.diag == nullptr) {
      return;
    }
    // Build a name -> direction map for ports inside the same
    // declare block, so we can validate dummy-arg references.
    for (const auto &item : ctx.unit->items()) {
      if (!item || item->kind() != ast::NodeKind::NK_DeclareBlock) {
        continue;
      }
      const auto &db = static_cast<const ast::DeclareBlock &>(*item);
      llvm::DenseMap<llvm::StringRef, ast::PortDecl::Direction> portDirs;
      for (const auto &p : db.ports()) {
        if (p && p->kind() == ast::NodeKind::NK_PortDecl) {
          const auto &pd = static_cast<const ast::PortDecl &>(*p);
          portDirs[pd.name()] = pd.direction();
        }
      }
      for (const auto &p : db.ports()) {
        if (!p) {
          continue;
        }
        if (p->kind() == ast::NodeKind::NK_PortDecl) {
          const auto &pd = static_cast<const ast::PortDecl &>(*p);
          if (pd.direction() == ast::PortDecl::Direction::FuncIn) {
            for (auto arg : pd.dummyArgs()) {
              auto it = portDirs.find(arg);
              if (it == portDirs.end()) {
                continue;
              }
              if (it->second != ast::PortDecl::Direction::Input) {
                ctx.diag->report(Severity::Error, pd.loc().begin(),
                                 "dummy argument of 'func_in' must be declared "
                                 "'input' (S4)");
              }
            }
          } else if (pd.direction() == ast::PortDecl::Direction::FuncOut) {
            for (auto arg : pd.dummyArgs()) {
              auto it = portDirs.find(arg);
              if (it == portDirs.end()) {
                continue;
              }
              if (it->second != ast::PortDecl::Direction::Output) {
                ctx.diag->report(
                    Severity::Error, pd.loc().begin(),
                    "dummy argument of 'func_out' must be declared "
                    "'output' (S4)");
              }
            }
          }
        }
        // S4 func_self variant: DeclareBlock::ports() only yields
        // PortDecls — FuncSelfDecls live elsewhere in the AST (or
        // are represented as a non-port form). Until the AST shape
        // is reconciled, the func_self variant of S4 is deferred;
        // the func_in / func_out variants above are wired and
        // turn the s04/fail_funcin.nsl + s04/fail_funcout.nsl
        // fixtures green. The s04/fail_funcself.nsl fixture stays
        // red as a known TODO.
      }
    }
  }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(4, S04Visitor)
