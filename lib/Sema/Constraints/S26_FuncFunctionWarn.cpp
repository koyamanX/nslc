// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S26_FuncFunctionWarn.cpp - S26 checker.
// Spec: lang.ebnf:959 — `function` is accepted as a synonym for
// `func` but warned as non-canonical.
//
// Implementation note: FuncDefn AST doesn't carry which keyword was
// used (M2-frozen). The parser cooperates by emitting the locked
// S26 warning + FixIt at the parser site. This Sema TU registers a
// no-op visitor so the constraint registry's deterministic Sn-
// numeric iteration order has an entry for S26.

#include "../ConstraintCheckRegistry.h"

namespace nsl::sema {
namespace {

class S26Visitor : public ConstraintVisitor {
public:
  void run(const ConstraintContext &ctx) const override { (void)ctx; }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(26, S26Visitor)
