// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/Fmt/diff_emitter_test.cpp
//
// TDD fixtures for the LCS-based unified-diff emitter
// (`nsl::fmt::computeUnifiedDiff` + the public `emit_unified_diff`
// in `Fmt.h`) per `specs/010-t2-formatter-v0/data-model.md` §7
// + research §5.
//
// Tasks covered (`tasks.md`):
//   T073 — `EmptyOnIdentical`: identical inputs produce empty diff.
//   T074 — `HunkFormat`: one-hunk output matches the canonical
//          `--- a / +++ b / @@ -1,1 +1,1 @@ / -old / +new` shape.
//   T075 — `Determinism`: two calls with the same inputs produce
//          byte-identical output (Principle V).

#include "Diff.h"
#include "nsl/Fmt/Fmt.h"

#include "llvm/ADT/StringRef.h"

#include "gtest/gtest.h"

#include <string>

using llvm::StringRef;
using nsl::fmt::computeUnifiedDiff;

namespace {

// -----------------------------------------------------------------------------
// T073 — EmptyOnIdentical
// -----------------------------------------------------------------------------
TEST(DiffEmitterTest, EmptyOnIdentical) {
  EXPECT_EQ(computeUnifiedDiff(StringRef("a\nb\nc\n"),
                               StringRef("a\nb\nc\n"), "x", "y"),
            std::string{});
}

TEST(DiffEmitterTest, EmptyOnIdenticalEmpty) {
  EXPECT_EQ(computeUnifiedDiff(StringRef(""), StringRef(""), "x", "y"),
            std::string{});
}

// -----------------------------------------------------------------------------
// T074 — HunkFormat
// -----------------------------------------------------------------------------
TEST(DiffEmitterTest, HunkFormatSingleLineReplacement) {
  std::string out =
      computeUnifiedDiff(StringRef("old\n"), StringRef("new\n"), "a", "b");
  // Expected exact bytes:
  //   --- a
  //   +++ b
  //   @@ -1,1 +1,1 @@
  //   -old
  //   +new
  EXPECT_EQ(out, std::string("--- a\n+++ b\n@@ -1,1 +1,1 @@\n-old\n+new\n"));
}

TEST(DiffEmitterTest, HunkFormatPureAddition) {
  // Empty -> 2 lines: hunk shows +<line> for each.
  std::string out =
      computeUnifiedDiff(StringRef(""), StringRef("hello\nworld\n"),
                         "empty.txt", "filled.txt");
  // Old is empty (0 lines); new has 2 lines. Hunk:
  //   @@ -0,0 +1,2 @@
  //   +hello
  //   +world
  EXPECT_EQ(out,
            std::string("--- empty.txt\n+++ filled.txt\n"
                        "@@ -0,0 +1,2 @@\n+hello\n+world\n"));
}

TEST(DiffEmitterTest, HunkFormatContextLinesEmitted) {
  // 5 unchanged lines + 1 changed + 5 unchanged = 11 lines total.
  // Hunk should emit 3 context above + 1 changed + 3 context below
  // = 7 lines (lines 3-9 in 1-indexed terms).
  StringRef oldText = "1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n";
  StringRef newText = "1\n2\n3\n4\n5\n6X\n7\n8\n9\n10\n11\n";
  std::string out = computeUnifiedDiff(oldText, newText, "x", "y");
  // Header + hunk header + 7 lines.
  EXPECT_EQ(out,
            std::string("--- x\n+++ y\n"
                        "@@ -3,7 +3,7 @@\n"
                        " 3\n 4\n 5\n-6\n+6X\n 7\n 8\n 9\n"));
}

// -----------------------------------------------------------------------------
// T075 — Determinism
// -----------------------------------------------------------------------------
TEST(DiffEmitterTest, DeterminismRepeatedCalls) {
  StringRef oldText = "alpha\nbeta\ngamma\ndelta\n";
  StringRef newText = "alpha\nBETA\ngamma\ndelta\n";
  std::string out1 = computeUnifiedDiff(oldText, newText, "old", "new");
  std::string out2 = computeUnifiedDiff(oldText, newText, "old", "new");
  EXPECT_EQ(out1, out2);
  // Sanity: not the empty string (the inputs differ on line 2).
  EXPECT_FALSE(out1.empty());
}

// -----------------------------------------------------------------------------
// Bonus — public-API parity with `nsl::fmt::emit_unified_diff`
// -----------------------------------------------------------------------------
//
// The public Fmt.h entry point delegates to computeUnifiedDiff; the
// outputs MUST match for any inputs. This guards against future
// refactors that diverge the two paths.
TEST(DiffEmitterTest, PublicAPIParityWithInternalImpl) {
  StringRef oldText = "a\nb\nc\n";
  StringRef newText = "a\nB\nc\n";
  std::string viaInternal =
      computeUnifiedDiff(oldText, newText, "x", "y");
  std::string viaPublic =
      nsl::fmt::emit_unified_diff(oldText, newText, "x", "y");
  EXPECT_EQ(viaInternal, viaPublic);
}

} // namespace
