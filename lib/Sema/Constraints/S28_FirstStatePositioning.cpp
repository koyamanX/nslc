// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S28_FirstStatePositioning.cpp - S28 checker.
// Spec: lang.ebnf:986 — `first_state` rules:
//   - target must reference a `state_name` declared in enclosing proc;
//   - may appear at most once per proc.
//
// Implementation note: M2's parser drops `first_state` declarations
// inside proc bodies (the action-block dispatcher consumes them but
// emits no AST node). Without those decls in the AST, M3 cannot
// detect the violations. Stub registered for deterministic registry
// iteration order.

#include "../ConstraintCheckRegistry.h"

namespace nsl::sema {
namespace {

class S28Visitor : public ConstraintVisitor {
public:
  void run(const ConstraintContext &ctx) const override { (void)ctx; }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(28, S28Visitor)
