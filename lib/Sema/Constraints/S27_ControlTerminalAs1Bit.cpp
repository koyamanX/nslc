// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S27_ControlTerminalAs1Bit.cpp - constructive
// S27. Spec: lang.ebnf:967 — control-terminal taps in expression
// position evaluate to a 1-bit value.
//
// Implementation: the introspection observable is
// `Sema::classifyIdentifierExpr(IdentifierExpr&)` returning
// `ControlTerminalTap` for control-shaped Symbol kinds. The
// classifier is implemented in Sema.cpp; this visitor is registered
// for deterministic registry iteration order.

#include "../ConstraintCheckRegistry.h"

namespace nsl::sema {
namespace {

class S27Visitor : public ConstraintVisitor {
public:
  void run(const ConstraintContext &ctx) const override { (void)ctx; }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(27, S27Visitor)
