// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Fmt/CommentScanner.cpp — raw-source comment scanner used by
// the LayoutPlanner R6 (attached-comment preservation) machinery.

#include "CommentScanner.h"

#include <algorithm>
#include <cstdint>

namespace nsl::fmt {

std::uint32_t lineForOffsetIn(const std::vector<std::uint32_t> &lineTable,
                              std::uint32_t offset) noexcept {
  if (lineTable.empty()) {
    return 1;
  }
  auto it = std::upper_bound(lineTable.begin(), lineTable.end(), offset);
  std::uint32_t line = static_cast<std::uint32_t>(it - lineTable.begin());
  if (line < 1) {
    line = 1;
  }
  return line;
}

std::vector<CommentTok>
scanComments(llvm::StringRef src, std::uint32_t begin, std::uint32_t end,
             const std::vector<std::uint32_t> &lineTable) {
  std::vector<CommentTok> out;
  if (begin >= end) {
    return out;
  }
  std::uint32_t limit = static_cast<std::uint32_t>(src.size());
  if (end > limit) {
    end = limit;
  }
  if (begin >= end) {
    return out;
  }
  std::uint32_t i = begin;
  while (i < end) {
    char c = src[i];
    // Skip double-quoted string literals. NSL `'` is the zero-
    // extend prefix per `lang.ebnf §11`, so single-quotes do NOT
    // open strings here.
    if (c == '"') {
      // Consume until closing `"` (respecting `\\` escapes).
      ++i;
      while (i < end && src[i] != '"') {
        if (src[i] == '\\' && i + 1 < end) {
          i += 2;
          continue;
        }
        ++i;
      }
      if (i < end) {
        ++i; // eat the closing `"`
      }
      continue;
    }
    if (c == '/' && i + 1 < end) {
      char d = src[i + 1];
      if (d == '/') {
        // Line comment: `//` to end-of-line (or end-of-buffer).
        CommentTok tok;
        tok.kind = CommentKind::Line;
        tok.begin = i;
        tok.startLine = lineForOffsetIn(lineTable, i);
        std::uint32_t j = i + 2;
        while (j < end && src[j] != '\n') {
          ++j;
        }
        tok.end = j; // exclusive; points at `\n` (or end)
        tok.endLine = tok.startLine;
        out.push_back(tok);
        i = j;
        continue;
      }
      if (d == '*') {
        // Block comment: `/*` to `*/`.
        CommentTok tok;
        tok.kind = CommentKind::Block;
        tok.begin = i;
        tok.startLine = lineForOffsetIn(lineTable, i);
        std::uint32_t j = i + 2;
        while (j + 1 < end) {
          if (src[j] == '*' && src[j + 1] == '/') {
            j += 2;
            break;
          }
          ++j;
        }
        // If we ran past `end` without finding `*/`, clamp; the
        // input is malformed but we don't want to walk past `end`.
        if (j > end) {
          j = end;
        }
        tok.end = j;
        tok.endLine = lineForOffsetIn(lineTable, j > 0 ? j - 1 : j);
        out.push_back(tok);
        i = j;
        continue;
      }
    }
    ++i;
  }
  return out;
}

} // namespace nsl::fmt
