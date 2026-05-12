// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/LSP/PositionEncoding.cpp — UTF-8 ↔ UTF-16 conversion impl.
// Pure regex-free state machine over the UTF-8 byte sequence; no
// ICU dependency.

#include "PositionEncoding.h"

namespace nsl {
namespace lsp {

namespace {

// Returns the number of bytes consumed by the UTF-8 sequence
// starting at `b`, and writes the corresponding UTF-16 code-unit
// count (1 for BMP, 2 for supplementary) to `*utf16_units`.
std::size_t utf8SequenceLen(uint8_t b, uint32_t *utf16_units) {
  if (b < 0x80) {
    *utf16_units = 1;
    return 1;
  } // ASCII
  if ((b & 0xE0) == 0xC0) {
    *utf16_units = 1;
    return 2;
  } // 2-byte
  if ((b & 0xF0) == 0xE0) {
    *utf16_units = 1;
    return 3;
  } // 3-byte
  if ((b & 0xF8) == 0xF0) {
    *utf16_units = 2;
    return 4;
  } // 4-byte (surrogate pair)
  // Invalid lead byte — treat as 1-byte / 1-unit. Defensive only;
  // libNSLFrontend.a's lexer rejects malformed UTF-8 upstream.
  *utf16_units = 1;
  return 1;
}

} // namespace

uint32_t byteToUtf16Column(llvm::StringRef line, std::size_t byteOffset) {
  if (byteOffset >= line.size())
    byteOffset = line.size();

  uint32_t utf16 = 0;
  std::size_t i = 0;
  while (i < byteOffset) {
    uint32_t units;
    std::size_t len = utf8SequenceLen(static_cast<uint8_t>(line[i]), &units);
    // Truncated multibyte sequence at the end of `line`: clamp `len`
    // to the bytes that remain so `i` cannot advance past
    // `line.size()`. The lead byte alone advertised 2/3/4 bytes, but
    // the trailing continuation bytes are missing — treat as a
    // single byte / one unit.
    if (i + len > line.size()) {
      len = 1;
      units = 1;
    }
    if (i + len > byteOffset)
      break; // partial mid-sequence — round down
    utf16 += units;
    i += len;
  }
  return utf16;
}

std::size_t utf16ToByteOffset(llvm::StringRef line, uint32_t character) {
  uint32_t utf16 = 0;
  std::size_t i = 0;
  while (i < line.size() && utf16 < character) {
    uint32_t units;
    std::size_t len = utf8SequenceLen(static_cast<uint8_t>(line[i]), &units);
    // Truncated multibyte sequence at the end of `line`: clamp `len`
    // to the bytes that remain so `i` cannot advance past
    // `line.size()`. The lead byte alone advertised 2/3/4 bytes, but
    // the trailing continuation bytes are missing — treat as a
    // single byte / one unit.
    if (i + len > line.size()) {
      len = 1;
      units = 1;
    }
    // If `character` lands inside a supplementary surrogate pair,
    // bumping utf16 by 2 would overshoot. Round DOWN — leave i at
    // the start of the supplementary character.
    if (utf16 + units > character)
      break;
    utf16 += units;
    i += len;
  }
  return i;
}

} // namespace lsp
} // namespace nsl
