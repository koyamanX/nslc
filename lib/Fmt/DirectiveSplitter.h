//===- DirectiveSplitter.h - Pre-pass that separates directives ---*- C++ -*-=//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Internal header — NOT exported through `Fmt.h` (Principle II).
//
// `DirectiveSplitter` is the directive-aware pre-pass that consumes
// raw NSL source and emits a vector of `Slice`s, each of which is
// either a complete preprocessor directive line (or `\\`-continued
// span) or a contiguous run of NSL source between directives.
//
// **Why this exists**: per /speckit-clarify Q1 → Option A
// (recorded in spec.md `## Clarifications`), `nsl-fmt` parses raw
// source BEFORE preprocessing — preserving every directive
// byte-for-byte. This pre-pass is the only new parsing code T2
// owns; NSL fragments between directives flow through the existing
// `libNSLFrontend.a` parser (Principle II — no-duplication).
//
// **Spec / contract anchors**:
//   - spec.md FR-012a (raw-source parse + opaque directives)
//   - research.md §1 (implementation shape: line-oriented scanner,
//     `\\`-continuation handling, BOM-prefix retention)
//   - data-model.md §2 (`split()` invariants)
//   - cst-shape.contract.md §1, §5 (slice partition + DirectiveTok)
//
//===----------------------------------------------------------------------===//

#ifndef NSL_FMT_LIB_DIRECTIVE_SPLITTER_H
#define NSL_FMT_LIB_DIRECTIVE_SPLITTER_H

#include "CST.h"
#include "nsl/Basic/SourceLocation.h"

#include "llvm/ADT/StringRef.h"

#include <vector>

namespace nsl::fmt {

class DirectiveSplitter {
public:
  /// Split `sourceBuffer` into a vector of `Slice`s.
  ///
  /// Postconditions (data-model §2):
  ///   * Every byte of `sourceBuffer` is covered by exactly ONE slice
  ///     (no gaps, no overlaps).
  ///   * Slice ranges are monotonically non-decreasing (source order).
  ///   * Each `Slice::Kind::Directive` covers a complete directive
  ///     line including all `\\`-continued continuation lines.
  ///   * BOM bytes at file start (if present) are retained as the
  ///     leading bytes of the first slice's `rawText` (caller decides
  ///     how to render — `LayoutRenderer` preserves them; FR for BOM
  ///     preservation is in spec edge cases).
  ///   * Determinism: pure function of `sourceBuffer` (Principle V).
  std::vector<Slice> split(llvm::StringRef sourceBuffer,
                           ::nsl::FileID fileID) const;
};

} // namespace nsl::fmt

#endif // NSL_FMT_LIB_DIRECTIVE_SPLITTER_H
