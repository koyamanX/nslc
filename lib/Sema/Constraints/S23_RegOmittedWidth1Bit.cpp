// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S23_RegOmittedWidth1Bit.cpp - constructive S23.
// Spec: lang.ebnf:919 — `reg <name> = <init>;` with omitted width
// is a 1-bit register with the given init.
//
// Implementation note: Phase 3's ResolutionPass::declReg already
// sets `RegSymbol::type = bitVector(1)` whenever `width == nullptr`.
// This visitor exists for the deterministic registry order; the
// constructive observable is satisfied by the resolution pass.

#include "../ConstraintCheckRegistry.h"

namespace nsl::sema {
namespace {

class S23Visitor : public ConstraintVisitor {
public:
  void run(const ConstraintContext &ctx) const override { (void)ctx; }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(23, S23Visitor)
