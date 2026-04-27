// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/Preprocess/MacroExpander.h — textual-substitution
// pre-pass for bare-identifier macro references inside `#define`
// body and `#if` condition expression text. Implements data-model
// entity 1 of specs/003-macro-textual-concat/data-model.md.
//
// PUBLIC because the gtest suite at test_unit/macro_expander_test/
// includes this header directly. Private to nsl-preprocess in the
// sense that no consumer outside lib/Preprocess/ + the test_unit
// gtest expects to use it (matches the M1 precedent for MacroTable
// and HelperEvaluator, both promoted to public for testability).
//
// Per pp.ebnf P10 (amended 2026-04-27 in 003-macro-textual-concat
// PR): bare-identifier macro references and %IDENT% splices BOTH
// undergo textual substitution before expression tokenization.
// Substituted text adjoins surrounding characters without inserted
// whitespace; recursion is bounded at 256 levels.

#ifndef NSL_PREPROCESS_MACROEXPANDER_H
#define NSL_PREPROCESS_MACROEXPANDER_H

#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Preprocess/MacroTable.h"

#include <string>

#include "llvm/ADT/StringRef.h"

namespace nsl::preprocess {

/// Textual-substitution pre-pass for bare-identifier macro
/// references inside `#define` body and `#if` condition
/// expression text. Walks the input character stream, replaces
/// every defined-macro identifier with its body text, and
/// returns the fully-substituted result. Recursive references
/// re-trigger lookup; cycles bounded at kMaxExpansionDepth (256).
///
/// **Invariants** (per data-model entity 1):
///
/// - `expand("", loc)` returns `""`.
/// - `expand` is a pure function of `(text, macros, diag)`.
/// - Recursion depth is bounded by `kMaxExpansionDepth = 256`.
///   Exceeding the bound emits the FR-007 locked diagnostic
///   `recursive macro expansion: <NAME>` at `use_loc` and the
///   triggering identifier is left unsubstituted at its position
///   in the output. Other (non-cyclic) identifiers earlier or
///   later in the same `text` continue to substitute normally —
///   the fail-soft is identifier-level, not whole-text.
/// - A defined macro's body is substituted **textually** at the
///   identifier's character span. Adjacent characters are NOT
///   separated by inserted whitespace.
/// - An undefined identifier is left as-is; no diagnostic.
/// - String-literal content (between matched `"..."`) is NOT
///   scanned for identifier substitution.
class MacroExpander {
public:
  /// Recursion budget. Matches `Preprocessor::kMaxIncludeDepth`
  /// from M1 (FR-022) for consistency.
  static constexpr unsigned kMaxExpansionDepth = 256;

  MacroExpander(MacroTable &macros, DiagnosticEngine &diag);

  /// Expand bare-identifier macro references and `%IDENT%` splices
  /// in `text` (per amended pp.ebnf P10). Returns the substituted
  /// text. Cycle detection is identifier-level fail-soft: the
  /// triggering identifier is left unsubstituted in the output
  /// (so partial substitution of OTHER, non-cyclic identifiers in
  /// the same `text` is preserved) and the FR-007 locked diagnostic
  /// `recursive macro expansion: <NAME>` is emitted at `use_loc`.
  std::string expand(llvm::StringRef text, SourceRange use_loc);

private:
  std::string expandImpl(llvm::StringRef text, SourceRange use_loc,
                         unsigned depth);

  MacroTable &macros_;
  DiagnosticEngine &diag_;
};

} // namespace nsl::preprocess

#endif // NSL_PREPROCESS_MACROEXPANDER_H
