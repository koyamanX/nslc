// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/LSP/FormattingHandler.h — T5 LSP formatting handler (helper
// layer below `NslLSPServer::onFormatting` / `onRangeFormatting`).
//
// Parallels `lib/LSP/FoldingRangeBuilder.h` — the dispatch lives
// in `NslLSPServer.cpp`, the actual builder logic lives here.
// This separation lets the protocol layer stay thin (request
// parsing + worker management) while the language-logic layer
// (this file) owns the `nsl::fmt::format_buffer` invocation, the
// Configuration resolver, and the TextEdit encoding.
//
// **Specification anchors**:
//   - `specs/011-t5-lsp-formatting/contracts/formatting-api.contract.md`
//   - `specs/011-t5-lsp-formatting/contracts/config-resolution.contract.md`
//   - `specs/011-t5-lsp-formatting/contracts/text-edit-shape.contract.md`
//   - `specs/011-t5-lsp-formatting/data-model.md` §2
//
// **Status (Phase 2 — foundational stubs)**: the builder function
// is declared with its eventual signature but currently returns
// `llvm::json::Value(nullptr)` unconditionally — the FR-007 "no
// formatting available" wire shape per the formatting-api
// contract §2.2.3 / §3.3.3. Phase 3 (US1, MVP) and Phase 4 (US2)
// fill the body per the contracts. The seam is established here
// at Phase 2 so the dispatch layer in `NslLSPServer.cpp` can
// land its `onFormatting` / `onRangeFormatting` member functions
// without further restructuring when Phase 3 lands.

#ifndef NSL_LSP_FORMATTING_HANDLER_H
#define NSL_LSP_FORMATTING_HANDLER_H

#include "CancellationToken.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/JSON.h"

#include <optional>

namespace nsl {
namespace lsp {

class NslServer;

/// 1-indexed inclusive line range. Mirrors `nsl::fmt::LineRange`
/// from `include/nsl/Fmt/Fmt.h` (T2's frozen 10-symbol public
/// API) — duplicated here to keep `FormattingHandler.h`'s
/// public surface independent of `Fmt.h`'s exact field layout,
/// so changes to T2's API don't ripple into this header's ABI.
/// The two structs are converted at the call boundary in
/// `FormattingHandler.cpp`.
struct LineRange {
  int firstLine; // 1-indexed inclusive; >= 1
  int lastLine;  // 1-indexed inclusive; >= firstLine
};

/// Compute the LSP `TextEdit[]` response for a
/// `textDocument/formatting` or `textDocument/rangeFormatting`
/// request.
///
/// Per
/// `specs/011-t5-lsp-formatting/contracts/formatting-api.contract.md`
/// §2.2 / §3.3, the returned JSON value is one of:
///   - `llvm::json::Array{}` (empty) — input is already canonical
///     (FR-006a (ii)) OR input is empty.
///   - `llvm::json::Array{singleTextEdit}` — input changed by
///     formatting; the single TextEdit replaces the whole
///     formatted span (FR-006 — Session 2026-05-12 single
///     whole-span TextEdit).
///   - `llvm::json::Value(nullptr)` — parse-error refusal, range
///     out of bounds, inverted range, or unknown document URI
///     (FR-007 / FR-003 / FR-014). Caller (dispatch method)
///     wraps as `result: null` per LSP "no formatting available"
///     convention.
///
/// Cancellation: the handler polls `token` at coarse-grained
/// poll points per FR-012 / contract §2.2.4. On cancellation,
/// returns `llvm::json::Value(nullptr)`; the caller is
/// responsible for distinguishing cancellation (via
/// `token.isCancelled()` post-call) from refusal so the dispatch
/// layer can send `RequestCancelled` (`-32800`) instead of
/// `result: null`.
///
/// **Phase 2 stub**: always returns `llvm::json::Value(nullptr)`.
/// `documentURI`, `contents`, `range`, `backend`, and `token`
/// are accepted but ignored. Phase 3 (US1) and Phase 4 (US2)
/// fill the body per the
/// [`contracts/formatting-api.contract.md`](../../specs/011-t5-lsp-formatting/contracts/formatting-api.contract.md)
/// and
/// [`contracts/config-resolution.contract.md`](../../specs/011-t5-lsp-formatting/contracts/config-resolution.contract.md)
/// frozen surfaces.
llvm::json::Value buildFormattingResponse(llvm::StringRef documentURI,
                                          llvm::StringRef contents,
                                          std::optional<LineRange> range,
                                          NslServer &backend,
                                          CancellationToken &token);

} // namespace lsp
} // namespace nsl

#endif // NSL_LSP_FORMATTING_HANDLER_H
