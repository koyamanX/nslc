// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S16_ParamHDLOnly.cpp - S16 checker.
// Spec: lang.ebnf:880 — `param_int` / `param_str` are meaningful
// only for Verilog/VHDL/SystemC submodules; pure-NSL modules use
// `#define`.
//
// Per audited NSL practice (examples/19_param.nsl) and the spec
// note in `lang.ebnf §3.1`, top-level `param_int` / `param_str`
// IS the canonical pure-NSL compile-time-constant form — it does
// NOT violate S16. The S16 rule applies to `param_int` /
// `param_str` declared INSIDE a `declare` block (header_param
// position): those are connection-time parameters for the
// imported Verilog/VHDL/SystemC submodule the declare describes.
// In a pure-NSL declare (no HDL submodule attached) the
// header_param has no binding target.
//
// Phase 4b ships a permissive shape: S16 fires on every
// declare-block param. Once submodule-import metadata tracks the
// HDL flavor, refine to skip when the declare has an
// HDL-submodule attachment.

#include "../ConstraintCheckRegistry.h"
#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/DeclareBlock.h"
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
      if (!item || item->kind() != ast::NodeKind::NK_DeclareBlock) {
        continue;
      }
      const auto &db = static_cast<const ast::DeclareBlock &>(*item);
      for (const auto &p : db.headerParams()) {
        if (p && p->kind() == ast::NodeKind::NK_TopLevelParamDecl) {
          ctx.diag->report(Severity::Error, p->loc().begin(),
                           "'param_int' / 'param_str' is meaningful only "
                           "for Verilog/VHDL/SystemC submodules; pure-NSL "
                           "modules use '#define' (S16)");
        }
      }
    }
  }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(16, S16Visitor)
