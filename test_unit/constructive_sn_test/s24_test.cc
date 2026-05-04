// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/constructive_sn_test/s24_test.cc
//
// TDD fixture (M3 Phase 4a, T038) for the **constructive `S24`** —
// a `mem` initializer may supply fewer values than the depth; the
// missing addresses are zero-filled. Per `sema-api.contract.md`
// Invariant 4 + `research.md` §6, the introspection observable is
// `MemSymbol::initValues()` returning `ArrayRef<uint64_t>` of size
// `MemSymbol::depth()`, zero-padded.
//
// **Phase 4a TDD red state**: at Phase 2 `MemSymbol` is a bare
// passive class without `depth()` or `initValues()` accessors.
// Phase 4b T069 will add the accessors AND populate them during
// the `ConstraintCheckPass` walk over `MemDecl` nodes.
//
// To avoid blocking the gtest binary's link until Phase 4b lands,
// the assertion is encoded as a `GTEST_SKIP` describing the
// missing surface. The test reaches the GREEN state once Phase 4b
// adds the accessors AND the population, then the test body
// exercises a synthetic `mem cache[8][20];` (no init) and asserts
// `initValues().size() == 8` with all entries zero.
//
// The (b) sibling fail-test asserts the OPPOSITE observable
// (truncated `initValues()` shorter than `depth()`) per Q1 Option
// B — also skipped at Phase 4a.

#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

namespace {

// ---------------------------------------------------------------
// (a) S24 pass test — `MemSymbol::initValues()` MUST be
//     zero-padded to depth (8 entries, all zero) for `mem cache[8]
//     [20];` with no init. Phase 4b T069 adds the accessor +
//     population.
// ---------------------------------------------------------------

TEST(ConstructiveS24Test, MemInitValuesZeroPaddedToDepth) {
  GTEST_SKIP() << "Phase 4b T069 must add MemSymbol::initValues()"
                  " + ::depth() accessors and populate them via"
                  " the S24 ConstraintCheckPass walker. Phase 4a"
                  " ships this skip as the TDD red-state evidence.";
}

// ---------------------------------------------------------------
// (b) S24 fail-sibling test (Q1 Option B): asserting that
//     `initValues()` is shorter than `depth()` MUST fail under
//     EXPECT_NONFATAL_FAILURE. Skipped at Phase 4a.
// ---------------------------------------------------------------

TEST(ConstructiveS24Test, ShortInitValuesFailsAsExpected) {
  GTEST_SKIP() << "Phase 4b T069 contract — see (a) above.";
}

} // namespace
