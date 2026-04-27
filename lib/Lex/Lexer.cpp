// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lex/Lexer.cpp — pull-model scanner over a single buffer.
//
// Implements data-model entity 9. Decision-flow diagram lives in
// `specs/002-m1-lex-preprocess/data-model.md` "Lexer::next() decision
// flow". Disambiguation rules consumed at this layer:
//
//   - N5 (`#` line-marker vs sign-extend): when `at_line_start_` and
//     the current char is `#`, peek for a digit-bearing line and
//     emit `tk_line_directive`; otherwise emit `tk_hash_sign_extend`.
//   - N11 (`_`-prefix system names): three closed sets of identifiers
//     classify into `tk_system_task`, `tk_system_function`, or
//     `tk_unused_underscore`. The preprocessor-helper class
//     (lang.ebnf:1101) is preprocessor-only — by construction, the
//     lexer never sees a `_pow` / `_int` / etc. (the preprocessor
//     evaluates and removes them per P12). Should one slip through,
//     it falls into `tk_unused_underscore`.
//
// The lexer is single-threaded; it raises diagnostics through the
// constructor-injected `DiagnosticEngine&` (FR-024). Direct stderr
// writes are forbidden.

#include "nsl/Lex/Lexer.h"

#include "NumberLiteral.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"
#include "nsl/Lex/KeywordSet.h"
#include "nsl/Lex/Token.h"

#include "llvm/ADT/StringRef.h"

#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>

namespace nsl {

namespace {

bool isIdentStart(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool isIdentBody(char c) {
  return isIdentStart(c) || (c >= '0' && c <= '9') || c == '_';
}

bool isDecDigit(char c) {
  return c >= '0' && c <= '9';
}

bool isWhitespace(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

/// Classify a `_`-prefix identifier per parser-note N11. The closed
/// sets below come from `docs/spec/nsl_lang.ebnf` lines 1083–1105.
/// Anything not in (a) or (b) is the third class
/// (`tk_unused_underscore`); see N11 final paragraph.
TokenKind classifyUnderscoreName(llvm::StringRef text) {
  // (a) System tasks (statement position, `_NAME(...)` form).
  static constexpr const char *kTasks[] = {
      "_display",  "_monitor",  "_write", "_finish", "_stop",
      "_readmemh", "_readmemb", "_delay", "_init",
  };
  for (const char *t : kTasks) {
    if (text == llvm::StringRef(t)) {
      return TokenKind::tk_system_task;
    }
  }
  // (b) System variables (expression position, no-parens).
  static constexpr const char *kVars[] = {"_random", "_time"};
  for (const char *v : kVars) {
    if (text == llvm::StringRef(v)) {
      return TokenKind::tk_system_function;
    }
  }
  return TokenKind::tk_unused_underscore;
}

} // namespace

// -----------------------------------------------------------------------------
// Impl
// -----------------------------------------------------------------------------

class Lexer::Impl {
public:
  SourceManager &sm;
  DiagnosticEngine &diag;
  FileID fid;
  llvm::StringRef buf;
  uint32_t cur = 0;
  std::deque<Token> peek_cache;
  bool at_line_start = true; // start of file IS start of line

  Impl(SourceManager &s, FileID f, DiagnosticEngine &d)
      : sm(s), diag(d), fid(f), buf(s.getBuffer(f)) {}

  /// Wrap `(begin, end)` byte offsets into a `SourceRange` rooted at
  /// `fid`. End is exclusive; `length() == end - begin`.
  [[nodiscard]] SourceRange makeRange(uint32_t begin, uint32_t end) const {
    return {SourceLocation::make(fid, begin), SourceLocation::make(fid, end)};
  }

  /// Skip ASCII whitespace and `//` line / `/* … */` block comments.
  /// Maintains `at_line_start` across newlines.
  void skipWhitespaceAndComments() {
    while (cur < buf.size()) {
      char const c = buf[cur];
      if (c == '\n') {
        ++cur;
        at_line_start = true;
        continue;
      }
      if (isWhitespace(c)) {
        ++cur;
        continue;
      }
      if (c == '/' && cur + 1 < buf.size()) {
        char const n = buf[cur + 1];
        if (n == '/') {
          // Line comment: consume up to (not including) the '\n'.
          cur += 2;
          while (cur < buf.size() && buf[cur] != '\n') {
            ++cur;
          }
          continue;
        }
        if (n == '*') {
          // Block comment: consume up to and including `*/`.
          // Non-nestable per lang.ebnf §14 line 781.
          cur += 2;
          while (cur + 1 < buf.size() &&
                 (buf[cur] != '*' || buf[cur + 1] != '/')) {
            if (buf[cur] == '\n') {
              at_line_start = true;
            }
            ++cur;
          }
          if (cur + 1 < buf.size()) {
            cur += 2; // consume the `*/`
          } else {
            // Unterminated block comment: consume to EOF. M1 does not
            // diagnose this as a hard error (no contract entry).
            cur = static_cast<uint32_t>(buf.size());
          }
          continue;
        }
      }
      break;
    }
  }

  /// Scan an identifier or keyword starting at `cur` (caller has
  /// confirmed `isIdentStart(buf[cur])` or `buf[cur] == '_'`).
  Token scanIdentifierOrKeyword() {
    uint32_t const begin = cur;
    bool const starts_with_underscore = (buf[cur] == '_');
    ++cur;
    while (cur < buf.size() && isIdentBody(buf[cur])) {
      ++cur;
    }
    llvm::StringRef const text = buf.substr(begin, cur - begin);
    TokenKind kind;
    if (starts_with_underscore) {
      kind = classifyUnderscoreName(text);
    } else {
      kind = classifyKeyword(text);
    }
    return {kind, makeRange(begin, cur), text};
  }

  /// Scan a string literal. The opening `"` is at `cur`. Honors the
  /// escape-sequence subset from lang.ebnf §13 lines 748–752 (`\n`,
  /// `\t`, `\r`, `\\`, `\"`, `\0`). Newlines inside the literal are
  /// not permitted (they trigger the unterminated diagnostic).
  Token scanString() {
    uint32_t const begin = cur;
    ++cur; // consume opening `"`
    while (cur < buf.size()) {
      char const c = buf[cur];
      if (c == '"') {
        ++cur;
        llvm::StringRef const text = buf.substr(begin, cur - begin);
        return {TokenKind::tk_string_lit, makeRange(begin, cur), text};
      }
      if (c == '\n') {
        // Unterminated: newline closes the string-scan attempt.
        // FR-037 / diagnostic-output.contract.md mandates this exact
        // message text.
        diag.report(Severity::Error, SourceLocation::make(fid, begin),
                    "unterminated string literal");
        llvm::StringRef const text = buf.substr(begin, cur - begin);
        return {TokenKind::tk_unknown, makeRange(begin, cur), text};
      }
      if (c == '\\' && cur + 1 < buf.size()) {
        // Skip the escaped character; classification of which escapes
        // are valid is deferred to a later milestone (M3 string-cook
        // pass). At M1 we just advance two bytes so a `\"` doesn't
        // close the literal.
        cur += 2;
        continue;
      }
      ++cur;
    }
    // Hit EOF without finding closing quote.
    diag.report(Severity::Error, SourceLocation::make(fid, begin),
                "unterminated string literal");
    llvm::StringRef const text = buf.substr(begin, cur - begin);
    return {TokenKind::tk_unknown, makeRange(begin, cur), text};
  }

  /// Scan a `#line ...` directive that survives the preprocessor →
  /// lexer seam (P13). Caller has already verified the N5 conditions:
  /// `at_line_start && buf[cur] == '#'` and the next non-whitespace
  /// character is a decimal digit. The token spans from the `#` to
  /// (but not including) the next newline; the inner content is
  /// preserved for the M2 parser to re-parse.
  Token scanLineDirective() {
    uint32_t const begin = cur;
    while (cur < buf.size() && buf[cur] != '\n') {
      ++cur;
    }
    llvm::StringRef const text = buf.substr(begin, cur - begin);
    return {TokenKind::tk_line_directive, makeRange(begin, cur), text};
  }

  /// Scan a numeric literal starting at `cur`. Delegates to the
  /// pure-function `scanNumber` recognizer.
  Token scanNumberToken() {
    uint32_t const begin = cur;
    detail::NumberScanResult const r = detail::scanNumber(buf, cur);
    if (r.end == begin) {
      // Defensive: should not happen given caller pre-check, but if
      // it does, advance one byte and emit `tk_unknown` rather than
      // looping forever.
      ++cur;
      return {TokenKind::tk_unknown, makeRange(begin, cur),
              buf.substr(begin, 1)};
    }
    cur = r.end;
    return {r.kind, makeRange(begin, cur), buf.substr(begin, cur - begin),
            r.flags};
  }

  /// One- or two-character punctuation lookup. Caller has already
  /// skipped whitespace/comments; `buf[cur]` is the first byte of
  /// what should be a punctuation token.
  Token scanPunctuation() {
    uint32_t begin = cur;
    char const c = buf[cur];
    char const n = (cur + 1 < buf.size()) ? buf[cur + 1] : '\0';

    // Two-char operators first to win precedence.
    auto emit2 = [&](TokenKind k) -> Token {
      cur += 2;
      return {k, makeRange(begin, cur), buf.substr(begin, 2)};
    };
    auto emit1 = [&](TokenKind k) -> Token {
      ++cur;
      return {k, makeRange(begin, cur), buf.substr(begin, 1)};
    };

    switch (c) {
    case '(':
      return emit1(TokenKind::tk_lparen);
    case ')':
      return emit1(TokenKind::tk_rparen);
    case '{':
      return emit1(TokenKind::tk_lbrace);
    case '}':
      return emit1(TokenKind::tk_rbrace);
    case '[':
      return emit1(TokenKind::tk_lbracket);
    case ']':
      return emit1(TokenKind::tk_rbracket);
    case ',':
      return emit1(TokenKind::tk_comma);
    case ';':
      return emit1(TokenKind::tk_semicolon);
    case ':':
      if (n == '=') {
        return emit2(TokenKind::tk_assign_seq);
      }
      return emit1(TokenKind::tk_colon);
    case '.':
      if (n == '{') {
        return emit2(TokenKind::tk_dot_lbrace);
      }
      return emit1(TokenKind::tk_dot);
    case '=':
      if (n == '=') {
        return emit2(TokenKind::tk_equal);
      }
      return emit1(TokenKind::tk_assign);
    case '!':
      if (n == '=') {
        return emit2(TokenKind::tk_not_equal);
      }
      return emit1(TokenKind::tk_logical_not);
    case '<':
      if (n == '=') {
        return emit2(TokenKind::tk_less_equal);
      }
      if (n == '<') {
        return emit2(TokenKind::tk_shift_left);
      }
      return emit1(TokenKind::tk_less);
    case '>':
      if (n == '=') {
        return emit2(TokenKind::tk_greater_equal);
      }
      if (n == '>') {
        return emit2(TokenKind::tk_shift_right);
      }
      return emit1(TokenKind::tk_greater);
    case '&':
      if (n == '&') {
        return emit2(TokenKind::tk_logical_and);
      }
      return emit1(TokenKind::tk_amp);
    case '|':
      if (n == '|') {
        return emit2(TokenKind::tk_logical_or);
      }
      return emit1(TokenKind::tk_pipe);
    case '^':
      return emit1(TokenKind::tk_caret);
    case '~':
      return emit1(TokenKind::tk_tilde);
    case '+':
      return emit1(TokenKind::tk_plus);
    case '-':
      return emit1(TokenKind::tk_minus);
    case '*':
      return emit1(TokenKind::tk_star);
    case '/':
      return emit1(TokenKind::tk_slash);
    case '%':
      return emit1(TokenKind::tk_percent);
    case '?':
      return emit1(TokenKind::tk_question);
    case '@':
      return emit1(TokenKind::tk_at);
    case '#':
      // Expression-position `#` (sign-extend operator). The line-
      // marker form was already filtered upstream by N5
      // disambiguation in nextImpl().
      return emit1(TokenKind::tk_hash_sign_extend);
    case '\'':
      return emit1(TokenKind::tk_apostrophe_zero_extend);
    default:
      // Unknown byte: consume one and report `tk_unknown` so callers
      // can choose to escalate. M1 does not diagnose individual
      // unknown bytes (the parser at M2 will).
      ++cur;
      return {TokenKind::tk_unknown, makeRange(begin, cur),
              buf.substr(begin, 1)};
    }
  }

  /// Peek past whitespace from a cursor `p` without consuming. Used
  /// by N5 to look one non-whitespace char ahead of `#` for a digit.
  [[nodiscard]] uint32_t peekPastSpaces(uint32_t p) const {
    while (p < buf.size() && (buf[p] == ' ' || buf[p] == '\t')) {
      ++p;
    }
    return p;
  }

  /// Pull one token, ignoring the peek cache. Internal helper used by
  /// both `next()` and `peek()`.
  Token nextImpl() {
    skipWhitespaceAndComments();
    if (cur >= buf.size()) {
      auto end = static_cast<uint32_t>(buf.size());
      return {TokenKind::tk_eof, makeRange(end, end), llvm::StringRef()};
    }

    // N5 disambiguation: `#line` at start of line is the line-marker
    // form (P13's canonical post-preprocess form is `#line N [ "file" ]`).
    // A bare mid-line `#` is the sign-extend operator. We accept the
    // literal `#line` keyword followed by whitespace + a decimal digit;
    // anything else with `#` falls through to the punctuation scan
    // (becoming `tk_hash_sign_extend`).
    if (at_line_start && buf[cur] == '#') {
      // Match `#line` literally: cur..cur+4 == "#line", cur+5 is space.
      if (cur + 5 < buf.size() && buf.substr(cur + 1, 4) == "line" &&
          (buf[cur + 5] == ' ' || buf[cur + 5] == '\t')) {
        uint32_t const look = peekPastSpaces(cur + 5);
        if (look < buf.size() && isDecDigit(buf[look])) {
          Token t = scanLineDirective();
          // The directive consumed up to (not including) '\n'; the
          // newline itself is consumed on the next pass through
          // `skipWhitespaceAndComments` and re-arms `at_line_start`.
          at_line_start = false;
          return t;
        }
      }
      // Not a `#line` directive: fall through to punctuation scan.
    }

    // Past the at-line-start window: any non-whitespace token clears
    // the flag for the rest of the line.
    at_line_start = false;

    char const c = buf[cur];
    if (isIdentStart(c) || c == '_') {
      return scanIdentifierOrKeyword();
    }
    if (isDecDigit(c)) {
      return scanNumberToken();
    }
    if (c == '"') {
      return scanString();
    }
    return scanPunctuation();
  }
};

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

Lexer::Lexer(SourceManager &sm, FileID fid, DiagnosticEngine &diag)
    : impl_(std::make_unique<Impl>(sm, fid, diag)) {}

Lexer::~Lexer() = default;
Lexer::Lexer(Lexer &&) noexcept = default;
Lexer &Lexer::operator=(Lexer &&) noexcept = default;

Token Lexer::next() {
  if (!impl_->peek_cache.empty()) {
    Token t = impl_->peek_cache.front();
    impl_->peek_cache.pop_front();
    return t;
  }
  return impl_->nextImpl();
}

Token Lexer::peek(int n) {
  if (n < 0) {
    n = 0;
  }
  while (static_cast<int>(impl_->peek_cache.size()) <= n) {
    impl_->peek_cache.push_back(impl_->nextImpl());
  }
  return impl_->peek_cache[static_cast<size_t>(n)];
}

bool Lexer::atEOF() const noexcept {
  return impl_->peek_cache.empty() && impl_->cur >= impl_->buf.size();
}

} // namespace nsl
