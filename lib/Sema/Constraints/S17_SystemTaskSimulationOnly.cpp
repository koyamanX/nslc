// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S17_SystemTaskSimulationOnly.cpp - S17 checker.
// Spec: lang.ebnf:884 — system tasks (`_display`, `_finish`, …) are
// permitted only in modules whose declare carries the `simulation`
// modifier.

#include "../ConstraintCheckRegistry.h"
#include "ConstraintHelpers.h"

#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/DeclareBlock.h"
#include "nsl/AST/ModuleBlock.h"
#include "nsl/AST/SystemTaskStmt.h"
#include "nsl/Basic/Diagnostic.h"

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/StringRef.h"

#include <string>

namespace nsl::sema {
namespace {

class S17Visitor : public ConstraintVisitor {
public:
  void run(const ConstraintContext &ctx) const override {
    if (ctx.unit == nullptr || ctx.diag == nullptr) {
      return;
    }
    // Build set of module names whose paired declare carries the
    // simulation modifier.
    llvm::DenseSet<llvm::StringRef> simModules;
    for (const auto &item : ctx.unit->items()) {
      if (!item ||
          item->kind() != ast::NodeKind::NK_DeclareBlock) {
        continue;
      }
      const auto &db = static_cast<const ast::DeclareBlock &>(*item);
      if (db.modifier() == ast::DeclareBlock::Modifier::Simulation) {
        simModules.insert(db.name());
      }
    }

    // Walk per-module so we know whether the SystemTaskStmt's
    // enclosing module is a simulation module.
    for (const auto &item : ctx.unit->items()) {
      if (!item ||
          item->kind() != ast::NodeKind::NK_ModuleBlock) {
        continue;
      }
      const auto &mb = static_cast<const ast::ModuleBlock &>(*item);
      bool is_sim = simModules.count(mb.name()) != 0U;
      if (is_sim) {
        continue;
      }
      auto fire = [&](const ast::Stmt &s, uint32_t /*lex*/) {
        if (s.kind() != ast::NodeKind::NK_SystemTaskStmt) {
          return;
        }
        const auto &st = static_cast<const ast::SystemTaskStmt &>(s);
        std::string msg = "system task '";
        msg += st.name().str();
        msg += "' is permitted only in modules whose 'declare' "
               "carries the 'simulation' modifier (S17)";
        ctx.diag->report(Severity::Error, st.loc().begin(), std::move(msg));
      };
      for (const auto &a : mb.actions()) {
        if (a) {
          detail::walkStmt(*a, 0U, fire);
        }
      }
      for (const auto &p : mb.procs()) {
        if (p) {
          detail::walkDecl(*p, 0U, /*dcb=*/nullptr, fire);
        }
      }
      for (const auto &f : mb.funcs()) {
        if (f) {
          detail::walkDecl(*f, 0U, /*dcb=*/nullptr, fire);
        }
      }
    }
  }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(17, S17Visitor)
