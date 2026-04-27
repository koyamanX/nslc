// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lex/NumberLiteral.cpp — pure-function numeric-literal scanner
// per `docs/spec/nsl_lang.ebnf` §13 (lines 716–741) and data-model
// entity 7 (research §7).

#include "NumberLiteral.h"

#include "nsl/Lex/Token.h"

#include "llvm/ADT/StringRef.h"

#include <cstdint>

namespace nsl::detail {

namespace {

/// True iff `c` is a base-10 digit.
bool isDecDigit(char c) {
  return c >= '0' && c <= '9';
}

/// True iff `c` is a base-16 digit (0-9, a-f, A-F).
bool isHexDigit(char c) {
  return isDecDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/// True iff `c` is a "value digit" Z/X/U marker (case-insensitive)
/// per `value_digit` in lang.ebnf §13 line 739.
///
/// Returns the lowercased marker character via `out_marker` (or
/// `'\0'` if `c` is not a marker).
bool isValueMarker(char c, char *out_marker) {
  switch (c) {
  case 'Z':
  case 'z':
    *out_marker = 'z';
    return true;
  case 'X':
  case 'x':
    *out_marker = 'x';
    return true;
  case 'U':
  case 'u':
    *out_marker = 'u';
    return true;
  default:
    *out_marker = '\0';
    return false;
  }
}

/// Update `flags` for a value marker (Z/X/U).
void setMarkerFlag(char marker, uint16_t &flags) {
  switch (marker) {
  case 'z':
    flags |= Token::NF_HasZ;
    break;
  case 'x':
    flags |= Token::NF_HasX;
    break;
  case 'u':
    flags |= Token::NF_HasU;
    break;
  default:
    break;
  }
}

/// Consume a `value_run` for the given base, starting at `pos`.
/// `base` is 2 / 8 / 16 (decimal handled separately because Z/X/U are
/// not legal in plain decimal per the lang.ebnf §13 commentary at
/// lines 735–736).
///
/// Returns the offset just past the last consumed digit/marker/`_`.
/// Sets `Z`/`X`/`U` flag bits on `flags` as markers are seen.
uint32_t consumeValueRun(llvm::StringRef buf, uint32_t pos, int base,
                         uint16_t &flags) {
  while (pos < buf.size()) {
    char c = buf[pos];
    char marker = '\0';
    if (c == '_') {
      ++pos;
      continue;
    }
    if (isValueMarker(c, &marker)) {
      setMarkerFlag(marker, flags);
      ++pos;
      continue;
    }
    bool ok = false;
    switch (base) {
    case 2:
      ok = (c == '0' || c == '1');
      break;
    case 8:
      ok = (c >= '0' && c <= '7');
      break;
    case 16:
      ok = isHexDigit(c);
      break;
    default:
      ok = false;
    }
    if (!ok) {
      break;
    }
    ++pos;
  }
  return pos;
}

/// Map a Verilog radix char (b/o/d/h, case-insensitive) to a
/// `(TokenKind, base)` pair. Returns `{tk_unknown, 0}` if the char is
/// not a recognized radix.
struct RadixInfo {
  TokenKind kind;
  int base;
};

RadixInfo radixFromChar(char c) {
  switch (c) {
  case 'b':
  case 'B':
    return {TokenKind::tk_binary_lit, 2};
  case 'o':
  case 'O':
    return {TokenKind::tk_octal_lit, 8};
  case 'd':
  case 'D':
    return {TokenKind::tk_decimal_lit, 10};
  case 'h':
  case 'H':
    return {TokenKind::tk_hex_lit, 16};
  default:
    return {TokenKind::tk_unknown, 0};
  }
}

} // namespace

NumberScanResult scanNumber(llvm::StringRef buf, uint32_t cur) {
  if (cur >= buf.size() || !isDecDigit(buf[cur])) {
    return {TokenKind::tk_unknown, cur, 0};
  }

  // ---- C-style: `0b...` / `0B...` / `0x...` / `0X...` ----
  if (buf[cur] == '0' && cur + 1 < buf.size()) {
    char p = buf[cur + 1];
    if (p == 'x' || p == 'X') {
      uint16_t flags = 0;
      uint32_t end = consumeValueRun(buf, cur + 2, /*base=*/16, flags);
      return {TokenKind::tk_hex_lit, end, flags};
    }
    if (p == 'b' || p == 'B') {
      uint16_t flags = 0;
      uint32_t end = consumeValueRun(buf, cur + 2, /*base=*/2, flags);
      return {TokenKind::tk_binary_lit, end, flags};
    }
    // `0o` / `0O` is NOT in lang.ebnf §13's c_radix_char alternation
    // (only b and x are). Fall through to decimal handling — a leading
    // zero is a valid decimal_integer per `digit { digit | "_" }`.
  }

  // ---- Scan a leading decimal integer (the `width` of a Verilog-
  //      style number, or a plain decimal literal). ----
  uint32_t pos = cur;
  while (pos < buf.size() && (isDecDigit(buf[pos]) || buf[pos] == '_')) {
    ++pos;
  }

  // ---- Verilog-style `<width>'<radix><value_run>` ----
  if (pos < buf.size() && buf[pos] == '\'') {
    if (pos + 1 < buf.size()) {
      RadixInfo r = radixFromChar(buf[pos + 1]);
      if (r.kind != TokenKind::tk_unknown) {
        uint16_t flags = 0;
        // Decimal verilog-style ('d) does not permit Z/X/U markers
        // per lang.ebnf §13 lines 735–736; but the consumer treats
        // markers as illegal naturally because `consumeValueRun` for
        // base 10 would never accept them — handle this by routing
        // base 10 through the decimal-only loop.
        uint32_t end = 0;
        if (r.base == 10) {
          uint32_t p = pos + 2;
          while (p < buf.size() && (isDecDigit(buf[p]) || buf[p] == '_')) {
            ++p;
          }
          end = p;
        } else {
          end = consumeValueRun(buf, pos + 2, r.base, flags);
        }
        return {r.kind, end, flags};
      }
    }
    // Apostrophe without a radix char: leave the apostrophe to be
    // re-tokenized as `tk_apostrophe_zero_extend` by the caller.
  }

  // ---- Plain decimal literal ----
  return {TokenKind::tk_decimal_lit, pos, 0};
}

} // namespace nsl::detail
