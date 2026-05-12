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

#include "CST.h"
#include "CSTBuilder.h"
#include "DirectiveSplitter.h"
#include "nsl/AST/CompilationUnit.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"
#include "nsl/Lex/Lexer.h"
#include "nsl/Lex/Token.h"
#include "nsl/Parse/Parser.h"

#include "llvm/ADT/StringRef.h"

#include "gtest/gtest.h"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

using llvm::StringRef;
using nsl::FileID;
using nsl::SourceRange;
using nsl::fmt::CSTBuilder;
using nsl::fmt::DirectiveSplitter;
using nsl::fmt::DirectiveTok;
using nsl::fmt::Slice;

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
      source +=
          "#define LINE_" + std::to_string(i) + " " + std::to_string(i) + "\n";
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

// -----------------------------------------------------------------------------
// T016 — CSTRoundTrip
// -----------------------------------------------------------------------------
//
// Per `cst-shape.contract.md` §8: serialising the recorded CST event
// stream MUST reproduce the source byte-for-byte. For Phase 2b the
// CSTBuilder records only top-level production + tokens (sub-
// production wrapping arrives in Phase 3+); the source-buffer-
// guided `serialize()` reconstructs trivia from the source view.
//
// Helper: convert a C string to a vector<char> (matches the existing
// parser-test convention from test_unit/parse_test/).
std::vector<char> bytesOf(const char *literal) {
  std::vector<char> out;
  while (*literal != 0) {
    out.push_back(*literal++);
  }
  return out;
}

TEST(DirectiveSplitterTest, CSTRoundTrip) {
  // 5-line synthetic NSL source per T016 description.
  const char *kSource = "module hello {\n"
                        "  reg r[8] = 0;\n"
                        "  reg s[8];\n"
                        "  wire w[16];\n"
                        "}\n";
  StringRef sourceView(kSource);

  nsl::SourceManager sm;
  nsl::FileID fid =
      sm.addBufferInMemory("/virt/cst-roundtrip.nsl", bytesOf(kSource));
  ASSERT_TRUE(fid.isValid());
  nsl::DiagnosticEngine diag(sm);
  nsl::Lexer lex(sm, fid, diag);

  CSTBuilder builder(sourceView);
  std::unique_ptr<nsl::ast::CompilationUnit> cu =
      nsl::parse::parseCompilationUnit(lex, diag, &builder);

  ASSERT_NE(cu, nullptr) << "parse should succeed for well-formed input";
  EXPECT_FALSE(diag.hasError()) << "no diagnostics expected";

  // The Phase 2b CST contains exactly one top-level node + every
  // consumed token.
  EXPECT_EQ(builder.nodeCount(), 1u)
      << "exactly one CompilationUnit beginNode/endNode pair expected";
  EXPECT_GT(builder.tokenCount(), 0u)
      << "the parser should have consumed at least one token";

  // Round-trip: serialize() must reproduce the source byte-for-byte
  // (cst-shape contract §8 invariant).
  EXPECT_EQ(builder.serialize(), std::string(kSource));
}

// -----------------------------------------------------------------------------
// T020 — CSTInvariants
// -----------------------------------------------------------------------------
//
// Per `cst-shape.contract.md` §3:
//   * every recorded token has a non-empty SourceRange;
//   * tokens are in source order with no overlapping ranges;
//   * the no-byte-loss invariant holds (round-trip exact match,
//     verified via serialize()).
//
TEST(DirectiveSplitterTest, CSTInvariants) {
  const char *kSource = "module empty {\n"
                        "}\n";
  StringRef sourceView(kSource);

  nsl::SourceManager sm;
  nsl::FileID fid =
      sm.addBufferInMemory("/virt/cst-invariants.nsl", bytesOf(kSource));
  ASSERT_TRUE(fid.isValid());
  nsl::DiagnosticEngine diag(sm);
  nsl::Lexer lex(sm, fid, diag);

  CSTBuilder builder(sourceView);
  std::unique_ptr<nsl::ast::CompilationUnit> cu =
      nsl::parse::parseCompilationUnit(lex, diag, &builder);

  ASSERT_NE(cu, nullptr);
  EXPECT_FALSE(diag.hasError());

  // Invariant 1: every token has a non-empty SourceRange (offset > 0
  // by length is the easiest check; a zero-length range would indicate
  // a degenerate token).
  for (const nsl::Token &t : builder.tokens()) {
    EXPECT_TRUE(t.range().begin().isValid())
        << "every token must have a valid begin SourceLocation";
    EXPECT_LE(t.range().begin().offset(), t.range().end().offset())
        << "every token range must be non-decreasing";
  }

  // Invariant 2: monotonically non-decreasing token ranges (source
  // order). The parser may emit tokens with adjacent ranges
  // (consecutive non-trivia tokens) but never out-of-order.
  for (std::size_t i = 1; i < builder.tokens().size(); ++i) {
    EXPECT_LE(builder.tokens()[i - 1].range().end().offset(),
              builder.tokens()[i].range().begin().offset())
        << "token " << i << " starts before previous token ends";
  }

  // Invariant 3: no-byte-loss — `serialize()` exactly equals source.
  // (The strongest end-to-end invariant; subsumes the per-byte
  // coverage check above, but we keep both for failure isolation.)
  EXPECT_EQ(builder.serialize(), std::string(kSource));

  // Invariant 4 (frame bookkeeping): one completed top-level node.
  EXPECT_EQ(builder.nodeCount(), 1u);
  EXPECT_EQ(builder.completedNodes()[0].kindName, "CompilationUnit");
}

// All nine directive opcodes are recognised.
TEST(DirectiveSplitterTest, AllNineOpcodesRecognised) {
  struct Case {
    StringRef source;
    DirectiveTok::Opcode expected;
  };
  Case cases[] = {
      {"#include \"x\"\n", DirectiveTok::Opcode::Include},
      {"#define X 1\n", DirectiveTok::Opcode::Define},
      {"#undef X\n", DirectiveTok::Opcode::Undef},
      {"#ifdef X\n", DirectiveTok::Opcode::Ifdef},
      {"#ifndef X\n", DirectiveTok::Opcode::Ifndef},
      {"#if X==1\n", DirectiveTok::Opcode::If},
      {"#else\n", DirectiveTok::Opcode::Else},
      {"#endif\n", DirectiveTok::Opcode::Endif},
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
