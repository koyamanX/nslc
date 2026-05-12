//===- DirectiveSplitter.cpp ----------------------------------*- C++ -*-=//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "DirectiveSplitter.h"

#include "nsl/Basic/SourceLocation.h"

#include "llvm/ADT/StringRef.h"

#include <cstddef>
#include <utility>

using llvm::StringRef;
using ::nsl::FileID;
using ::nsl::SourceLocation;
using ::nsl::SourceRange;

namespace nsl::fmt {

namespace {

// Recognised opcode names in lookup order. Ordered most-frequent-
// first (`include` / `define` / `ifdef`) to bias the linear scan
// for hot inputs. The matching enum value is `Opcode::<PascalName>`
// — see `decodeOpcode()` below.
struct OpcodeEntry {
  StringRef name;
  DirectiveTok::Opcode op;
};

constexpr OpcodeEntry kOpcodeTable[] = {
    {"include", DirectiveTok::Opcode::Include},
    {"define", DirectiveTok::Opcode::Define},
    {"ifdef", DirectiveTok::Opcode::Ifdef},
    {"ifndef", DirectiveTok::Opcode::Ifndef},
    {"endif", DirectiveTok::Opcode::Endif},
    {"if", DirectiveTok::Opcode::If}, // After `ifdef`/`ifndef`.
    {"else", DirectiveTok::Opcode::Else},
    {"undef", DirectiveTok::Opcode::Undef},
    {"line", DirectiveTok::Opcode::Line},
};

// Skip horizontal whitespace (space + tab; NOT newlines) starting at
// `pos`. Returns the next position.
constexpr std::size_t skipHorizontal(StringRef s, std::size_t pos) {
  while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) {
    ++pos;
  }
  return pos;
}

// Decode the opcode at `pos`. The caller has positioned `pos`
// immediately AFTER the leading `#`. Returns the matching opcode
// + the position one-past the opcode token, or {std::nullopt, pos}
// if no opcode matches (caller treats the whole line as
// NSLFragment then — but in well-formed NSL this should not
// happen, since every `#`-led line is a directive).
struct OpcodeMatch {
  bool matched;
  DirectiveTok::Opcode op;
  std::size_t afterOpcodeEnd;
};

OpcodeMatch decodeOpcode(StringRef s, std::size_t pos) {
  // Skip any whitespace between `#` and the opcode (the preprocessor
  // grammar allows `#  include` though it is uncommon).
  pos = skipHorizontal(s, pos);
  for (const OpcodeEntry &e : kOpcodeTable) {
    if (pos + e.name.size() > s.size()) {
      continue;
    }
    if (s.substr(pos, e.name.size()) != e.name) {
      continue;
    }
    // Boundary — opcode must end at whitespace, EOL, or EOF (so
    // `#include` matches but `#includer` would not — the latter is
    // ill-formed but we want strict matching).
    std::size_t after = pos + e.name.size();
    if (after == s.size()) {
      return {true, e.op, after};
    }
    char c = s[after];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      return {true, e.op, after};
    }
  }
  return {false, DirectiveTok::Opcode::Include, pos};
}

// Find the position one past the end of the directive line starting
// at `start`. Handles `\\`-continued multi-line directives by
// skipping over `\\<newline>` sequences.
std::size_t findDirectiveLineEnd(StringRef s, std::size_t start) {
  std::size_t pos = start;
  while (pos < s.size()) {
    char c = s[pos];
    if (c == '\\' && pos + 1 < s.size() &&
        (s[pos + 1] == '\n' || s[pos + 1] == '\r')) {
      // `\\` + newline = continuation; eat both (and the `\r\n`
      // pair if CRLF).
      ++pos; // past `\`
      if (s[pos] == '\r' && pos + 1 < s.size() && s[pos + 1] == '\n') {
        pos += 2;
      } else {
        ++pos; // past `\n` or `\r`
      }
      continue;
    }
    if (c == '\n') {
      return pos + 1; // include the trailing `\n` in the directive
    }
    if (c == '\r') {
      // CRLF: include both bytes; bare CR: include just `\r`.
      if (pos + 1 < s.size() && s[pos + 1] == '\n') {
        return pos + 2;
      }
      return pos + 1;
    }
    ++pos;
  }
  // Unterminated last line — directive runs to EOF.
  return pos;
}

// Find the position of the next directive-start (i.e., `#` as the
// first non-whitespace character on a line) at or after `start`,
// or `s.size()` if no further directive exists.
std::size_t findNextDirectiveStart(StringRef s, std::size_t start) {
  std::size_t pos = start;
  while (pos < s.size()) {
    // Find the start of this line.
    std::size_t lineStart = pos;
    // Inspect the first non-whitespace character on the line.
    std::size_t scan = skipHorizontal(s, lineStart);
    if (scan < s.size() && s[scan] == '#') {
      return lineStart;
    }
    // Skip to next line.
    while (pos < s.size() && s[pos] != '\n' && s[pos] != '\r') {
      ++pos;
    }
    if (pos < s.size()) {
      if (s[pos] == '\r' && pos + 1 < s.size() && s[pos + 1] == '\n') {
        pos += 2;
      } else {
        ++pos;
      }
    }
  }
  return s.size();
}

// Build a `SourceRange` covering bytes `[begin, end)` in `fileID`.
// `SourceLocation::make(FileID, uint32_t)` is the canonical
// constructor (see include/nsl/Basic/SourceLocation.h:70). Note
// `kMaxOffset = 1U << 24` (16 MiB) — enforced by an `assert` in
// `make()`. NSL files in the audited corpus are far below this.
SourceRange makeRange(FileID fid, std::size_t begin, std::size_t end) {
  SourceLocation b =
      SourceLocation::make(fid, static_cast<std::uint32_t>(begin));
  SourceLocation e = SourceLocation::make(fid, static_cast<std::uint32_t>(end));
  return SourceRange(b, e);
}

} // namespace

std::vector<Slice> DirectiveSplitter::split(StringRef sourceBuffer,
                                            FileID fileID) const {
  std::vector<Slice> result;
  std::size_t pos = 0;

  while (pos < sourceBuffer.size()) {
    // Find the next directive (or EOF).
    std::size_t directiveLineStart = findNextDirectiveStart(sourceBuffer, pos);

    // Emit any NSL fragment before the directive.
    if (directiveLineStart > pos) {
      Slice frag;
      frag.kind = Slice::Kind::NSLFragment;
      frag.range = makeRange(fileID, pos, directiveLineStart);
      frag.rawText = sourceBuffer.substr(pos, directiveLineStart - pos);
      result.push_back(std::move(frag));
    }

    if (directiveLineStart >= sourceBuffer.size()) {
      break;
    }

    // We're on a `#`-led line. Skip leading whitespace + `#` to
    // decode the opcode.
    std::size_t hashPos = skipHorizontal(sourceBuffer, directiveLineStart);
    // Invariant: sourceBuffer[hashPos] == '#'.
    std::size_t afterHash = hashPos + 1;
    OpcodeMatch m = decodeOpcode(sourceBuffer, afterHash);
    std::size_t lineEnd = findDirectiveLineEnd(sourceBuffer, m.afterOpcodeEnd);

    Slice slice;
    slice.kind = Slice::Kind::Directive;
    slice.range = makeRange(fileID, directiveLineStart, lineEnd);
    slice.rawText =
        sourceBuffer.substr(directiveLineStart, lineEnd - directiveLineStart);

    DirectiveTok &tok = slice.directive;
    tok.opcode = m.matched ? m.op : DirectiveTok::Opcode::Include;
    tok.range = slice.range;
    tok.rawText = slice.rawText;

    result.push_back(std::move(slice));
    pos = lineEnd;
  }

  return result;
}

} // namespace nsl::fmt
