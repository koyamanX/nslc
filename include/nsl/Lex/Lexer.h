// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/Lex/Lexer.h
//
// Public scanner API for `nsl-lex` (data-model entity 9 in
// `specs/002-m1-lex-preprocess/data-model.md`).
//
// `Lexer` is a stateful pull-model scanner over a single `Buffer`.
// Drivers (M1's `nslc -emit=tokens`) and the future M2 parser pull
// one token at a time via `next()` (or look ahead via `peek(n)`); the
// lexer emits `tk_eof` once the cursor reaches end of file and
// continues to return `tk_eof` on subsequent calls (idempotent at
// EOF).
//
// Header split: `Token.h` is intentionally separable so simple token-
// emit consumers (LSP `semanticTokens` provider at T4) need not pull
// in this stateful scanner. See Token.h's banner for the rationale.
//
// The lexer raises diagnostics through the `DiagnosticEngine&`
// injected at construction (FR-024 — direct writes to stderr from
// the lex layer are forbidden). The unterminated-string-literal path
// is the only diagnostic emitted at M1; further error sites land
// alongside their introducing features in later milestones.

#ifndef NSL_LEX_LEXER_H
#define NSL_LEX_LEXER_H

#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Basic/SourceManager.h"
#include "nsl/Lex/Token.h"

#include "llvm/ADT/StringRef.h"

#include <cstdint>
#include <deque>
#include <memory>

namespace nsl {

class Lexer {
public:
  /// Construct a scanner over `fid`'s buffer. The `SourceManager`
  /// must outlive the lexer (its `Buffer` aliases the spelling views
  /// returned by every emitted `Token`).
  Lexer(SourceManager &sm, FileID fid, DiagnosticEngine &diag);

  /// Movable; non-copyable (the `peek_cache_` deque is best owned by
  /// exactly one driver).
  Lexer(const Lexer &) = delete;
  Lexer &operator=(const Lexer &) = delete;
  Lexer(Lexer &&) noexcept;
  Lexer &operator=(Lexer &&) noexcept;
  ~Lexer();

  /// Pull one token. Emits `tk_eof` at end of file and on subsequent
  /// calls (idempotent at EOF).
  Token next();

  /// Look ahead `n` tokens without consuming. `peek(0)` peeks the
  /// next-to-be-returned token. Cached so a `peek(0)` followed by
  /// `next()` returns the same token.
  Token peek(int n = 0);

  /// True iff the cursor has consumed every byte of the buffer AND
  /// the peek-cache is empty. Useful for parser-side tight loops
  /// that prefer `while (!atEOF())` over `while (next() != eof)`.
  [[nodiscard]] bool atEOF() const noexcept;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace nsl

#endif // NSL_LEX_LEXER_H
