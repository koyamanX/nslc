// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test/lsp/folding_test.cpp — Phase 5 / US3 folding + cancellation
// integration tests (T090-T096, lean subset). Per
// `specs/010-t3-lsp-skeleton/contracts/folding-range.contract.md`.

#include "LspSession.h"

#include "llvm/Support/JSON.h"

#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

using namespace nsl::lsp::test;
using namespace std::chrono_literals;

namespace {

#ifndef NSL_LSP_FIXTURES_DIR
#error "NSL_LSP_FIXTURES_DIR must be defined by CMake"
#endif

std::string readFixture(const std::string &name) {
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

llvm::json::Value foldingRange(LspSession &s, llvm::StringRef uri) {
  int64_t id = s.sendRequest(
      "textDocument/foldingRange",
      llvm::json::Object{
          {"textDocument", llvm::json::Object{{"uri", uri.str()}}},
      });
  auto resp = s.waitForResponse(id);
  EXPECT_TRUE(resp.has_value());
  return resp.value_or(llvm::json::Value(nullptr));
}

const llvm::json::Array *getResultArray(const llvm::json::Value &resp) {
  auto *obj = resp.getAsObject();
  if (!obj) return nullptr;
  auto *r = obj->get("result");
  if (!r) return nullptr;
  return r->getAsArray();
}

} // namespace

TEST(FoldingSuite, MultiLineBlocksProduceFolds) {
  // module + declare + func + seq = 4 multi-line braces = 4 folds.
  LspSession s({.nsl_lsp_log_level = "warn"});
  initialize(s);
  std::string text = readFixture("module_with_blocks.nsl");
  ASSERT_FALSE(text.empty());
  didOpen(s, "file:///b.nsl", 1, text);
  s.waitForDiagnostics();

  auto resp = foldingRange(s, "file:///b.nsl");
  const auto *arr = getResultArray(resp);
  ASSERT_NE(arr, nullptr);
  EXPECT_EQ(arr->size(), 4u)
      << "expected one fold per multi-line block; "
         "got " << arr->size();

  s.doShutdownExit();
}

TEST(FoldingSuite, SingleLineBlocksNotFolded) {
  LspSession s({.nsl_lsp_log_level = "warn"});
  initialize(s);
  std::string text = readFixture("single_line_blocks.nsl");
  didOpen(s, "file:///s.nsl", 1, text);
  s.waitForDiagnostics();

  auto resp = foldingRange(s, "file:///s.nsl");
  const auto *arr = getResultArray(resp);
  ASSERT_NE(arr, nullptr);
  EXPECT_EQ(arr->size(), 0u);

  s.doShutdownExit();
}

TEST(FoldingSuite, MultiLineBlockCommentKindExact) {
  // FR-015: multi-line `/* */` emits a fold with `kind: "comment"`.
  LspSession s({.nsl_lsp_log_level = "warn"});
  initialize(s);
  std::string text = readFixture("multiline_block_comment.nsl");
  didOpen(s, "file:///c.nsl", 1, text);
  s.waitForDiagnostics();

  auto resp = foldingRange(s, "file:///c.nsl");
  const auto *arr = getResultArray(resp);
  ASSERT_NE(arr, nullptr);
  // At minimum 1 comment fold; the trailing `module m { }` is
  // single-line so no code-block fold is added.
  ASSERT_GE(arr->size(), 1u);
  bool found_comment = false;
  for (const auto &v : *arr) {
    auto kind = v.getAsObject()->getString("kind");
    if (kind && *kind == "comment") {
      found_comment = true;
      break;
    }
  }
  EXPECT_TRUE(found_comment);

  s.doShutdownExit();
}

TEST(FoldingSuite, ZeroBasedLines) {
  // LSP positions are zero-based. A `module m {` opening on
  // physical line 1 of the source should produce startLine = 0.
  LspSession s({.nsl_lsp_log_level = "warn"});
  initialize(s);
  // 2-line module starting on line 1 (zero-indexed line 0).
  std::string text = "module m {\n}\n";
  didOpen(s, "file:///z.nsl", 1, text);
  s.waitForDiagnostics();

  auto resp = foldingRange(s, "file:///z.nsl");
  const auto *arr = getResultArray(resp);
  ASSERT_NE(arr, nullptr);
  ASSERT_EQ(arr->size(), 1u);
  auto *fold = (*arr)[0].getAsObject();
  EXPECT_EQ(fold->getInteger("startLine").value_or(-1), 0);
  EXPECT_EQ(fold->getInteger("endLine").value_or(-1), 1);

  s.doShutdownExit();
}

TEST(FoldingSuite, ParseErrorRecovery) {
  // FR-017: an unparseable document still produces folds for the
  // brace pairs the walker can recognize. The text walker is
  // independent of the M2 parser, so this trivially holds — but
  // the test makes the contract observable.
  LspSession s({.nsl_lsp_log_level = "warn"});
  initialize(s);
  std::string text = "module m {\n  oops syntax error\n  func {\n    x\n  }\n}\n";
  didOpen(s, "file:///p.nsl", 1, text);
  s.waitForDiagnostics();

  auto resp = foldingRange(s, "file:///p.nsl");
  // Must be a `result` (not an error) per FR-017.
  ASSERT_NE(resp.getAsObject()->get("result"), nullptr);
  const auto *arr = getResultArray(resp);
  ASSERT_NE(arr, nullptr);
  // module + func = 2 folds.
  EXPECT_EQ(arr->size(), 2u);

  s.doShutdownExit();
}

TEST(FoldingSuite, Determinism_TwoRunsByteIdentical) {
  auto runOnce = [&]() -> std::string {
    LspSession s({.nsl_lsp_log_level = "warn"});
    initialize(s);
    std::string text = readFixture("module_with_blocks.nsl");
    didOpen(s, "file:///d.nsl", 1, text);
    s.waitForDiagnostics();
    auto resp = foldingRange(s, "file:///d.nsl");
    std::string out;
    llvm::raw_string_ostream os(out);
    os << resp;
    s.doShutdownExit();
    return out;
  };
  EXPECT_EQ(runOnce(), runOnce());
}
