//===- CSTBuilder.h - CSTSink impl that records parser events ----*- C++ -*-=//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Internal header — NOT exported through `Fmt.h`.
//
// `CSTBuilder` implements `nsl::parse::CSTSink` from above the
// `nsl-parse` layer (Principle II layer-table rule — dependency
// direction stays downward; `nsl-parse` knows nothing about
// `nsl-fmt`). It records every `beginNode`/`recordToken`/`endNode`
// call and reconstitutes a CST tree consumable by the LayoutPlanner
// (Phase 3).
//
// **Phase 2b scope**: only the top-level production is wrapped by
// the parser today (one beginNode/endNode pair per `parseCompilationUnit`).
// Tokens flow in via `recordToken`. The CST tree at this stage is a
// shallow root containing one `NSLToken` leaf per consumed token.
// Phase 3+ adds per-production `beginNode`/`endNode` instrumentation
// to the parser, at which point CSTBuilder will build a deeper tree
// — with no API change here.
//
// The builder also captures a `StringRef` of the source buffer so
// `serialize()` can reconstitute the source byte-for-byte (FR-008
// idempotence root case + cst-shape contract §8 round-trip
// invariant). Lifetime contract: the source buffer MUST outlive the
// builder.
//
// **Spec / contract anchors**:
//   - `specs/010-t2-formatter-v0/data-model.md` §3 (CSTBuilder API).
//   - `specs/010-t2-formatter-v0/contracts/cst-shape.contract.md` §8
//     (serialize round-trip invariant).
//   - `include/nsl/Parse/Parser.h` (CSTSink interface T2 Phase 2b
//     extends).
//
//===----------------------------------------------------------------------===//

#ifndef NSL_FMT_LIB_CST_BUILDER_H
#define NSL_FMT_LIB_CST_BUILDER_H

#include "CST.h"

#include "nsl/Basic/SourceLocation.h"
#include "nsl/Lex/Token.h"
#include "nsl/Parse/Parser.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"

#include <string>
#include <vector>

namespace nsl::fmt {

/// Concrete CSTSink that records parser events and serialises the
/// captured source byte-for-byte.
class CSTBuilder : public ::nsl::parse::CSTSink {
public:
  /// `sourceBuffer` is the raw source the parser is consuming.
  /// Its lifetime MUST exceed every subsequent parser callback +
  /// every call to `serialize()`. Typically the source MemoryBuffer
  /// is owned by the caller and outlives the parse pass.
  explicit CSTBuilder(llvm::StringRef sourceBuffer) noexcept
      : src_(sourceBuffer) {}

  // ---- CSTSink overrides ----------------------------------------

  void beginNode(llvm::StringRef kindName,
                 const ::nsl::SourceLocation &start) override;

  void recordToken(const ::nsl::Token &tok) override;

  void endNode(const ::nsl::SourceLocation &end) override;

  // ---- Public observers used by tests + the LayoutPlanner -------

  /// Number of completed top-level nodes (one per `endNode` call).
  /// Phase 2b expects exactly 1 after a successful parse.
  [[nodiscard]] std::size_t nodeCount() const noexcept {
    return completedNodes_.size();
  }

  /// Number of recorded tokens.
  [[nodiscard]] std::size_t tokenCount() const noexcept {
    return tokens_.size();
  }

  /// Reconstruct the source buffer byte-for-byte from the recorded
  /// token stream and the held source view. Per cst-shape contract
  /// §8: emit `src_[prev_end .. tok_i.begin)` (trivia) followed by
  /// `src_[tok_i.begin .. tok_i.end)` (token bytes); after the last
  /// token, emit `src_[last_end .. src_.size())` (trailing newline /
  /// EOF trivia). Returns `src_.str()` byte-for-byte iff the
  /// no-byte-loss invariant holds.
  [[nodiscard]] std::string serialize() const;

  // ---- Frame data (one per beginNode/endNode pair) --------------

  struct Frame {
    std::string         kindName;  // owns the kind string
    ::nsl::SourceLocation start;
    ::nsl::SourceLocation end;
  };

  [[nodiscard]] llvm::ArrayRef<Frame> completedNodes() const noexcept {
    return completedNodes_;
  }

  [[nodiscard]] llvm::ArrayRef<::nsl::Token> tokens() const noexcept {
    return tokens_;
  }

private:
  llvm::StringRef                  src_;
  llvm::SmallVector<Frame, 4>      openStack_;
  std::vector<Frame>               completedNodes_;
  std::vector<::nsl::Token>        tokens_;
};

} // namespace nsl::fmt

#endif // NSL_FMT_LIB_CST_BUILDER_H
