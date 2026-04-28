// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S02_WireNoInit.cpp - S2 checker.
// Spec: lang.ebnf:832 — `wire` may not have an initializer.
//
// Implementation note: the M2-frozen WireDecl AST node carries no
// `init` slot (per data-model §1.4 — only RegDecl has init). The
// parser cooperates with this Sema rule by accepting the `=`-init
// shape, discarding the parsed init expression, and emitting the
// frozen S2 diagnostic at the parser site (`lib/Parse/ParseDecl.cpp`
// `parseInternalDecl` wire branch). This Sema TU registers a
// no-op visitor so the constraint registry has an entry for S2 in
// the deterministic Sn-numeric iteration order — the actual rule
// lands at parse time.

#include "../ConstraintCheckRegistry.h"

namespace nsl::sema {
namespace {

class S02Visitor : public ConstraintVisitor {
public:
  void run(const ConstraintContext &ctx) const override { (void)ctx; }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(2, S02Visitor)
