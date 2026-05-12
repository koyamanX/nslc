// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/LSP/FormattingHandler.cpp — T5 LSP formatting handler
// (Phase 2 stub).
//
// Per FR-007 (clarified Session 2026-05-12 — `null` on refusal)
// AND Phase 2's "dispatch wired, handlers stubbed" posture, this
// translation unit currently returns `llvm::json::Value(nullptr)`
// unconditionally for every formatting request. Phase 3 (US1)
// replaces this body with the real handler per
// `specs/011-t5-lsp-formatting/contracts/formatting-api.contract.md`
// §2.2 / §3.3, plumbing in the Configuration resolver
// (FR-005..FR-005c), the `nsl::fmt::format_buffer` call, the
// TextEdit converter (FR-006), and the cancellation polling
// (FR-012).
//
// The translation unit deliberately includes `nsl/Fmt/Fmt.h` even
// at the stub stage so the `NslFmt` link edge in
// `lib/LSP/CMakeLists.txt` (T001) is exercised — without this
// include, the linker would not pull in `libNslFmt.a` and the
// Principle II structural-check script
// (`scripts/audit_lsp_no_formatter_duplication.sh`, T030) would
// pass vacuously. The unused-declaration warning is suppressed
// by the `(void)` cast at first use; Phase 3 removes the cast
// as the symbols become live.

#include "FormattingHandler.h"

#include "NslServer.h"
#include "nsl/Fmt/Fmt.h"

namespace nsl {
namespace lsp {

llvm::json::Value buildFormattingResponse(llvm::StringRef documentURI,
                                          llvm::StringRef contents,
                                          std::optional<LineRange> range,
                                          NslServer &backend,
                                          CancellationToken &token) {
  // Phase 2 stub: return `null` per FR-007 unconditionally.
  // Phase 3 (US1 — `textDocument/formatting`) and Phase 4
  // (US2 — `textDocument/rangeFormatting`) fill the real body
  // per `specs/011-t5-lsp-formatting/contracts/formatting-api.contract.md`
  // §2.2 / §3.3.
  //
  // The five parameters are silenced via `(void)` casts at the
  // stub stage so `-Wunused-parameter` stays clean. Phase 3
  // removes each cast as the parameter becomes live.
  (void)documentURI;
  (void)contents;
  (void)range;
  (void)backend;
  (void)token;

  // Exercise the `NslFmt` link edge by naming one of its public
  // symbols. Without a use-site reference the linker drops the
  // archive; SC-007's `nm` script (T030) needs T2 symbols to be
  // present so it can assert they're absent from `libNSLLSP.a`.
  // `version_string()` is the lightest-weight symbol (constexpr-
  // adjacent; no allocation) — see T2 contract §3 / §4.
  (void)::nsl::fmt::version_string();

  return llvm::json::Value(nullptr);
}

} // namespace lsp
} // namespace nsl
