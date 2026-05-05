// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test/lsp/cancellation_test.cpp — Phase 5 / US3 `$/cancelRequest`
// edge-case tests. Per
// `specs/010-t3-lsp-skeleton/contracts/lsp-protocol.contract.md` §6.

#include "LspSession.h"

#include "llvm/Support/JSON.h"

#include <chrono>
#include <gtest/gtest.h>
#include <string>

using namespace nsl::lsp::test;
using namespace std::chrono_literals;

namespace {

void initialize(LspSession &s) {
  int64_t id = s.sendRequest("initialize", llvm::json::Object{});
  ASSERT_TRUE(s.waitForResponse(id).has_value());
  s.sendNotification("initialized", llvm::json::Object{});
}

} // namespace

TEST(CancellationSuite, CancelCompletedRequestSilentlyIgnored) {
  // Per FR-020j: $/cancelRequest for a request that has already
  // completed is silently ignored.
  LspSession s({.nsl_lsp_log_level = "warn"});
  initialize(s);

  int64_t id = s.sendRequest(
      "textDocument/foldingRange",
      llvm::json::Object{
          {"textDocument", llvm::json::Object{{"uri", "file:///x.nsl"}}},
      });
  auto resp = s.waitForResponse(id);
  ASSERT_TRUE(resp.has_value());
  // The request has completed. Now cancel it — should be a no-op.
  s.sendNotification("$/cancelRequest", llvm::json::Object{{"id", id}});

  // No further response or stderr ERROR should arrive.
  auto extra = s.waitForResponse(id, std::chrono::milliseconds(150));
  EXPECT_FALSE(extra.has_value());

  s.doShutdownExit();
  EXPECT_EQ(s.exitCode(), 0);
}

TEST(CancellationSuite, CancelNeverSeenIdSilentlyIgnored) {
  LspSession s({.nsl_lsp_log_level = "warn"});
  initialize(s);

  // Cancel an id we never sent.
  s.sendNotification("$/cancelRequest",
                       llvm::json::Object{{"id", int64_t{9999}}});

  // Server should still be functional — round-trip a foldingRange.
  int64_t id = s.sendRequest(
      "textDocument/foldingRange",
      llvm::json::Object{
          {"textDocument", llvm::json::Object{{"uri", "file:///y.nsl"}}},
      });
  EXPECT_TRUE(s.waitForResponse(id).has_value());

  s.doShutdownExit();
  EXPECT_EQ(s.exitCode(), 0);
}
