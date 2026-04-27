// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/diagnostic_engine_test/include_stack_test.cpp
//
// FR-026: diagnostics raised inside an `#include`'d file MUST carry
// one trailing `note: included from <ancestor-path>:<line>` per
// ancestor in the include stack. The notes are populated at emit
// time by `Builder::addIncludedFromNote()`, which reads the
// `SourceManager`'s active include stack.

#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"

#include "llvm/Support/raw_ostream.h"

#include "gtest/gtest.h"
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

TEST(IncludeStackTest, NotesAppendedInInnermostFirstOrder) {
  SourceManager sm;
  // Outer.nsl line 1 has `#include "middle.nsl"` at column 1; the
  // include directive's location is offset 0 of outer.nsl.
  FileID outer =
      sm.addBufferInMemory("outer.nsl", bytesOf("#include \"middle.nsl\"\n"));
  FileID middle =
      sm.addBufferInMemory("middle.nsl", bytesOf("#include \"inner.nsl\"\n"));
  FileID inner =
      sm.addBufferInMemory("inner.nsl", bytesOf("aaaa\nbbbb\ncccc\n"));

  // Push include frames as the preprocessor would.
  sm.pushIncludeFrame(SourceLocation::make(outer, 0), middle);
  sm.pushIncludeFrame(SourceLocation::make(middle, 0), inner);

  DiagnosticEngine diag(sm);
  // Emit an error inside inner.nsl, with a builder that attaches the
  // include-from notes from the SourceManager's active stack.
  diag.report(Severity::Error, SourceLocation::make(inner, 5),
              "unterminated string literal")
      .addIncludedFromNotes();

  std::string out = render(diag, DiagnosticEngine::Format::Text);

  // The first diagnostic line cites inner.nsl:2:1 (offset 5 = line 2).
  EXPECT_NE(out.find("inner.nsl:2:1: error: unterminated string literal"),
            std::string::npos)
      << out;

  // Two trailing notes, innermost first.
  size_t note_middle = out.find("note: included from middle.nsl:1:1");
  size_t note_outer = out.find("note: included from outer.nsl:1:1");
  ASSERT_NE(note_middle, std::string::npos) << out;
  ASSERT_NE(note_outer, std::string::npos) << out;
  EXPECT_LT(note_middle, note_outer);
}

TEST(IncludeStackTest, NoNotesWhenStackEmpty) {
  SourceManager sm;
  FileID f = sm.addBufferInMemory("solo.nsl", bytesOf("alpha\n"));
  DiagnosticEngine diag(sm);
  diag.report(Severity::Error, SourceLocation::make(f, 0), "boom")
      .addIncludedFromNotes();

  std::string out = render(diag, DiagnosticEngine::Format::Text);
  EXPECT_EQ(out.find("note: included from"), std::string::npos);
}

TEST(IncludeStackTest, NotesPropagateToJsonOutput) {
  SourceManager sm;
  FileID outer = sm.addBufferInMemory("outer.nsl", bytesOf("xxxx\n"));
  FileID inner = sm.addBufferInMemory("inner.nsl", bytesOf("yyy"));
  sm.pushIncludeFrame(SourceLocation::make(outer, 0), inner);

  DiagnosticEngine diag(sm);
  diag.report(Severity::Error, SourceLocation::make(inner, 0), "err")
      .addIncludedFromNotes();

  std::string out = render(diag, DiagnosticEngine::Format::JSON);
  // The smoke test expects two NDJSON lines (the error + one note).
  size_t newline_count = 0;
  for (char c : out) {
    if (c == '\n') {
      ++newline_count;
    }
  }
  EXPECT_GE(newline_count, 2U) << out;
  EXPECT_NE(out.find("included_from"), std::string::npos) << out;
}

} // namespace
