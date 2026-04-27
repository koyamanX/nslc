// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Preprocess/MacroExpander.cpp — textual-substitution +
// cycle-detection implementation per data-model entity 1 of
// specs/003-macro-textual-concat/data-model.md.
//
// Algorithm (research §1):
// 1. Walk input characters left to right.
// 2. At each position, attempt to lex an identifier.
// 3. Look up the identifier in the MacroTable.
// 4. If defined: replace the identifier's character span with the
//    macro body's text, then resume scanning at the start of the
//    substituted text (so recursive references re-trigger the
//    lookup until either a non-macro identifier or the cycle
//    budget kicks in).
// 5. If undefined: leave the identifier in place, advance.
// 6. Skip identifier scanning inside string literals (string
//    content is not subject to substitution).
//
// Cycle detection (research §4): depth counter passed through
// recursive expandImpl calls; bounded at kMaxExpansionDepth (256).
// On excess, emit the FR-007 locked diagnostic
// `recursive macro expansion: <NAME>` at use_loc and return the
// original unsubstituted text (failsoft).

#include "nsl/Preprocess/MacroExpander.h"

#include <cctype>
#include <string>

#include "llvm/ADT/StringRef.h"

namespace nsl::preprocess {

namespace {

bool isIdentStart(char c) {
  return c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool isIdentChar(char c) {
  return isIdentStart(c) || (c >= '0' && c <= '9');
}

/// Find the end (exclusive) of the identifier starting at `text[off]`.
/// Caller has already verified `isIdentStart(text[off])`.
std::size_t scanIdentEnd(llvm::StringRef text, std::size_t off) {
  std::size_t end = off + 1;
  while (end < text.size() && isIdentChar(text[end])) {
    ++end;
  }
  return end;
}

/// Find the end (exclusive) of a string literal starting at
/// `text[off]` (which is '"'). The end is the position past the
/// closing '"'. Backslash escapes are honored. If no closing
/// quote is found the entire remainder is treated as the
/// literal (the downstream lexer will surface the error).
std::size_t scanStringEnd(llvm::StringRef text, std::size_t off) {
  std::size_t i = off + 1;
  while (i < text.size()) {
    char c = text[i];
    if (c == '\\' && i + 1 < text.size()) {
      i += 2;
      continue;
    }
    if (c == '"') {
      return i + 1;
    }
    ++i;
  }
  return text.size();
}

} // namespace

MacroExpander::MacroExpander(MacroTable &macros, DiagnosticEngine &diag)
    : macros_(macros), diag_(diag) {}

std::string MacroExpander::expand(llvm::StringRef text,
                                  SourceRange use_loc) {
  return expandImpl(text, use_loc, /*depth=*/0);
}

std::string MacroExpander::expandImpl(llvm::StringRef text,
                                      SourceRange use_loc,
                                      unsigned depth) {
  std::string out;
  out.reserve(text.size());

  std::size_t i = 0;
  while (i < text.size()) {
    char c = text[i];

    // Skip string literals verbatim.
    if (c == '"') {
      std::size_t end = scanStringEnd(text, i);
      out.append(text.data() + i, end - i);
      i = end;
      continue;
    }

    // Skip `%IDENT%` splice markers verbatim — these are P3 splice
    // references handled by IdentSplicer / PPExpression's own
    // parsePercentMacroRef(), NOT by textual substitution. If the
    // `%...%` form is malformed (no closing `%`), pass the lone `%`
    // through and resume scanning at the next character.
    if (c == '%') {
      std::size_t j = i + 1;
      if (j < text.size() && isIdentStart(text[j])) {
        std::size_t name_end = j + 1;
        while (name_end < text.size() && isIdentChar(text[name_end])) {
          ++name_end;
        }
        if (name_end < text.size() && text[name_end] == '%') {
          // Well-formed %IDENT% — emit verbatim.
          std::size_t end = name_end + 1;
          out.append(text.data() + i, end - i);
          i = end;
          continue;
        }
      }
      // Bare `%` (modulo operator or malformed splice): pass through.
      out.push_back(c);
      ++i;
      continue;
    }

    // Identifier?
    if (isIdentStart(c)) {
      std::size_t end = scanIdentEnd(text, i);
      llvm::StringRef ident = text.substr(i, end - i);
      const MacroDef *def = macros_.lookup(ident);
      if (def != nullptr) {
        // Recursive expansion: substitute body and re-scan the
        // body's characters (so the body might itself contain
        // macro references).
        if (depth >= kMaxExpansionDepth) {
          diag_.report(Severity::Error, use_loc.begin(),
                       std::string("recursive macro expansion: ") +
                           ident.str());
          // Failsoft: emit the original (unsubstituted) text and
          // advance past the identifier.
          out.append(text.data() + i, end - i);
          i = end;
          continue;
        }
        std::string substituted =
            expandImpl(def->body, use_loc, depth + 1);
        out.append(substituted);
        i = end;
        continue;
      }
      // Undefined identifier: pass through unchanged (FR-017).
      out.append(text.data() + i, end - i);
      i = end;
      continue;
    }

    // Non-identifier character: pass through.
    out.push_back(c);
    ++i;
  }

  return out;
}

} // namespace nsl::preprocess
