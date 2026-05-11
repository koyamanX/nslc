// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/LSP/IncludeSearchPath.h — angle-form `#include` search path
// for `nsl-lsp`. Per Clarifications session 2026-05-05 Q2 →
// Option A and FR-020a, the search path is read once at server
// startup from `NSL_INCLUDE`, with the same colon-separated POSIX
// semantic the `nslc` CLI driver uses. Quote-form `#include "…"`
// resolution uses the open document's parent directory and is
// handled by NslTU::reparse, not here.
//
// **Specification anchors**:
//   - `specs/010-t3-lsp-skeleton/contracts/lsp-protocol.contract.md` §8
//   - `specs/010-t3-lsp-skeleton/data-model.md` §2.6

#ifndef NSL_LSP_INCLUDE_SEARCH_PATH_H
#define NSL_LSP_INCLUDE_SEARCH_PATH_H

#include "llvm/ADT/ArrayRef.h"

#include <string>
#include <vector>

namespace nsl {
namespace lsp {

class IncludeSearchPath {
public:
  /// Read `NSL_INCLUDE` from the environment, split on the
  /// platform separator (`:` on POSIX, `;` on Windows — the
  /// project targets only the former per Principle IX), and
  /// return the resulting `IncludeSearchPath`. Empty / unset
  /// produces an empty path.
  ///
  /// The resolved path is logged at INFO per contract §8.4.
  static IncludeSearchPath fromEnv();

  IncludeSearchPath() = default;
  explicit IncludeSearchPath(std::vector<std::string> angle_paths)
      : angle_paths_(std::move(angle_paths)) {}

  llvm::ArrayRef<std::string> anglePaths() const { return angle_paths_; }
  bool empty() const { return angle_paths_.empty(); }

private:
  std::vector<std::string> angle_paths_;
};

} // namespace lsp
} // namespace nsl

#endif // NSL_LSP_INCLUDE_SEARCH_PATH_H
