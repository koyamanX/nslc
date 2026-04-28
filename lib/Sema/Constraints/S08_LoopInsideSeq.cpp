// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S08_LoopInsideSeq.cpp - S8 checker.
// Spec: lang.ebnf:854 — `while`/`for` blocks may appear only inside
// a `seq` block.

#include "../ConstraintCheckRegistry.h"
#include "ConstraintHelpers.h"

#include "nsl/Basic/Diagnostic.h"

namespace nsl::sema {
namespace {

class S08Visitor : public ConstraintVisitor {
public:
  void run(const ConstraintContext &ctx) const override {
    if (ctx.unit == nullptr || ctx.diag == nullptr) {
      return;
    }
    detail::walkUnit(
        *ctx.unit, /*dcb=*/nullptr,
        [&](const ast::Stmt &s, uint32_t lex) {
          if (detail::has(lex, detail::LexCtx::InSeq)) {
            return;
          }
          // Only fire if we're at least inside a func/proc body —
          // otherwise S7 will already have fired and S8 is noise.
          bool in_func_or_proc =
              detail::has(lex, detail::LexCtx::InFunc) ||
              detail::has(lex, detail::LexCtx::InProc);
          if (!in_func_or_proc) {
            return;
          }
          if (s.kind() == ast::NodeKind::NK_WhileBlock) {
            ctx.diag->report(
                Severity::Error, s.loc().begin(),
                "'while' block may appear only inside a 'seq' block (S8)");
          } else if (s.kind() == ast::NodeKind::NK_ForBlock) {
            ctx.diag->report(
                Severity::Error, s.loc().begin(),
                "'for' block may appear only inside a 'seq' block (S8)");
          }
        });
  }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(8, S08Visitor)
