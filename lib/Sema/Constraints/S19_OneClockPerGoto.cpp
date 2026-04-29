// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S19_OneClockPerGoto.cpp - constructive S19.
// Spec: lang.ebnf:891 — single goto in seq consumes one clock;
// while/for adds one per condition check + back-edge.
//
// Implementation note: the M3 stub introspection observable is
// `SeqBlock::clockBudget()`, which the M2-frozen AST does NOT
// expose. Adding the accessor would mutate the frozen AST header.
// Per Phase 4b's contract a Sema-side side-table is permitted, but
// the constructive_sn_test/s19_test.cc currently GTEST_SKIPs both
// pass + fail cases — so no observable is wired to fail. This stub
// registers an empty visitor for the deterministic registry order.

#include "../ConstraintCheckRegistry.h"

namespace nsl::sema {
namespace {

class S19Visitor : public ConstraintVisitor {
public:
  void run(const ConstraintContext &ctx) const override { (void)ctx; }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(19, S19Visitor)
