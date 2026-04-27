// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Preprocess/PPExpression.h — PRIVATE header for nsl-preprocess.
//
// Recursive-descent parser+evaluator for `pp.ebnf §3` (compile-time
// expression sub-grammar; lines 247–264 + helper-call lines 278–289).
// Used in two places (per pp.ebnf §3 commentary):
//   (a) `#if <expr>` evaluation (P4 truth test on result).
//   (b) `#define` body reduction (P10 step-3 operator-precedence
//       reduction, after %IDENT% splice and helper eval).
//
// Operator precedence (low to high):
//     ||
//     &&
//     == !=
//     <  <= >  >=
//     +  -
//     *  /  %
//     unary +  -  !  ~
//     primary
//
// `primary` accepts:
//   - integer literal (decimal, hex, binary; matches `pp.ebnf §5`
//     `number_literal` cross-reference to `nsl_lang.ebnf §13`);
//   - float literal (per `pp.ebnf §3.2`);
//   - bare identifier — looked up in `MacroTable`; expands per P4 / P10
//     by re-parsing the macro body as an expression;
//   - `%IDENT%` macro reference — looked up in `MacroTable`; the
//     replacement TEXT is re-parsed as an expression at this point;
//   - helper call `_NAME(arg, ...)` — evaluated via `HelperEvaluator`;
//   - parenthesized expression.
//
// String literals are also accepted by the grammar (pp.ebnf §3.1
// pp_primary_expr), but at M1 they appear only inside `#line N "FILE"`
// where the directive parser handles them directly. The expression
// evaluator treats strings as "unsupported in arithmetic context"
// and emits a diagnostic — not a P4/P5 violation, just an arithmetic
// type error.

#ifndef NSL_LIB_PREPROCESS_PPEXPRESSION_H
#define NSL_LIB_PREPROCESS_PPEXPRESSION_H

#include "nsl/Preprocess/HelperEvaluator.h"
#include "nsl/Preprocess/MacroTable.h"

#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"

#include <cstddef>
#include <string>

#include "llvm/ADT/StringRef.h"

namespace nsl::preprocess {

class PPExpression {
public:
  PPExpression(MacroTable &macros, HelperEvaluator &helpers,
               DiagnosticEngine &diag)
      : macros_(macros), helpers_(helpers), diag_(diag) {}

  /// Parse `text` as a `pp.ebnf §3` compile-time expression and
  /// evaluate to a `PPValue`. `loc` is the SourceLocation of the
  /// FIRST byte of `text` in the originating file (used to attribute
  /// diagnostics inside the expression to plausible coordinates).
  ///
  /// On a parse error, emits an error diagnostic and returns
  /// `PPValue(0)` for fail-soft continuation.
  PPValue parse(llvm::StringRef text, SourceLocation loc);

  /// Reduce a `#define` body to its final `PPValue` per P10 step 3.
  /// Equivalent to `parse(body, loc)` except that an empty body is
  /// treated as undefined (returns false; out_value untouched). This
  /// is the form used when the directive parser later substitutes a
  /// macro into a `%IDENT%` site.
  bool reduceDefineBody(llvm::StringRef body, SourceLocation loc,
                        PPValue *out_value);

private:
  MacroTable &macros_;
  HelperEvaluator &helpers_;
  DiagnosticEngine &diag_;

  // The recursive-descent state is maintained on the stack inside
  // `parse()`; no member state.
  class Parser;
  friend class Parser;
};

} // namespace nsl::preprocess

#endif // NSL_LIB_PREPROCESS_PPEXPRESSION_H
