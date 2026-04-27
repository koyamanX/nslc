// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Preprocess/IdentSplicer.h — PRIVATE header for nsl-preprocess.
//
// `%IDENT%` splicing per **P3** + **P10 step 1** (textual splice into
// the surrounding identifier text). This is the only macro expansion
// performed on PASSTHROUGH lines per **P2** ("bare identifiers on
// passthrough lines are NOT macro-expanded — only `%IDENT%` and
// helper-call sites are"; pp.ebnf §4 + P2 commentary).
//
// API surface is small on purpose: `splice(line, base_loc) -> string`.
// The result is appended to the preprocessor's output buffer. An
// undefined `%IDENT%` raises the FR-037 locked diagnostic
// `undefined macro reference: '%X%'` and the substring is left
// unchanged so subsequent layers (lexer, parser) can still see the
// offender if they want to.

#ifndef NSL_LIB_PREPROCESS_IDENTSPLICER_H
#define NSL_LIB_PREPROCESS_IDENTSPLICER_H

#include "PPExpression.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Preprocess/MacroTable.h"

#include "llvm/ADT/StringRef.h"

#include <string>

namespace nsl::preprocess {

class IdentSplicer {
public:
  IdentSplicer(MacroTable &macros, PPExpression &expr, DiagnosticEngine &diag)
      : macros_(macros), expr_(expr), diag_(diag) {}

  /// Resolve every `%IDENT%` reference in `line` against the macro
  /// table; return the resulting text. `line_loc` is the
  /// SourceLocation of the FIRST byte of `line` in the originating
  /// file (used to attribute diagnostics to the correct column).
  ///
  /// Substitution policy (P3 textual splice + spec acceptance scenario
  /// 3 helper-reduction):
  ///   - If the macro's body contains a helper-call (`_NAME(`) or
  ///     float-literal pattern, the body is REDUCED via PPExpression
  ///     and the rendered VALUE is spliced in. This is required so a
  ///     `#define MEMDEPTH _int(_pow(2.0,8.0))` produces `256` at the
  ///     `%MEMDEPTH%` use site (per spec US2 scenario 3 + **P5**).
  ///   - Otherwise the body's TEXT is spliced verbatim (P3 textual
  ///     splice — preserves identifiers like `#define ID my_signal`).
  ///
  /// `%X%` where `X` is undefined emits the FR-037 P3 diagnostic
  /// `undefined macro reference: '%X%'` at the splice site; the
  /// original `%X%` is left in the output stream so downstream layers
  /// can still see the offender.
  std::string splice(llvm::StringRef line, SourceLocation line_loc);

private:
  MacroTable &macros_;
  PPExpression &expr_;
  DiagnosticEngine &diag_;
};

} // namespace nsl::preprocess

#endif // NSL_LIB_PREPROCESS_IDENTSPLICER_H
