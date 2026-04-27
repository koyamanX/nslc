// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/LiteralExpr.h — `literal` expression
// (`lang.ebnf §11`; data-model §1.6). Fields: `kind` (the
// numeric base or string variant the lexer reported), `spelling`
// (the raw textual form — lossless verbatim from source). Numeric
// flag bits (Z/X/U digit content per `Token::NumericFlag`) are
// stashed alongside in `flags` for printer roundtrip; M3 Sema
// will revisit if a richer numeric model is needed.

#ifndef NSL_AST_LITERAL_EXPR_H
#define NSL_AST_LITERAL_EXPR_H

#include "nsl/AST/ASTNode.h"
#include "nsl/AST/Expr.h"

#include <cstdint>

namespace nsl::ast {

class LiteralExpr final : public Expr {
public:
  /// Mirrors the matching subset of `nsl::TokenKind` literal
  /// enumerators. Defined here (not aliased) so the AST surface
  /// is independent of the lexer's TokenKind enum at later
  /// milestones.
  enum class Lit { Decimal, Hex, Binary, Octal, String };

  LiteralExpr(SourceRange range, Lit kind, Identifier spelling,
              uint16_t flags = 0)
      : Expr(NodeKind::NK_LiteralExpr, range), litKind_(kind),
        spelling_(spelling), flags_(flags) {}

  [[nodiscard]] Lit litKind() const noexcept { return litKind_; }
  /// The verbatim source-text of the literal — printer reproduces
  /// it without re-formatting.
  [[nodiscard]] Identifier spelling() const noexcept { return spelling_; }
  /// `Token::NumericFlag` bitmask (Z/X/U digit content). Zero for
  /// string literals.
  [[nodiscard]] uint16_t flags() const noexcept { return flags_; }

  NSL_AST_NODE_BOILERPLATE(LiteralExpr)

private:
  Lit litKind_;
  Identifier spelling_;
  uint16_t flags_;
};

} // namespace nsl::ast

#endif // NSL_AST_LITERAL_EXPR_H
