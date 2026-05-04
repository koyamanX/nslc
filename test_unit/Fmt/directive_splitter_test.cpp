// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/Fmt/directive_splitter_test.cpp
//
// TDD fixtures for the `DirectiveSplitter` pre-pass per
// `specs/010-t2-formatter-v0/contracts/cst-shape.contract.md` §1, §5
// + `data-model.md` §2 invariants.
//
// Tasks covered (`tasks.md`):
//   T011 — `IncludePassthrough`: a single `#include "foo.nsl"` line
//          becomes a Slice{Directive, opcode=Include, rawText=BYTES}.
//   T012 — `FullCoverage`: every byte of a 100-line synthetic file
//          is covered by exactly one Slice; ranges are monotonically
//          non-decreasing.
//   T013 — `LineContinuation`: `\\`-continued multi-line directives
//          produce ONE Slice spanning all continuation lines.
//
// Phase 2b will add CSTRoundTrip + CSTInvariants once CSTBuilder
// lands (T016, T020).

#include "DirectiveSplitter.h"
#include "CST.h"

#include "nsl/Basic/SourceLocation.h"

#include "llvm/ADT/StringRef.h"

#include "gtest/gtest.h"

#include <string>

using nsl::FileID;
using nsl::SourceRange;
using nsl::fmt::DirectiveSplitter;
using nsl::fmt::DirectiveTok;
using nsl::fmt::Slice;
using llvm::StringRef;

namespace {

// Mint a fresh FileID for tests. The DirectiveSplitter only uses
// FileID for SourceLocation packing — no SourceManager lookup
// happens — so any non-zero FileID is fine.
FileID kTestFileID = FileID(7);

// -----------------------------------------------------------------------------
// T011 — IncludePassthrough
// -----------------------------------------------------------------------------
TEST(DirectiveSplitterTest, IncludePassthrough) {
  StringRef source = "#include \"foo.nsl\"\n";
  DirectiveSplitter splitter;

  std::vector<Slice> slices = splitter.split(source, kTestFileID);

  ASSERT_EQ(slices.size(), 1u);
  EXPECT_EQ(slices[0].kind, Slice::Kind::Directive);
  EXPECT_EQ(slices[0].directive.opcode, DirectiveTok::Opcode::Include);
  EXPECT_EQ(slices[0].rawText, source);
  EXPECT_EQ(slices[0].directive.rawText, source);
}

// -----------------------------------------------------------------------------
// T012 — FullCoverage
// -----------------------------------------------------------------------------
//
// Synthesize a 100-line file with a mix of NSL fragments and
// directives, then assert:
//   (a) every byte is covered by exactly one Slice (no gaps, no
//       overlaps);
//   (b) Slice ranges are monotonically non-decreasing.
//
TEST(DirectiveSplitterTest, FullCoverage) {
  std::string source;
  for (int i = 0; i < 100; ++i) {
    if (i % 10 == 5) {
      // One directive every ten lines.
      source += "#define LINE_" + std::to_string(i) + " " +
                std::to_string(i) + "\n";
    } else {
      source += "wire x" + std::to_string(i) + "[8];\n";
    }
  }

  DirectiveSplitter splitter;
  std::vector<Slice> slices = splitter.split(source, kTestFileID);

  ASSERT_GE(slices.size(), 1u);

  // (a) Coverage: walk the slices and assert their byte ranges
  //     concatenate to the entire source buffer with no overlap.
  std::uint32_t cursor = 0;
  for (const Slice &s : slices) {
    EXPECT_EQ(s.range.begin().offset(), cursor)
        << "slice begin should equal previous slice end (no gap)";
    EXPECT_EQ(s.range.end().offset(),
              cursor + static_cast<std::uint32_t>(s.rawText.size()))
        << "slice end should equal begin + rawText.size() (no overlap)";
    cursor = s.range.end().offset();
  }
  EXPECT_EQ(cursor, source.size())
      << "last slice should reach end of source (no tail gap)";

  // (b) Monotonicity follows from (a) by construction; assert
  //     explicitly to catch any future contradiction.
  for (std::size_t i = 1; i < slices.size(); ++i) {
    EXPECT_LE(slices[i - 1].range.end().offset(),
              slices[i].range.begin().offset())
        << "slice " << i << " starts before previous slice ends";
  }
}

// -----------------------------------------------------------------------------
// T013 — LineContinuation
// -----------------------------------------------------------------------------
//
// A `\`-continued directive spanning three source lines must produce
// ONE Slice whose rawText is the entire span byte-for-byte.
//
TEST(DirectiveSplitterTest, LineContinuation) {
  StringRef source = "#define LONG_MACRO(x, y) \\\n"
                     "    ((x) + (y) + \\\n"
                     "     42)\n";
  DirectiveSplitter splitter;

  std::vector<Slice> slices = splitter.split(source, kTestFileID);

  ASSERT_EQ(slices.size(), 1u)
      << "continuation lines should NOT split into multiple slices";
  EXPECT_EQ(slices[0].kind, Slice::Kind::Directive);
  EXPECT_EQ(slices[0].directive.opcode, DirectiveTok::Opcode::Define);
  EXPECT_EQ(slices[0].rawText, source)
      << "rawText should preserve the entire multi-line span";
}

// -----------------------------------------------------------------------------
// Bonus invariant tests (not explicitly in tasks.md, but the
// FR-012a no-byte-loss invariant is important enough to gate here).
// -----------------------------------------------------------------------------

// Empty input → empty result (FR-008 idempotence root case).
TEST(DirectiveSplitterTest, EmptyInputProducesNoSlices) {
  DirectiveSplitter splitter;
  EXPECT_EQ(splitter.split({}, kTestFileID).size(), 0u);
}

// Pure NSL fragment with no directives → one NSLFragment slice
// covering the whole input.
TEST(DirectiveSplitterTest, AllNSLFragmentNoDirectives) {
  StringRef source = "module foo {\n  reg r[8];\n}\n";
  DirectiveSplitter splitter;
  std::vector<Slice> slices = splitter.split(source, kTestFileID);

  ASSERT_EQ(slices.size(), 1u);
  EXPECT_EQ(slices[0].kind, Slice::Kind::NSLFragment);
  EXPECT_EQ(slices[0].rawText, source);
}

// All nine directive opcodes are recognised.
TEST(DirectiveSplitterTest, AllNineOpcodesRecognised) {
  struct Case {
    StringRef            source;
    DirectiveTok::Opcode expected;
  };
  Case cases[] = {
      {"#include \"x\"\n", DirectiveTok::Opcode::Include},
      {"#define X 1\n",    DirectiveTok::Opcode::Define},
      {"#undef X\n",       DirectiveTok::Opcode::Undef},
      {"#ifdef X\n",       DirectiveTok::Opcode::Ifdef},
      {"#ifndef X\n",      DirectiveTok::Opcode::Ifndef},
      {"#if X==1\n",       DirectiveTok::Opcode::If},
      {"#else\n",          DirectiveTok::Opcode::Else},
      {"#endif\n",         DirectiveTok::Opcode::Endif},
      {"#line 42 \"f\"\n", DirectiveTok::Opcode::Line},
  };
  DirectiveSplitter splitter;
  for (const Case &c : cases) {
    std::vector<Slice> slices = splitter.split(c.source, kTestFileID);
    ASSERT_EQ(slices.size(), 1u) << "for input: " << c.source.str();
    EXPECT_EQ(slices[0].kind, Slice::Kind::Directive);
    EXPECT_EQ(slices[0].directive.opcode, c.expected)
        << "for input: " << c.source.str();
  }
}

} // namespace
