// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/source_manager_test/source_location_test.cpp
//
// TDD fixtures for data-model entities 1–3 (SourceLocation,
// SourceRange, FileID) per
// `specs/002-m1-lex-preprocess/data-model.md`. Authored RED before
// `include/nsl/Basic/SourceLocation.h` exists.

#include "nsl/Basic/SourceLocation.h"

#include <cstdint>

#include "gtest/gtest.h"

using nsl::FileID;
using nsl::SourceLocation;
using nsl::SourceRange;

namespace {

// FileID is constructed by the SourceManager in production; tests
// invoke the public constructor directly to exercise the type in
// isolation.

TEST(FileIDTest, DefaultIsInvalid) {
  FileID f;
  EXPECT_FALSE(f.isValid());
  EXPECT_EQ(f.raw(), 0u);
}

TEST(FileIDTest, NonZeroIsValid) {
  FileID f(1);
  EXPECT_TRUE(f.isValid());
  EXPECT_EQ(f.raw(), 1u);
}

TEST(FileIDTest, EqualityOperator) {
  FileID a(2);
  FileID b(2);
  FileID c(3);
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a == c);
}

TEST(SourceLocationTest, DefaultIsInvalid) {
  SourceLocation loc;
  EXPECT_FALSE(loc.isValid());
}

TEST(SourceLocationTest, MakeStoresFileIDAndOffset) {
  FileID fid(1);
  SourceLocation loc = SourceLocation::make(fid, 42);
  EXPECT_TRUE(loc.isValid());
  EXPECT_EQ(loc.file().raw(), fid.raw());
  EXPECT_EQ(loc.offset(), 42u);
}

TEST(SourceLocationTest, MakeAcceptsMaxOffset) {
  FileID fid(1);
  // 24-bit offset field allows offsets in [0, 2^24).
  uint32_t kMax = (1u << 24) - 1;
  SourceLocation loc = SourceLocation::make(fid, kMax);
  EXPECT_TRUE(loc.isValid());
  EXPECT_EQ(loc.offset(), kMax);
  EXPECT_EQ(loc.file().raw(), fid.raw());
}

TEST(SourceLocationDeathTest, MakeRejectsOffsetAtSixteenMib) {
  FileID fid(1);
  EXPECT_DEATH({ (void)SourceLocation::make(fid, 1u << 24); }, ".*");
}

TEST(SourceLocationTest, EqualityWithinSameFile) {
  FileID fid(7);
  SourceLocation a = SourceLocation::make(fid, 100);
  SourceLocation b = SourceLocation::make(fid, 100);
  SourceLocation c = SourceLocation::make(fid, 101);
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a == c);
}

TEST(SourceLocationTest, TotalOrderByFileThenOffset) {
  FileID f1(1);
  FileID f2(2);
  SourceLocation a = SourceLocation::make(f1, 100);
  SourceLocation b = SourceLocation::make(f1, 200);
  SourceLocation c = SourceLocation::make(f2, 50);
  // Within a file: lower offset < higher offset.
  EXPECT_TRUE(a < b);
  EXPECT_FALSE(b < a);
  // Across files: lower file id < higher file id, regardless of offset.
  EXPECT_TRUE(b < c);
  EXPECT_FALSE(c < b);
}

TEST(SourceRangeTest, DefaultIsInvalid) {
  SourceRange r;
  EXPECT_FALSE(r.isValid());
}

TEST(SourceRangeTest, ConstructAndQuery) {
  FileID fid(1);
  SourceLocation begin = SourceLocation::make(fid, 10);
  SourceLocation end = SourceLocation::make(fid, 25);
  SourceRange r(begin, end);
  EXPECT_TRUE(r.isValid());
  EXPECT_TRUE(r.begin() == begin);
  EXPECT_TRUE(r.end() == end);
  EXPECT_EQ(r.length(), 15u);
}

TEST(SourceRangeTest, EmptyRangeAllowed) {
  FileID fid(1);
  SourceLocation p = SourceLocation::make(fid, 5);
  SourceRange r(p, p);
  EXPECT_TRUE(r.isValid());
  EXPECT_EQ(r.length(), 0u);
}

TEST(SourceRangeTest, ContainsHalfOpen) {
  FileID fid(1);
  SourceLocation begin = SourceLocation::make(fid, 10);
  SourceLocation mid = SourceLocation::make(fid, 15);
  SourceLocation end = SourceLocation::make(fid, 20);
  SourceRange r(begin, end);
  EXPECT_TRUE(r.contains(begin));
  EXPECT_TRUE(r.contains(mid));
  // Half-open: end is NOT contained.
  EXPECT_FALSE(r.contains(end));
}

TEST(SourceRangeDeathTest, RejectsCrossFileEndpoints) {
  FileID f1(1);
  FileID f2(2);
  SourceLocation a = SourceLocation::make(f1, 0);
  SourceLocation b = SourceLocation::make(f2, 0);
  EXPECT_DEATH({ SourceRange r(a, b); (void)r; }, ".*");
}

TEST(SourceRangeDeathTest, RejectsBeginAfterEnd) {
  FileID fid(1);
  SourceLocation a = SourceLocation::make(fid, 100);
  SourceLocation b = SourceLocation::make(fid, 50);
  EXPECT_DEATH({ SourceRange r(a, b); (void)r; }, ".*");
}

TEST(SourceRangeTest, InvalidIfEitherEndpointInvalid) {
  FileID fid(1);
  SourceLocation valid = SourceLocation::make(fid, 0);
  SourceLocation invalid;
  // The constructor preconditions reject invalid endpoints; isValid()
  // false comes from the default SourceRange().
  SourceRange r;
  EXPECT_FALSE(r.isValid());
  // Confirm a valid range remains valid.
  SourceRange ok(valid, valid);
  EXPECT_TRUE(ok.isValid());
  // Suppress unused-variable warnings on `invalid`.
  (void)invalid;
}

} // namespace
