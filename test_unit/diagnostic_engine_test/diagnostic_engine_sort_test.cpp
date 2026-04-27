// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/diagnostic_engine_test/diagnostic_engine_sort_test.cpp
//
// FR-039 / research §4: `renderAll` MUST sort by `(SourceLocation,
// Severity)` regardless of emit order. Two consecutive `renderAll`
// calls on the same buffer MUST produce byte-identical output.

#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"

#include "llvm/Support/raw_ostream.h"

#include "gtest/gtest.h"
#include <cstddef>
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
  while (*s != 0) {
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

TEST(DiagnosticEngineSortTest, SortsByLocationAtRenderTime) {
  SourceManager sm;
  FileID const fid =
      sm.addBufferInMemory("s.nsl", bytesOf("aaaa\nbbbb\ncccc\ndddd\neeee\n"));
  DiagnosticEngine diag(sm);

  // Emit out of order: B, A, C (B first, then earlier A, then later C).
  diag.report(Severity::Error, SourceLocation::make(fid, 5), "B");
  diag.report(Severity::Error, SourceLocation::make(fid, 0), "A");
  diag.report(Severity::Error, SourceLocation::make(fid, 10), "C");

  std::string const out = render(diag, DiagnosticEngine::Format::Text);
  // Expect A-line before B-line before C-line.
  size_t const a_pos = out.find(" error: A");
  size_t const b_pos = out.find(" error: B");
  size_t const c_pos = out.find(" error: C");
  ASSERT_NE(a_pos, std::string::npos);
  ASSERT_NE(b_pos, std::string::npos);
  ASSERT_NE(c_pos, std::string::npos);
  EXPECT_LT(a_pos, b_pos);
  EXPECT_LT(b_pos, c_pos);
}

TEST(DiagnosticEngineSortTest, RenderAllIsIdempotent) {
  SourceManager sm;
  FileID const fid =
      sm.addBufferInMemory("s.nsl", bytesOf("aa\nbb\ncc\ndd\nee\n"));
  DiagnosticEngine diag(sm);
  diag.report(Severity::Error, SourceLocation::make(fid, 3), "X");
  diag.report(Severity::Warning, SourceLocation::make(fid, 0), "Y");
  diag.report(Severity::Note, SourceLocation::make(fid, 6), "Z");

  std::string const first = render(diag, DiagnosticEngine::Format::Text);
  std::string const second = render(diag, DiagnosticEngine::Format::Text);
  EXPECT_EQ(first, second);

  // Same idempotence must hold for JSON.
  std::string const j1 = render(diag, DiagnosticEngine::Format::JSON);
  std::string const j2 = render(diag, DiagnosticEngine::Format::JSON);
  EXPECT_EQ(j1, j2);
}

TEST(DiagnosticEngineSortTest, SecondaryKeyIsSeverity) {
  SourceManager sm;
  FileID const fid = sm.addBufferInMemory("s.nsl", bytesOf("aaa"));
  DiagnosticEngine diag(sm);
  // Same SourceLocation, different severities. The contract orders
  // by (loc, severity); Severity::Note < Warning < Error per the
  // declared enum (data-model entity 6).
  diag.report(Severity::Error, SourceLocation::make(fid, 1), "ERR");
  diag.report(Severity::Note, SourceLocation::make(fid, 1), "NOTE");
  diag.report(Severity::Warning, SourceLocation::make(fid, 1), "WARN");

  std::string const out = render(diag, DiagnosticEngine::Format::Text);
  size_t const note_pos = out.find("NOTE");
  size_t const warn_pos = out.find("WARN");
  size_t const err_pos = out.find("ERR");
  ASSERT_NE(note_pos, std::string::npos);
  ASSERT_NE(warn_pos, std::string::npos);
  ASSERT_NE(err_pos, std::string::npos);
  // Note < Warning < Error in the canonical sort.
  EXPECT_LT(note_pos, warn_pos);
  EXPECT_LT(warn_pos, err_pos);
}

} // namespace
