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

#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Preprocess/MacroTable.h"

#include "llvm/ADT/StringRef.h"

#include <cctype>
#include <cstddef>
#include <string>

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
    char const c = text[i];
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

std::string MacroExpander::expand(llvm::StringRef text, SourceRange use_loc) {
  return expandImpl(text, use_loc, /*depth=*/0);
}

std::string MacroExpander::expandImpl(llvm::StringRef text, SourceRange use_loc,
                                      unsigned depth) {
  std::string out;
  out.reserve(text.size());

  std::size_t i = 0;
  while (i < text.size()) {
    char const c = text[i];

    // Skip string literals verbatim.
    if (c == '"') {
      std::size_t const end = scanStringEnd(text, i);
      out.append(text.data() + i, end - i);
      i = end;
      continue;
    }

    // `%IDENT%` splice markers — textually substitute per amended
    // pp.ebnf P10 step 1 (003-macro-textual-concat): replace the
    // entire `%IDENT%` span (incl. the surrounding `%`s) with the
    // referenced macro's body TEXT. The recursion guard applies the
    // same way as for bare identifiers; substituted text is itself
    // re-scanned via `expandImpl(def->body, …)` so chains like
    // `#define X %Y%` followed by `#define Y 8` reduce in one pass.
    // An undefined `%UNDEF%` is left in place so the downstream
    // `parsePercentMacroRef` (or `IdentSplicer`) can surface the
    // FR-037 diagnostic at its canonical site. A malformed `%`
    // (no identifier or no closing `%`) is passed through so the
    // expression parser can use it as the modulo operator.
    if (c == '%') {
      std::size_t const j = i + 1;
      if (j < text.size() && isIdentStart(text[j])) {
        std::size_t name_end = j + 1;
        while (name_end < text.size() && isIdentChar(text[name_end])) {
          ++name_end;
        }
        if (name_end < text.size() && text[name_end] == '%') {
          llvm::StringRef const name = text.substr(j, name_end - j);
          std::size_t const end = name_end + 1;
          const MacroDef *def = macros_.lookup(name);
          if (def != nullptr) {
            if (depth >= kMaxExpansionDepth) {
              diag_.report(Severity::Error, use_loc.begin(),
                           std::string("recursive macro expansion: ") +
                               name.str());
              out.append(text.data() + i, end - i);
              i = end;
              continue;
            }
            std::string const substituted =
                expandImpl(def->body, use_loc, depth + 1);
            out.append(substituted);
            i = end;
            continue;
          }
          // Undefined `%IDENT%` — emit verbatim so the canonical
          // diagnostic is produced by the downstream `%IDENT%`
          // consumer (FR-037).
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
      std::size_t const end = scanIdentEnd(text, i);
      llvm::StringRef const ident = text.substr(i, end - i);
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
        std::string const substituted =
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
