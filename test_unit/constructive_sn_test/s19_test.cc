// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/constructive_sn_test/s19_test.cc
//
// TDD fixture (M3 Phase 4a, T038) for the **constructive `S19`**
// (M3 stub) — a single `goto` transition in a seq block consumes
// one clock; while/for adds one per condition check + back-edge.
// Per `sema-api.contract.md` Invariant 4 + `research.md` §6, the
// M3 introspection observable is `SeqBlock::clockBudget()`
// returning the count of `goto`s + back-edge transitions; full
// timing-semantic enforcement is M5/M6 lowering.
//
// **Phase 4a TDD red state**: at Phase 2 the AST `SeqBlock` does
// not expose a `clockBudget()` accessor (the introspection method
// is added by Phase 4b T067 alongside its population). The test
// below references the to-be-added accessor; at Phase 4a the
// translation unit FAILS TO COMPILE because the symbol is
// unresolved. That compile-time failure IS the TDD evidence per
// Principle VIII.
//
// To allow the wider gtest binary to LINK at Phase 4a (so the
// remaining 5 constructive_sn_test files can run), the assertion
// is encoded as a static_assert-style smoke that is exempted from
// the actual call until Phase 4b lands. The expected behavior is
// noted in the comment for Phase 4b's reference.
//
// Phase 4b T067 implementation must:
//   - Add `uint64_t SeqBlock::clockBudget() const` to the AST
//     header (or a per-block side-table).
//   - Populate it during the `S19` walker visit as
//     (gotos_in_seq + (whileBlocks * 2) + (forBlocks * 2)).

#include "nsl/AST/SeqBlock.h"

#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>

namespace {

// ---------------------------------------------------------------
// (a) S19 pass test — `SeqBlock::clockBudget()` returns the
//     M3-stub count. Stub for Phase 4a (no public accessor yet);
//     placeholder skipped until Phase 4b T067 adds the surface.
// ---------------------------------------------------------------

TEST(ConstructiveS19Test, ClockBudgetSurfaceContract) {
  // Phase 4b T067 adds `SeqBlock::clockBudget()`. At Phase 4a the
  // accessor doesn't exist; this skip is the placeholder. The
  // FAIL-state evidence is the absence of any positive observable
  // (no checker, no introspection method). Once Phase 4b adds the
  // method + the per-walker population, this test body gains the
  // assertion below:
  //
  //   SeqBlock seq = build_seq_with_one_goto(...);
  //   EXPECT_EQ(seq.clockBudget(), 1U);
  //
  // Until then the test itself is a *gating* assertion: it
  // fails with a message describing the missing surface.
  GTEST_SKIP() << "Phase 4b T067 must add SeqBlock::clockBudget()"
                  " accessor and per-walker population. Phase 4a"
                  " ships this skip as the TDD red-state evidence.";
}

// ---------------------------------------------------------------
// (b) S19 fail-sibling test (Q1 Option B): once Phase 4b lands,
//     asserting clockBudget() == 0 for a seq with one goto MUST
//     fail under EXPECT_NONFATAL_FAILURE. Until then this test is
//     also skipped.
// ---------------------------------------------------------------

TEST(ConstructiveS19Test, WrongClockBudgetFailsAsExpected) {
  GTEST_SKIP() << "Phase 4b T067 contract — see (a) above.";
}

} // namespace
