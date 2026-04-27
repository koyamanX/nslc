// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Preprocess/PPExpression.cpp — recursive-descent expression
// evaluator for `pp.ebnf §3` (T056). See `PPExpression.h` for
// grammar + precedence.

#include "PPExpression.h"

#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Preprocess/HelperEvaluator.h"
#include "nsl/Preprocess/MacroExpander.h"
#include "nsl/Preprocess/MacroTable.h"

#include "llvm/ADT/StringRef.h"

#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nsl::preprocess {

namespace {

/// Compute a SourceLocation by adding `delta` bytes to `base`. The
/// underlying buffer is the originating file; this preserves Principle
/// IV by keeping diagnostics inside an expression close to the actual
/// offending byte.
SourceLocation locAt(SourceLocation base, std::size_t delta) {
  if (!base.isValid()) {
    return {};
  }
  uint32_t const off = base.offset() + static_cast<uint32_t>(delta);
  if (off >= SourceLocation::kMaxOffset) {
    return base;
  }
  return SourceLocation::make(base.file(), off);
}

SourceRange rangeAt(SourceLocation base, std::size_t begin, std::size_t end) {
  SourceLocation const b = locAt(base, begin);
  SourceLocation const e = locAt(base, end);
  if (!b.isValid() || !e.isValid()) {
    return {};
  }
  return {b, e};
}

bool isDigit(char c) {
  return c >= '0' && c <= '9';
}
bool isHexDigit(char c) {
  return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
bool isIdentStart(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}
bool isIdentBody(char c) {
  return isIdentStart(c) || (c >= '0' && c <= '9');
}

int hexDigitValue(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return 10 + (c - 'a');
  }
  if (c >= 'A' && c <= 'F') {
    return 10 + (c - 'A');
  }
  return -1;
}

} // namespace

class PPExpression::Parser {
public:
  Parser(PPExpression &owner, llvm::StringRef text, SourceLocation base)
      : owner_(owner), text_(text), base_(base),
        initial_error_count_(owner.diag_.numErrors()) {}

  PPValue parseTop() {
    skipWS();
    PPValue v = parseLogicalOr();
    skipWS();
    if (pos_ < text_.size()) {
      report("unexpected trailing characters in compile-time expression", pos_);
    }
    return v;
  }

private:
  PPExpression &owner_;
  llvm::StringRef text_;
  SourceLocation base_;
  std::size_t pos_ = 0;
  bool errored_ = false;
  // Snapshot of the diagnostic-engine error count at Parser
  // construction. Used to detect whether errors were emitted DURING
  // this Parser's lifetime (e.g. by MacroExpander pre-pass cycle
  // detection) without misattributing unrelated earlier errors.
  std::size_t initial_error_count_;
  // Storage for MacroExpander-substituted bodies of macros referenced
  // recursively from this Parser. The sub-Parser constructed inside
  // parseIdentOrHelper / parsePercentMacroRef holds a StringRef into
  // the substituted string; the string must outlive the sub-Parser.
  std::vector<std::string> expanded_bodies_;

  void report(const std::string &msg, std::size_t at) {
    if (errored_) {
      return; // First error wins; suppress cascade.
    }
    errored_ = true;
    SourceLocation loc = locAt(base_, at);
    if (!loc.isValid()) {
      loc = base_;
    }
    owner_.diag_.report(Severity::Error, loc, msg);
  }

  void skipWS() {
    while (pos_ < text_.size()) {
      char const c = text_[pos_];
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
        ++pos_;
      } else {
        break;
      }
    }
  }

  bool match(char c) {
    skipWS();
    if (pos_ < text_.size() && text_[pos_] == c) {
      ++pos_;
      return true;
    }
    return false;
  }

  bool match2(char a, char b) {
    skipWS();
    if (pos_ + 1 < text_.size() && text_[pos_] == a && text_[pos_ + 1] == b) {
      pos_ += 2;
      return true;
    }
    return false;
  }

  // Try matching a 2-char operator first to avoid eating a prefix.
  bool peek(char c) {
    skipWS();
    return pos_ < text_.size() && text_[pos_] == c;
  }
  bool peek2(char a, char b) {
    skipWS();
    return pos_ + 1 < text_.size() && text_[pos_] == a && text_[pos_ + 1] == b;
  }

  // ---- precedence ladder ----

  PPValue parseLogicalOr() {
    PPValue v = parseLogicalAnd();
    while (true) {
      if (peek2('|', '|')) {
        match2('|', '|');
        PPValue const r = parseLogicalAnd();
        bool const result = v.isTruthy() || r.isTruthy();
        v = PPValue(static_cast<int64_t>(result ? 1 : 0));
      } else {
        break;
      }
    }
    return v;
  }

  PPValue parseLogicalAnd() {
    PPValue v = parseEquality();
    while (true) {
      if (peek2('&', '&')) {
        match2('&', '&');
        PPValue const r = parseEquality();
        bool const result = v.isTruthy() && r.isTruthy();
        v = PPValue(static_cast<int64_t>(result ? 1 : 0));
      } else {
        break;
      }
    }
    return v;
  }

  PPValue parseEquality() {
    PPValue v = parseRelational();
    while (true) {
      if (peek2('=', '=')) {
        match2('=', '=');
        PPValue const r = parseRelational();
        bool result = false;
        if (v.isInt() && r.isInt()) {
          result = v.toInt() == r.toInt();
        } else {
          result = v.toReal() == r.toReal();
        }
        v = PPValue(static_cast<int64_t>(result ? 1 : 0));
      } else if (peek2('!', '=')) {
        match2('!', '=');
        PPValue const r = parseRelational();
        bool result = false;
        if (v.isInt() && r.isInt()) {
          result = v.toInt() != r.toInt();
        } else {
          result = v.toReal() != r.toReal();
        }
        v = PPValue(static_cast<int64_t>(result ? 1 : 0));
      } else {
        break;
      }
    }
    return v;
  }

  PPValue parseRelational() {
    PPValue v = parseAdditive();
    while (true) {
      if (peek2('<', '=')) {
        match2('<', '=');
        PPValue const r = parseAdditive();
        bool result = false;
        if (v.isInt() && r.isInt()) {
          result = v.toInt() <= r.toInt();
        } else {
          result = v.toReal() <= r.toReal();
        }
        v = PPValue(static_cast<int64_t>(result ? 1 : 0));
      } else if (peek2('>', '=')) {
        match2('>', '=');
        PPValue const r = parseAdditive();
        bool result = false;
        if (v.isInt() && r.isInt()) {
          result = v.toInt() >= r.toInt();
        } else {
          result = v.toReal() >= r.toReal();
        }
        v = PPValue(static_cast<int64_t>(result ? 1 : 0));
      } else if (peek('<') && !peek2('<', '<')) {
        match('<');
        PPValue const r = parseAdditive();
        bool result = false;
        if (v.isInt() && r.isInt()) {
          result = v.toInt() < r.toInt();
        } else {
          result = v.toReal() < r.toReal();
        }
        v = PPValue(static_cast<int64_t>(result ? 1 : 0));
      } else if (peek('>') && !peek2('>', '>')) {
        match('>');
        PPValue const r = parseAdditive();
        bool result = false;
        if (v.isInt() && r.isInt()) {
          result = v.toInt() > r.toInt();
        } else {
          result = v.toReal() > r.toReal();
        }
        v = PPValue(static_cast<int64_t>(result ? 1 : 0));
      } else {
        break;
      }
    }
    return v;
  }

  PPValue parseAdditive() {
    PPValue v = parseMultiplicative();
    while (true) {
      if (peek('+')) {
        match('+');
        PPValue const r = parseMultiplicative();
        if (v.isInt() && r.isInt()) {
          v = PPValue(v.toInt() + r.toInt());
        } else {
          v = PPValue(v.toReal() + r.toReal());
        }
      } else if (peek('-')) {
        match('-');
        PPValue const r = parseMultiplicative();
        if (v.isInt() && r.isInt()) {
          v = PPValue(v.toInt() - r.toInt());
        } else {
          v = PPValue(v.toReal() - r.toReal());
        }
      } else {
        break;
      }
    }
    return v;
  }

  PPValue parseMultiplicative() {
    PPValue v = parseUnary();
    while (true) {
      if (peek('*')) {
        match('*');
        PPValue const r = parseUnary();
        if (v.isInt() && r.isInt()) {
          v = PPValue(v.toInt() * r.toInt());
        } else {
          v = PPValue(v.toReal() * r.toReal());
        }
      } else if (peek('/')) {
        match('/');
        std::size_t const op_at = pos_;
        PPValue const r = parseUnary();
        if (v.isInt() && r.isInt()) {
          if (r.toInt() == 0) {
            report("compile-time division by zero", op_at);
            v = PPValue(int64_t{0});
          } else {
            v = PPValue(v.toInt() / r.toInt());
          }
        } else {
          long double const rd = r.toReal();
          if (rd == 0.0L) {
            report("compile-time division by zero", op_at);
            v = PPValue(int64_t{0});
          } else {
            v = PPValue(v.toReal() / rd);
          }
        }
      } else if (peek('%')) {
        // Disambiguate from `%IDENT%`: real `%` is the modulo operator
        // ONLY between expression operands. The `%IDENT%` reference is
        // a primary form starting with `%`; in the precedence ladder we
        // get here only AFTER a `parseUnary` returned, so a stray `%`
        // here is unambiguously the modulo operator.
        match('%');
        std::size_t const op_at = pos_;
        PPValue const r = parseUnary();
        if (v.isInt() && r.isInt()) {
          if (r.toInt() == 0) {
            report("compile-time modulo by zero", op_at);
            v = PPValue(int64_t{0});
          } else {
            v = PPValue(v.toInt() % r.toInt());
          }
        } else {
          long double const rd = r.toReal();
          if (rd == 0.0L) {
            report("compile-time modulo by zero", op_at);
            v = PPValue(int64_t{0});
          } else {
            // glibc declares fmodl in <math.h> but does not always
            // expose `std::fmodl`. Use the unqualified C name.
            v = PPValue(::fmodl(v.toReal(), rd));
          }
        }
      } else {
        break;
      }
    }
    return v;
  }

  PPValue parseUnary() {
    skipWS();
    if (pos_ >= text_.size()) {
      report("unexpected end of compile-time expression", pos_);
      return PPValue(int64_t{0});
    }
    char const c = text_[pos_];
    if (c == '+') {
      ++pos_;
      return parseUnary();
    }
    if (c == '-') {
      ++pos_;
      PPValue const v = parseUnary();
      if (v.isInt()) {
        return PPValue(-v.toInt());
      }
      return PPValue(-v.toReal());
    }
    if (c == '!') {
      ++pos_;
      PPValue const v = parseUnary();
      return PPValue(static_cast<int64_t>(v.isTruthy() ? 0 : 1));
    }
    if (c == '~') {
      ++pos_;
      PPValue const v = parseUnary();
      // Bitwise complement is integer-only.
      return PPValue(~v.toInt());
    }
    return parsePrimary();
  }

  PPValue parsePrimary() {
    skipWS();
    if (pos_ >= text_.size()) {
      report("unexpected end of compile-time expression", pos_);
      return PPValue(int64_t{0});
    }
    char const c = text_[pos_];

    // Parenthesized expression.
    if (c == '(') {
      ++pos_;
      PPValue v = parseLogicalOr();
      skipWS();
      if (pos_ >= text_.size() || text_[pos_] != ')') {
        report("missing ')' in compile-time expression", pos_);
        return v;
      }
      ++pos_;
      return v;
    }

    // %IDENT% macro reference.
    if (c == '%') {
      return parsePercentMacroRef();
    }

    // Number (decimal/hex/binary; floats handled inside parseNumber).
    if (isDigit(c) ||
        (c == '.' && pos_ + 1 < text_.size() && isDigit(text_[pos_ + 1]))) {
      return parseNumber();
    }

    // Identifier / helper call / bare-macro reference.
    if (isIdentStart(c)) {
      return parseIdentOrHelper();
    }

    report(std::string("unexpected character '") + c +
               "' in compile-time expression",
           pos_);
    ++pos_;
    return PPValue(int64_t{0});
  }

  PPValue parsePercentMacroRef() {
    std::size_t const begin = pos_;
    ++pos_; // consume opening '%'
    std::size_t const name_begin = pos_;
    while (pos_ < text_.size() && isIdentBody(text_[pos_])) {
      ++pos_;
    }
    if (pos_ == name_begin) {
      report("missing identifier in '%IDENT%' macro reference", begin);
      return PPValue(int64_t{0});
    }
    llvm::StringRef const name = text_.substr(name_begin, pos_ - name_begin);
    if (pos_ >= text_.size() || text_[pos_] != '%') {
      report("missing closing '%' in '%IDENT%' macro reference", begin);
      return PPValue(int64_t{0});
    }
    ++pos_; // consume closing '%'

    const MacroDef *def = owner_.macros_.lookup(name);
    if (def == nullptr) {
      // FR-037 locked diagnostic for P3 — emitted here in expression
      // context too so `#if %X% > 0` with undefined %X% raises the
      // same canonical text as a passthrough-line use.
      std::string msg = "undefined macro reference: '%";
      msg += name.str();
      msg += "%'";
      report(msg, begin);
      return PPValue(int64_t{0});
    }

    // Substitute textually and re-parse the result as an expression
    // (P10 step 1 / 2 ordering — we already are in the expression
    // sub-grammar, so the "splice" here is "parse the body in place").
    // Per pp.ebnf P10 (003-macro-textual-concat): the body's bare
    // identifiers are first run through MacroExpander so cycles trip
    // the depth bound (kMaxExpansionDepth = 256) instead of segfaulting
    // through unbounded Parser → parseTop → parseIdentOrHelper recursion.
    // The use_loc points at the actual `%IDENT%` reference (begin..pos_)
    // so any FR-007 cycle diagnostic is attributed to the use site,
    // not the start of the enclosing expression.
    MacroExpander expander(owner_.macros_, owner_.diag_);
    expanded_bodies_.push_back(
        expander.expand(def->body, rangeAt(base_, begin, pos_)));
    Parser sub(owner_, expanded_bodies_.back(), base_);
    return sub.parseTop();
  }

  PPValue parseNumber() {
    std::size_t const begin = pos_;

    // Detect base prefix.
    auto consumeWhile = [&](bool (*pred)(char)) {
      while (pos_ < text_.size() && (pred(text_[pos_]) || text_[pos_] == '_')) {
        ++pos_;
      }
    };

    bool is_float = false;
    bool is_hex = false;
    bool is_binary = false;

    if (text_[pos_] == '0' && pos_ + 1 < text_.size() &&
        (text_[pos_ + 1] == 'x' || text_[pos_ + 1] == 'X')) {
      is_hex = true;
      pos_ += 2;
      consumeWhile(isHexDigit);
    } else if (text_[pos_] == '0' && pos_ + 1 < text_.size() &&
               (text_[pos_ + 1] == 'b' || text_[pos_ + 1] == 'B')) {
      is_binary = true;
      pos_ += 2;
      consumeWhile([](char c) { return c == '0' || c == '1'; });
    } else {
      // Decimal integer or float.
      consumeWhile(isDigit);
      // Float continuation.
      if (pos_ < text_.size() && text_[pos_] == '.') {
        is_float = true;
        ++pos_;
        consumeWhile(isDigit);
      }
      if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
        is_float = true;
        ++pos_;
        if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) {
          ++pos_;
        }
        consumeWhile(isDigit);
      }
    }

    llvm::StringRef const text = text_.substr(begin, pos_ - begin);
    // Strip embedded `_` separators per pp.ebnf §5 decimal_digits.
    std::string filtered;
    filtered.reserve(text.size());
    for (char const c : text) {
      if (c != '_') {
        filtered.push_back(c);
      }
    }

    if (is_float) {
      try {
        long double const v = std::stold(filtered);
        return PPValue(v);
      } catch (...) {
        report("malformed compile-time float literal: '" + filtered + "'",
               begin);
        return PPValue(int64_t{0});
      }
    }

    int64_t v = 0;
    if (is_hex) {
      // Skip the `0x` prefix in `filtered`.
      for (std::size_t i = 2; i < filtered.size(); ++i) {
        int const d = hexDigitValue(filtered[i]);
        if (d < 0) {
          report("malformed hex literal: '" + filtered + "'", begin);
          return PPValue(int64_t{0});
        }
        v = (v << 4) | d;
      }
    } else if (is_binary) {
      for (std::size_t i = 2; i < filtered.size(); ++i) {
        char const c = filtered[i];
        if (c != '0' && c != '1') {
          report("malformed binary literal: '" + filtered + "'", begin);
          return PPValue(int64_t{0});
        }
        v = (v << 1) | (c - '0');
      }
    } else {
      for (char const c : filtered) {
        if (!isDigit(c)) {
          report("malformed decimal literal: '" + filtered + "'", begin);
          return PPValue(int64_t{0});
        }
        v = v * 10 + (c - '0');
      }
    }
    return PPValue(v);
  }

  PPValue parseIdentOrHelper() {
    std::size_t const begin = pos_;
    while (pos_ < text_.size() && isIdentBody(text_[pos_])) {
      ++pos_;
    }
    llvm::StringRef const name = text_.substr(begin, pos_ - begin);

    // Helper call? `_NAME (` form.
    skipWS();
    if (!name.empty() && name.front() == '_' && pos_ < text_.size() &&
        text_[pos_] == '(') {
      // Verify this is a recognized helper. If not, emit the P6
      // diagnostic — but `parseIdentOrHelper` is invoked WITH the
      // permissive context flag from #define / #if. The P6 outside-
      // context check is done at the directive-parser level (the
      // `Preprocessor` only invokes `PPExpression::parse` on
      // expressions that originate from `#if` or from `#define` body
      // reduction; outside those contexts, the directive parser never
      // gets here).
      int arity = 0;
      bool returns_real = false;
      if (!lookupHelper(name, &arity, &returns_real)) {
        std::string msg = "compile-time helper '";
        msg += name.str();
        msg += "' is not in the recognized closed set";
        report(msg, begin);
        // Skip the call to keep the parser sane.
        skipBalancedParens();
        return PPValue(int64_t{0});
      }
      ++pos_; // consume '('
      std::vector<PPValue> args;
      skipWS();
      if (!peek(')')) {
        args.push_back(parseLogicalOr());
        skipWS();
        while (peek(',')) {
          match(',');
          args.push_back(parseLogicalOr());
          skipWS();
        }
      }
      if (!match(')')) {
        report("missing ')' in helper call '" + name.str() + "'", pos_);
        return PPValue(int64_t{0});
      }
      if (static_cast<int>(args.size()) != arity) {
        // Arity mismatch (research §10): emit error and abort
        // evaluation with safe default.
        std::string msg = "helper '";
        msg += name.str();
        msg += "' expects ";
        msg += std::to_string(arity);
        msg += " arguments, got ";
        msg += std::to_string(args.size());
        report(msg, begin);
        return PPValue(int64_t{0});
      }
      SourceRange const call_loc = rangeAt(base_, begin, pos_);
      return owner_.helpers_.invoke(name, args, call_loc);
    }

    // Bare identifier — look up as a macro.
    const MacroDef *def = owner_.macros_.lookup(name);
    if (def == nullptr) {
      // Per pp.ebnf §3.x bare identifiers in expression context are
      // macro references (lines 261–262). An unknown identifier is
      // an error.
      std::string msg = "undefined macro '";
      msg += name.str();
      msg += "' in compile-time expression";
      report(msg, begin);
      return PPValue(int64_t{0});
    }
    if (def->body.empty()) {
      report("macro '" + name.str() +
                 "' has empty body and cannot be evaluated as expression",
             begin);
      return PPValue(int64_t{0});
    }
    // Per pp.ebnf P10 (003-macro-textual-concat): bare-identifier macro
    // references in the input were already textually substituted by
    // MacroExpander BEFORE this Parser saw the text (see
    // PPExpression::parse and PPExpression::reduceDefineBody). Reaching
    // this point with a bare identifier therefore means MacroExpander
    // declined to substitute it — i.e. it was a cycle (depth bound
    // reached, diagnostic already emitted) or otherwise unresolved.
    // The legacy "construct a sub-Parser on the body" path is no
    // longer correct: it would re-trigger the cycle through Parser
    // recursion, which is unbounded and segfaults. Emit an unresolved
    // error and return the safe default.
    if (owner_.diag_.numErrors() > initial_error_count_) {
      // A diagnostic was emitted during this Parser's lifetime —
      // typically the FR-007 cycle diagnostic from MacroExpander's
      // pre-pass on the input text. Suppress the cascading
      // "unresolved macro" report so we don't pile on a second
      // (less informative) error for the same root cause. Earlier
      // unrelated errors (from prior directives or files) do NOT
      // trigger this branch because we snapshot the count at
      // Parser construction.
      return PPValue(int64_t{0});
    }
    report("unresolved macro '" + name.str() + "' in compile-time expression",
           begin);
    return PPValue(int64_t{0});
  }

  void skipBalancedParens() {
    if (pos_ >= text_.size() || text_[pos_] != '(') {
      return;
    }
    int depth = 0;
    while (pos_ < text_.size()) {
      char const c = text_[pos_++];
      if (c == '(') {
        ++depth;
      } else if (c == ')') {
        --depth;
        if (depth == 0) {
          return;
        }
      }
    }
  }
};

PPValue PPExpression::parse(llvm::StringRef text, SourceLocation loc) {
  // Per pp.ebnf P10 (amended in 003-macro-textual-concat): textual
  // substitution of bare-identifier macro references happens BEFORE
  // expression tokenization. Hand `text` to MacroExpander first; the
  // parser then sees the fully-substituted character stream so
  // adjacent-substitution cases like `DEPTH.0` → `8.0` work.
  MacroExpander expander(macros_, diag_);
  std::string const substituted = expander.expand(text, SourceRange(loc, loc));
  Parser p(*this, substituted, loc);
  return p.parseTop();
}

bool PPExpression::reduceDefineBody(llvm::StringRef body, SourceLocation loc,
                                    PPValue *out_value) {
  // Trim leading/trailing whitespace.
  std::size_t b = 0;
  std::size_t e = body.size();
  while (b < e && (body[b] == ' ' || body[b] == '\t' || body[b] == '\r' ||
                   body[b] == '\n')) {
    ++b;
  }
  while (e > b && (body[e - 1] == ' ' || body[e - 1] == '\t' ||
                   body[e - 1] == '\r' || body[e - 1] == '\n')) {
    --e;
  }
  if (b == e) {
    return false;
  }
  llvm::StringRef const trimmed = body.substr(b, e - b);
  // Per pp.ebnf P10 (amended in 003-macro-textual-concat): textual
  // substitution of bare-identifier macro references in the body
  // happens BEFORE tokenization. This is what makes the canonical
  // P5 example work: `_int(_pow(2.0, DEPTH.0))` becomes
  // `_int(_pow(2.0, 8.0))` when DEPTH is `#define`d as `8`.
  MacroExpander expander(macros_, diag_);
  std::string const substituted =
      expander.expand(trimmed, SourceRange(loc, loc));
  Parser p(*this, substituted, loc);
  if (out_value != nullptr) {
    *out_value = p.parseTop();
  } else {
    (void)p.parseTop();
  }
  return true;
}

} // namespace nsl::preprocess
