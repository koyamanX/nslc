// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test/lsp/LspSession.h — gtest harness driving `nsl-lsp` as a
// subprocess over stdio pipes. Per
// `specs/010-t3-lsp-skeleton/contracts/lsp-test-harness.contract.md`
// §1.
//
// Linux x86_64 only (POSIX `pipe`/`fork`/`execve`). The project
// targets only this platform per Constitution Principle IX.

#ifndef NSL_LSP_TEST_LSP_SESSION_H
#define NSL_LSP_TEST_LSP_SESSION_H

#include "llvm/Support/JSON.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <sys/types.h>
#include <thread>
#include <unordered_map>

namespace nsl {
namespace lsp {
namespace test {

struct LspEnvVars {
  std::optional<std::string> nsl_include;
  std::optional<std::string> nsl_lsp_log_level;
  std::optional<std::string> nsl_lsp_workers;
};

class LspSession {
public:
  /// Spawn `nsl-lsp` (path resolved from the env var
  /// `NSL_LSP_BINARY`, fallback `./bin/nsl-lsp` relative to the
  /// build dir). Configure the child env from `env`. The
  /// constructor returns once stdin/stdout/stderr pipes are open
  /// and the background reader threads are running.
  explicit LspSession(LspEnvVars env = {});

  /// Send shutdown+exit if the test did not, then wait for the
  /// process to terminate (5 s upper bound; SIGKILL otherwise).
  ~LspSession();

  LspSession(const LspSession &) = delete;
  LspSession &operator=(const LspSession &) = delete;

  /// Send a JSON-RPC request. Returns the assigned id.
  int64_t sendRequest(llvm::StringRef method, llvm::json::Value params);

  /// Send a JSON-RPC notification.
  void sendNotification(llvm::StringRef method, llvm::json::Value params);

  /// Block (up to `timeout`) for the next incoming envelope of any
  /// kind (response or notification).
  std::optional<llvm::json::Value> waitForMessage(
      std::chrono::milliseconds timeout = std::chrono::milliseconds(2000));

  /// Block for the response matching `id`. Other envelopes
  /// arriving in the interim are queued back for `waitForMessage`.
  std::optional<llvm::json::Value> waitForResponse(
      int64_t id,
      std::chrono::milliseconds timeout = std::chrono::milliseconds(2000));

  /// Block for the next `textDocument/publishDiagnostics`.
  std::optional<llvm::json::Value> waitForDiagnostics(
      std::chrono::milliseconds timeout = std::chrono::milliseconds(2000));

  /// Block until the subprocess exits and return its exit code.
  /// May only be called once per session.
  int exitCode();

  /// The subprocess's accumulated stderr, after exit.
  std::string capturedStderr();

  /// True once the subprocess has been observed to exit (does NOT
  /// block). For tests that want to assert "did NOT exit yet".
  bool hasExited() const { return exited_.load(); }

  /// Send shutdown+exit explicitly (the destructor does this if
  /// the test forgot, but explicit calls flow more cleanly through
  /// a typical test body).
  int doShutdownExit();

private:
  void readerLoop();
  void stderrLoop();
  std::optional<llvm::json::Value>
  popMessage(std::chrono::milliseconds timeout, bool diagnostics_only,
             const std::optional<int64_t> &response_id);

  pid_t child_ = -1;
  int stdin_fd_ = -1;
  int stdout_fd_ = -1;
  int stderr_fd_ = -1;

  std::thread reader_thread_;
  std::thread stderr_thread_;

  std::mutex queue_mtx_;
  std::condition_variable queue_cv_;
  std::deque<llvm::json::Value> queue_;
  bool reader_done_ = false;

  std::mutex stderr_mtx_;
  std::string captured_stderr_;
  bool stderr_done_ = false;

  std::mutex write_mtx_;

  std::atomic<int64_t> next_id_{1};
  std::atomic<bool> exited_{false};
  std::atomic<bool> shutdown_sent_{false};
  int exit_code_ = -1;
};

} // namespace test
} // namespace lsp
} // namespace nsl

#endif // NSL_LSP_TEST_LSP_SESSION_H
