// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/LSP/PositionEncoding.h — UTF-8 ↔ UTF-16 code-unit conversion
// at the LSP protocol boundary (T3). LSP 3.16 floor uses UTF-16
// unconditionally per Clarifications session 2026-05-05 Q3 →
// Option A and FR-004; this seam converts between the project's
// internal byte-offset representation (libNSLFrontend.a) and the
// wire-format UTF-16 code-unit positions.
//
// **Specification anchors**:
//   - `specs/010-t3-lsp-skeleton/research.md` §R6
//   - `specs/010-t3-lsp-skeleton/data-model.md` §2.6
//   - `specs/010-t3-lsp-skeleton/contracts/diagnostic-mapping.contract.md` §3
//
// Pure functions (no mutable state); ASCII fast-path short-circuits
// to byte-equals-UTF-16 in the audited-corpus norm.

#ifndef NSL_LSP_POSITION_ENCODING_H
#define NSL_LSP_POSITION_ENCODING_H

#include "llvm/ADT/StringRef.h"

#include <cstddef>
#include <cstdint>

namespace nsl {
namespace lsp {

/// Convert a byte offset within a single source line to a UTF-16
/// code-unit offset (LSP 3.16 default `Position.character` units).
///
/// `line` is the line text without the trailing newline. `byteOffset`
/// is in [0, line.size()]; values past end clamp to end.
///
/// Supplementary-plane characters (U+10000…U+10FFFF) count as TWO
/// UTF-16 code units (the surrogate pair). BMP characters count as
/// ONE. Pure-ASCII lines short-circuit to byte = code-unit (the
/// audited-corpus norm).
uint32_t byteToUtf16Column(llvm::StringRef line, std::size_t byteOffset);

/// Inverse of `byteToUtf16Column`: convert a UTF-16 code-unit
/// `character` index to a byte offset within `line`. Values past
/// end clamp to `line.size()`. If `character` lands inside a
/// surrogate pair (mid-supplementary), this rounds DOWN to the
/// preceding character boundary (an edge case that does not arise
/// in the audited corpus per FR-004 / LSP 3.16 documentation).
std::size_t utf16ToByteOffset(llvm::StringRef line, uint32_t character);

} // namespace lsp
} // namespace nsl

#endif // NSL_LSP_POSITION_ENCODING_H
