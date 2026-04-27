// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/diagnostic_engine_test/diagnostic_engine_text_test.cpp
//
// FR-025 / SC-004: every emitted diagnostic renders in the canonical
// `<path>:<line>:<col>: <severity>: <message>` form. Severity is
// lowercase; message is non-empty. Authored RED before
// `nsl/Basic/Diagnostic.h` exists.

#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"

#include "llvm/Support/raw_ostream.h"

#include "gtest/gtest.h"
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using nsl::DiagnosticEngine;
using nsl::FileID;
using nsl::Severity;
using nsl::SourceLocation;
using nsl::SourceManager;

namespace {

std::vector<char> bytesOf(const char *s) {
  std::vector<char> out;
  while (*s) {
    out.push_back(*s++);
  }
  return out;
}

std::string render(DiagnosticEngine &diag, DiagnosticEngine::Format fmt) {
  std::string buf;
  llvm::raw_string_ostream os(buf);
  diag.renderAll(os, fmt);
  os.flush();
  return buf;
}

TEST(DiagnosticEngineTextTest, EmitsCanonicalFormat) {
  SourceManager sm;
  FileID fid = sm.addBufferInMemory("a.nsl", bytesOf("abc\ndef\n"));
  DiagnosticEngine diag(sm);
  diag.report(Severity::Error, SourceLocation::make(fid, 4),
              "unterminated string literal");

  std::string out = render(diag, DiagnosticEngine::Format::Text);
  // FR-025 / SC-004: every diagnostic line MUST match the regex.
  std::regex line_regex("^[^:]+:\\d+:\\d+: (error|warning|note): .+$");

  // Split on newlines and check the first line of the first diagnostic.
  std::istringstream lines(out);
  std::string first_line;
  std::getline(lines, first_line);

  EXPECT_TRUE(std::regex_match(first_line, line_regex))
      << "first_line: " << first_line;
  EXPECT_NE(first_line.find("a.nsl:2:1: error: unterminated string literal"),
            std::string::npos)
      << "first_line: " << first_line;
}

TEST(DiagnosticEngineTextTest, SeverityRenderedLowercase) {
  SourceManager sm;
  FileID fid = sm.addBufferInMemory("x.nsl", bytesOf("hello"));
  DiagnosticEngine diag(sm);
  diag.report(Severity::Warning, SourceLocation::make(fid, 0), "wrn-msg");
  diag.report(Severity::Note, SourceLocation::make(fid, 1), "note-msg");
  diag.report(Severity::Error, SourceLocation::make(fid, 2), "err-msg");

  std::string out = render(diag, DiagnosticEngine::Format::Text);
  EXPECT_NE(out.find(" warning: "), std::string::npos);
  EXPECT_NE(out.find(" note: "), std::string::npos);
  EXPECT_NE(out.find(" error: "), std::string::npos);
  // No uppercase variant should leak through.
  EXPECT_EQ(out.find(" Error: "), std::string::npos);
  EXPECT_EQ(out.find(" Warning: "), std::string::npos);
}

TEST(DiagnosticEngineTextTest, NumErrorsAndWarningsCount) {
  SourceManager sm;
  FileID fid = sm.addBufferInMemory("c.nsl", bytesOf("abcdef"));
  DiagnosticEngine diag(sm);
  EXPECT_EQ(diag.numErrors(), 0u);
  EXPECT_EQ(diag.numWarnings(), 0u);

  diag.report(Severity::Error, SourceLocation::make(fid, 0), "e1");
  diag.report(Severity::Warning, SourceLocation::make(fid, 1), "w1");
  diag.report(Severity::Note, SourceLocation::make(fid, 2), "n1");
  diag.report(Severity::Error, SourceLocation::make(fid, 3), "e2");

  EXPECT_EQ(diag.numErrors(), 2u);
  EXPECT_EQ(diag.numWarnings(), 1u);
  EXPECT_TRUE(diag.hasError());
}

TEST(DiagnosticEngineTextTest, ClearResetsBuffer) {
  SourceManager sm;
  FileID fid = sm.addBufferInMemory("c.nsl", bytesOf("abc"));
  DiagnosticEngine diag(sm);
  diag.report(Severity::Error, SourceLocation::make(fid, 0), "x");
  EXPECT_EQ(diag.numErrors(), 1u);
  diag.clear();
  EXPECT_EQ(diag.numErrors(), 0u);
  EXPECT_EQ(diag.numWarnings(), 0u);
  EXPECT_FALSE(diag.hasError());
}

TEST(DiagnosticEngineTextTest, AllRenderedLinesMatchRegex) {
  SourceManager sm;
  FileID fid = sm.addBufferInMemory("multi.nsl",
                                    bytesOf("line1\nline2\nline3\nline4\n"));
  DiagnosticEngine diag(sm);
  diag.report(Severity::Error, SourceLocation::make(fid, 0), "e1");
  diag.report(Severity::Warning, SourceLocation::make(fid, 6), "w1");
  diag.report(Severity::Note, SourceLocation::make(fid, 12), "n1");

  std::string out = render(diag, DiagnosticEngine::Format::Text);
  std::regex line_regex("^[^:]+:\\d+:\\d+: (error|warning|note): .+$");

  // Walk every line that begins a diagnostic (starts with the path);
  // skip trailing context lines (source-line + caret) which are not
  // required to match the regex.
  std::istringstream lines(out);
  std::string line;
  size_t header_count = 0;
  while (std::getline(lines, line)) {
    // Header lines start with `multi.nsl:` (the path).
    if (line.rfind("multi.nsl:", 0) == 0) {
      EXPECT_TRUE(std::regex_match(line, line_regex)) << "line: " << line;
      ++header_count;
    }
  }
  EXPECT_EQ(header_count, 3u);
}

} // namespace
