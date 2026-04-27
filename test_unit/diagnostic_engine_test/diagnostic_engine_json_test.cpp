// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/diagnostic_engine_test/diagnostic_engine_json_test.cpp
//
// FR-027 (smoke-only NDJSON per research §9): each rendered line is
// valid JSON and carries the five mandatory fields. **No content
// equality** beyond shape — that locks at T3 against a real LSP
// consumer.

#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"

#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"

#include "gtest/gtest.h"
#include <cstddef>
#include <llvm/ADT/StringRef.h>
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

bool hasField(const llvm::json::Object &obj, llvm::StringRef key) {
  return obj.find(key) != obj.end();
}

TEST(DiagnosticEngineJsonTest, EachLineParses) {
  SourceManager sm;
  FileID fid = sm.addBufferInMemory("a.nsl", bytesOf("abc\ndef\n"));
  DiagnosticEngine diag(sm);
  diag.report(Severity::Error, SourceLocation::make(fid, 0), "e1");
  diag.report(Severity::Warning, SourceLocation::make(fid, 4), "w1");

  std::string out = render(diag, DiagnosticEngine::Format::JSON);

  std::istringstream lines(out);
  std::string line;
  size_t line_count = 0;
  while (std::getline(lines, line)) {
    if (line.empty()) {
      continue;
    }
    auto parsed = llvm::json::parse(line);
    ASSERT_TRUE(static_cast<bool>(parsed)) << "line failed to parse: " << line;
    ++line_count;
  }
  EXPECT_EQ(line_count, 2U);
}

TEST(DiagnosticEngineJsonTest, FiveMandatoryFieldsPerObject) {
  SourceManager sm;
  FileID fid = sm.addBufferInMemory("a.nsl", bytesOf("abcdef"));
  DiagnosticEngine diag(sm);
  diag.report(Severity::Error, SourceLocation::make(fid, 0), "msg1");
  diag.report(Severity::Note, SourceLocation::make(fid, 2), "msg2");

  std::string out = render(diag, DiagnosticEngine::Format::JSON);

  std::istringstream lines(out);
  std::string line;
  while (std::getline(lines, line)) {
    if (line.empty()) {
      continue;
    }
    auto parsed = llvm::json::parse(line);
    ASSERT_TRUE(static_cast<bool>(parsed));
    auto *obj = parsed->getAsObject();
    ASSERT_NE(obj, nullptr);
    EXPECT_TRUE(hasField(*obj, "path"));
    EXPECT_TRUE(hasField(*obj, "line"));
    EXPECT_TRUE(hasField(*obj, "col"));
    EXPECT_TRUE(hasField(*obj, "severity"));
    EXPECT_TRUE(hasField(*obj, "message"));

    // Type checks per contract:
    //   path: string non-empty; line/col: positive integer; severity:
    //   one of three strings; message: string non-empty.
    auto path = obj->getString("path");
    ASSERT_TRUE(path.has_value());
    EXPECT_FALSE(path->empty());

    auto line_num = obj->getInteger("line");
    ASSERT_TRUE(line_num.has_value());
    EXPECT_GT(*line_num, 0);

    auto col_num = obj->getInteger("col");
    ASSERT_TRUE(col_num.has_value());
    EXPECT_GT(*col_num, 0);

    auto sev = obj->getString("severity");
    ASSERT_TRUE(sev.has_value());
    EXPECT_TRUE(*sev == "error" || *sev == "warning" || *sev == "note")
        << "severity: " << sev->str();

    auto msg = obj->getString("message");
    ASSERT_TRUE(msg.has_value());
    EXPECT_FALSE(msg->empty());
  }
}

TEST(DiagnosticEngineJsonTest, NoTrailingContextLinesInJson) {
  // Text mode emits source-line + caret context after the header
  // line; JSON mode must NOT emit those — every line must be JSON.
  SourceManager sm;
  FileID fid = sm.addBufferInMemory("a.nsl", bytesOf("source-line"));
  DiagnosticEngine diag(sm);
  diag.report(Severity::Error, SourceLocation::make(fid, 0), "msg");

  std::string out = render(diag, DiagnosticEngine::Format::JSON);

  std::istringstream lines(out);
  std::string line;
  size_t json_line_count = 0;
  while (std::getline(lines, line)) {
    if (line.empty()) {
      continue;
    }
    auto parsed = llvm::json::parse(line);
    ASSERT_TRUE(static_cast<bool>(parsed))
        << "non-JSON line in NDJSON output: " << line;
    ++json_line_count;
  }
  EXPECT_EQ(json_line_count, 1U);
}

} // namespace
