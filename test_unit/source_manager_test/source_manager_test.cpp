// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/source_manager_test/source_manager_test.cpp
//
// TDD fixtures for data-model entities 4 (private `Buffer`) and 5
// (`SourceManager`) per
// `specs/002-m1-lex-preprocess/data-model.md`. Authored RED before
// `include/nsl/Basic/SourceManager.h` exists.

#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"

#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/raw_ostream.h"

#include "gtest/gtest.h"
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using nsl::FileID;
using nsl::SourceLocation;
using nsl::SourceManager;

namespace {

std::vector<char> bytesOf(const char *literal) {
  std::vector<char> out;
  while (*literal) {
    out.push_back(*literal++);
  }
  return out;
}

class SourceManagerTest : public ::testing::Test {
protected:
  SourceManager sm;
};

TEST_F(SourceManagerTest, AddBufferInMemoryRoundTrip) {
  FileID fid = sm.addBufferInMemory("/virt/a.nsl", bytesOf("hello\nworld\n"));
  EXPECT_TRUE(fid.isValid());
  EXPECT_EQ(sm.getPath(fid), "/virt/a.nsl");
  // The buffer round-trips, NUL-terminator implementation detail not
  // exposed: getBuffer returns the size-N visible bytes.
  EXPECT_EQ(sm.getBuffer(fid).size(), 12U);
  EXPECT_EQ(sm.getBuffer(fid).str(), "hello\nworld\n");
}

TEST_F(SourceManagerTest, AddBufferAllocatesDistinctFileIDs) {
  FileID a = sm.addBufferInMemory("/virt/a.nsl", bytesOf("a"));
  FileID b = sm.addBufferInMemory("/virt/b.nsl", bytesOf("b"));
  EXPECT_NE(a.raw(), b.raw());
  EXPECT_TRUE(a.isValid());
  EXPECT_TRUE(b.isValid());
}

TEST_F(SourceManagerTest, GetLineColMultiLine) {
  // "abc\ndef\nghi"
  //  ^0  ^4   ^8
  FileID fid = sm.addBufferInMemory("/virt/a.nsl", bytesOf("abc\ndef\nghi"));
  // Offset 0 is line 1, column 1.
  auto p0 = sm.getLineCol(SourceLocation::make(fid, 0));
  EXPECT_EQ(p0.first, 1U);
  EXPECT_EQ(p0.second, 1U);

  // Offset 2 is line 1, column 3 ('c').
  auto p2 = sm.getLineCol(SourceLocation::make(fid, 2));
  EXPECT_EQ(p2.first, 1U);
  EXPECT_EQ(p2.second, 3U);

  // Offset 4 is line 2, column 1 ('d').
  auto p4 = sm.getLineCol(SourceLocation::make(fid, 4));
  EXPECT_EQ(p4.first, 2U);
  EXPECT_EQ(p4.second, 1U);

  // Offset 8 is line 3, column 1 ('g').
  auto p8 = sm.getLineCol(SourceLocation::make(fid, 8));
  EXPECT_EQ(p8.first, 3U);
  EXPECT_EQ(p8.second, 1U);

  // Offset 10 is line 3, column 3 ('i').
  auto p10 = sm.getLineCol(SourceLocation::make(fid, 10));
  EXPECT_EQ(p10.first, 3U);
  EXPECT_EQ(p10.second, 3U);
}

TEST_F(SourceManagerTest, GetLineReturnsLineSlice) {
  FileID fid = sm.addBufferInMemory("/virt/a.nsl", bytesOf("abc\ndefg\nhij"));
  // Offset 5 is on line 2 ("defg").
  llvm::StringRef line = sm.getLine(SourceLocation::make(fid, 5));
  EXPECT_EQ(line.str(), "defg");
  // First-line slice.
  llvm::StringRef first = sm.getLine(SourceLocation::make(fid, 0));
  EXPECT_EQ(first.str(), "abc");
  // Last line (no trailing newline).
  llvm::StringRef last = sm.getLine(SourceLocation::make(fid, 9));
  EXPECT_EQ(last.str(), "hij");
}

TEST_F(SourceManagerTest, AddLineDirectiveAndResolveVirtual) {
  FileID fid = sm.addBufferInMemory("/virt/a.nsl",
                                    bytesOf("line1\nline2\nline3\nline4\n"));
  // Without any #line directive, virtual = physical.
  auto v0 = sm.resolveVirtual(SourceLocation::make(fid, 0));
  EXPECT_EQ(v0.path.str(), "/virt/a.nsl");
  EXPECT_EQ(v0.line, 1U);
  EXPECT_EQ(v0.col, 1U);

  // After #line 100 "synth.v" applied at offset 6 (start of line 2),
  // every offset >= 6 reports synth.v at line 100 + (line-1).
  sm.addLineDirective(SourceLocation::make(fid, 6), 100, "synth.v");

  // Offset 6 is the first byte AFTER the directive; it's line 100 col 1.
  auto v6 = sm.resolveVirtual(SourceLocation::make(fid, 6));
  EXPECT_EQ(v6.path.str(), "synth.v");
  EXPECT_EQ(v6.line, 100U);
  EXPECT_EQ(v6.col, 1U);

  // Offset 12 is the start of physical line 3 ("line3"), so the
  // virtual line is 101.
  auto v12 = sm.resolveVirtual(SourceLocation::make(fid, 12));
  EXPECT_EQ(v12.path.str(), "synth.v");
  EXPECT_EQ(v12.line, 101U);

  // Offsets BEFORE the directive resolve to the physical path.
  auto v3 = sm.resolveVirtual(SourceLocation::make(fid, 3));
  EXPECT_EQ(v3.path.str(), "/virt/a.nsl");
  EXPECT_EQ(v3.line, 1U);
}

TEST_F(SourceManagerTest, AddLineDirectiveReusesPathOnEmptyArg) {
  FileID fid = sm.addBufferInMemory("/virt/a.nsl", bytesOf("a\nb\nc\nd\n"));
  // Empty virtual_path = reuse current path.
  sm.addLineDirective(SourceLocation::make(fid, 2), 50, "");
  auto v = sm.resolveVirtual(SourceLocation::make(fid, 2));
  EXPECT_EQ(v.path.str(), "/virt/a.nsl");
  EXPECT_EQ(v.line, 50U);
}

TEST(SourceManagerDeathTest, AddLineDirectiveRequiresStrictlyIncreasingOffset) {
  SourceManager sm;
  FileID fid = sm.addBufferInMemory("/virt/a.nsl", bytesOf("a\nb\nc\nd\n"));
  sm.addLineDirective(SourceLocation::make(fid, 2), 100, "x.v");
  // Re-inserting at the same or lower offset must abort.
  EXPECT_DEATH(
      { sm.addLineDirective(SourceLocation::make(fid, 2), 200, "y.v"); }, ".*");
  EXPECT_DEATH(
      { sm.addLineDirective(SourceLocation::make(fid, 1), 200, "y.v"); }, ".*");
}

TEST_F(SourceManagerTest, IncludeStackPushPopOrder) {
  FileID outer =
      sm.addBufferInMemory("/virt/outer.nsl", bytesOf("#include \"middle\"\n"));
  FileID middle =
      sm.addBufferInMemory("/virt/middle.nsl", bytesOf("#include \"inner\"\n"));
  FileID inner = sm.addBufferInMemory("/virt/inner.nsl", bytesOf("err\n"));

  // outer at offset 0 directly includes middle.
  sm.pushIncludeFrame(SourceLocation::make(outer, 0), middle);
  // middle at offset 0 includes inner.
  sm.pushIncludeFrame(SourceLocation::make(middle, 0), inner);

  // For a location in `inner`, the stack ancestry (innermost-first)
  // is [middle:include-site, outer:include-site].
  auto stack = sm.getIncludeStackFor(inner);
  ASSERT_EQ(stack.size(), 2U);
  EXPECT_EQ(stack[0].file().raw(), middle.raw());
  EXPECT_EQ(stack[1].file().raw(), outer.raw());

  // Pop the inner frame; now the active stack ancestor for `middle`
  // is just [outer:include-site].
  sm.popIncludeFrame();
  auto stack_after_pop = sm.getIncludeStackFor(middle);
  ASSERT_EQ(stack_after_pop.size(), 1U);
  EXPECT_EQ(stack_after_pop[0].file().raw(), outer.raw());
}

TEST_F(SourceManagerTest, LoadFileIdempotent) {
  // Write a temp file in $TMPDIR (writable in sandbox).
  const char *tmpdir = std::getenv("TMPDIR");
  std::string base = tmpdir ? tmpdir : "/tmp";
  std::string path = base + "/nslc_sm_test_load.nsl";
  {
    std::ofstream out(path);
    out << "alpha\nbeta\n";
  }

  llvm::ErrorOr<FileID> first = sm.loadFile(path);
  ASSERT_TRUE(static_cast<bool>(first));
  llvm::ErrorOr<FileID> second = sm.loadFile(path);
  ASSERT_TRUE(static_cast<bool>(second));
  // Loading the same canonical path twice returns the same FileID
  // (data-model entity 5 invariant).
  EXPECT_EQ(first.get().raw(), second.get().raw());
  EXPECT_EQ(sm.getBuffer(first.get()).str(), "alpha\nbeta\n");

  std::remove(path.c_str());
}

TEST_F(SourceManagerTest, LoadFileMissingReturnsError) {
  llvm::ErrorOr<FileID> r =
      sm.loadFile("/no/such/path/please/nslc/missing.nsl");
  EXPECT_FALSE(static_cast<bool>(r));
}

} // namespace
