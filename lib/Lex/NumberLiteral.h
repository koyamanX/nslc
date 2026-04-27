// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lex/NumberLiteral.h — internal interface to the numeric-literal
// scanner used by `Lexer::nextImpl()`. NOT a public header; consumed
// only by `lib/Lex/Lexer.cpp`.

#ifndef NSL_LIB_LEX_NUMBERLITERAL_H
#define NSL_LIB_LEX_NUMBERLITERAL_H

#include "nsl/Lex/Token.h"

#include <cstdint>

#include "llvm/ADT/StringRef.h"

namespace nsl::detail {

/// Result of scanning a numeric literal starting at `buf[cur]`.
struct NumberScanResult {
  TokenKind kind;     ///< Always one of tk_{decimal,hex,binary,octal}_lit.
  uint32_t end;       ///< First offset NOT consumed (i.e. length is `end - cur`).
  uint16_t flags;     ///< Bitwise OR of `Token::NumericFlag` bits.
};

/// Scan a numeric literal beginning at `buf[cur]`. Recognized forms
/// (per `docs/spec/nsl_lang.ebnf` §13 lines 716–741):
///
/// - `c_style_number`     : `0b` / `0B` / `0x` / `0X` followed by a
///                          base-appropriate `value_run`. Emits
///                          `tk_binary_lit` or `tk_hex_lit`.
/// - `verilog_style_number`: `<decimal>'<b|o|d|h><value_run>`. Emits
///                          the matching `tk_*_lit` for the radix
///                          char; the entire span (width + apostrophe
///                          + body) is the single token.
/// - `decimal_integer`    : `digit { digit | "_" }`. Emits
///                          `tk_decimal_lit`.
///
/// `value_digit` for hex/binary/octal forms includes the "value"
/// digits `Z` / `X` / `U` (and lowercase variants); each presence
/// sets the corresponding `Token::NF_Has{Z,X,U}` flag bit. Underscore
/// `_` is a digit separator and is consumed silently.
///
/// Pure function over `(buf, cur)` — no state, no diagnostics.
/// Invariant: `cur < buf.size()` and `buf[cur]` is a digit; otherwise
/// the result is `{tk_unknown, cur, 0}`.
NumberScanResult scanNumber(llvm::StringRef buf, uint32_t cur);

} // namespace nsl::detail

#endif // NSL_LIB_LEX_NUMBERLITERAL_H
