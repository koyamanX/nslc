// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S21_ProcMethodsFinishInvoke.cpp - S21 checker.
// Spec: lang.ebnf:900 — built-in proc methods `finish` / `invoke`.
//   - bare form (`finish;` / `invoke;`) only inside a `proc` body.
//   - dotted form (`<inst>.finish()`) requires <inst> to resolve to
//     a `proc_name` declaration.

#include "../ConstraintCheckRegistry.h"
#include "ConstraintHelpers.h"
#include "nsl/AST/BareFinishStmt.h"
#include "nsl/AST/ControlCallStmt.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Sema/SymbolTable.h"

namespace nsl::sema {
namespace {

class S21Visitor : public ConstraintVisitor {
public:
  void run(const ConstraintContext &ctx) const override {
    if (ctx.unit == nullptr || ctx.diag == nullptr || ctx.symbols == nullptr) {
      return;
    }
    detail::walkUnit(
        *ctx.unit, /*dcb=*/nullptr, [&](const ast::Stmt &s, uint32_t lex) {
          if (s.kind() == ast::NodeKind::NK_BareFinishStmt) {
            if (!detail::has(lex, detail::LexCtx::InProc)) {
              ctx.diag->report(
                  Severity::Error, s.loc().begin(),
                  "'finish' / 'invoke' is a built-in proc method; "
                  "bare form is permitted only inside a 'proc' body "
                  "(S21)");
            }
            return;
          }
          if (s.kind() == ast::NodeKind::NK_ControlCallStmt) {
            const auto &cc = static_cast<const ast::ControlCallStmt &>(s);
            if (cc.target().parts.size() < 2) {
              return;
            }
            llvm::StringRef tail = cc.target().parts.back();
            if (tail != "finish" && tail != "invoke") {
              return;
            }
            llvm::StringRef head = cc.target().parts.front();
            Symbol *sym = ctx.symbols->lookup(head);
            if (sym == nullptr || sym->kind() != SymbolKind::SK_Proc) {
              ctx.diag->report(Severity::Error, s.loc().begin(),
                               "dotted form '<inst>.finish()' / "
                               "'<inst>.invoke()' requires '<inst>' to resolve "
                               "to a 'proc_name' declaration (S21)");
            }
          }
        });
  }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(21, S21Visitor)
