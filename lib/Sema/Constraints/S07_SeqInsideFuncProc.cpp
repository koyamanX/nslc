// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S07_SeqInsideFuncProc.cpp - S7 checker.
// Spec: lang.ebnf:850 — `seq`/`while`/`for` blocks may appear only
// inside a function or procedure body.

#include "../ConstraintCheckRegistry.h"
#include "ConstraintHelpers.h"
#include "nsl/Basic/Diagnostic.h"

namespace nsl::sema {
namespace {

class S07Visitor : public ConstraintVisitor {
public:
  void run(const ConstraintContext &ctx) const override {
    if (ctx.unit == nullptr || ctx.diag == nullptr) {
      return;
    }
    detail::walkUnit(
        *ctx.unit, /*dcb=*/nullptr, [&](const ast::Stmt &s, uint32_t lex) {
          bool in_func_or_proc = detail::has(lex, detail::LexCtx::InFunc) ||
                                 detail::has(lex, detail::LexCtx::InProc);
          if (in_func_or_proc) {
            return;
          }
          if (s.kind() == ast::NodeKind::NK_SeqBlock) {
            // No fix-it: the previous attempt attached an empty
            // replacement to the entire `seq { ... }` range, which
            // would delete the block AND its contents on auto-apply
            // (Copilot review PR#8 line 33). The "right" fix-it
            // would remove only the `seq` keyword + braces while
            // preserving the body, but M2's SeqBlock AST doesn't
            // carry separate keyword + brace SourceRanges. The
            // diagnostic is strictly informational here.
            ctx.diag->report(Severity::Error, s.loc().begin(),
                             "'seq' block may appear only inside a function or "
                             "procedure body (S7)");
          } else if (s.kind() == ast::NodeKind::NK_WhileBlock) {
            ctx.diag->report(
                Severity::Error, s.loc().begin(),
                "'while' block may appear only inside a function or "
                "procedure body (S7)");
          } else if (s.kind() == ast::NodeKind::NK_ForBlock) {
            ctx.diag->report(Severity::Error, s.loc().begin(),
                             "'for' block may appear only inside a function or "
                             "procedure body (S7)");
          }
        });
  }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(7, S07Visitor)
