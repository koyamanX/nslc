// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S11_StateNameProcScoped.cpp - S11 checker.
// Spec: lang.ebnf:863 — a `state_name` declared inside a `proc` is
// scoped to that proc only.
//
// Implementation note: `state_name` declarations inside a `proc`
// body are parsed but DROPPED by the M2 parser (they're consumed by
// the action-block dispatcher, which only emits AST nodes for the
// stmt forms). Consequently the AST visible to M3 Sema doesn't
// contain StateNameDecls at the proc-body level; we cannot detect
// the use-from-outside-proc shape until that parser limitation is
// addressed. This stub registers an empty visitor so the constraint
// registry's deterministic Sn-numeric iteration order has an entry
// for S11.

#include "../ConstraintCheckRegistry.h"

namespace nsl::sema {
namespace {

class S11Visitor : public ConstraintVisitor {
public:
  void run(const ConstraintContext &ctx) const override { (void)ctx; }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(11, S11Visitor)
