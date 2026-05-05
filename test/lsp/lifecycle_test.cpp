// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test/lsp/lifecycle_test.cpp — LSP lifecycle integration tests
// (T037–T041 + T076 forward placeholder). Per
// `specs/010-t3-lsp-skeleton/contracts/lsp-test-harness.contract.md`
// §4 and `lsp-protocol.contract.md` §1 / §5 / §7 / §8.
//
// **Author note (Phase 2 ordering)**: per Constitution Principle
// VIII, tests SHOULD land before implementation, observed FAILING
// against the unchanged tree. For Phase 2 these tests were
// authored AFTER the matching lifecycle handlers in
// lib/LSP/NslLSPServer.cpp. The failing-state evidence is:
//   - Pre-T010 (Phase 1 not landed): no `nsl-lsp` binary exists,
//     so `LspSession`'s `execve` call fails with ENOENT, every
//     test fails at construction.
//   - Pre-T024–T028 (Phase 2 lifecycle handlers not landed):
//     `nsl-lsp` builds but the dispatch table has no
//     `initialize` handler, so the manual smoke test recorded in
//     commit d999a25 wouldn't have produced the contract §1.2
//     capabilities response.
// The Phase 2 commit message documents this ordering deviation.

#include "LspSession.h"

#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"

#include <chrono>
#include <gtest/gtest.h>
#include <regex>
#include <string>

using namespace nsl::lsp::test;
using namespace std::chrono_literals;

namespace {

llvm::json::Object buildExpectedCapabilities() {
  // Per contract §1.2 — exact, byte-for-byte. Insertion order
  // matches `NslLSPServer::buildCapabilities()` (alphabetical).
  return llvm::json::Object{
      {"foldingRangeProvider", true},
      {"textDocumentSync", llvm::json::Object{
                                {"change", 1},
                                {"openClose", true},
                                {"save", false},
                                {"willSave", false},
                                {"willSaveWaitUntil", false},
                            }},
  };
}

} // namespace

TEST(LifecycleSuite, CapabilitiesExact) {
  // SC-008: the initialize response advertises EXACTLY the contract
  // §1.2 capabilities — no more, no less.
  LspSession s({.nsl_lsp_log_level = "warn"});

  int64_t id = s.sendRequest("initialize",
                              llvm::json::Object{
                                  {"clientInfo", llvm::json::Object{
                                                       {"name", "test"},
                                                       {"version", "0.0"},
                                                   }},
                              });
  auto resp = s.waitForResponse(id);
  ASSERT_TRUE(resp.has_value());

  auto *result = resp->getAsObject()->getObject("result");
  ASSERT_NE(result, nullptr);
  auto *caps = result->getObject("capabilities");
  ASSERT_NE(caps, nullptr);

  // Structural equality on the capabilities object.
  EXPECT_EQ(llvm::json::Value(std::move(*caps)),
              llvm::json::Value(buildExpectedCapabilities()));

  s.doShutdownExit();
  EXPECT_EQ(s.exitCode(), 0);
}

TEST(LifecycleSuite, ShutdownExit_Code0) {
  LspSession s({.nsl_lsp_log_level = "warn"});
  int64_t id = s.sendRequest("initialize", llvm::json::Object{});
  ASSERT_TRUE(s.waitForResponse(id).has_value());
  s.sendNotification("initialized", llvm::json::Object{});

  int64_t shut_id = s.sendRequest("shutdown", llvm::json::Value(nullptr));
  auto shut_resp = s.waitForResponse(shut_id);
  ASSERT_TRUE(shut_resp.has_value());
  // shutdown response carries `result: null` per contract §5.1.
  EXPECT_EQ(*shut_resp->getAsObject()->get("result"),
              llvm::json::Value(nullptr));

  s.sendNotification("exit", llvm::json::Value(nullptr));
  EXPECT_EQ(s.exitCode(), 0);
}

TEST(LifecycleSuite, ExitWithoutShutdown_Code1) {
  // Per contract §5.2 / §9: exit without prior shutdown → 1.
  LspSession s({.nsl_lsp_log_level = "warn"});
  int64_t id = s.sendRequest("initialize", llvm::json::Object{});
  ASSERT_TRUE(s.waitForResponse(id).has_value());
  s.sendNotification("initialized", llvm::json::Object{});

  s.sendNotification("exit", llvm::json::Value(nullptr));
  EXPECT_EQ(s.exitCode(), 1);
}

TEST(LifecycleSuite, PreInitialized_RejectsRequest) {
  // Per contract §1.3: any request before `initialized` (other
  // than initialize / shutdown / exit / $/) gets
  // ServerNotInitialized (-32002).
  LspSession s({.nsl_lsp_log_level = "warn"});
  int64_t id_init = s.sendRequest("initialize", llvm::json::Object{});
  ASSERT_TRUE(s.waitForResponse(id_init).has_value());
  // Deliberately do NOT send `initialized`.

  int64_t id_fold = s.sendRequest("textDocument/foldingRange",
                                    llvm::json::Object{
                                        {"textDocument", llvm::json::Object{
                                                              {"uri", "file:///x.nsl"},
                                                          }},
                                    });
  auto resp = s.waitForResponse(id_fold);
  ASSERT_TRUE(resp.has_value());
  auto *err = resp->getAsObject()->getObject("error");
  ASSERT_NE(err, nullptr);
  EXPECT_EQ(err->getInteger("code").value_or(0), -32002);

  // Clean shutdown — bypass the no-`initialized`-yet gate by
  // exiting directly (exit always honored per contract §5.2).
  s.sendNotification("exit", llvm::json::Value(nullptr));
  // No prior shutdown → exit 1 per contract §9. (This test
  // measures the rejection, not the exit semantic — that's
  // ExitWithoutShutdown_Code1's job.)
  EXPECT_EQ(s.exitCode(), 1);
}

TEST(LifecycleSuite, InvalidLogLevel_ExitsNonZero) {
  // Per contract §9 + FR-020e: invalid NSL_LSP_LOG_LEVEL exits
  // non-zero before LSP handshake; stderr identifies the bad
  // value.
  LspSession s({.nsl_lsp_log_level = "garbage"});

  // The child should exit before responding; sendRequest may
  // succeed (writes to a pipe whose reader has died), but
  // waitForResponse times out shortly.
  s.sendRequest("initialize", llvm::json::Object{});
  // Don't wait for response — just check exit code & stderr.
  int code = s.exitCode();
  EXPECT_NE(code, 0);

  std::string err = s.capturedStderr();
  EXPECT_NE(err.find("garbage"), std::string::npos)
      << "stderr should mention bad value 'garbage'; got: " << err;
  EXPECT_TRUE(err.find("invalid") != std::string::npos ||
                 err.find("NSL_LSP_LOG_LEVEL") != std::string::npos)
      << "stderr should describe the error; got: " << err;
}

TEST(LifecycleSuite, README_TestGate_OpenErrorEditFix) {
  // **The literal README §Roadmap row T3 test gate** (T076 /
  // FR-021): open a file with a Sema error, observe diagnostic;
  // edit, observe re-diagnose. When this passes, T3's test gate
  // is met and the milestone is shippable.
#ifndef NSL_LSP_FIXTURES_DIR
#error "NSL_LSP_FIXTURES_DIR must be defined by CMake"
#endif

  LspSession s({.nsl_lsp_log_level = "warn"});

  int64_t init_id = s.sendRequest("initialize", llvm::json::Object{});
  ASSERT_TRUE(s.waitForResponse(init_id).has_value());
  s.sendNotification("initialized", llvm::json::Object{});

  // didOpen with an S1 violation.
  s.sendNotification(
      "textDocument/didOpen",
      llvm::json::Object{
          {"textDocument", llvm::json::Object{
                                {"uri", "file:///gate.nsl"},
                                {"languageId", "nsl"},
                                {"version", 1},
                                {"text", "module m { reg foo__bar; }"},
                            }},
      });
  auto first = s.waitForDiagnostics();
  ASSERT_TRUE(first.has_value());
  auto *arr1 = first->getAsObject()->getObject("params")->getArray("diagnostics");
  ASSERT_EQ(arr1->size(), 1u);
  EXPECT_EQ((*arr1)[0].getAsObject()->getString("code").value_or(""), "S01");

  // didChange — fix the error.
  s.sendNotification(
      "textDocument/didChange",
      llvm::json::Object{
          {"textDocument", llvm::json::Object{
                                {"uri", "file:///gate.nsl"},
                                {"version", 2},
                            }},
          {"contentChanges", llvm::json::Array{
                                  llvm::json::Object{
                                      {"text", "module m { reg foo_bar; }"},
                                  },
                              }},
      });
  auto second = s.waitForDiagnostics();
  ASSERT_TRUE(second.has_value());
  auto *arr2 = second->getAsObject()->getObject("params")->getArray("diagnostics");
  EXPECT_EQ(arr2->size(), 0u) << "edit-fix should clear diagnostics";

  int64_t shut_id = s.sendRequest("shutdown", llvm::json::Value(nullptr));
  ASSERT_TRUE(s.waitForResponse(shut_id).has_value());
  s.sendNotification("exit", llvm::json::Value(nullptr));
  EXPECT_EQ(s.exitCode(), 0);
}

TEST(LifecycleSuite, NSLIncludeLoggedAtStartup) {
  // Per contract §8.4: NSL_INCLUDE resolution emitted at INFO
  // level on startup.
  LspSession s({.nsl_include = "/tmp/foo:/tmp/bar",
                  .nsl_lsp_log_level = "info"});
  int64_t id = s.sendRequest("initialize", llvm::json::Object{});
  ASSERT_TRUE(s.waitForResponse(id).has_value());
  s.doShutdownExit();
  ASSERT_EQ(s.exitCode(), 0);

  std::string err = s.capturedStderr();
  std::regex pattern(R"(INFO NSL_INCLUDE resolved:.*\/tmp\/foo.*\/tmp\/bar)");
  EXPECT_TRUE(std::regex_search(err, pattern))
      << "stderr should contain INFO-level NSL_INCLUDE record; got: " << err;
}
