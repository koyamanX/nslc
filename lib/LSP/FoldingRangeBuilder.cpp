// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/LSP/FoldingRangeBuilder.cpp — text-based brace-and-comment
// walker. Per `contracts/folding-range.contract.md`.
//
// Algorithm:
//   - Track the current source line as we scan byte-by-byte.
//   - Skip strings (`" … "` with `\"` escape).
//   - Skip line comments (`// …` to end of line).
//   - Recognize `/* … */`. If end-line > start-line, emit a
//     `kind: "comment"` fold.
//   - Recognize `{` / `}`. Push start-line on `{`; pop on `}`. If
//     end-line > start-line, emit a code-block fold (no `kind`
//     field per LSP 3.16 §FoldingRange).
//
// The walker is O(N) in the source byte length. Polling cadence
// for cancellation: once per top-level `}` (i.e., when popping
// down to depth 0). Pathological deep nesting still bounded by
// the SC-010 200ms budget on the >10000-node fixture.

#include "FoldingRangeBuilder.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace nsl::lsp {

namespace {

struct FoldingRecord {
  uint32_t start_line;
  uint32_t end_line;
  bool is_comment;

  bool operator<(const FoldingRecord &o) const {
    if (start_line != o.start_line)
      return start_line < o.start_line;
    return end_line < o.end_line;
  }
};

} // namespace

llvm::json::Array buildFoldingRanges(llvm::StringRef source,
                                     const CancellationToken &cancel) {
  std::vector<FoldingRecord> records;
  std::vector<uint32_t> brace_stack; // open-line per open brace
  uint32_t line = 0;
  std::size_t i = 0;
  const std::size_t n = source.size();

  while (i < n) {
    char c = source[i];

    if (c == '\n') {
      ++line;
      ++i;
      continue;
    }

    // Line comment: skip to end of line.
    if (c == '/' && i + 1 < n && source[i + 1] == '/') {
      while (i < n && source[i] != '\n')
        ++i;
      continue;
    }

    // Block comment: scan to `*/`. Multi-line emits a fold.
    if (c == '/' && i + 1 < n && source[i + 1] == '*') {
      uint32_t start_line = line;
      i += 2;
      while (i + 1 < n) {
        if (source[i] == '*' && source[i + 1] == '/') {
          i += 2;
          break;
        }
        if (source[i] == '\n')
          ++line;
        ++i;
      }
      if (line > start_line) {
        records.push_back({start_line, line, /*is_comment=*/true});
      }
      continue;
    }

    // String literal: skip to closing `"`, honoring `\"` escape.
    if (c == '"') {
      ++i;
      while (i < n) {
        if (source[i] == '\\' && i + 1 < n) {
          i += 2;
          continue;
        }
        if (source[i] == '\n')
          ++line;
        if (source[i] == '"') {
          ++i;
          break;
        }
        ++i;
      }
      continue;
    }

    if (c == '{') {
      brace_stack.push_back(line);
      ++i;
      continue;
    }

    if (c == '}') {
      if (!brace_stack.empty()) {
        uint32_t start_line = brace_stack.back();
        brace_stack.pop_back();
        if (line > start_line) {
          records.push_back({start_line, line, /*is_comment=*/false});
        }
      }
      ++i;

      // Cancellation polling: once per top-level close brace
      // (when we just returned to depth 0). Cheap atomic load.
      if (brace_stack.empty() && cancel.isCancelled()) {
        // Caller observes the cancellation flag and substitutes
        // the RequestCancelled error response.
        break;
      }
      continue;
    }

    ++i;
  }

  std::sort(records.begin(), records.end());

  llvm::json::Array out;
  out.reserve(records.size());
  for (const auto &r : records) {
    llvm::json::Object obj{
        {"endLine", static_cast<int64_t>(r.end_line)},
        {"startLine", static_cast<int64_t>(r.start_line)},
    };
    if (r.is_comment)
      obj["kind"] = "comment";
    out.emplace_back(std::move(obj));
  }
  return out;
}

} // namespace nsl::lsp
