//===- CommentScanner.h - Raw-source comment scanner for nsl-fmt --*- C++ -*-=//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Internal header — NOT exported through `Fmt.h` (Principle II).
//
// R6 (attached-comment preservation) requires the formatter to know
// where each `//`-line and `/* */`-block comment is in the raw
// source, and to classify each as "leading" (above the next decl),
// "trailing" (same line as the preceding decl), or "inline" (between
// two tokens of a single statement). The Lexer
// (`lib/Lex/Lexer.cpp::skipWhitespaceAndComments`) discards comments
// entirely by design — they never reach the AST. The CSTBuilder
// (`lib/Fmt/CSTBuilder.h`) is Phase 2b and records flat tokens
// without trivia attachment. So R6 needs its own raw-source scanner
// that walks the byte range between AST node positions, recognises
// the two comment forms, and reports each occurrence with its byte
// span + start/end line. The LayoutPlanner then classifies each
// occurrence by inspecting the surrounding tokens / newlines.
//
// Principle II compliance: this scanner is a TRIVIA-ONLY pass that
// runs ONCE per gap region and produces only span-and-line records.
// It does NOT re-lex identifiers, numbers, operators, or string
// literals. The Lexer remains the single source of truth for all
// non-trivia tokenisation.
//
//===----------------------------------------------------------------------===//

#ifndef NSL_FMT_LIB_COMMENT_SCANNER_H
#define NSL_FMT_LIB_COMMENT_SCANNER_H

#include "llvm/ADT/StringRef.h"

#include <cstdint>
#include <vector>

namespace nsl::fmt {

/// Kind of a single comment occurrence.
enum class CommentKind {
  Line,  // `// ... \n`  (terminated by newline or end-of-buffer)
  Block, // `/* ... */`  (may span multiple lines)
};

/// One comment occurrence scanned out of a source byte range.
struct CommentTok {
  CommentKind kind;
  std::uint32_t begin;     // byte offset of first `/`
  std::uint32_t end;       // one-past-last byte (exclusive); for
                           // Line, this is the offset of the
                           // terminating `\n` (or end-of-buffer);
                           // for Block, the byte right after `*/`.
  std::uint32_t startLine; // 1-indexed (within `src`)
  std::uint32_t endLine;   // 1-indexed; for Line == startLine.
};

/// Scan `src[begin..end)` for comments. The half-open byte range is
/// clamped to `[0, src.size())` before scanning. Out-of-order
/// `begin > end` returns empty.
///
/// **String literals**: the scanner treats `"..."` as opaque. Any
/// `//` or `/*` inside a string literal does NOT start a comment.
/// (Bare `'` characters do NOT open a quote — NSL uses `'` as the
/// zero-extend prefix per `lang.ebnf §11`; only `"` opens strings.)
///
/// `lineTable` MUST be the line-start-offsets table for the same
/// `src` buffer (entry 0 = 0; entry i = byte offset of line i+1's
/// first byte). `lineForOffsetIn(...)` maps any offset to a 1-
/// indexed line via `std::upper_bound`.
std::vector<CommentTok>
scanComments(llvm::StringRef src, std::uint32_t begin, std::uint32_t end,
             const std::vector<std::uint32_t> &lineTable);

/// Compute the 1-indexed line number for a byte offset, given a
/// pre-built line-start table. Out-of-range offsets clamp to the
/// last line.
std::uint32_t lineForOffsetIn(const std::vector<std::uint32_t> &lineTable,
                              std::uint32_t offset) noexcept;

} // namespace nsl::fmt

#endif // NSL_FMT_LIB_COMMENT_SCANNER_H
