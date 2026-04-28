// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S06_ProcArgRegOnly.cpp - S6 checker.
// Spec: lang.ebnf:848 — `proc_name` arguments must be `reg`.

#include "../ConstraintCheckRegistry.h"
#include "ConstraintHelpers.h"

#include "nsl/AST/ProcNameDecl.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Sema/SymbolTable.h"

namespace nsl::sema {
namespace {

class S06Visitor : public ConstraintVisitor {
public:
  void run(const ConstraintContext &ctx) const override {
    if (ctx.unit == nullptr || ctx.diag == nullptr ||
        ctx.symbols == nullptr) {
      return;
    }
    detail::walkUnit(
        *ctx.unit,
        [&](const ast::Decl &d, uint32_t /*lex*/) {
          if (d.kind() != ast::NodeKind::NK_ProcNameDecl) {
            return;
          }
          const auto &pn = static_cast<const ast::ProcNameDecl &>(d);
          for (auto arg : pn.regArgs()) {
            Symbol *sym = ctx.symbols->lookup(arg);
            if (sym != nullptr && sym->kind() != SymbolKind::SK_Reg) {
              ctx.diag->report(
                  Severity::Error, pn.loc().begin(),
                  "'proc_name' arguments must be 'reg' identifiers (S6)");
            }
          }
        },
        /*scb=*/nullptr);
  }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(6, S06Visitor)
