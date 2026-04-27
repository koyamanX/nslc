// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Preprocess/DirectiveParser.h — PRIVATE header for nsl-preprocess.
//
// `DirectiveParser` recognizes the line-oriented preprocessor
// directive forms from `pp.ebnf §2`:
//   #include "f"   /  #include <f>      (P8)
//   #define X body /  #undef X          (P10)
//   #ifdef X / #ifndef X / #if expr     (P9)
//   #else / #endif
//   #line N        /  #line N "FILE"    (P13)
// and extracts the operand bytes for the cooperating `Preprocessor`
// to dispatch on.
//
// This module is INTENTIONALLY thin: it does NOT itself maintain the
// macro table or include stack. Those live in `Preprocessor::Impl`
// because they cross-cut multiple directives. The parser's job is
// the syntactic split — token boundaries, operand extraction, error
// reporting at the directive level.

#ifndef NSL_LIB_PREPROCESS_DIRECTIVEPARSER_H
#define NSL_LIB_PREPROCESS_DIRECTIVEPARSER_H

#include "nsl/Basic/SourceLocation.h"

#include <cstdint>
#include <string>

#include "llvm/ADT/StringRef.h"

namespace nsl::preprocess {

/// One parsed directive. The `kind` field selects which operand
/// fields are valid; the remaining fields are zero/empty otherwise.
struct ParsedDirective {
  enum class Kind : uint8_t {
    None,       ///< Not a directive (passthrough line).
    Include,    ///< `#include "f"` or `#include <f>`.
    Define,     ///< `#define <name> <body>`.
    Undef,      ///< `#undef <name>`.
    Ifdef,      ///< `#ifdef <name>`.
    Ifndef,     ///< `#ifndef <name>`.
    If,         ///< `#if <expr>`.
    Else,       ///< `#else`.
    Endif,      ///< `#endif`.
    Line,       ///< `#line ...` (variant 1, 2, or 3).
    Unknown,    ///< `#xxx` where `xxx` is not recognized.
  };

  Kind kind = Kind::None;

  /// Byte offset within the originating buffer of the FIRST byte of
  /// the directive line (the `#`). Used to construct SourceLocations.
  uint32_t line_begin_offset = 0;

  /// Byte offset of the byte AFTER the trailing newline (or end of
  /// buffer if no trailing newline). The preprocessor advances its
  /// cursor to this position after handling the directive.
  uint32_t line_end_offset = 0;

  /// `Include`: filename inside quotes/angle brackets.
  std::string include_filename;
  /// `Include`: true for `<...>`, false for `"..."`.
  bool include_is_angle = false;

  /// `Define` / `Undef` / `Ifdef` / `Ifndef`: the name operand.
  std::string name;

  /// `Define`: the body (everything after `<name>` to end of line).
  std::string body;

  /// `If`: the expression text (everything after `#if` to end of line).
  std::string if_expr;

  /// `Line`: the raw operand bytes (everything after `#line` to end
  /// of line; the cooperating `Preprocessor` will macro-expand and
  /// re-parse for variant 3).
  std::string line_operand;
};

/// Classify a single physical line as a directive or a passthrough
/// line. `line_begin` is the byte offset of the first byte of the
/// line in the buffer; `line` is the line text WITHOUT the trailing
/// newline. `line_end_offset` is the offset just past the trailing
/// newline (or buffer end).
///
/// Returns a fully-populated `ParsedDirective`. If the line is not a
/// directive (per P1: starts with `#` after optional whitespace),
/// the result has `kind == None`.
///
/// The parser issues NO diagnostics on its own; it returns a
/// `Kind::Unknown` for unrecognized `#name` directives so the caller
/// can decide whether to error or pass through (per pp.ebnf, all
/// unrecognized `#X` are errors at M1; this is enforced in
/// `Preprocessor::Impl`).
ParsedDirective classifyLine(llvm::StringRef line, uint32_t line_begin,
                             uint32_t line_end_offset);

} // namespace nsl::preprocess

#endif // NSL_LIB_PREPROCESS_DIRECTIVEPARSER_H
