// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/Lex/Token.h
//
// Public Token API for `nsl-lex` (data-model entities 7 + 8 in
// `specs/002-m1-lex-preprocess/data-model.md`).
//
// Token.h is intentionally separable from Lexer.h: simple token-emit
// consumers (the M1 `nslc -emit=tokens` driver code path; the future
// LSP `textDocument/semanticTokens` provider at T4) need only the
// Token + TokenKind types, not the stateful scanner. Per
// Constitution Principle II this is the rare case where the layer
// genuinely warrants two public headers; consumers that want only
// the Token vocabulary include Token.h alone.
//
// The `TokenKind` enum is populated from two X-macro source-of-truth
// `.def` files plus a hand-enumerated punctuation set:
//   - `nsl/Lex/KeywordSet.def` (research §6) — one enumerator per
//     reserved keyword from `docs/spec/nsl_lang.ebnf` §15.
//   - The Z/X/U "value digits" in numeric literals are NOT distinct
//     kinds; they ride on `Token::flags()` as `NumericFlag` bits
//     (research §7) so the parser sees a single `tk_*_lit` for every
//     base regardless of digit content.

#ifndef NSL_LEX_TOKEN_H
#define NSL_LEX_TOKEN_H

#include "nsl/Basic/SourceLocation.h"

#include "llvm/ADT/StringRef.h"

#include <cstdint>

namespace nsl {

/// A token's lexical category.
///
/// Numeric subkinds (Z/X/U digit content) are NOT enumerated here —
/// they are carried as `Token::flags()` bits per `NumericFlag`. See
/// research §7 for the rationale (avoiding 4 bases × 4 digit-forms =
/// 16 redundant enumerators).
enum class TokenKind : uint16_t {
  // ----- Sentinels -----
  tk_unknown,
  tk_eof,

  // ----- Identifiers + literals -----
  tk_identifier,
  tk_string_lit,

  // One enumerator per numeric base (research §7).
  tk_decimal_lit,
  tk_hex_lit,
  tk_binary_lit,
  tk_octal_lit,

  // ----- _-prefix system names (per parser-note N11) -----
  tk_system_task,       ///< `_display`, `_finish`, `_init`, ...
  tk_system_function,   ///< `_random`, `_time` (no-parens system var)
  tk_unused_underscore, ///< Any other reserved-but-unused `_`-prefix

// ----- Reserved keywords (one enumerator per entry in §15) -----
// Generated from include/nsl/Lex/KeywordSet.def.
#define KEYWORD(name, spelling) tk_##name,
#include "nsl/Lex/KeywordSet.def"
#undef KEYWORD

  // ----- Punctuation / operators (data-model entity 7) -----
  tk_lparen,
  tk_rparen,
  tk_lbrace,
  tk_rbrace,
  tk_lbracket,
  tk_rbracket,
  tk_comma,
  tk_semicolon,
  tk_colon,
  tk_dot,
  tk_assign,     ///< `=`
  tk_assign_seq, ///< `:=` (S3 sequential assignment)
  tk_plus,
  tk_minus,
  tk_star,
  tk_slash,
  tk_percent,
  tk_amp,
  tk_pipe,
  tk_caret,
  tk_tilde,
  tk_logical_and, ///< `&&`
  tk_logical_or,  ///< `||`
  tk_logical_not, ///< `!`
  tk_equal,       ///< `==`
  tk_not_equal,   ///< `!=`
  tk_less,
  tk_less_equal,
  tk_greater,
  tk_greater_equal,
  tk_shift_left,  ///< `<<`
  tk_shift_right, ///< `>>`
  tk_question,
  tk_at,
  tk_hash_sign_extend,       ///< `#` in expression position (N5)
  tk_apostrophe_zero_extend, ///< `'` (zero-extend)
  tk_dot_lbrace,             ///< `.{` (N3)

  // ----- Preprocessor seam markers (P13) -----
  /// `#line N "file"` passed through to the M2 parser.
  tk_line_directive,

  /// Sentinel for fixed-size tables keyed by kind. Always last.
  tk_count
};

/// Convert a `TokenKind` to its string name (e.g., `tk_module`,
/// `tk_decimal_lit`). Used by the `-emit=tokens` driver per the
/// `nslc-emit-tokens.contract.md` stdout schema; also useful for
/// diagnostic messages and gtest output.
llvm::StringRef toString(TokenKind k);

/// A token emitted by `Lexer::next()`.
///
/// Cheap to copy (3 pointers + 32-bit ints). The `spelling()` view
/// aliases the originating `Buffer` and is valid for the lifetime of
/// the `SourceManager` that owns it (which is the entire compile —
/// `Buffer.bytes` is stable post-load per data-model entity 4).
class Token {
public:
  /// Per-token flag bits. Used today only for numeric literal
  /// `Z`/`X`/`U` digit content; reserved bits remain available for
  /// later milestones (e.g., `tk_string_lit` raw vs cooked at M5).
  enum NumericFlag : uint16_t {
    NF_Plain = 0,
    NF_HasZ = 1U << 0,
    NF_HasX = 1U << 1,
    NF_HasU = 1U << 2,
  };

  /// Default-construct an `tk_unknown` token (sentinel).
  Token() noexcept = default;

  /// Construct a fully-specified token. `flags` defaults to plain.
  Token(TokenKind kind, SourceRange range, llvm::StringRef spelling,
        uint16_t flags = NF_Plain) noexcept
      : kind_(kind), flags_(flags), range_(range), spelling_(spelling) {}

  [[nodiscard]] TokenKind kind() const noexcept { return kind_; }
  [[nodiscard]] SourceRange range() const noexcept { return range_; }
  [[nodiscard]] llvm::StringRef spelling() const noexcept { return spelling_; }
  [[nodiscard]] uint16_t flags() const noexcept { return flags_; }

private:
  TokenKind kind_ = TokenKind::tk_unknown;
  uint16_t flags_ = 0;
  SourceRange range_;
  llvm::StringRef spelling_;
};

} // namespace nsl

#endif // NSL_LEX_TOKEN_H
