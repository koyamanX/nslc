// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/LSP/TUScheduler.h — threading + per-document cache for
// `nsl-lsp`. Per `docs/design/nsl_tooling_design.md` §3.3, the
// scheduler keeps one `NslTU` per open document URI, runs parse +
// sema on a worker pool, serializes writes per document, and
// publishes diagnostics back to the protocol layer through a
// caller-supplied callback.
//
// **Specification anchors**:
//   - `specs/010-t3-lsp-skeleton/data-model.md` §2.4
//   - `specs/010-t3-lsp-skeleton/research.md` §R2

#ifndef NSL_LSP_TU_SCHEDULER_H
#define NSL_LSP_TU_SCHEDULER_H

#include "NslTU.h"

#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/ThreadPool.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace nsl {
namespace lsp {

class IncludeSearchPath;

class TUScheduler {
public:
  /// Worker count: defaults to `min(hardware_concurrency, 4)` per
  /// research §R2. `NSL_LSP_WORKERS` env var overrides if set
  /// (1..64). Out-of-range values exit non-zero per
  /// contracts/lsp-protocol.contract.md §9.
  static unsigned workersFromEnv();

  explicit TUScheduler(unsigned worker_count);
  ~TUScheduler();

  TUScheduler(const TUScheduler &) = delete;
  TUScheduler &operator=(const TUScheduler &) = delete;

  /// Diagnostics-publication callback. Invoked by worker threads
  /// once a parse + sema cycle completes; the protocol layer
  /// wraps the diagnostics in `publishDiagnostics` and writes them
  /// to the transport. The callback receives the URI, the version
  /// that was diagnosed, and an `NslTU::State` reference (read-only).
  using DiagnosticsCallback = std::function<void(
      llvm::StringRef uri, int version, const NslTU::State &state)>;
  void setOnDiagnostics(DiagnosticsCallback cb);

  void open(llvm::StringRef uri);
  void update(llvm::StringRef uri, int version, std::string contents,
              const IncludeSearchPath &includes);
  void close(llvm::StringRef uri);

  /// Read access to a document's most recent state. The callback
  /// observes nothing if the URI is not currently open.
  void withState(llvm::StringRef uri,
                 std::function<void(const NslTU::State &)> fn);

  /// Block until all in-flight work drains — used by the protocol
  /// layer's `shutdown` handler.
  void waitForIdle();

private:
  void schedule(std::string uri, int version, std::string contents,
                const IncludeSearchPath *includes);

  std::mutex tus_mtx_;
  llvm::StringMap<std::unique_ptr<NslTU>> tus_;
  DiagnosticsCallback on_diagnostics_;
  llvm::DefaultThreadPool pool_;
};

} // namespace lsp
} // namespace nsl

#endif // NSL_LSP_TU_SCHEDULER_H
