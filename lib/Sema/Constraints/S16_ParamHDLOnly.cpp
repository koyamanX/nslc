// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S16_ParamHDLOnly.cpp - S16 checker.
// Spec: lang.ebnf:880 — `param_int` / `param_str` are meaningful
// only for Verilog/VHDL/SystemC submodules; pure-NSL modules use
// `#define`.
//
// At M3 the AST does not carry an HDL-flavor flag on submodules, so
// this checker fires whenever a `param_int` / `param_str` declaration
// appears at compilation-unit top level outside of a submodule
// import context. The fixture s16/fail.nsl exercises this case.

#include "../ConstraintCheckRegistry.h"

#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/ModuleBlock.h"
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
    // Heuristic: the unit contains BOTH a top-level param_int/param_str
    // AND a pure-NSL ModuleBlock. If so, the param has no HDL submodule
    // to attach to → fire S16.
    bool has_top_param = false;
    SourceLocation param_loc;
    bool has_pure_module = false;
    for (const auto &item : ctx.unit->items()) {
      if (!item) {
        continue;
      }
      if (item->kind() == ast::NodeKind::NK_TopLevelParamDecl) {
        has_top_param = true;
        param_loc = item->loc().begin();
      } else if (item->kind() == ast::NodeKind::NK_ModuleBlock) {
        has_pure_module = true;
      }
    }
    if (has_top_param && has_pure_module) {
      ctx.diag->report(
          Severity::Error, param_loc,
          "'param_int' / 'param_str' is meaningful only for Verilog/"
          "VHDL/SystemC submodules; pure-NSL modules use '#define' (S16)");
    }
  }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(16, S16Visitor)
