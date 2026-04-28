// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S13_AltAnyClassification.cpp - constructive
// S13. Spec: lang.ebnf:870 — `alt` block fires by priority,
// `any` block fires in parallel (no order). Per Q1 Option B the
// observable is the parser-emitted node kind: `AltBlock` carries
// cases in priority/textual order, `AnyBlock` carries them in
// declaration order — both are exactly what the parser produces
// already (M2-frozen behavior). No diagnostic, no transformation
// needed: the constructive guarantee is inherited from the parser.
// This visitor is registered for the deterministic Sn-numeric
// iteration order in the constraint registry.

#include "../ConstraintCheckRegistry.h"

namespace nsl::sema {
namespace {

class S13Visitor : public ConstraintVisitor {
public:
  void run(const ConstraintContext &ctx) const override { (void)ctx; }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(13, S13Visitor)
