// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test/lsp/diagnostics_test.cpp — Phase 3 / US1 diagnostic-seam
// integration tests (T057-T066, leaner subset). Per
// `specs/010-t3-lsp-skeleton/contracts/lsp-test-harness.contract.md`
// §4 + `lsp-protocol.contract.md` §3.
//
// Strict-TDD red state captured in `${TMPDIR:-/tmp}/t3-us1-red.txt`
// before the matching implementation tasks (T068-T072) land.

#include "LspSession.h"

#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"

#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

using namespace nsl::lsp::test;
using namespace std::chrono_literals;

namespace {

std::string readFixture(const std::string &name) {
  // Path baked at compile time per test/lsp/CMakeLists.txt's
  // `target_compile_definitions(... NSL_LSP_FIXTURES_DIR=...)`.
#ifndef NSL_LSP_FIXTURES_DIR
#error "NSL_LSP_FIXTURES_DIR must be defined by CMake"
#endif
  std::ifstream f(std::string(NSL_LSP_FIXTURES_DIR) + "/" + name);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

void initialize(LspSession &s) {
  int64_t id = s.sendRequest("initialize", llvm::json::Object{});
  ASSERT_TRUE(s.waitForResponse(id).has_value());
  s.sendNotification("initialized", llvm::json::Object{});
}

void didOpen(LspSession &s, llvm::StringRef uri, int version,
              llvm::StringRef text) {
  s.sendNotification("textDocument/didOpen",
                      llvm::json::Object{
                          {"textDocument", llvm::json::Object{
                                                {"uri", uri.str()},
                                                {"languageId", "nsl"},
                                                {"version", version},
                                                {"text", text.str()},
                                            }},
                      });
}

const llvm::json::Array *getDiagnosticsArray(const llvm::json::Value &env) {
  auto *obj = env.getAsObject();
  if (!obj) return nullptr;
  auto *params = obj->getObject("params");
  if (!params) return nullptr;
  return params->getArray("diagnostics");
}

} // namespace

TEST(DiagnosticsSuite, EmptyArrayOnClean) {
  LspSession s({.nsl_lsp_log_level = "warn"});
  initialize(s);
  std::string text = readFixture("clean_module.nsl");
  ASSERT_FALSE(text.empty());
  didOpen(s, "file:///clean.nsl", 1, text);

  auto diag = s.waitForDiagnostics();
  ASSERT_TRUE(diag.has_value());
  const auto *arr = getDiagnosticsArray(*diag);
  ASSERT_NE(arr, nullptr);
  EXPECT_EQ(arr->size(), 0u) << "clean module should produce zero diagnostics";

  s.doShutdownExit();
  EXPECT_EQ(s.exitCode(), 0);
}

TEST(DiagnosticsSuite, SingleS01) {
  LspSession s({.nsl_lsp_log_level = "warn"});
  initialize(s);
  std::string text = readFixture("s01_double_underscore.nsl");
  ASSERT_FALSE(text.empty());
  didOpen(s, "file:///s01.nsl", 1, text);

  auto diag = s.waitForDiagnostics();
  ASSERT_TRUE(diag.has_value());
  const auto *arr = getDiagnosticsArray(*diag);
  ASSERT_NE(arr, nullptr);
  ASSERT_EQ(arr->size(), 1u);

  const auto *d = (*arr)[0].getAsObject();
  ASSERT_NE(d, nullptr);
  EXPECT_EQ(d->getString("code").value_or(""), "S01");
  EXPECT_EQ(d->getInteger("severity").value_or(0), 1); // Error
  EXPECT_EQ(d->getString("source").value_or(""), "nsl-sema");
  // Message contains the frozen text fragment (mapper preserves it).
  auto msg = d->getString("message").value_or("");
  EXPECT_NE(msg.find("identifier may not contain"), std::string::npos)
      << "message: " << msg.str();

  s.doShutdownExit();
  EXPECT_EQ(s.exitCode(), 0);
}

TEST(DiagnosticsSuite, SortOrder_LineThenColumn) {
  LspSession s({.nsl_lsp_log_level = "warn"});
  initialize(s);
  std::string text = readFixture("two_errors_same_line.nsl");
  ASSERT_FALSE(text.empty());
  didOpen(s, "file:///two.nsl", 1, text);

  auto diag = s.waitForDiagnostics();
  ASSERT_TRUE(diag.has_value());
  const auto *arr = getDiagnosticsArray(*diag);
  ASSERT_NE(arr, nullptr);
  ASSERT_EQ(arr->size(), 2u);

  // Line ascending: (*arr)[0].range.start.line <= (*arr)[1].range.start.line
  auto extractStart = [](const llvm::json::Value &d) {
    const auto *o = d.getAsObject()->getObject("range")->getObject("start");
    return std::pair<int64_t, int64_t>{
        o->getInteger("line").value_or(-1),
        o->getInteger("character").value_or(-1)};
  };
  auto a = extractStart((*arr)[0]);
  auto b = extractStart((*arr)[1]);
  EXPECT_TRUE(a < b) << "expected (a.line, a.col) < (b.line, b.col); "
                        << "got (" << a.first << "," << a.second
                        << ") vs (" << b.first << "," << b.second << ")";

  s.doShutdownExit();
  EXPECT_EQ(s.exitCode(), 0);
}

TEST(DiagnosticsSuite, ParseError) {
  LspSession s({.nsl_lsp_log_level = "warn"});
  initialize(s);
  std::string text = readFixture("parse_error_missing_brace.nsl");
  ASSERT_FALSE(text.empty());
  didOpen(s, "file:///pe.nsl", 1, text);

  auto diag = s.waitForDiagnostics();
  ASSERT_TRUE(diag.has_value());
  const auto *arr = getDiagnosticsArray(*diag);
  ASSERT_NE(arr, nullptr);
  ASSERT_GE(arr->size(), 1u);

  // At least one diagnostic with source = "nsl-parse".
  bool found_parse_source = false;
  for (const auto &d : *arr) {
    auto src = d.getAsObject()->getString("source").value_or("");
    if (src == "nsl-parse") { found_parse_source = true; break; }
  }
  EXPECT_TRUE(found_parse_source)
      << "expected at least one diagnostic with source = nsl-parse";

  s.doShutdownExit();
  EXPECT_EQ(s.exitCode(), 0);
}

TEST(DiagnosticsSuite, Determinism_TwoRunsByteIdentical) {
  // SC-003: two runs over identical input produce byte-identical
  // publishDiagnostics payloads.
  auto runOnce = [&]() -> std::string {
    LspSession s({.nsl_lsp_log_level = "warn"});
    initialize(s);
    std::string text = readFixture("s01_double_underscore.nsl");
    didOpen(s, "file:///d.nsl", 1, text);
    auto diag = s.waitForDiagnostics();
    EXPECT_TRUE(diag.has_value());
    std::string out;
    llvm::raw_string_ostream os(out);
    if (diag.has_value()) os << *diag;
    s.doShutdownExit();
    return out;
  };
  std::string a = runOnce();
  std::string b = runOnce();
  EXPECT_EQ(a, b);
}
