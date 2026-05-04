// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S16_ParamHDLOnly.cpp - S16 checker.
// Spec: lang.ebnf:880 — `param_int` / `param_str` are meaningful
// only for Verilog/VHDL/SystemC submodules; pure-NSL modules use
// `#define`.
//
// Per the 2026-05-04 §3.1 + S16 alignment amendment, top-level
// `param_int` / `param_str` declarations are meaningful only as
// the source value bound to a Verilog/VHDL/SystemC submodule's
// connection-time parameter slot at instantiation. The HDL-foreign
// submodule import infrastructure is not implemented yet (no AST
// flag distinguishes HDL-foreign `declare` blocks from pure-NSL
// declare/module pairs), so every top-level param declaration is
// currently dead code per the audited corpus (none of the seven
// audited NSL projects use top-level params for pure-NSL contexts;
// the HDL-foreign use sites are out-of-scope for this milestone).
// Fire S16 unconditionally on any `TopLevelParamDecl`. When the
// HDL-foreign submodule import infrastructure lands in a future
// milestone, this checker gains a "param is bound to an HDL
// submodule's parameter slot" exemption path.

#include "../ConstraintCheckRegistry.h"
#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/TopLevelParamDecl.h"
#include "nsl/Basic/Diagnostic.h"

namespace nsl::sema {
namespace {

class S16Visitor : public ConstraintVisitor {
public:
  void run(const ConstraintContext &ctx) const override {
    if (ctx.unit == nullptr || ctx.diag == nullptr) {
      return;
    }
    for (const auto &item : ctx.unit->items()) {
      if (!item) {
        continue;
      }
      if (item->kind() != ast::NodeKind::NK_TopLevelParamDecl) {
        continue;
      }
      ctx.diag->report(
          Severity::Error, item->loc().begin(),
          "'param_int' / 'param_str' is meaningful only for Verilog/"
          "VHDL/SystemC submodules; pure-NSL modules use '#define' (S16)");
    }
  }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(16, S16Visitor)
