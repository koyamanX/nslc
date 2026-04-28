// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Parse/ParseExpr.cpp — Pratt-style precedence-climbing parser for
// `lang.ebnf §11` expressions (FR-014). The Pratt table itself lives
// in `lib/Parse/PrecedenceTable.h` (private to nsl-parse, shared with
// the test suite).
//
// Entry point: `parseExpr()` ≡ `parseExprAtPrecedence(0)`. The Pratt
// loop alternates `nud` (null-denotation: prefix / leaf) consumption
// with `led` (left-denotation: infix continuation) consumption,
// stopping when the next token's `ledPrec` drops below `floor`.
//
// Parser-note dispatch covered here:
//   - N1: `if (...) a else b` expression form. `tk_if_`'s nud builds
//     a `ConditionalExpr`. The statement-form `IfStmt` lives in
//     ParseStmt.cpp; routing happens by call site (statement-position
//     vs expression-position).
//   - N2: `&` `|` `^` reduction-vs-bitwise. The token has both `nud`
//     (reduction) and `led` (bitwise). The parser is currently at
//     `parseNudExpr()` if no left operand has been built; the Pratt
//     loop reaches `led` only after a left operand is on the stack.
//   - N3: `.{` LHS concat — parsed via `tk_dot_lbrace` nud. The
//     same node kind (`ConcatExpr`) covers RHS and LHS forms; the
//     LHS-form discriminator is positional (TransferStmt::lhs).
//   - N5: `#` sign-extend is INFIX per `lang.ebnf §11` line 702. The
//     PrecedenceTable wires `tk_hash_sign_extend` as `led`-only at
//     Multiplicative precedence. A bare prefix `#` is a syntax error.
//   - N11(b): `_random` / `_time` (no parens) → `SystemVarExpr`. The
//     lexer emits `tk_system_function` for these names; we route them
//     as a leaf nud.

#include "ParserImpl.h"
#include "PrecedenceTable.h"

#include "nsl/AST/BinaryExpr.h"
#include "nsl/AST/CallExpr.h"
#include "nsl/AST/ConcatExpr.h"
#include "nsl/AST/ConditionalExpr.h"
#include "nsl/AST/Expr.h"
#include "nsl/AST/FieldAccessExpr.h"
#include "nsl/AST/IdentifierExpr.h"
#include "nsl/AST/LiteralExpr.h"
#include "nsl/AST/RepeatExpr.h"
#include "nsl/AST/SignExtendExpr.h"
#include "nsl/AST/SliceExpr.h"
#include "nsl/AST/StructCastExpr.h"
#include "nsl/AST/SystemVarExpr.h"
#include "nsl/AST/UnaryExpr.h"
#include "nsl/AST/ZeroExtendExpr.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Lex/Token.h"

#include <memory>
#include <utility>
#include <vector>

namespace nsl::parse {

namespace {

/// Map a token-kind that is BOTH a literal kind in the lexer AND a
/// `LiteralExpr::Lit` enumerator in the AST. Returns `Decimal` as a
/// safe default; callers gate on the lexer-side kind first.
ast::LiteralExpr::Lit toLitKind(TokenKind k) {
  switch (k) {
  case TokenKind::tk_decimal_lit:
    return ast::LiteralExpr::Lit::Decimal;
  case TokenKind::tk_hex_lit:
    return ast::LiteralExpr::Lit::Hex;
  case TokenKind::tk_binary_lit:
    return ast::LiteralExpr::Lit::Binary;
  case TokenKind::tk_octal_lit:
    return ast::LiteralExpr::Lit::Octal;
  case TokenKind::tk_string_lit:
    return ast::LiteralExpr::Lit::String;
  default:
    // Should never reach — caller has gated on a literal kind.
    return ast::LiteralExpr::Lit::Decimal;
  }
}

bool isLiteralKind(TokenKind k) {
  switch (k) {
  case TokenKind::tk_decimal_lit:
  case TokenKind::tk_hex_lit:
  case TokenKind::tk_binary_lit:
  case TokenKind::tk_octal_lit:
  case TokenKind::tk_string_lit:
    return true;
  default:
    return false;
  }
}

ast::BinaryExpr::Op binaryOpFor(TokenKind k) {
  using Op = ast::BinaryExpr::Op;
  switch (k) {
  case TokenKind::tk_plus:
    return Op::Add;
  case TokenKind::tk_minus:
    return Op::Sub;
  case TokenKind::tk_star:
    return Op::Mul;
  case TokenKind::tk_slash:
    return Op::Div;
  case TokenKind::tk_percent:
    return Op::Mod;
  case TokenKind::tk_amp:
    return Op::BitAnd;
  case TokenKind::tk_pipe:
    return Op::BitOr;
  case TokenKind::tk_caret:
    return Op::BitXor;
  case TokenKind::tk_shift_left:
    return Op::ShiftLeft;
  case TokenKind::tk_shift_right:
    return Op::ShiftRight;
  case TokenKind::tk_equal:
    return Op::Equal;
  case TokenKind::tk_not_equal:
    return Op::NotEqual;
  case TokenKind::tk_less:
    return Op::Less;
  case TokenKind::tk_less_equal:
    return Op::LessEqual;
  case TokenKind::tk_greater:
    return Op::Greater;
  case TokenKind::tk_greater_equal:
    return Op::GreaterEqual;
  case TokenKind::tk_logical_and:
    return Op::LogicalAnd;
  case TokenKind::tk_logical_or:
    return Op::LogicalOr;
  default:
    return Op::Add; // Unreachable: caller has consulted the table.
  }
}

bool isPostfixStart(TokenKind k) {
  return k == TokenKind::tk_lbracket || k == TokenKind::tk_dot ||
         k == TokenKind::tk_lparen;
}

} // namespace

// ---------- Nud (prefix / leaf) ----------

std::unique_ptr<ast::Expr> Parser::parseNudExpr() {
  Token t = peek();
  TokenKind k = t.kind();

  // Literals
  if (isLiteralKind(k)) {
    Token tok = consume();
    auto lit = std::make_unique<ast::LiteralExpr>(
        tok.range(), toLitKind(tok.kind()), tok.spelling(), tok.flags());

    // §11 sign_extend / zero_extend / repeat have the constant_expression
    // (typically a literal) as the LEFT operand — Pratt's `led` for
    // `#` / `'` handles that. Repeat is `<count> { <body> }`: an
    // infix-like form starting with `{` after the count. Keep the
    // dispatch inside the Pratt loop (parseExprAtPrecedence) — `nud`
    // returns just the literal; the loop sees `tk_lbrace` next and
    // needs to know whether `<expr> { <expr> }` is the repeat form.
    //
    // We disambiguate by special-case in the Pratt loop after returning
    // the literal: if the very next token is `tk_lbrace`, treat as
    // repeat (only valid when LHS is a literal/constant — a soft
    // restriction; Sema will validate compile-time-ness at M3).

    return lit;
  }

  // Identifier-like leaves (with possible scoped form `a.b.c`)
  if (k == TokenKind::tk_identifier || k == TokenKind::tk_label) {
    SourceRange whole;
    ast::ScopedName name = parseScopedName(whole);
    if (name.parts.empty()) {
      return nullptr;
    }
    return std::make_unique<ast::IdentifierExpr>(whole, std::move(name));
  }

  // System variables (`_random`, `_time` — no parens; per N11(b))
  if (k == TokenKind::tk_system_function) {
    Token tok = consume();
    auto var = ast::SystemVarExpr::Var::Random;
    if (tok.spelling() == "_time") {
      var = ast::SystemVarExpr::Var::Time;
    }
    return std::make_unique<ast::SystemVarExpr>(tok.range(), var);
  }

  // Parenthesized expression OR struct-cast
  if (k == TokenKind::tk_lparen) {
    Token lpar = consume();
    // Struct-cast `(TypeName)(expr).member` — peek for the
    // `tk_identifier ) ( ... ) . tk_identifier` shape. The simple
    // recognizer: if the inner is exactly an identifier and the next
    // tokens are `) (` then we have a struct cast.
    if (peek().kind() == TokenKind::tk_identifier &&
        peekAhead(1).kind() == TokenKind::tk_rparen &&
        peekAhead(2).kind() == TokenKind::tk_lparen) {
      Token type_tok = consume(); // type-name identifier
      consume();                  // `)`
      consume();                  // `(`
      auto inner = parseExpr();
      if (!inner) {
        return nullptr;
      }
      if (!expect(TokenKind::tk_rparen, "')' after struct-cast expression")) {
        return nullptr;
      }
      // At least one `.member` per the EBNF.
      if (!expect(TokenKind::tk_dot,
                  "'.' after struct-cast — member access required")) {
        return nullptr;
      }
      std::vector<ast::Identifier> path;
      ast::Identifier first_part;
      SourceRange first_range;
      if (!consumeIdentifierLike(first_part, first_range)) {
        errorAtPeek("expected member name after '.'");
        return nullptr;
      }
      path.push_back(first_part);
      SourceLocation end = first_range.end();
      while (check(TokenKind::tk_dot)) {
        consume();
        ast::Identifier nxt;
        SourceRange nxt_range;
        if (!consumeIdentifierLike(nxt, nxt_range)) {
          errorAtPeek("expected member name after '.'");
          return nullptr;
        }
        path.push_back(nxt);
        end = nxt_range.end();
      }
      return std::make_unique<ast::StructCastExpr>(
          rangeFromTo(lpar.range().begin(), end), type_tok.spelling(),
          std::move(inner), std::move(path));
    }
    // Plain parenthesized expression
    auto inner = parseExpr();
    if (!inner) {
      return nullptr;
    }
    if (!expect(TokenKind::tk_rparen, "')' after expression")) {
      return nullptr;
    }
    return inner;
  }

  // Concat `{ a, b, c }`
  if (k == TokenKind::tk_lbrace) {
    Token lbr = consume();
    std::vector<std::unique_ptr<ast::Expr>> parts;
    if (!check(TokenKind::tk_rbrace)) {
      auto first = parseExpr();
      if (!first) {
        return nullptr;
      }
      parts.push_back(std::move(first));
      while (check(TokenKind::tk_comma)) {
        consume();
        auto nxt = parseExpr();
        if (!nxt) {
          return nullptr;
        }
        parts.push_back(std::move(nxt));
      }
    }
    Token rbr;
    if (!expect(TokenKind::tk_rbrace, "'}' after concat expression", &rbr)) {
      return nullptr;
    }
    return std::make_unique<ast::ConcatExpr>(
        rangeFromTo(lbr.range().begin(), rbr.range().end()), std::move(parts));
  }

  // N3: `.{` LHS-concat marker. Same shape as `{ ... }` but distinct
  // entry token. Used only on the LHS of a transfer; the parser builds
  // the same `ConcatExpr` node and the position-discriminator is the
  // enclosing `TransferStmt`.
  if (k == TokenKind::tk_dot_lbrace) {
    Token mark = consume();
    std::vector<std::unique_ptr<ast::Expr>> parts;
    if (!check(TokenKind::tk_rbrace)) {
      auto first = parseExpr();
      if (!first) {
        return nullptr;
      }
      parts.push_back(std::move(first));
      while (check(TokenKind::tk_comma)) {
        consume();
        auto nxt = parseExpr();
        if (!nxt) {
          return nullptr;
        }
        parts.push_back(std::move(nxt));
      }
    }
    Token rbr;
    if (!expect(TokenKind::tk_rbrace, "'}' after .{...} concat", &rbr)) {
      return nullptr;
    }
    return std::make_unique<ast::ConcatExpr>(
        rangeFromTo(mark.range().begin(), rbr.range().end()), std::move(parts));
  }

  // Prefix unary
  if (k == TokenKind::tk_minus || k == TokenKind::tk_plus ||
      k == TokenKind::tk_tilde || k == TokenKind::tk_logical_not ||
      k == TokenKind::tk_amp || k == TokenKind::tk_pipe ||
      k == TokenKind::tk_caret) {
    Token op_tok = consume();
    using UOp = ast::UnaryExpr::Op;
    UOp uop = UOp::Plus;
    switch (op_tok.kind()) {
    case TokenKind::tk_minus:
      uop = UOp::Neg;
      break;
    case TokenKind::tk_plus:
      uop = UOp::Plus;
      break;
    case TokenKind::tk_tilde:
      uop = UOp::BitNot;
      break;
    case TokenKind::tk_logical_not:
      uop = UOp::LogicalNot;
      break;
    case TokenKind::tk_amp:
      uop = UOp::ReduceAnd;
      break;
    case TokenKind::tk_pipe:
      uop = UOp::ReduceOr;
      break;
    case TokenKind::tk_caret:
      uop = UOp::ReduceXor;
      break;
    default:
      break;
    }
    // Recurse to grab a unary-tight operand. We pass the highest
    // useful precedence floor (Multiplicative) so nothing weaker
    // binds inside the unary's operand without parentheses.
    auto sub = parseExprAtPrecedence(static_cast<int>(PrecLevel::Multiplicative));
    if (!sub) {
      return nullptr;
    }
    SourceLocation end_loc = sub->loc().end();
    return std::make_unique<ast::UnaryExpr>(
        rangeFromTo(op_tok.range().begin(), end_loc), uop, std::move(sub));
  }

  // N1 expression form: `if (cond) thenE else elseE`
  if (k == TokenKind::tk_if_) {
    Token if_tok = consume();
    if (!expect(TokenKind::tk_lparen, "'(' after 'if'")) {
      return nullptr;
    }
    auto cond = parseExpr();
    if (!cond) {
      return nullptr;
    }
    if (!expect(TokenKind::tk_rparen, "')' after if-condition")) {
      return nullptr;
    }
    auto thenE = parseExprAtPrecedence(static_cast<int>(PrecLevel::LogicalOr));
    if (!thenE) {
      return nullptr;
    }
    if (!expect(TokenKind::tk_else_, "'else' after if-then expression")) {
      return nullptr;
    }
    auto elseE = parseExprAtPrecedence(static_cast<int>(PrecLevel::LogicalOr));
    if (!elseE) {
      return nullptr;
    }
    SourceLocation end_loc = elseE->loc().end();
    return std::make_unique<ast::ConditionalExpr>(
        rangeFromTo(if_tok.range().begin(), end_loc), std::move(cond),
        std::move(thenE), std::move(elseE));
  }

  // Fallthrough — unrecognized expression start.
  errorAtPeek("expected expression");
  return nullptr;
}

// ---------- Postfix tail walker ----------

std::unique_ptr<ast::Expr> Parser::parsePostfix(std::unique_ptr<ast::Expr> head) {
  while (head && isPostfixStart(peekKind())) {
    if (check(TokenKind::tk_lbracket)) {
      Token lbr = consume();
      auto hi = parseExpr();
      if (!hi) {
        return nullptr;
      }
      std::unique_ptr<ast::Expr> lo;
      if (check(TokenKind::tk_colon)) {
        consume();
        lo = parseExpr();
        if (!lo) {
          return nullptr;
        }
      }
      Token rbr;
      if (!expect(TokenKind::tk_rbracket, "']' after slice", &rbr)) {
        return nullptr;
      }
      SourceLocation begin = head->loc().begin();
      SourceLocation end = rbr.range().end();
      head = std::make_unique<ast::SliceExpr>(rangeFromTo(begin, end),
                                              std::move(head), std::move(hi),
                                              std::move(lo));
      continue;
    }
    if (check(TokenKind::tk_dot)) {
      consume();
      ast::Identifier field;
      SourceRange field_range;
      if (!consumeIdentifierLike(field, field_range)) {
        errorAtPeek("expected field name after '.'");
        return nullptr;
      }
      SourceLocation begin = head->loc().begin();
      SourceLocation end = field_range.end();
      head = std::make_unique<ast::FieldAccessExpr>(rangeFromTo(begin, end),
                                                    std::move(head), field);
      continue;
    }
    if (check(TokenKind::tk_lparen)) {
      // Postfix call. Convert `head` to a CallExpr — we accept any
      // Expr LHS but the AST shape requires a `ScopedName`. If the
      // head isn't a plain `IdentifierExpr`, leave the ScopedName
      // empty and rely on Sema (M3) to flag the shape.
      consume(); // `(`
      std::vector<std::unique_ptr<ast::Expr>> args;
      // Hand-roll the parse so we can capture the closing-`)` end
      // location for the call-expr SourceRange.
      SourceLocation end_loc;
      if (check(TokenKind::tk_rparen)) {
        Token rpar = consume();
        end_loc = rpar.range().end();
      } else {
        for (;;) {
          auto e = parseExpr();
          if (!e) {
            return nullptr;
          }
          args.push_back(std::move(e));
          if (check(TokenKind::tk_comma)) {
            consume();
            continue;
          }
          break;
        }
        Token rpar;
        if (!expect(TokenKind::tk_rparen, "')' after argument list", &rpar)) {
          return nullptr;
        }
        end_loc = rpar.range().end();
      }
      ast::ScopedName target;
      if (head->kind() == ast::NodeKind::NK_IdentifierExpr) {
        const auto *ident = static_cast<const ast::IdentifierExpr *>(head.get());
        target = ident->name();
      }
      SourceLocation begin = head->loc().begin();
      head = std::make_unique<ast::CallExpr>(rangeFromTo(begin, end_loc),
                                              std::move(target),
                                              std::move(args));
      continue;
    }
    break;
  }
  return head;
}

// ---------- Pratt loop ----------

std::unique_ptr<ast::Expr> Parser::parseExpr() {
  return parseExprAtPrecedence(0);
}

std::unique_ptr<ast::Expr> Parser::parseExprAtPrecedence(int floor) {
  auto lhs = parseNudExpr();
  if (!lhs) {
    return nullptr;
  }
  // Postfix tail wraps the leaf head.
  lhs = parsePostfix(std::move(lhs));
  if (!lhs) {
    return nullptr;
  }

  // Repeat `<count> { <body> }` — fires when the lhs is a
  // (constant-evaluable) expression and the next token is `{`. We
  // accept it whenever an `{` follows; Sema validates count-ness.
  if (check(TokenKind::tk_lbrace)) {
    Token lbr = consume();
    auto body = parseExpr();
    if (!body) {
      return nullptr;
    }
    Token rbr;
    if (!expect(TokenKind::tk_rbrace, "'}' after repeat-expression body",
                &rbr)) {
      return nullptr;
    }
    SourceLocation begin = lhs->loc().begin();
    SourceLocation end = rbr.range().end();
    lhs = std::make_unique<ast::RepeatExpr>(rangeFromTo(begin, end),
                                             std::move(lhs), std::move(body));
  }

  for (;;) {
    TokenKind k = peekKind();
    PrecEntry entry = getPrecedence(k);
    if (entry.ledPrec == PrecLevel::None) {
      break;
    }
    int prec = static_cast<int>(entry.ledPrec);
    if (prec < floor) {
      break;
    }
    Token op_tok = consume();

    // N5 sign-extend: `<width> # <primary>`
    if (k == TokenKind::tk_hash_sign_extend) {
      // The grammar allows either `N#sig` (a primary) or `N#(expr)`.
      // parseNudExpr handles both via the `(`-leaf path.
      auto sub = parseNudExpr();
      if (!sub) {
        return nullptr;
      }
      sub = parsePostfix(std::move(sub));
      if (!sub) {
        return nullptr;
      }
      SourceLocation begin = lhs->loc().begin();
      SourceLocation end = sub->loc().end();
      lhs = std::make_unique<ast::SignExtendExpr>(rangeFromTo(begin, end),
                                                   std::move(lhs),
                                                   std::move(sub));
      continue;
    }

    // Zero-extend: `<width> ' ( expr )`
    if (k == TokenKind::tk_apostrophe_zero_extend) {
      if (!expect(TokenKind::tk_lparen, "'(' after zero-extend ''")) {
        return nullptr;
      }
      auto sub = parseExpr();
      if (!sub) {
        return nullptr;
      }
      Token rpar;
      if (!expect(TokenKind::tk_rparen, "')' after zero-extend expression",
                  &rpar)) {
        return nullptr;
      }
      SourceLocation begin = lhs->loc().begin();
      SourceLocation end = rpar.range().end();
      lhs = std::make_unique<ast::ZeroExtendExpr>(rangeFromTo(begin, end),
                                                   std::move(lhs),
                                                   std::move(sub));
      continue;
    }

    // Conditional `?:`
    if (k == TokenKind::tk_question) {
      auto thenE =
          parseExprAtPrecedence(static_cast<int>(PrecLevel::Conditional));
      if (!thenE) {
        return nullptr;
      }
      if (!expect(TokenKind::tk_colon, "':' in ternary expression")) {
        return nullptr;
      }
      auto elseE =
          parseExprAtPrecedence(static_cast<int>(PrecLevel::Conditional));
      if (!elseE) {
        return nullptr;
      }
      SourceLocation begin = lhs->loc().begin();
      SourceLocation end = elseE->loc().end();
      lhs = std::make_unique<ast::ConditionalExpr>(
          rangeFromTo(begin, end), std::move(lhs), std::move(thenE),
          std::move(elseE));
      continue;
    }

    // Binary infix
    int next_floor = (entry.ledAssoc == Assoc::Left) ? prec + 1 : prec;
    auto rhs = parseExprAtPrecedence(next_floor);
    if (!rhs) {
      return nullptr;
    }
    SourceLocation begin = lhs->loc().begin();
    SourceLocation end = rhs->loc().end();
    auto bop = binaryOpFor(k);
    lhs = std::make_unique<ast::BinaryExpr>(rangeFromTo(begin, end), bop,
                                             std::move(lhs), std::move(rhs));
  }
  return lhs;
}

// ---------- Argument-list helper ----------

bool Parser::parseArgumentList(std::vector<std::unique_ptr<ast::Expr>> &out) {
  // Caller has consumed the `(`. Empty arg list is `( )`.
  if (check(TokenKind::tk_rparen)) {
    consume();
    return true;
  }
  for (;;) {
    auto e = parseExpr();
    if (!e) {
      return false;
    }
    out.push_back(std::move(e));
    if (check(TokenKind::tk_comma)) {
      consume();
      continue;
    }
    break;
  }
  return expect(TokenKind::tk_rparen, "')' after argument list");
}

} // namespace nsl::parse
