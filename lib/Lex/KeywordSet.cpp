// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lex/KeywordSet.cpp — exact-match keyword recognizer.
//
// Built from `include/nsl/Lex/KeywordSet.def` via the X-macro
// pattern (research §6). The `llvm::StringMap` lookup is
// hash-keyed; iteration order is never exposed to user-visible
// output, preserving determinism (Principle V; research §4 — the
// same rule that bans `std::unordered_map` from output paths
// permits its use behind a sealed lookup).
//
// The Meyers-singleton initialization is thread-safe (C++17 magic
// statics) and runs once per process; subsequent calls are O(1).

#include "nsl/Lex/KeywordSet.h"

#include "nsl/Lex/Token.h"

#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"

namespace nsl {

namespace {

const llvm::StringMap<TokenKind> &keywordMap() {
  static const llvm::StringMap<TokenKind> *map = []() {
    auto *m = new llvm::StringMap<TokenKind>();
#define KEYWORD(name, spelling)                                                \
  m->insert({llvm::StringRef(spelling), TokenKind::tk_##name});
#include "nsl/Lex/KeywordSet.def"
#undef KEYWORD
    return m;
  }();
  return *map;
}

} // namespace

TokenKind classifyKeyword(llvm::StringRef ident) {
  const auto &m = keywordMap();
  auto it = m.find(ident);
  if (it == m.end()) {
    return TokenKind::tk_identifier;
  }
  return it->second;
}

} // namespace nsl
