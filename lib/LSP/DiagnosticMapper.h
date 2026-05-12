// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/LSP/DiagnosticMapper.h — `nsl::Diagnostic` → LSP `Diagnostic`
// adapter (Phase 3 / US1, T068). Per
// `specs/010-t3-lsp-skeleton/contracts/diagnostic-mapping.contract.md`.
//
// Free functions; no mutable state. The `code` field extraction
// uses a `\(S\d+\)$` / `\(N\d+\)$` / `\(P\d+\)$` regex over the
// frozen `nsl::Diagnostic.message` strings (per
// `specs/006-m3-sema/contracts/diagnostic-string.contract.md`).

#ifndef NSL_LSP_DIAGNOSTIC_MAPPER_H
#define NSL_LSP_DIAGNOSTIC_MAPPER_H

#include "nsl/Basic/Diagnostic.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/JSON.h"

namespace nsl {
class SourceManager;
} // namespace nsl

namespace nsl::lsp {

/// Convert a single `nsl::Diagnostic` to its LSP wire shape per
/// `contracts/diagnostic-mapping.contract.md` §1–§7. Field order
/// follows §6: range, severity, code, source, message,
/// relatedInformation (omitted when empty).
llvm::json::Value toLspDiagnostic(const nsl::Diagnostic &d,
                                  const nsl::SourceManager &sm);

/// Convert an array of `nsl::Diagnostic` and apply the contract
/// §6 sort order (line, character, severity).
llvm::json::Array toLspDiagnosticArray(llvm::ArrayRef<nsl::Diagnostic> diags,
                                       const nsl::SourceManager &sm);

} // namespace nsl::lsp

#endif // NSL_LSP_DIAGNOSTIC_MAPPER_H
