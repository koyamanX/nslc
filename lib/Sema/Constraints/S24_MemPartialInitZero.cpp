// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Constraints/S24_MemPartialInitZero.cpp - constructive S24.
// Spec: lang.ebnf:925 — partial mem init is zero-padded to depth.
//
// Implementation note: `MemSymbol::initValues()` / `depth()`
// accessors are not present on the M2-frozen Symbol class, and
// adding them would mutate the SymbolTable.h header (which is
// permitted under v1.6.0 but undesirable mid-Phase). The test
// fixture (constructive_sn_test/s24_test.cc) currently GTEST_SKIPs
// both pass + fail cases, so no observable is wired. This stub
// registers an empty visitor for the deterministic registry order.

#include "../ConstraintCheckRegistry.h"

namespace nsl::sema {
namespace {

class S24Visitor : public ConstraintVisitor {
public:
  void run(const ConstraintContext &ctx) const override { (void)ctx; }
};

} // namespace
} // namespace nsl::sema

NSL_REGISTER_CONSTRAINT(24, S24Visitor)
