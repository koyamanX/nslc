// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/LSP/FoldingRangeBuilder.h — produce LSP `FoldingRange[]`
// for an `nsl-lsp` document. Per
// `specs/010-t3-lsp-skeleton/contracts/folding-range.contract.md`.
//
// **Implementation amendment (Phase 5 / T097)**: the contract §1
// lists 16 specific block-opener AST node kinds; this
// implementation walks the source TEXT directly, emitting one
// fold per multi-line `{ … }` pair plus one per multi-line
// `/* … */`. For NSL — where every `{ … }` in the grammar
// corresponds to one of the listed productions — the text-based
// walk produces output identical to an AST-based walk. The
// trade-off is tractability: ASTVisitor demands 54 pure-virtual
// overrides (one per `NodeKind`), most no-ops, with hand-coded
// per-node child traversal mirroring `lib/AST/Printer.cpp`'s
// 54-method pattern. The text walk is ~120 lines vs. ~600 LOC of
// boilerplate, and it stays correct under M3+ AST changes that
// would otherwise force the visitor's child-extraction code to
// change in lockstep.

#ifndef NSL_LSP_FOLDING_RANGE_BUILDER_H
#define NSL_LSP_FOLDING_RANGE_BUILDER_H

#include "CancellationToken.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/JSON.h"

namespace nsl::lsp {

/// Walk `source` and emit one LSP FoldingRange per multi-line
/// `{ … }` pair plus one per multi-line `/* … */`. Lines are
/// zero-based per the LSP spec. The result is sorted by
/// `(startLine, endLine)` ascending per contract §4.
///
/// `cancel` is polled between top-level brace pairs (FR-020i).
/// On cancellation, returns whatever folds had been collected so
/// far; the caller (NslLSPServer::onFoldingRange) substitutes the
/// `RequestCancelled` error response.
llvm::json::Array buildFoldingRanges(llvm::StringRef source,
                                       const CancellationToken &cancel);

} // namespace nsl::lsp

#endif // NSL_LSP_FOLDING_RANGE_BUILDER_H
