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
  if (!raw || !*raw) return cap;

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
    if (parsed > 64) break;
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
  NSL_LSP_LOG_DEBUG(llvm::formatv(
      "TUScheduler: started with {0} workers", worker_count).str());
}

TUScheduler::~TUScheduler() { waitForIdle(); }

void TUScheduler::setOnDiagnostics(DiagnosticsCallback cb) {
  std::lock_guard<std::mutex> guard(tus_mtx_);
  on_diagnostics_ = std::move(cb);
}

void TUScheduler::open(llvm::StringRef uri) {
  std::lock_guard<std::mutex> guard(tus_mtx_);
  if (tus_.find(uri) == tus_.end()) {
    tus_[uri] = std::make_unique<NslTU>();
  }
}

void TUScheduler::update(llvm::StringRef uri, int version,
                          std::string contents,
                          const IncludeSearchPath &includes) {
  schedule(uri.str(), version, std::move(contents), &includes);
}

void TUScheduler::close(llvm::StringRef uri) {
  std::lock_guard<std::mutex> guard(tus_mtx_);
  tus_.erase(uri);
}

void TUScheduler::withState(llvm::StringRef uri,
                              std::function<void(const NslTU::State &)> fn) {
  NslTU *tu = nullptr;
  {
    std::lock_guard<std::mutex> guard(tus_mtx_);
    auto it = tus_.find(uri);
    if (it != tus_.end()) tu = it->second.get();
  }
  if (tu) tu->withState(fn);
}

void TUScheduler::waitForIdle() { pool_.wait(); }

void TUScheduler::schedule(std::string uri, int version,
                            std::string contents,
                            const IncludeSearchPath *includes) {
  // Ensure a TU exists. (Common path: open() has already been
  // called via the LSP didOpen handler.)
  {
    std::lock_guard<std::mutex> guard(tus_mtx_);
    if (tus_.find(uri) == tus_.end()) {
      tus_[uri] = std::make_unique<NslTU>();
    }
  }

  pool_.async([this, uri, version, contents = std::move(contents),
               includes]() mutable {
    NslTU *tu = nullptr;
    DiagnosticsCallback cb;
    {
      std::lock_guard<std::mutex> guard(tus_mtx_);
      auto it = tus_.find(uri);
      if (it == tus_.end()) return; // closed before this work ran
      tu = it->second.get();
      cb = on_diagnostics_;
    }

    int diagnosed = tu->reparse(version, std::move(contents),
                                  includes ? *includes : IncludeSearchPath());

    // Stale-drop per FR-008: if a newer version has arrived while
    // we were running, do NOT publish.
    if (diagnosed < tu->latestVersion()) return;

    if (cb) {
      tu->withState([&](const NslTU::State &st) {
        cb(uri, diagnosed, st);
      });
    }
  });
}

} // namespace lsp
} // namespace nsl
