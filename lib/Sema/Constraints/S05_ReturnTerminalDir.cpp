// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S05_ReturnTerminalDir.cpp - S5 checker.
// Spec: lang.ebnf:843 — return-value terminal of `func_in` must be
// `output` or `inout`; of `func_out` must be `input` or `inout`.

#include "../ConstraintCheckRegistry.h"

#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/DeclareBlock.h"
#include "nsl/AST/PortDecl.h"
#include "nsl/Basic/Diagnostic.h"

#include "llvm/ADT/DenseMap.h"

namespace nsl::sema {
namespace {

class S05Visitor : public ConstraintVisitor {
public:
  void run(const ConstraintContext &ctx) const override {
    if (ctx.unit == nullptr || ctx.diag == nullptr) {
      return;
    }
    for (const auto &item : ctx.unit->items()) {
      if (!item ||
          item->kind() != ast::NodeKind::NK_DeclareBlock) {
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
        if (!p || p->kind() != ast::NodeKind::NK_PortDecl) {
          continue;
        }
        const auto &pd = static_cast<const ast::PortDecl &>(*p);
        if (pd.returnTerminal().empty()) {
          continue;
        }
        auto it = portDirs.find(pd.returnTerminal());
        if (it == portDirs.end()) {
          continue;
        }
        ast::PortDecl::Direction rt_dir = it->second;
        if (pd.direction() == ast::PortDecl::Direction::FuncIn) {
          if (rt_dir != ast::PortDecl::Direction::Output &&
              rt_dir != ast::PortDecl::Direction::Inout) {
            ctx.diag->report(
                Severity::Error, pd.loc().begin(),
                "return-value terminal of 'func_in' must be declared "
                "'output' or 'inout' (S5)");
          }
        } else if (pd.direction() == ast::PortDecl::Direction::FuncOut) {
          if (rt_dir != ast::PortDecl::Direction::Input &&
              rt_dir != ast::PortDecl::Direction::Inout) {
            ctx.diag->report(
                Severity::Error, pd.loc().begin(),
                "return-value terminal of 'func_out' must be declared "
                "'input' or 'inout' (S5)");
          }
        }
      }
    }
  }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(5, S05Visitor)
