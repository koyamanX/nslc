// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S10_GenerateVarInteger.cpp - S10 checker.
// Spec: lang.ebnf:860 — `generate` loop variable must be an
// `integer` identifier.

#include "../ConstraintCheckRegistry.h"
#include "ConstraintHelpers.h"
#include "nsl/AST/StructuralGenerate.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Sema/SymbolTable.h"

namespace nsl::sema {
namespace {

class S10Visitor : public ConstraintVisitor {
public:
  void run(const ConstraintContext &ctx) const override {
    if (ctx.unit == nullptr || ctx.diag == nullptr || ctx.symbols == nullptr) {
      return;
    }
    detail::walkUnit(
        *ctx.unit, /*dcb=*/nullptr, [&](const ast::Stmt &s, uint32_t /*lex*/) {
          if (s.kind() != ast::NodeKind::NK_StructuralGenerate) {
            return;
          }
          const auto &sg = static_cast<const ast::StructuralGenerate &>(s);
          ast::Identifier name = sg.init();
          if (name.empty()) {
            return;
          }
          Symbol *sym = ctx.symbols->lookup(name);
          if (sym != nullptr && sym->kind() != SymbolKind::SK_Integer) {
            ctx.diag->report(Severity::Error, sg.loc().begin(),
                             "'generate' loop variable must be an 'integer' "
                             "identifier (S10)");
          }
        });
  }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(10, S10Visitor)
