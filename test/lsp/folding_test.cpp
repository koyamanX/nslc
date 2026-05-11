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
  s.sendNotification("textDocument/didOpen", llvm::json::Object{
                                                 {"textDocument",
                                                  llvm::json::Object{
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
  if (!obj) {
    return nullptr;
  }
  auto *r = obj->get("result");
  if (!r) {
    return nullptr;
  }
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
  EXPECT_EQ(arr->size(), 4u) << "expected one fold per multi-line block; "
                                "got "
                             << arr->size();

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
  std::string text =
      "module m {\n  oops syntax error\n  func {\n    x\n  }\n}\n";
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

TEST(FoldingSuite, IncludeAdjustsLines) {
  // T086 / FR-011 / folding contract §1: fold-range line numbers
  // reflect the physical position in the open document, not any
  // post-#line virtual coordinates of included content. The
  // `module mod {` opens at line index 8 (physical line 9) per
  // the fixture's layout; it closes at line index 11.
  LspSession s({.nsl_lsp_log_level = "warn"});
  initialize(s);
  std::string text = readFixture("include_adjusts_lines.nsl");
  ASSERT_FALSE(text.empty());
  didOpen(s, "file:///incadj.nsl", 1, text);
  s.waitForDiagnostics();

  auto resp = foldingRange(s, "file:///incadj.nsl");
  const auto *arr = getResultArray(resp);
  ASSERT_NE(arr, nullptr);

  // Find the fold for the module. The fixture has 9 lines of
  // header comment (idx 0-8) then a blank line (idx 9), then
  // `#include` (idx 10), blank (idx 11), `module mod {` (idx 12)
  // and `}` (idx 15). The walker counts physical source lines,
  // so the module's fold is startLine=12, endLine=15 regardless
  // of any post-#line virtual coordinates the preprocessor would
  // assign to the included content.
  bool saw_module = false;
  for (const auto &v : *arr) {
    auto *o = v.getAsObject();
    auto start = o->getInteger("startLine").value_or(-1);
    auto end = o->getInteger("endLine").value_or(-1);
    if (start == 12 && end == 15) {
      saw_module = true;
      break;
    }
  }
  EXPECT_TRUE(saw_module)
      << "expected fold for module mod at startLine=12 endLine=15";

  s.doShutdownExit();
}

TEST(FoldingSuite, Cancellation_Under200ms) {
  // T093 / SC-010 / harness contract §4.5: open the large
  // `cancellation_target.nsl` fixture, send `foldingRange`,
  // immediately follow with `$/cancelRequest`, and assert the
  // server returns `RequestCancelled` (-32800) within 200 ms.
  // The fixture's > 10,000-node AST guarantees the per-node
  // cancellation poll in FoldingRangeBuilder (folding-range
  // contract §5) has a window to observe the cancel signal —
  // the request would otherwise complete with a fold array.
  LspSession s({.nsl_lsp_log_level = "warn"});
  initialize(s);
  std::string text = readFixture("cancellation_target.nsl");
  ASSERT_FALSE(text.empty());
  didOpen(s, "file:///cancel.nsl", 1, text);
  s.waitForDiagnostics();

  int64_t reqId = s.sendRequest(
      "textDocument/foldingRange",
      llvm::json::Object{
          {"textDocument", llvm::json::Object{{"uri", "file:///cancel.nsl"}}},
      });
  s.sendNotification("$/cancelRequest", llvm::json::Object{{"id", reqId}});

  auto t0 = std::chrono::steady_clock::now();
  auto resp = s.waitForResponse(reqId, std::chrono::milliseconds(500));
  auto elapsed = std::chrono::steady_clock::now() - t0;

  ASSERT_TRUE(resp.has_value())
      << "expected a JSON-RPC response (cancellation acknowledgment)";
  const auto *err = resp->getAsObject()->getObject("error");
  ASSERT_NE(err, nullptr)
      << "cancelled request must respond with an `error` object, "
         "not a `result`";
  EXPECT_EQ(err->getInteger("code").value_or(0), -32800)
      << "expected RequestCancelled (-32800)";
  auto msg = err->getString("message").value_or("");
  EXPECT_EQ(msg, "request cancelled")
      << "expected message 'request cancelled' per FR-020j; got: " << msg.str();
  EXPECT_LT(elapsed, std::chrono::milliseconds(200))
      << "SC-010 200 ms cancellation budget missed";

  s.doShutdownExit();
  EXPECT_EQ(s.exitCode(), 0);
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
