// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/Lex/KeywordSet.h
//
// Reserved-keyword classifier for the lexer. The `KeywordSet.def`
// X-macro file (research §6) is the single source of truth; both
// the recognizer in `lib/Lex/KeywordSet.cpp` and the `TokenKind`
// enum in `Token.h` consume it. Adding a new keyword is one line in
// the .def file.
//
// Public-header rationale: the recognizer is also exercised
// directly by `test_unit/source_manager_test/keyword_set_test.cpp`
// (T024) which asserts every `KEYWORD(...)` entry in the .def file
// resolves to a unique `TokenKind` and that prefix-only matches
// (e.g., `module_x`) classify as `tk_identifier`. Exposing the
// recognizer through a public header lets the unit test link
// against `nsl-lex` without reaching into the library's internal
// translation units.

#ifndef NSL_LEX_KEYWORDSET_H
#define NSL_LEX_KEYWORDSET_H

#include "nsl/Lex/Token.h"

#include "llvm/ADT/StringRef.h"

namespace nsl {

/// Classify `ident` as either a reserved keyword (returning the
/// matching `tk_<name>` enumerator) or a generic identifier
/// (returning `TokenKind::tk_identifier` if no keyword matches).
///
/// Match is exact: `module_x` is NOT recognized as `tk_module`
/// despite the prefix. The recognizer relies on `llvm::StringMap`'s
/// hash-based exact-match lookup; iteration order is not exposed,
/// preserving determinism (Principle V; research §4).
TokenKind classifyKeyword(llvm::StringRef ident);

} // namespace nsl

#endif // NSL_LEX_KEYWORDSET_H
