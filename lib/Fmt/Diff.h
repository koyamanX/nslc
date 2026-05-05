//===- Diff.h - Unified-diff emitter for nsl-fmt --check --------*- C++ -*-=//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Internal header — NOT exported through `Fmt.h`.
//
// LCS-based unified-diff emitter (T2 Phase 3b — T076). The
// public-facing entry point is `nsl::fmt::emit_unified_diff` in
// `Fmt.h`; that function delegates to `computeUnifiedDiff()` below
// for the real implementation.
//
// **Algorithm**: line-level Longest Common Subsequence (Wagner-
// Fischer DP, O(N*M) time and memory). For typical formatter
// inputs (a few hundred to a few thousand lines), O(NM) memory is
// well within budget — a 1000×1000 LCS table is ~4 MB. The Myers
// O(ND) algorithm would be faster asymptotically but is overkill
// for the file sizes the formatter sees.
//
// **Determinism** (Principle V): pure function of `(oldText,
// newText, oldName, newName)`. The DP table fills in deterministic
// order; the trace-back chooses removals before additions when
// LCS lengths tie (matches `diff -u` convention and gives stable
// hunk ordering).
//
// Output format matches `diff -u`:
//
//     --- oldName
//     +++ newName
//     @@ -<a>,<b> +<c>,<d> @@
//      context-line
//     -removed-line
//     +added-line
//      context-line
//
// where `<a>`/`<c>` are 1-indexed start lines and `<b>`/`<d>` are
// line counts. Hunks include up to `kContextLines` lines of
// context around each change run.
//
//===----------------------------------------------------------------------===//

#ifndef NSL_FMT_LIB_DIFF_H
#define NSL_FMT_LIB_DIFF_H

#include "llvm/ADT/StringRef.h"

#include <string>

namespace nsl::fmt {

/// Number of unchanged context lines emitted around each hunk.
/// Matches `diff -u`'s default. Not a Configuration knob at T2;
/// future enhancement could expose it.
inline constexpr int kContextLines = 3;

/// Compute the unified-diff representation of `(oldText -> newText)`.
/// Pure function. Returns the empty string when `oldText == newText`.
std::string computeUnifiedDiff(llvm::StringRef oldText, llvm::StringRef newText,
                               llvm::StringRef oldName,
                               llvm::StringRef newName);

} // namespace nsl::fmt

#endif // NSL_FMT_LIB_DIFF_H
