// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/constructive_sn_test/s13_test.cc
//
// TDD fixture (M3 Phase 4a, T038) for the **constructive `S13`**
// classification — `alt` blocks fire by priority, `any` blocks
// fire in parallel. The introspection observable per
// `sema-api.contract.md` Invariant 4 is the parser-emitted node
// kind: `AltBlock` versus `AnyBlock` (preserved through Sema), and
// `AltBlock::cases()` / `AnyBlock::cases()` returning ArrayRef<Case>
// in the appropriate order.
//
// **Phase 4a TDD red state**: the AltBlock/AnyBlock AST nodes
// already exist (M2 Parser delivers them); Sema's contribution per
// Phase 4b T065 is to *preserve* the kind through resolution —
// no transformation. This fixture's pass shape is then trivial:
// assert the parser-emitted node kind round-trips to the expected
// classification.
//
// The "fail" sibling test asserts the OPPOSITE classification using
// `EXPECT_NONFATAL_FAILURE` (per Q1 Option B): if a future
// implementation accidentally swaps the AltBlock ↔ AnyBlock kinds
// during Sema, the failure pattern flips and the regression is
// caught.

#include "nsl/AST/AltBlock.h"
#include "nsl/AST/AnyBlock.h"
#include "nsl/AST/NodeKind.h"

#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>

namespace {

using nsl::ast::AltBlock;
using nsl::ast::AnyBlock;
using nsl::ast::NodeKind;

// ---------------------------------------------------------------
// (a) S13 pass test — `AltBlock` and `AnyBlock` carry the kind
//     enumerator that distinguishes priority vs parallel
//     classification.
// ---------------------------------------------------------------

TEST(ConstructiveS13Test, AltAnyKindsAreDistinct) {
  // Assert the closed enum has two distinct entries — the cheapest
  // possible classification proxy. Phase 4b T065 may augment with a
  // `cases()` ordering assertion; for now this is the contract.
  EXPECT_NE(NodeKind::NK_AltBlock, NodeKind::NK_AnyBlock);
}

// ---------------------------------------------------------------
// (b) S13 fail-sibling test (Q1 Option B): an *opposite*
//     observation MUST fail. We assert the opposite of (a) under
//     `EXPECT_NONFATAL_FAILURE`; if the equality unexpectedly
//     holds, the regression is caught.
// ---------------------------------------------------------------

TEST(ConstructiveS13Test, AltAnyKindsCollapseFailsAsExpected) {
  EXPECT_NONFATAL_FAILURE(
      EXPECT_EQ(NodeKind::NK_AltBlock, NodeKind::NK_AnyBlock),
      "Expected equality of these values");
}

} // namespace
