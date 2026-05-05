// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/LSP/CancellationToken.h — per-request cancellation primitive
// for `$/cancelRequest` handling. Per Clarifications session
// 2026-05-05 Q5 → Option A and FR-020h–FR-020j: real cancellation
// (not a no-op stub) for `textDocument/foldingRange` (the only
// cancellable T3 request). Polling discipline per FR-020i +
// folding-range contract §5.
//
// **Specification anchors**:
//   - `specs/010-t3-lsp-skeleton/contracts/lsp-protocol.contract.md` §6
//   - `specs/010-t3-lsp-skeleton/data-model.md` §2.5
//   - `specs/010-t3-lsp-skeleton/research.md` §R7
//
// Header-only (zero-cost when unused).

#ifndef NSL_LSP_CANCELLATION_TOKEN_H
#define NSL_LSP_CANCELLATION_TOKEN_H

#include <atomic>
#include <memory>

namespace nsl {
namespace lsp {

struct CancellationToken {
  std::shared_ptr<std::atomic<bool>> flag;

  /// Construct a fresh, not-yet-cancelled token.
  static CancellationToken make() {
    return CancellationToken{std::make_shared<std::atomic<bool>>(false)};
  }

  /// Construct a "never cancellable" sentinel — handlers can poll
  /// it without a heap allocation when cancellation isn't wired.
  static CancellationToken never() { return CancellationToken{nullptr}; }

  bool isCancelled() const noexcept {
    return flag && flag->load(std::memory_order_acquire);
  }

  void cancel() const noexcept {
    if (flag) flag->store(true, std::memory_order_release);
  }

  bool valid() const noexcept { return static_cast<bool>(flag); }
};

} // namespace lsp
} // namespace nsl

#endif // NSL_LSP_CANCELLATION_TOKEN_H
