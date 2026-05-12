// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/LSP/TUScheduler.cpp — threading + cache impl.

#include "TUScheduler.h"

#include "IncludeSearchPath.h"
#include "Logger.h"

#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/Threading.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <thread>

namespace nsl {
namespace lsp {

unsigned TUScheduler::workersFromEnv() {
  unsigned hw = std::max(1u, std::thread::hardware_concurrency());
  unsigned cap = std::min(hw, 4u);

  const char *raw = std::getenv("NSL_LSP_WORKERS");
  if (!raw || !*raw)
    return cap;

  // Parse as positive integer in [1, 64].
  uint64_t parsed = 0;
  for (const char *p = raw; *p; ++p) {
    if (*p < '0' || *p > '9') {
      std::fprintf(stderr,
                   "nsl-lsp: invalid NSL_LSP_WORKERS value %s "
                   "(expected positive integer in [1, 64])\n",
                   raw);
      std::exit(1);
    }
    parsed = parsed * 10 + (*p - '0');
    if (parsed > 64)
      break;
  }
  if (parsed < 1 || parsed > 64) {
    std::fprintf(stderr,
                 "nsl-lsp: NSL_LSP_WORKERS value %s out of range [1, 64]\n",
                 raw);
    std::exit(1);
  }
  return static_cast<unsigned>(parsed);
}

TUScheduler::TUScheduler(unsigned worker_count)
    : pool_(llvm::hardware_concurrency(worker_count)) {
  NSL_LSP_LOG_DEBUG(
      llvm::formatv("TUScheduler: started with {0} workers", worker_count)
          .str());
}

TUScheduler::~TUScheduler() {
  waitForIdle();
}

void TUScheduler::setOnDiagnostics(DiagnosticsCallback cb) {
  std::lock_guard<std::mutex> guard(tus_mtx_);
  on_diagnostics_ = std::move(cb);
}

void TUScheduler::open(llvm::StringRef uri) {
  std::lock_guard<std::mutex> guard(tus_mtx_);
  if (tus_.find(uri) == tus_.end()) {
    tus_[uri] = std::make_shared<NslTU>();
  }
}

void TUScheduler::update(llvm::StringRef uri, int version, std::string contents,
                         const IncludeSearchPath &includes) {
  // Synchronously bump the TU's `latest_received_` atomic before
  // enqueueing the worker. Stale-drop on completion compares the
  // worker's diagnosed version against this — catches the case
  // where v=N completes AFTER v=N+1 has already published.
  {
    std::lock_guard<std::mutex> guard(tus_mtx_);
    auto it = tus_.find(uri);
    if (it == tus_.end()) {
      tus_[uri] = std::make_shared<NslTU>();
      it = tus_.find(uri);
    }
    it->second->noteReceived(version);
  }
  schedule(uri.str(), version, std::move(contents), &includes);
}

void TUScheduler::close(llvm::StringRef uri) {
  std::lock_guard<std::mutex> guard(tus_mtx_);
  tus_.erase(uri);
}

void TUScheduler::withState(llvm::StringRef uri,
                            std::function<void(const NslTU::State &)> fn) {
  // Capture a shared_ptr so the TU outlives a concurrent close(uri)
  // for the duration of the caller's read-only walk.
  std::shared_ptr<NslTU> tu;
  {
    std::lock_guard<std::mutex> guard(tus_mtx_);
    auto it = tus_.find(uri);
    if (it != tus_.end())
      tu = it->second;
  }
  if (tu)
    tu->withState(fn);
}

void TUScheduler::waitForIdle() {
  pool_.wait();
}

void TUScheduler::schedule(std::string uri, int version, std::string contents,
                           const IncludeSearchPath *includes) {
  // Ensure a TU exists. (Common path: open() has already been
  // called via the LSP didOpen handler.)
  {
    std::lock_guard<std::mutex> guard(tus_mtx_);
    if (tus_.find(uri) == tus_.end()) {
      tus_[uri] = std::make_shared<NslTU>();
    }
  }

  pool_.async([this, uri, version, contents = std::move(contents),
               includes]() mutable {
    // Capture a shared_ptr so the TU outlives a concurrent close(uri)
    // for the duration of this worker's reparse + diagnostics-publish
    // sequence — closes the use-after-free that a raw `NslTU*`
    // capture pattern would expose.
    std::shared_ptr<NslTU> tu;
    DiagnosticsCallback cb;
    {
      std::lock_guard<std::mutex> guard(tus_mtx_);
      auto it = tus_.find(uri);
      if (it == tus_.end())
        return; // closed before this work ran
      tu = it->second;
      cb = on_diagnostics_;
    }

    int diagnosed = tu->reparse(version, std::move(contents),
                                includes ? *includes : IncludeSearchPath());

    // Stale-drop per FR-008: if a newer version was received via
    // update() at any point (even if its reparse hasn't completed
    // yet), do NOT publish — the newer worker will publish when
    // it finishes. Compare against `latestReceivedVersion()`
    // (synchronously updated in update()) rather than
    // `latestVersion()` (the most-recently-completed reparse,
    // which doesn't anticipate queued newer work).
    if (diagnosed < tu->latestReceivedVersion())
      return;

    if (cb) {
      tu->withState([&](const NslTU::State &st) { cb(uri, diagnosed, st); });
    }
  });
}

} // namespace lsp
} // namespace nsl
