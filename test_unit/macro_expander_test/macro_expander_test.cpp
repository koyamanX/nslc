// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/macro_expander_test/macro_expander_test.cpp
//
// TDD fixtures for the `MacroExpander` class (data-model entity 1
// of specs/003-macro-textual-concat/data-model.md).
//
// Authored RED before lib/Preprocess/MacroExpander.{h,cpp} exist;
// this suite is expected to FAIL TO LINK against the unchanged
// post-M1 tree (no `nsl::preprocess::MacroExpander` symbol
// exists). Constitution Principle VIII RED-state evidence per
// spec FR-015.

#include "nsl/Preprocess/MacroExpander.h"

#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"
#include "nsl/Preprocess/MacroTable.h"

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "llvm/ADT/StringRef.h"

using nsl::DiagnosticEngine;
using nsl::FileID;
using nsl::Severity;
using nsl::SourceLocation;
using nsl::SourceManager;
using nsl::SourceRange;
using nsl::preprocess::MacroExpander;
using nsl::preprocess::MacroTable;

namespace {

// -----------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------

FileID makeBuf(SourceManager &sm) {
  std::vector<char> bytes{'X', '\0'};
  return sm.addBufferInMemory("synthetic.nsl", std::move(bytes));
}

SourceRange syntheticLoc(SourceManager &sm, FileID f) {
  SourceLocation b = SourceLocation::make(f, 0);
  SourceLocation e = SourceLocation::make(f, 1);
  return SourceRange(b, e);
}

bool diagHasError(const DiagnosticEngine &diag, llvm::StringRef needle) {
  for (const auto &d : diag.diagnostics()) {
    if (d.severity == Severity::Error &&
        llvm::StringRef(d.message).contains(needle)) {
      return true;
    }
  }
  return false;
}

// =================================================================
// GROUP 1 — Basic textual substitution (US1)
// =================================================================

TEST(MacroExpanderTest, BareIdentifierWithDefinedMacroSubstitutes) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  MacroTable mt;
  mt.insert("DEPTH", "8", syntheticLoc(sm, f));

  MacroExpander expander(mt, diag);
  std::string result = expander.expand("DEPTH", syntheticLoc(sm, f));
  EXPECT_EQ(result, "8");
}

TEST(MacroExpanderTest, BareIdentifierAdjacentToDotZeroFormsFloat) {
  // The canonical pp.ebnf P5 invariant: DEPTH.0 with #define
  // DEPTH 8 produces "8.0" textually after substitution — the
  // expression lexer then recognizes 8.0 as a single float
  // literal.
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  MacroTable mt;
  mt.insert("DEPTH", "8", syntheticLoc(sm, f));

  MacroExpander expander(mt, diag);
  std::string result = expander.expand("DEPTH.0", syntheticLoc(sm, f));
  EXPECT_EQ(result, "8.0");
}

TEST(MacroExpanderTest, UndefinedIdentifierLeftAsIs) {
  // Per FR-017: undefined bare identifier leaves the text
  // unchanged (no diagnostic at expansion time; the downstream
  // expression parser surfaces an "unknown identifier" error
  // when it tries to evaluate).
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  MacroTable mt;

  MacroExpander expander(mt, diag);
  std::string result = expander.expand("UNDEF", syntheticLoc(sm, f));
  EXPECT_EQ(result, "UNDEF");
  EXPECT_FALSE(diag.hasError());
}

TEST(MacroExpanderTest, EmptyInputReturnsEmptyOutput) {
  SourceManager sm;
  DiagnosticEngine diag(sm);
  MacroTable mt;
  FileID f = makeBuf(sm);

  MacroExpander expander(mt, diag);
  std::string result = expander.expand("", syntheticLoc(sm, f));
  EXPECT_EQ(result, "");
}

TEST(MacroExpanderTest, NonIdentifierCharactersPreserved) {
  // Operators, parens, and digits in the input are passed
  // through unchanged.
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  MacroTable mt;
  mt.insert("X", "42", syntheticLoc(sm, f));

  MacroExpander expander(mt, diag);
  std::string result = expander.expand("(X + 1) * 2", syntheticLoc(sm, f));
  EXPECT_EQ(result, "(42 + 1) * 2");
}

// =================================================================
// GROUP 2 — Adjacency edge cases (US2)
// =================================================================

TEST(MacroExpanderTest, AdjacentDotZeroNoWhitespace) {
  // `A.0` with `#define A 8` substitutes textually to `8.0` — the
  // contract's adjacency table line 1.
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  MacroTable mt;
  mt.insert("A", "8", syntheticLoc(sm, f));

  MacroExpander expander(mt, diag);
  std::string result = expander.expand("A.0", syntheticLoc(sm, f));
  EXPECT_EQ(result, "8.0");
}

TEST(MacroExpanderTest, WhitespaceBetweenIdentAndSuffixPreserved) {
  // `A 0` with `#define A 8` substitutes only A; the original
  // whitespace is preserved (no whitespace is inserted, none is
  // removed). The contract's adjacency table line 2.
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  MacroTable mt;
  mt.insert("A", "8", syntheticLoc(sm, f));

  MacroExpander expander(mt, diag);
  std::string result = expander.expand("A 0", syntheticLoc(sm, f));
  EXPECT_EQ(result, "8 0");
}

TEST(MacroExpanderTest, GreedyIdentifierScanDoesNotSplitAtUnderscore) {
  // `A_extra` is one identifier per the `[A-Za-z_][A-Za-z0-9_]*`
  // regex; it's NOT `A` followed by `_extra`. So `A_extra` is
  // looked up in the macro table as a whole, and since only `A`
  // is defined, `A_extra` is left as-is. The contract's adjacency
  // table line 3.
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  MacroTable mt;
  mt.insert("A", "8", syntheticLoc(sm, f));

  MacroExpander expander(mt, diag);
  std::string result = expander.expand("A_extra", syntheticLoc(sm, f));
  EXPECT_EQ(result, "A_extra");
}

// =================================================================
// GROUP 3 — Recursion + cycle detection (US3)
// =================================================================

TEST(MacroExpanderTest, ThreeLevelChainResolvesTransitively) {
  // `A → B → 8` chain: expand("A") returns "8" via two
  // textual-substitution steps.
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  MacroTable mt;
  mt.insert("A", "B", syntheticLoc(sm, f));
  mt.insert("B", "8", syntheticLoc(sm, f));

  MacroExpander expander(mt, diag);
  std::string result = expander.expand("A", syntheticLoc(sm, f));
  EXPECT_EQ(result, "8");
  EXPECT_FALSE(diag.hasError());
}

TEST(MacroExpanderTest, FourLevelChainResolvesTransitively) {
  // `A → B → C → 8` chain: expand("A") returns "8".
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  MacroTable mt;
  mt.insert("A", "B", syntheticLoc(sm, f));
  mt.insert("B", "C", syntheticLoc(sm, f));
  mt.insert("C", "8", syntheticLoc(sm, f));

  MacroExpander expander(mt, diag);
  std::string result = expander.expand("A", syntheticLoc(sm, f));
  EXPECT_EQ(result, "8");
  EXPECT_FALSE(diag.hasError());
}

TEST(MacroExpanderTest, SelfCycleEmitsLockedDiagnosticAndFailsoft) {
  // `#define A A` — self-cycle. The expander must emit the
  // FR-007 locked diagnostic `recursive macro expansion: A` and
  // return failsoft (data-model entity 1: "returns the original
  // unsubstituted text" — the downstream parser then surfaces the
  // resulting error). The unit test only pins the diagnostic;
  // the failsoft return-shape is exercised end-to-end by the lit
  // fixture test/preprocess/p10/cycle.fail.test.
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  MacroTable mt;
  mt.insert("A", "A", syntheticLoc(sm, f));

  MacroExpander expander(mt, diag);
  (void)expander.expand("A", syntheticLoc(sm, f));
  EXPECT_TRUE(diag.hasError());
  EXPECT_TRUE(diagHasError(diag, "recursive macro expansion: A"));
}

TEST(MacroExpanderTest, MutualCycleEmitsLockedDiagnostic) {
  // `#define A B`, `#define B A` — mutual cycle. Trips at
  // kMaxExpansionDepth (256) and emits the locked diagnostic for
  // whichever name was being expanded when the bound was hit.
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  MacroTable mt;
  mt.insert("A", "B", syntheticLoc(sm, f));
  mt.insert("B", "A", syntheticLoc(sm, f));

  MacroExpander expander(mt, diag);
  (void)expander.expand("A", syntheticLoc(sm, f));
  EXPECT_TRUE(diag.hasError());
  // Either A or B is named in the diagnostic; both are valid.
  bool named =
      diagHasError(diag, "recursive macro expansion: A") ||
      diagHasError(diag, "recursive macro expansion: B");
  EXPECT_TRUE(named);
}

TEST(MacroExpanderTest, PercentSpliceTextuallySubstituted) {
  // Per amended pp.ebnf P10 (003-macro-textual-concat): `%IDENT%`
  // splices undergo textual substitution alongside bare-identifier
  // references in step 1. With `#define Y 3` the entire `%Y%`
  // span (3 chars including the surrounding `%`s) is replaced by
  // the body text "3", so `_int(_pow(%Y%, 2.0))` becomes
  // `_int(_pow(3, 2.0))`. This is what makes the canonical P5
  // example work for both bare-`DEPTH.0` AND `%DEPTH%.0` forms.
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  MacroTable mt;
  mt.insert("Y", "3", syntheticLoc(sm, f));

  MacroExpander expander(mt, diag);
  std::string result =
      expander.expand("_int(_pow(%Y%, 2.0))", syntheticLoc(sm, f));
  EXPECT_EQ(result, "_int(_pow(3, 2.0))");
}

TEST(MacroExpanderTest, PercentSpliceConcatenatesAdjacentDot) {
  // `%DEPTH%.0` with `#define DEPTH 8` substitutes textually to
  // `8.0` — the same adjacency rule that applies to bare-`DEPTH.0`
  // applies to `%DEPTH%.0` per amended P10. This is the
  // CodeRabbit-flagged case the M1-vintage skip-rule got wrong.
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  MacroTable mt;
  mt.insert("DEPTH", "8", syntheticLoc(sm, f));

  MacroExpander expander(mt, diag);
  std::string result =
      expander.expand("%DEPTH%.0", syntheticLoc(sm, f));
  EXPECT_EQ(result, "8.0");
}

TEST(MacroExpanderTest, UndefinedPercentSpliceLeftAsIs) {
  // Undefined `%UNDEF%` is passed through verbatim so the
  // downstream `parsePercentMacroRef` / `IdentSplicer` surfaces
  // the canonical FR-037 diagnostic at its native site.
  SourceManager sm;
  DiagnosticEngine diag(sm);
  FileID f = makeBuf(sm);
  MacroTable mt;

  MacroExpander expander(mt, diag);
  std::string result =
      expander.expand("%UNDEF%", syntheticLoc(sm, f));
  EXPECT_EQ(result, "%UNDEF%");
  EXPECT_FALSE(diag.hasError());
}

} // namespace
