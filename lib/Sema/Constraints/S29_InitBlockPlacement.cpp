// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S29_InitBlockPlacement.cpp - S29 checker.
// Spec: lang.ebnf:1001 — `_init` block permitted only at module top
// level inside a `simulation`-modified module.

#include "../ConstraintCheckRegistry.h"
#include "ConstraintHelpers.h"
#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/DeclareBlock.h"
#include "nsl/AST/InitBlockStmt.h"
#include "nsl/AST/ModuleBlock.h"
#include "nsl/Basic/Diagnostic.h"

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/StringRef.h"

namespace nsl::sema {
namespace {

class S29Visitor : public ConstraintVisitor {
public:
  void run(const ConstraintContext &ctx) const override {
    if (ctx.unit == nullptr || ctx.diag == nullptr) {
      return;
    }
    llvm::DenseSet<llvm::StringRef> simModules;
    for (const auto &item : ctx.unit->items()) {
      if (!item || item->kind() != ast::NodeKind::NK_DeclareBlock) {
        continue;
      }
      const auto &db = static_cast<const ast::DeclareBlock &>(*item);
      if (db.modifier() == ast::DeclareBlock::Modifier::Simulation) {
        simModules.insert(db.name());
      }
    }

    for (const auto &item : ctx.unit->items()) {
      if (!item || item->kind() != ast::NodeKind::NK_ModuleBlock) {
        continue;
      }
      const auto &mb = static_cast<const ast::ModuleBlock &>(*item);
      bool is_sim = simModules.count(mb.name()) != 0U;
      // Top-level _init permitted iff the module is simulation-
      // modified. If non-simulation, fire on every InitBlockStmt
      // at any nesting depth. If simulation, allow only top-level.
      auto fire = [&](const ast::Stmt &s, uint32_t lex) {
        if (s.kind() != ast::NodeKind::NK_InitBlockStmt) {
          return;
        }
        if (!is_sim) {
          ctx.diag->report(
              Severity::Error, s.loc().begin(),
              "'_init' block is permitted only at module top level "
              "inside a 'simulation'-modified module (S29)");
          return;
        }
        // Simulation module: only top-level _init allowed; nested
        // ones (under any action block) violate.
        if (detail::has(lex, detail::LexCtx::InAnyAction)) {
          ctx.diag->report(
              Severity::Error, s.loc().begin(),
              "'_init' block is permitted only at module top level "
              "inside a 'simulation'-modified module (S29)");
        }
      };
      for (const auto &a : mb.actions()) {
        if (a) {
          detail::walkStmt(*a, 0U, fire);
        }
      }
    }
  }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(29, S29Visitor)
