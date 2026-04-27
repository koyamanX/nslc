// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/source_manager_test/keyword_set_test.cpp
//
// TDD fixtures for `lib/Lex/KeywordSet.cpp` per
// `specs/002-m1-lex-preprocess/tasks.md` T024 (Phase 3 / US1).
// Authored RED before `lib/Lex/KeywordSet.cpp` exists; the suite
// will FAIL TO LINK against the unchanged tree because `nsl-lex` is
// empty at this commit. That is the Constitution Principle VIII
// failing-state evidence.
//
// Coverage:
//   * Every KEYWORD(...) entry in include/nsl/Lex/KeywordSet.def
//     resolves via classifyKeyword(StringRef) to its corresponding
//     `tk_<enum_suffix>` TokenKind. The suite uses the X-macro
//     pattern itself to walk the entries — adding/removing a
//     KEYWORD(...) line in the .def propagates here automatically
//     (single-source-of-truth per research §6).
//   * An identifier that EQUALS a keyword resolves to that keyword's
//     kind (covered by the entry-walk above).
//   * An identifier that merely STARTS WITH a keyword (e.g.,
//     `module_x`, `if_then_else`) resolves to `tk_identifier`, NOT
//     to the keyword. This catches a prefix-vs-equality bug in the
//     recognizer.
//   * The empty StringRef resolves to `tk_identifier` — there is no
//     empty-string keyword in `lang.ebnf §15`, so the recognizer
//     must fall through to the identifier default.

#include "nsl/Lex/KeywordSet.h"
#include "nsl/Lex/Token.h"

#include "llvm/ADT/StringRef.h"

#include "gtest/gtest.h"
#include <string>

using nsl::classifyKeyword;
using nsl::TokenKind;

namespace {

// -----------------------------------------------------------------------------
// Per-keyword spelling → expected kind: walk KeywordSet.def via X-macro.
// -----------------------------------------------------------------------------
//
// The .def file declares lines of the form `KEYWORD(enum_suffix,
// "spelling")` — see include/nsl/Lex/KeywordSet.def. Re-defining the
// macro here lets us iterate every entry without hand-listing 42 of
// them; if the .def grows or shrinks, this suite tracks automatically.

TEST(KeywordSetTest, EveryEntryResolvesToItsKeywordKind){
#define KEYWORD(name, spelling)                                                \
  EXPECT_EQ(classifyKeyword(llvm::StringRef(spelling)), TokenKind::tk_##name)  \
      << "spelling=\"" << spelling << "\" expected tk_" #name;
#include "nsl/Lex/KeywordSet.def"
#undef KEYWORD
}

// -----------------------------------------------------------------------------
// Prefix-vs-equality: identifiers that START with a keyword must NOT
// classify as the keyword.
// -----------------------------------------------------------------------------

TEST(KeywordSetTest, PrefixIsNotEquality_module_x) {
  EXPECT_EQ(classifyKeyword(llvm::StringRef("module_x")),
            TokenKind::tk_identifier);
}

TEST(KeywordSetTest, PrefixIsNotEquality_if_then_else) {
  // Concatenation of three keywords with underscores — must still be
  // a plain identifier, not any of the constituent keywords.
  EXPECT_EQ(classifyKeyword(llvm::StringRef("if_then_else")),
            TokenKind::tk_identifier);
}

TEST(KeywordSetTest, PrefixIsNotEquality_proc_local) {
  EXPECT_EQ(classifyKeyword(llvm::StringRef("proc_local")),
            TokenKind::tk_identifier);
}

TEST(KeywordSetTest, PrefixIsNotEquality_state2) {
  EXPECT_EQ(classifyKeyword(llvm::StringRef("state2")),
            TokenKind::tk_identifier);
}

TEST(KeywordSetTest, SuffixIsNotEquality_xreg) {
  // Identifier ENDING with a keyword text — also a plain identifier.
  EXPECT_EQ(classifyKeyword(llvm::StringRef("xreg")), TokenKind::tk_identifier);
}

// -----------------------------------------------------------------------------
// Equality boundary: identifiers that EQUAL a keyword classify as the
// keyword. Spot-check a handful of representative entries; the
// X-macro walk above already covers all 42 entries comprehensively.
// -----------------------------------------------------------------------------

TEST(KeywordSetTest, ExactMatch_module) {
  EXPECT_EQ(classifyKeyword(llvm::StringRef("module")), TokenKind::tk_module);
}

TEST(KeywordSetTest, ExactMatch_if_uses_trailing_underscore_enum) {
  // `if` is a C++ keyword; data-model entity 7 / KeywordSet.def
  // preserves the trailing-underscore convention in the enum name.
  EXPECT_EQ(classifyKeyword(llvm::StringRef("if")), TokenKind::tk_if_);
}

TEST(KeywordSetTest, ExactMatch_function_synonym_of_func) {
  // S26: `func` ≡ `function`. The lexer classifies them as DISTINCT
  // kinds (`tk_func` vs `tk_function`); sema collapses the synonym.
  EXPECT_EQ(classifyKeyword(llvm::StringRef("func")), TokenKind::tk_func);
  EXPECT_EQ(classifyKeyword(llvm::StringRef("function")),
            TokenKind::tk_function);
}

// -----------------------------------------------------------------------------
// Default fallback: empty input.
// -----------------------------------------------------------------------------

TEST(KeywordSetTest, EmptyStringIsIdentifier) {
  EXPECT_EQ(classifyKeyword(llvm::StringRef("")), TokenKind::tk_identifier);
}

// -----------------------------------------------------------------------------
// Case sensitivity: NSL keywords are lower-case per `lang.ebnf §15`.
// `MODULE` is NOT the `module` keyword — it's a regular identifier
// (`letter { letter | digit | underscore }` per §13). This pins the
// case-sensitive recognition behavior.
// -----------------------------------------------------------------------------

TEST(KeywordSetTest, CaseSensitive_UPPERCASE_module_is_identifier) {
  EXPECT_EQ(classifyKeyword(llvm::StringRef("MODULE")),
            TokenKind::tk_identifier);
}

TEST(KeywordSetTest, CaseSensitive_MixedCase_Module_is_identifier) {
  EXPECT_EQ(classifyKeyword(llvm::StringRef("Module")),
            TokenKind::tk_identifier);
}

} // namespace
