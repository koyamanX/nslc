// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Preprocess/IdentSplicer.cpp — `%IDENT%` splicing on passthrough
// lines (T057). Per **P3** the substitution is TEXTUAL — we splice
// the macro body verbatim and let the downstream lexer re-tokenize.
//
// Quoted regions (`"..."`) and single-line + block comments are NOT
// scanned for `%IDENT%` references (the lexer's job is to keep them
// opaque; per pp.ebnf P2 passthrough comments are emitted verbatim).
// We DO honor the same skip rule here so a string containing `%X%`
// does not accidentally trigger macro lookup.

#include "IdentSplicer.h"

#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Preprocess/HelperEvaluator.h"
#include "nsl/Preprocess/MacroTable.h"

#include "llvm/ADT/StringRef.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace nsl::preprocess {

namespace {

bool isIdentStart(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}
bool isIdentBody(char c) {
  return isIdentStart(c) || (c >= '0' && c <= '9');
}

SourceLocation locAt(SourceLocation base, std::size_t delta) {
  if (!base.isValid()) {
    return {};
  }
  uint32_t off = base.offset() + static_cast<uint32_t>(delta);
  if (off >= SourceLocation::kMaxOffset) {
    return base;
  }
  return SourceLocation::make(base.file(), off);
}

} // namespace

std::string IdentSplicer::splice(llvm::StringRef line,
                                 SourceLocation line_loc) {
  std::string out;
  out.reserve(line.size());

  std::size_t i = 0;
  while (i < line.size()) {
    char c = line[i];

    // String literal: copy verbatim through closing quote, honoring
    // backslash escapes.
    if (c == '"') {
      out.push_back(c);
      ++i;
      while (i < line.size()) {
        char d = line[i];
        out.push_back(d);
        ++i;
        if (d == '\\' && i < line.size()) {
          out.push_back(line[i]);
          ++i;
          continue;
        }
        if (d == '"') {
          break;
        }
      }
      continue;
    }

    // Line comment: copy to end of line.
    if (c == '/' && i + 1 < line.size() && line[i + 1] == '/') {
      out.append(line.data() + i, line.size() - i);
      i = line.size();
      break;
    }

    // Block comment: copy through `*/`.
    if (c == '/' && i + 1 < line.size() && line[i + 1] == '*') {
      out.push_back(c);
      out.push_back('*');
      i += 2;
      while (i + 1 < line.size() && !(line[i] == '*' && line[i + 1] == '/')) {
        out.push_back(line[i]);
        ++i;
      }
      if (i + 1 < line.size()) {
        out.push_back('*');
        out.push_back('/');
        i += 2;
      } else if (i < line.size()) {
        out.push_back(line[i]);
        ++i;
      }
      continue;
    }

    // %IDENT% reference?
    if (c == '%') {
      std::size_t begin = i;
      std::size_t name_begin = i + 1;
      std::size_t j = name_begin;
      while (j < line.size() && isIdentBody(line[j])) {
        ++j;
      }
      if (j > name_begin && j < line.size() && line[j] == '%') {
        // Recognized %NAME% form.
        llvm::StringRef name = line.substr(name_begin, j - name_begin);
        const MacroDef *def = macros_.lookup(name);
        if (def) {
          // Per P10 step 1 + spec scenario 3: if the macro body
          // contains a helper call or float literal, REDUCE it via
          // PPExpression and splice the rendered VALUE. Otherwise
          // textual splice (P3).
          bool needs_reduction = false;
          {
            llvm::StringRef body = def->body;
            // Detect `_<lower>(` (a helper call pattern) or a float
            // literal (digit `.` digit, or digit `e[+-]?digit`).
            for (std::size_t k = 0; k + 1 < body.size(); ++k) {
              char a = body[k];
              char b = body[k + 1];
              // Helper call: `_<letter>` followed by `(` somewhere
              // before whitespace (cheap heuristic).
              if (a == '_' &&
                  ((b >= 'a' && b <= 'z') || (b >= 'A' && b <= 'Z'))) {
                // Look for `(` ahead.
                for (std::size_t m = k + 1; m < body.size(); ++m) {
                  char c = body[m];
                  if (c == '(') {
                    needs_reduction = true;
                    break;
                  }
                  if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '_')) {
                    break;
                  }
                }
                if (needs_reduction) {
                  break;
                }
              }
              // Float literal: digit `.` digit.
              if (a >= '0' && a <= '9' && b == '.' && k + 2 < body.size() &&
                  body[k + 2] >= '0' && body[k + 2] <= '9') {
                needs_reduction = true;
                break;
              }
              // Float literal: digit `e[+-]?digit`.
              if (a >= '0' && a <= '9' && (b == 'e' || b == 'E')) {
                std::size_t m = k + 2;
                if (m < body.size() && (body[m] == '+' || body[m] == '-')) {
                  ++m;
                }
                if (m < body.size() && body[m] >= '0' && body[m] <= '9') {
                  needs_reduction = true;
                  break;
                }
              }
            }
          }
          if (needs_reduction) {
            PPValue v;
            // The body lives in MacroDef storage (std::string) so the
            // StringRef is stable. We use the macro's defining_loc
            // begin as the diagnostic anchor; the body itself doesn't
            // map back to a buffer location at this point.
            SourceLocation body_loc = def->defining_loc.isValid()
                                          ? def->defining_loc.begin()
                                          : SourceLocation();
            if (expr_.reduceDefineBody(def->body, body_loc, &v)) {
              out.append(v.render());
            } else {
              out.append(def->body);
            }
          } else {
            out.append(def->body);
          }
        } else {
          // FR-037: emit canonical P3 diagnostic.
          std::string msg = "undefined macro reference: '%";
          msg += name.str();
          msg += "%'";
          diag_.report(Severity::Error, locAt(line_loc, begin), msg);
          // Leave the original `%NAME%` text in the output stream
          // so downstream layers can still see it (and the lexer
          // can produce a sensible token sequence even on error).
          out.append(line.data() + begin, (j + 1) - begin);
        }
        i = j + 1; // skip past closing '%'
        continue;
      }
      // Not a macro ref — emit `%` as-is.
      out.push_back(c);
      ++i;
      continue;
    }

    out.push_back(c);
    ++i;
  }
  return out;
}

} // namespace nsl::preprocess
