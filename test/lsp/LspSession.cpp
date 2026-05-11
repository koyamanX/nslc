// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test/lsp/LspSession.cpp — gtest harness impl. POSIX subprocess
// management with three-pipe IO and background reader threads.

#include "LspSession.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

extern "C" char **environ;

namespace nsl {
namespace lsp {
namespace test {

namespace {

constexpr int kReadEnd = 0;
constexpr int kWriteEnd = 1;

std::string resolveBinaryPath() {
  if (const char *p = std::getenv("NSL_LSP_BINARY"))
    return std::string(p);
  return "./bin/nsl-lsp";
}

ssize_t readAll(int fd, char *buf, size_t n) {
  size_t total = 0;
  while (total < n) {
    ssize_t r = ::read(fd, buf + total, n - total);
    if (r == 0)
      return total; // EOF
    if (r < 0) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    total += r;
  }
  return total;
}

ssize_t writeAll(int fd, const char *buf, size_t n) {
  size_t total = 0;
  while (total < n) {
    ssize_t w = ::write(fd, buf + total, n - total);
    if (w < 0) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    total += w;
  }
  return total;
}

bool readLine(int fd, std::string *out, char *peeked, bool *have_peeked) {
  out->clear();
  while (true) {
    char c;
    if (*have_peeked) {
      c = *peeked;
      *have_peeked = false;
    } else {
      ssize_t r = ::read(fd, &c, 1);
      if (r == 0)
        return false;
      if (r < 0) {
        if (errno == EINTR)
          continue;
        return false;
      }
    }
    if (c == '\r') {
      char nx;
      ssize_t r = ::read(fd, &nx, 1);
      if (r <= 0)
        return false;
      if (nx == '\n')
        return true;
      out->push_back(c);
      out->push_back(nx);
      continue;
    }
    if (c == '\n')
      return true;
    out->push_back(c);
  }
}

} // namespace

LspSession::LspSession(LspEnvVars env) {
  // Ignore SIGPIPE: when the child exits before consuming all
  // pending stdin (e.g., the InvalidLogLevel test where the child
  // dies during startup), `write` to its stdin pipe would raise
  // SIGPIPE and kill the parent (this test process). Tests
  // detect the dead-child case via the timeout-based wait helpers
  // and the exit-code check.
  static std::once_flag sigpipe_once;
  std::call_once(sigpipe_once, [] { ::signal(SIGPIPE, SIG_IGN); });

  int in_pipe[2];
  int out_pipe[2];
  int err_pipe[2];
  if (::pipe(in_pipe) != 0 || ::pipe(out_pipe) != 0 || ::pipe(err_pipe) != 0) {
    std::perror("LspSession: pipe");
    std::abort();
  }

  std::vector<std::string> env_strs;
  // Inherit current environment; override only the keys we care
  // about. This keeps PATH etc. so the child can resolve its own
  // shared libraries.
  for (char **e = environ; *e; ++e) {
    llvm::StringRef entry(*e);
    bool override_match = false;
    if (env.nsl_include && entry.starts_with("NSL_INCLUDE="))
      override_match = true;
    if (env.nsl_lsp_log_level && entry.starts_with("NSL_LSP_LOG_LEVEL="))
      override_match = true;
    if (env.nsl_lsp_workers && entry.starts_with("NSL_LSP_WORKERS="))
      override_match = true;
    if (!override_match)
      env_strs.emplace_back(*e);
  }
  if (env.nsl_include)
    env_strs.emplace_back("NSL_INCLUDE=" + *env.nsl_include);
  if (env.nsl_lsp_log_level)
    env_strs.emplace_back("NSL_LSP_LOG_LEVEL=" + *env.nsl_lsp_log_level);
  if (env.nsl_lsp_workers)
    env_strs.emplace_back("NSL_LSP_WORKERS=" + *env.nsl_lsp_workers);

  std::vector<char *> envp;
  envp.reserve(env_strs.size() + 1);
  for (auto &s : env_strs)
    envp.push_back(s.data());
  envp.push_back(nullptr);

  std::string bin = resolveBinaryPath();
  std::vector<char *> argv;
  argv.push_back(bin.data());
  argv.push_back(nullptr);

  pid_t pid = ::fork();
  if (pid < 0) {
    std::perror("LspSession: fork");
    std::abort();
  }
  if (pid == 0) {
    // Child.
    ::dup2(in_pipe[kReadEnd], 0);
    ::dup2(out_pipe[kWriteEnd], 1);
    ::dup2(err_pipe[kWriteEnd], 2);
    ::close(in_pipe[kReadEnd]);
    ::close(in_pipe[kWriteEnd]);
    ::close(out_pipe[kReadEnd]);
    ::close(out_pipe[kWriteEnd]);
    ::close(err_pipe[kReadEnd]);
    ::close(err_pipe[kWriteEnd]);
    ::execve(bin.c_str(), argv.data(), envp.data());
    std::fprintf(stderr, "LspSession: execve(%s) failed: %s\n", bin.c_str(),
                 std::strerror(errno));
    std::_Exit(127);
  }

  // Parent.
  child_ = pid;
  ::close(in_pipe[kReadEnd]);
  ::close(out_pipe[kWriteEnd]);
  ::close(err_pipe[kWriteEnd]);
  stdin_fd_ = in_pipe[kWriteEnd];
  stdout_fd_ = out_pipe[kReadEnd];
  stderr_fd_ = err_pipe[kReadEnd];

  reader_thread_ = std::thread([this] { readerLoop(); });
  stderr_thread_ = std::thread([this] { stderrLoop(); });
}

LspSession::~LspSession() {
  if (!exited_.load() && stdin_fd_ >= 0) {
    if (!shutdown_sent_.load()) {
      // Best-effort clean shutdown.
      doShutdownExit();
    }
  }
  // Final close of stdin to signal EOF.
  if (stdin_fd_ >= 0) {
    std::lock_guard<std::mutex> g(write_mtx_);
    ::close(stdin_fd_);
    stdin_fd_ = -1;
  }

  // Wait up to 5 seconds; SIGKILL if not exited.
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!exited_.load() && std::chrono::steady_clock::now() < deadline) {
    int status;
    pid_t r = ::waitpid(child_, &status, WNOHANG);
    if (r == child_) {
      exit_code_ = WIFEXITED(status) ? WEXITSTATUS(status) : 128;
      exited_.store(true);
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  if (!exited_.load()) {
    ::kill(child_, SIGKILL);
    int status;
    ::waitpid(child_, &status, 0);
    exit_code_ = 137;
    exited_.store(true);
  }

  if (reader_thread_.joinable())
    reader_thread_.join();
  if (stderr_thread_.joinable())
    stderr_thread_.join();
  if (stdout_fd_ >= 0)
    ::close(stdout_fd_);
  if (stderr_fd_ >= 0)
    ::close(stderr_fd_);
}

void LspSession::readerLoop() {
  char peeked = 0;
  bool have_peeked = false;

  while (true) {
    uint64_t content_length = 0;
    bool got_length = false;

    // Read header section.
    while (true) {
      std::string line;
      if (!readLine(stdout_fd_, &line, &peeked, &have_peeked)) {
        std::lock_guard<std::mutex> g(queue_mtx_);
        reader_done_ = true;
        queue_cv_.notify_all();
        return;
      }
      if (line.empty())
        break;
      llvm::StringRef sr(line);
      if (sr.starts_with("Content-Length:")) {
        sr = sr.drop_front(15).trim();
        uint64_t n = 0;
        if (!sr.consumeInteger(10, n) && sr.empty()) {
          content_length = n;
          got_length = true;
        }
      }
    }

    if (!got_length) {
      std::lock_guard<std::mutex> g(queue_mtx_);
      reader_done_ = true;
      queue_cv_.notify_all();
      return;
    }

    std::string body;
    body.resize(content_length);
    if (content_length > 0 &&
        readAll(stdout_fd_, body.data(), content_length) !=
            static_cast<ssize_t>(content_length)) {
      std::lock_guard<std::mutex> g(queue_mtx_);
      reader_done_ = true;
      queue_cv_.notify_all();
      return;
    }

    auto parsed = llvm::json::parse(body);
    if (!parsed) {
      llvm::consumeError(parsed.takeError());
      // Skip malformed body — keep reading.
      continue;
    }

    {
      std::lock_guard<std::mutex> g(queue_mtx_);
      queue_.push_back(std::move(*parsed));
      queue_cv_.notify_all();
    }
  }
}

void LspSession::stderrLoop() {
  char buf[4096];
  while (true) {
    ssize_t r = ::read(stderr_fd_, buf, sizeof(buf));
    if (r <= 0) {
      std::lock_guard<std::mutex> g(stderr_mtx_);
      stderr_done_ = true;
      return;
    }
    std::lock_guard<std::mutex> g(stderr_mtx_);
    captured_stderr_.append(buf, static_cast<size_t>(r));
  }
}

int64_t LspSession::sendRequest(llvm::StringRef method,
                                llvm::json::Value params) {
  int64_t id = next_id_.fetch_add(1);
  llvm::json::Value envelope = llvm::json::Object{
      {"id", id},
      {"jsonrpc", "2.0"},
      {"method", method.str()},
      {"params", std::move(params)},
  };
  std::string body;
  {
    llvm::raw_string_ostream os(body);
    os << envelope;
  }
  std::string framed =
      "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;

  std::lock_guard<std::mutex> g(write_mtx_);
  if (stdin_fd_ < 0)
    return id;
  writeAll(stdin_fd_, framed.data(), framed.size());
  return id;
}

void LspSession::sendNotification(llvm::StringRef method,
                                  llvm::json::Value params) {
  llvm::json::Value envelope = llvm::json::Object{
      {"jsonrpc", "2.0"},
      {"method", method.str()},
      {"params", std::move(params)},
  };
  std::string body;
  {
    llvm::raw_string_ostream os(body);
    os << envelope;
  }
  std::string framed =
      "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;

  std::lock_guard<std::mutex> g(write_mtx_);
  if (stdin_fd_ < 0)
    return;
  writeAll(stdin_fd_, framed.data(), framed.size());
}

std::optional<llvm::json::Value>
LspSession::popMessage(std::chrono::milliseconds timeout, bool diagnostics_only,
                       const std::optional<int64_t> &response_id) {
  std::unique_lock<std::mutex> g(queue_mtx_);
  auto deadline = std::chrono::steady_clock::now() + timeout;

  // Walk the queue front-to-back; return the first match. Non-
  // matching messages are left in the queue.
  while (true) {
    for (auto it = queue_.begin(); it != queue_.end(); ++it) {
      if (response_id) {
        auto *obj = it->getAsObject();
        if (!obj)
          continue;
        auto *id_val = obj->get("id");
        if (!id_val)
          continue;
        auto id_int = id_val->getAsInteger();
        if (!id_int || *id_int != *response_id)
          continue;
        llvm::json::Value v = std::move(*it);
        queue_.erase(it);
        return v;
      }
      if (diagnostics_only) {
        auto *obj = it->getAsObject();
        if (!obj)
          continue;
        auto m = obj->getString("method");
        if (!m || *m != "textDocument/publishDiagnostics")
          continue;
        llvm::json::Value v = std::move(*it);
        queue_.erase(it);
        return v;
      }
      // Any-message mode: pop front.
      if (it == queue_.begin()) {
        llvm::json::Value v = std::move(queue_.front());
        queue_.pop_front();
        return v;
      }
    }
    if (reader_done_)
      return std::nullopt;
    if (queue_cv_.wait_until(g, deadline) == std::cv_status::timeout)
      return std::nullopt;
  }
}

std::optional<llvm::json::Value>
LspSession::waitForMessage(std::chrono::milliseconds timeout) {
  return popMessage(timeout, false, std::nullopt);
}

std::optional<llvm::json::Value>
LspSession::waitForResponse(int64_t id, std::chrono::milliseconds timeout) {
  return popMessage(timeout, false, id);
}

std::optional<llvm::json::Value>
LspSession::waitForDiagnostics(std::chrono::milliseconds timeout) {
  return popMessage(timeout, true, std::nullopt);
}

int LspSession::doShutdownExit() {
  if (shutdown_sent_.exchange(true))
    return exit_code_;
  int64_t shut_id = sendRequest("shutdown", llvm::json::Value(nullptr));
  waitForResponse(shut_id, std::chrono::milliseconds(2000));
  sendNotification("exit", llvm::json::Value(nullptr));
  return exitCode();
}

int LspSession::exitCode() {
  if (!exited_.load()) {
    if (stdin_fd_ >= 0) {
      std::lock_guard<std::mutex> g(write_mtx_);
      ::close(stdin_fd_);
      stdin_fd_ = -1;
    }
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!exited_.load() && std::chrono::steady_clock::now() < deadline) {
      int status;
      pid_t r = ::waitpid(child_, &status, WNOHANG);
      if (r == child_) {
        exit_code_ = WIFEXITED(status) ? WEXITSTATUS(status) : 128;
        exited_.store(true);
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
  }
  return exit_code_;
}

std::string LspSession::capturedStderr() {
  // Wait briefly for stderr drain after exit.
  if (exited_.load()) {
    auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    while (std::chrono::steady_clock::now() < deadline) {
      {
        std::lock_guard<std::mutex> g(stderr_mtx_);
        if (stderr_done_)
          break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }
  std::lock_guard<std::mutex> g(stderr_mtx_);
  return captured_stderr_;
}

} // namespace test
} // namespace lsp
} // namespace nsl
