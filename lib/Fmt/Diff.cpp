// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Fmt/Diff.cpp — LCS-based unified-diff implementation
// (T2 Phase 3b — T076).

#include "Diff.h"

#include "llvm/ADT/StringRef.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nsl::fmt {

namespace {

// -----------------------------------------------------------------------------
// Line splitting
// -----------------------------------------------------------------------------
//
// Split `text` on '\n'. The newline character is NOT included in
// the resulting lines (we add it back during emission). A trailing
// '\n' produces a final empty line which we drop, so a non-newline-
// terminated text and a newline-terminated one with the same
// content compare equal at line level — matching `diff -u`.

std::vector<llvm::StringRef> splitLines(llvm::StringRef text) {
  std::vector<llvm::StringRef> lines;
  if (text.empty()) {
    return lines;
  }
  std::size_t start = 0;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '\n') {
      lines.push_back(text.substr(start, i - start));
      start = i + 1;
    }
  }
  if (start < text.size()) {
    lines.push_back(text.substr(start));
  }
  return lines;
}

// -----------------------------------------------------------------------------
// LCS DP table
// -----------------------------------------------------------------------------
//
// Wagner-Fischer LCS: L[i][j] = length of the LCS of a[0..i) and
// b[0..j). Stored as a flat (n+1)*(m+1) row-major buffer.

class LCSTable {
public:
  LCSTable(std::size_t n, std::size_t m)
      : n_(n), m_(m), data_(static_cast<std::size_t>(n + 1) * (m + 1), 0) {}

  std::uint32_t &at(std::size_t i, std::size_t j) {
    return data_[i * (m_ + 1) + j];
  }

  std::uint32_t at(std::size_t i, std::size_t j) const {
    return data_[i * (m_ + 1) + j];
  }

  std::size_t n() const noexcept { return n_; }
  std::size_t m() const noexcept { return m_; }

private:
  std::size_t n_;
  std::size_t m_;
  std::vector<std::uint32_t> data_;
};

void fillLCS(LCSTable &L, const std::vector<llvm::StringRef> &a,
             const std::vector<llvm::StringRef> &b) {
  for (std::size_t i = 1; i <= L.n(); ++i) {
    for (std::size_t j = 1; j <= L.m(); ++j) {
      if (a[i - 1] == b[j - 1]) {
        L.at(i, j) = L.at(i - 1, j - 1) + 1;
      } else {
        L.at(i, j) = std::max(L.at(i - 1, j), L.at(i, j - 1));
      }
    }
  }
}

// -----------------------------------------------------------------------------
// Trace back to recover edit script
// -----------------------------------------------------------------------------

enum class EditKind : std::uint8_t { Context, Removed, Added };

struct Edit {
  EditKind kind;
  llvm::StringRef text;
};

// Walk the LCS table from (n, m) back to (0, 0). To produce the
// `diff -u` convention of "Removed before Added" in the FORWARD
// edit stream, the BACKWARD walk must prefer Added before Removed
// on ties (post-`std::reverse`, that flips to Removed-before-
// Added). The strict `>` (not `>=`) is what makes the tie go to
// Added.

std::vector<Edit> traceDiff(const LCSTable &L,
                            const std::vector<llvm::StringRef> &a,
                            const std::vector<llvm::StringRef> &b) {
  std::vector<Edit> edits;
  std::size_t i = L.n();
  std::size_t j = L.m();
  while (i > 0 && j > 0) {
    if (a[i - 1] == b[j - 1]) {
      edits.push_back({EditKind::Context, a[i - 1]});
      --i;
      --j;
    } else if (L.at(i - 1, j) > L.at(i, j - 1)) {
      edits.push_back({EditKind::Removed, a[i - 1]});
      --i;
    } else {
      edits.push_back({EditKind::Added, b[j - 1]});
      --j;
    }
  }
  while (i > 0) {
    edits.push_back({EditKind::Removed, a[--i]});
  }
  while (j > 0) {
    edits.push_back({EditKind::Added, b[--j]});
  }
  std::reverse(edits.begin(), edits.end());
  return edits;
}

// -----------------------------------------------------------------------------
// Hunk grouping + unified-diff emission
// -----------------------------------------------------------------------------
//
// A hunk is a maximal contiguous span of edits that contains at
// least one Removed/Added line, padded by up to `kContextLines`
// Context lines on each side (truncated at the file boundary).
// Adjacent change runs whose Context gap is <= 2 * kContextLines
// are merged into one hunk (avoids overlapping context).

struct Hunk {
  std::size_t oldStart; // 1-indexed
  std::size_t oldCount;
  std::size_t newStart; // 1-indexed
  std::size_t newCount;
  std::vector<Edit> edits;
};

std::vector<Hunk> groupIntoHunks(const std::vector<Edit> &edits) {
  std::vector<Hunk> hunks;

  // Build per-edit (oldLine, newLine) cursors so we can reconstruct
  // (start, count) for each hunk.
  std::vector<std::size_t> oldLineAt(edits.size() + 1, 0);
  std::vector<std::size_t> newLineAt(edits.size() + 1, 0);
  std::size_t o = 1;
  std::size_t n = 1;
  for (std::size_t i = 0; i < edits.size(); ++i) {
    oldLineAt[i] = o;
    newLineAt[i] = n;
    switch (edits[i].kind) {
    case EditKind::Context:
      ++o;
      ++n;
      break;
    case EditKind::Removed:
      ++o;
      break;
    case EditKind::Added:
      ++n;
      break;
    }
  }
  oldLineAt[edits.size()] = o;
  newLineAt[edits.size()] = n;

  // Identify change-edit indices (Removed or Added).
  std::vector<std::size_t> changeIdx;
  for (std::size_t i = 0; i < edits.size(); ++i) {
    if (edits[i].kind != EditKind::Context) {
      changeIdx.push_back(i);
    }
  }
  if (changeIdx.empty()) {
    return hunks;
  }

  // Group change indices that are within 2*kContextLines of each
  // other (post-merge would have overlapping context, so merge).
  std::size_t group_start = changeIdx.front();
  std::size_t group_end = changeIdx.front();
  for (std::size_t k = 1; k < changeIdx.size(); ++k) {
    std::size_t gap = changeIdx[k] - group_end;
    if (gap <= static_cast<std::size_t>(2 * kContextLines)) {
      group_end = changeIdx[k];
    } else {
      // Emit the current group as a hunk.
      std::size_t lo = (group_start >= static_cast<std::size_t>(kContextLines))
                           ? group_start - kContextLines
                           : 0;
      std::size_t hi =
          std::min(group_end + static_cast<std::size_t>(kContextLines) + 1,
                   edits.size());
      Hunk h;
      h.oldStart = oldLineAt[lo];
      h.newStart = newLineAt[lo];
      for (std::size_t e = lo; e < hi; ++e) {
        h.edits.push_back(edits[e]);
      }
      h.oldCount = oldLineAt[hi] - h.oldStart;
      h.newCount = newLineAt[hi] - h.newStart;
      hunks.push_back(std::move(h));

      group_start = changeIdx[k];
      group_end = changeIdx[k];
    }
  }
  // Flush the last group.
  {
    std::size_t lo = (group_start >= static_cast<std::size_t>(kContextLines))
                         ? group_start - kContextLines
                         : 0;
    std::size_t hi = std::min(
        group_end + static_cast<std::size_t>(kContextLines) + 1, edits.size());
    Hunk h;
    h.oldStart = oldLineAt[lo];
    h.newStart = newLineAt[lo];
    for (std::size_t e = lo; e < hi; ++e) {
      h.edits.push_back(edits[e]);
    }
    h.oldCount = oldLineAt[hi] - h.oldStart;
    h.newCount = newLineAt[hi] - h.newStart;
    hunks.push_back(std::move(h));
  }

  return hunks;
}

void emitHunk(std::string &out, const Hunk &h) {
  // Header: `@@ -<oldStart>,<oldCount> +<newStart>,<newCount> @@`.
  //
  // `diff -u` convention for empty-side hunks (insertion before
  // any old content / deletion of all new content): the start is
  // reported as `start - 1`. So an addition at the very beginning
  // of an empty old text emits `-0,0`; an addition between old
  // lines 5 and 6 emits `-5,0`. Same logic for `+` side.
  std::size_t oldStartReported = h.oldCount == 0 ? h.oldStart - 1 : h.oldStart;
  std::size_t newStartReported = h.newCount == 0 ? h.newStart - 1 : h.newStart;

  out += "@@ -";
  out += std::to_string(oldStartReported);
  out += ',';
  out += std::to_string(h.oldCount);
  out += " +";
  out += std::to_string(newStartReported);
  out += ',';
  out += std::to_string(h.newCount);
  out += " @@\n";

  for (const Edit &e : h.edits) {
    char prefix = ' ';
    switch (e.kind) {
    case EditKind::Context:
      prefix = ' ';
      break;
    case EditKind::Removed:
      prefix = '-';
      break;
    case EditKind::Added:
      prefix = '+';
      break;
    }
    out += prefix;
    out.append(e.text.data(), e.text.size());
    out += '\n';
  }
}

} // namespace

std::string computeUnifiedDiff(llvm::StringRef oldText, llvm::StringRef newText,
                               llvm::StringRef oldName,
                               llvm::StringRef newName) {
  if (oldText == newText) {
    return std::string{};
  }

  std::vector<llvm::StringRef> oldLines = splitLines(oldText);
  std::vector<llvm::StringRef> newLines = splitLines(newText);

  LCSTable L(oldLines.size(), newLines.size());
  fillLCS(L, oldLines, newLines);

  std::vector<Edit> edits = traceDiff(L, oldLines, newLines);
  std::vector<Hunk> hunks = groupIntoHunks(edits);

  if (hunks.empty()) {
    // Inputs differ at byte level but produce no line-level diff
    // (e.g., trailing-newline difference). Emit a minimal
    // header-only output so the caller can still detect the
    // difference; hunks remain empty.
    std::string out;
    out += "--- ";
    out.append(oldName.data(), oldName.size());
    out += "\n+++ ";
    out.append(newName.data(), newName.size());
    out += "\n";
    return out;
  }

  std::string out;
  out += "--- ";
  out.append(oldName.data(), oldName.size());
  out += "\n+++ ";
  out.append(newName.data(), newName.size());
  out += "\n";
  for (const Hunk &h : hunks) {
    emitHunk(out, h);
  }
  return out;
}

} // namespace nsl::fmt
