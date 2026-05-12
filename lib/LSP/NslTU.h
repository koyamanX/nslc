// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/LSP/NslTU.h — per-document translation-unit state for the
// `nsl-lsp` server. Owned by `TUScheduler`; one instance per open
// document URI. At Phase 2 the `reparse` body is a stub that just
// stores the contents. Phase 3 (T069) wires the real
// `nsl::driver::Compilation` parse + sema invocation.
//
// **Specification anchors**:
//   - `specs/010-t3-lsp-skeleton/data-model.md` §2.4
//   - `docs/design/nsl_tooling_design.md` §3.3

#ifndef NSL_LSP_NSL_TU_H
#define NSL_LSP_NSL_TU_H

#include "llvm/ADT/StringRef.h"

#include <atomic>

// Heavy includes pulled in here so consumers of `NslTU::State`
// (e.g., TUScheduler instantiating std::unique_ptr<NslTU>'s
// inline destructor) don't need to repeat them. The cost is
// acceptable: the only consumers are inside lib/LSP/.
#include "nsl/AST/CompilationUnit.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Sema/SymbolTable.h"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace nsl {
namespace lsp {

class IncludeSearchPath;

class NslTU {
public:
  struct State {
    int version = -1;
    std::string contents;
    std::unique_ptr<nsl::ast::CompilationUnit> ast;
    std::vector<nsl::Diagnostic> diagnostics;
    std::unique_ptr<nsl::sema::SymbolTable> symbols;
    /// SourceManager that owns the buffers + line tables for this
    /// reparse. Held as shared_ptr because the diagnostic-mapping
    /// seam needs it during the publish callback (which runs on a
    /// worker thread); shared_ptr keeps it alive for the duration
    /// of the callback even if a fresh reparse races to replace
    /// the State.
    std::shared_ptr<nsl::SourceManager> source_manager;
  };

  NslTU();
  ~NslTU();
  NslTU(const NslTU &) = delete;
  NslTU &operator=(const NslTU &) = delete;

  /// Replace state in-place with a fresh parse + sema for `version`
  /// over `contents` against `includes`. At Phase 2 this is a stub
  /// that stores `version` + `contents` and produces an empty
  /// `diagnostics` vector — Phase 3 (T069) wires the real
  /// invocation. Returns the version that was diagnosed (which
  /// may be older than the latest if a stale-drop occurs).
  int reparse(int version, std::string contents,
              const IncludeSearchPath &includes);

  /// Read access to the most recent state. Mutex-protected.
  template <typename Fn>
  void withState(Fn &&fn) {
    std::lock_guard<std::mutex> guard(mtx_);
    fn(state_);
  }

  /// Accessor for the diagnostics-publication callback path.
  /// Returns the version of the most recently *completed* reparse
  /// (i.e., state_.version).
  int latestVersion() const;

  /// Track the latest version *received* via update(), even if its
  /// reparse hasn't yet started or completed. Used by TUScheduler
  /// to drop stale publishes that completed AFTER a newer worker
  /// has already published — see the FR-008 stale-drop logic.
  /// Atomic so writes from update() and reads from worker
  /// completion don't need the per-TU mutex.
  void noteReceived(int version) {
    int prev = latest_received_.load(std::memory_order_relaxed);
    while (version > prev && !latest_received_.compare_exchange_weak(
                                 prev, version, std::memory_order_release,
                                 std::memory_order_relaxed)) {
    }
  }
  int latestReceivedVersion() const {
    return latest_received_.load(std::memory_order_acquire);
  }

private:
  mutable std::mutex mtx_;
  State state_;
  std::atomic<int> latest_received_{-1};
};

} // namespace lsp
} // namespace nsl

#endif // NSL_LSP_NSL_TU_H
