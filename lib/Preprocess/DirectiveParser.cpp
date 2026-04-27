// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Preprocess/DirectiveParser.cpp — line-oriented directive
// classifier (T058). Recognizes the directive forms from pp.ebnf §2;
// extraction is byte-level on a single line per P1.

#include "DirectiveParser.h"

#include "nsl/Basic/SourceLocation.h"

#include <cstddef>
#include <cstdint>
#include <string>

#include "llvm/ADT/StringRef.h"

namespace nsl::preprocess {

namespace {

bool isWS(char c) { return c == ' ' || c == '\t'; }
bool isIdentStart(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}
bool isIdentBody(char c) {
  return isIdentStart(c) || (c >= '0' && c <= '9');
}

void skipWS(llvm::StringRef s, std::size_t &i) {
  while (i < s.size() && isWS(s[i])) {
    ++i;
  }
}

llvm::StringRef readIdent(llvm::StringRef s, std::size_t &i) {
  std::size_t b = i;
  if (i < s.size() && isIdentStart(s[i])) {
    ++i;
    while (i < s.size() && isIdentBody(s[i])) {
      ++i;
    }
  }
  return s.substr(b, i - b);
}

/// Trim ASCII whitespace from both ends of `s`.
llvm::StringRef trim(llvm::StringRef s) {
  std::size_t b = 0;
  std::size_t e = s.size();
  while (b < e && (s[b] == ' ' || s[b] == '\t')) {
    ++b;
  }
  while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t')) {
    --e;
  }
  return s.substr(b, e - b);
}

} // namespace

ParsedDirective classifyLine(llvm::StringRef line, uint32_t line_begin,
                             uint32_t line_end_offset) {
  ParsedDirective d;
  d.line_begin_offset = line_begin;
  d.line_end_offset = line_end_offset;

  // P1: the leading `#` must be the FIRST non-whitespace character on
  // the line. An indented `#` is NOT a directive (it's part of a
  // passthrough line).
  std::size_t i = 0;
  std::size_t leading_ws = 0;
  while (i < line.size() && isWS(line[i])) {
    ++i;
    ++leading_ws;
  }
  if (leading_ws > 0 || i >= line.size() || line[i] != '#') {
    d.kind = ParsedDirective::Kind::None;
    return d;
  }
  ++i; // consume '#'
  skipWS(line, i);

  // Read the directive keyword.
  llvm::StringRef kw = readIdent(line, i);
  if (kw.empty()) {
    // `#` followed by no name — unknown directive.
    d.kind = ParsedDirective::Kind::Unknown;
    return d;
  }

  // Tail: rest of the line after the keyword.
  llvm::StringRef tail = trim(line.substr(i));

  if (kw == "include") {
    d.kind = ParsedDirective::Kind::Include;
    if (!tail.empty() && tail.front() == '"') {
      // Quote form: scan until closing `"`.
      d.include_is_angle = false;
      std::size_t j = 1;
      while (j < tail.size() && tail[j] != '"') {
        ++j;
      }
      if (j < tail.size()) {
        d.include_filename = tail.substr(1, j - 1).str();
      }
    } else if (!tail.empty() && tail.front() == '<') {
      // Angle form: scan until closing `>`.
      d.include_is_angle = true;
      std::size_t j = 1;
      while (j < tail.size() && tail[j] != '>') {
        ++j;
      }
      if (j < tail.size()) {
        d.include_filename = tail.substr(1, j - 1).str();
      }
    }
    // Either an empty `include_filename` or some malformed tail —
    // the cooperating `Preprocessor` raises the diagnostic.
    return d;
  }

  if (kw == "define") {
    d.kind = ParsedDirective::Kind::Define;
    std::size_t j = i;
    skipWS(line, j);
    llvm::StringRef name = readIdent(line, j);
    d.name = name.str();
    // Body: everything after `<name>` (with leading whitespace
    // stripped) up to end of line.
    skipWS(line, j);
    d.body = line.substr(j).str();
    // Trim trailing whitespace off the body (including any '\r' for
    // CRLF safety).
    while (!d.body.empty() &&
           (d.body.back() == ' ' || d.body.back() == '\t' ||
            d.body.back() == '\r')) {
      d.body.pop_back();
    }
    return d;
  }

  if (kw == "undef") {
    d.kind = ParsedDirective::Kind::Undef;
    d.name = trim(tail).str();
    return d;
  }

  if (kw == "ifdef") {
    d.kind = ParsedDirective::Kind::Ifdef;
    d.name = trim(tail).str();
    return d;
  }

  if (kw == "ifndef") {
    d.kind = ParsedDirective::Kind::Ifndef;
    d.name = trim(tail).str();
    return d;
  }

  if (kw == "if") {
    d.kind = ParsedDirective::Kind::If;
    d.if_expr = trim(tail).str();
    return d;
  }

  if (kw == "else") {
    d.kind = ParsedDirective::Kind::Else;
    return d;
  }

  if (kw == "endif") {
    d.kind = ParsedDirective::Kind::Endif;
    return d;
  }

  if (kw == "line") {
    d.kind = ParsedDirective::Kind::Line;
    d.line_operand = trim(tail).str();
    return d;
  }

  d.kind = ParsedDirective::Kind::Unknown;
  d.name = kw.str();
  return d;
}

} // namespace nsl::preprocess
