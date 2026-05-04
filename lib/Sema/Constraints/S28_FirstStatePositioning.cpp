// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S28_FirstStatePositioning.cpp - S28 checker.
// Spec: lang.ebnf:986 — `first_state` rules:
//   - target must reference a `state_name` declared in the
//     enclosing proc;
//   - may appear at most once per proc.

#include "../ConstraintCheckRegistry.h"
#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/FirstStateDecl.h"
#include "nsl/AST/ModuleBlock.h"
#include "nsl/AST/ParallelBlock.h"
#include "nsl/AST/ProcDefn.h"
#include "nsl/AST/StateNameDecl.h"
#include "nsl/AST/Stmt.h"
#include "nsl/Basic/Diagnostic.h"

#include "llvm/ADT/StringSet.h"

#include <vector>

namespace nsl::sema {
namespace {

void collectInProcBody(const ast::Stmt *s, llvm::StringSet<> &state_names,
                       std::vector<const ast::FirstStateDecl *> &first_states) {
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
          state_names.insert(nm);
        }
      } else if (d->kind() == ast::NodeKind::NK_FirstStateDecl) {
        first_states.push_back(
            static_cast<const ast::FirstStateDecl *>(d.get()));
      }
    }
    for (const auto &it : pb.items()) {
      collectInProcBody(it.get(), state_names, first_states);
    }
  }
}

class S28Visitor : public ConstraintVisitor {
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
        if (!p || p->kind() != ast::NodeKind::NK_ProcDefn) {
          continue;
        }
        const auto &pd = static_cast<const ast::ProcDefn &>(*p);
        llvm::StringSet<> state_names;
        std::vector<const ast::FirstStateDecl *> first_states;
        collectInProcBody(pd.body(), state_names, first_states);

        // Rule 1: at most one first_state per proc.
        if (first_states.size() > 1U) {
          for (size_t i = 1U; i < first_states.size(); ++i) {
            ctx.diag->report(
                Severity::Error, first_states[i]->loc().begin(),
                "'first_state' may appear at most once per 'proc' (S28)");
          }
        }

        // Rule 2: target must be a declared state_name in this proc.
        for (const auto *fs : first_states) {
          if (state_names.count(fs->target()) == 0U) {
            ctx.diag->report(
                Severity::Error, fs->loc().begin(),
                "'first_state' must reference a 'state_name' declared in "
                "the enclosing 'proc' (S28)");
          }
        }
      }
    }
  }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(28, S28Visitor)
