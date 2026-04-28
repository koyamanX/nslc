// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Parse/Recovery.cpp — runtime side of the per-rule recovery
// primitives declared in Recovery.h.
//
// Implementation notes:
//
//  * `skipUntil` is a forward-only scan: it consumes tokens via
//    `Parser::consume()` until `peek().kind()` is a member of the
//    incoming set OR the lexer is at EOF. Deterministic by
//    construction (no env state, no random choices, no hash-map
//    iteration; Principle V).
//
//  * `RecoveryGuard` pushes / pops a single `TokenSet` entry on the
//    parser's per-rule stack. It does NOT itself merge — merging is
//    done at observation time by `Parser::currentRecoverySet()`.

#include "Recovery.h"

#include "ParserImpl.h"

namespace nsl::parse {

TokenKind skipUntil(Parser &p, TokenSet set) {
  for (;;) {
    const TokenKind k = p.peekKind();
    if (k == TokenKind::tk_eof) {
      return TokenKind::tk_eof;
    }
    if (set.contains(k)) {
      return k;
    }
    p.consume();
  }
}

RecoveryGuard::RecoveryGuard(Parser &p, TokenSet local) noexcept : p_(p) {
  p_.pushRecoverySet(local);
}

RecoveryGuard::~RecoveryGuard() noexcept { p_.popRecoverySet(); }

} // namespace nsl::parse
