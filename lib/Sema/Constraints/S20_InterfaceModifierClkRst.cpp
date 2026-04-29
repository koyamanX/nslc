// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S20_InterfaceModifierClkRst.cpp - S20 checker.
// Spec: lang.ebnf:896 — `interface` modifier requires explicit
// clock and reset signal names.
//
// The parser now accepts `interface(clock=<name>, reset=<name>)`
// modifier args; DeclareBlock carries `clockName()` and
// `resetName()`. S20 fires only when the modifier is `Interface`
// AND either name is empty.

#include "../ConstraintCheckRegistry.h"
#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/DeclareBlock.h"
#include "nsl/AST/PortDecl.h"
#include "nsl/Basic/Diagnostic.h"

namespace nsl::sema {
namespace {

class S20Visitor : public ConstraintVisitor {
public:
  void run(const ConstraintContext &ctx) const override {
    if (ctx.unit == nullptr || ctx.diag == nullptr) {
      return;
    }
    for (const auto &item : ctx.unit->items()) {
      if (!item || item->kind() != ast::NodeKind::NK_DeclareBlock) {
        continue;
      }
      const auto &db = static_cast<const ast::DeclareBlock &>(*item);
      if (db.modifier() == ast::DeclareBlock::Modifier::Interface &&
          (db.clockName().empty() || db.resetName().empty())) {
        ctx.diag->report(
            Severity::Error, db.loc().begin(),
            "'interface' modifier requires explicit clock and reset "
            "signal names (S20)");
      }
    }
  }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(20, S20Visitor)
