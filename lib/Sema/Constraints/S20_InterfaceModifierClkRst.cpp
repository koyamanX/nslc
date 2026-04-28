// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S20_InterfaceModifierClkRst.cpp - S20 checker.
// Spec: lang.ebnf:896 — `interface` modifier requires explicit
// clock and reset signal names.
//
// Implementation note: the M2 DeclareBlock AST carries the interface
// modifier but NOT the `(clock=…, reset=…)` parenthesized form (the
// parser doesn't parse the form yet). Until the parser surfaces
// those names, this checker always fires for interface-modifier
// declares. Once the form parses, refine to skip when both names
// are present.

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
      if (!item ||
          item->kind() != ast::NodeKind::NK_DeclareBlock) {
        continue;
      }
      const auto &db = static_cast<const ast::DeclareBlock &>(*item);
      if (db.modifier() == ast::DeclareBlock::Modifier::Interface) {
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
