// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S25_GotoTwoKinds.cpp - S25 checker.
// Spec: lang.ebnf:944 — `goto` has two target kinds:
//   - inside a `seq` block, target is a label declared in same block;
//   - inside a `state` body, target is a state_name in scope.
//
// Implementation note: M2's parser drops `label_name` declarations
// (no AST node) and drops `state_name` decls inside proc bodies.
// Without those declarations surfacing in the AST, M3 Sema cannot
// classify goto targets. This stub registers an empty visitor so
// the constraint registry has an entry for S25 in deterministic
// Sn-numeric order.

#include "../ConstraintCheckRegistry.h"

namespace nsl::sema {
namespace {

class S25Visitor : public ConstraintVisitor {
public:
  void run(const ConstraintContext &ctx) const override { (void)ctx; }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(25, S25Visitor)
