// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Parse/ParseStmt.cpp — per-statement parsers for `lang.ebnf §§8–10`
// (FR-013). Each `parse*` returns a `std::unique_ptr<Stmt>` or nullptr
// on syntax error (with diagnostic raised).
//
// Statement-position parser-note dispatch:
//   - N1: `if` at statement position → `parseIfStatement` (returns
//     `IfStmt`). The expression-position form is handled by ParseExpr.
//   - N6: `inst.finish()` etc. → `parseLValueLedStatement` accepts a
//     `scoped_identifier` LHS; if followed by `(` it's a control call.
//   - N11(a): `_display(...)` and friends → `parseSystemTaskStatement`.
//     `_init { ... }` → `parseInitBlock`. `_delay(n)` → `parseDelayTask`.
//
// `seq_block_item` accepts `internal_declaration`, `action_statement`,
// `label_name_declaration`, and `line_marker`. Internal-declarations
// inside `seq` are dispatched via `parseInternalDecl()`; the AST
// model models them under `SeqBlock::items` as a vector of `Stmt`,
// but `*Decl`s are not `Stmt`s. We keep them under the parent's
// `internals` chain by parsing them into a wrapper synthetic — but
// since the SeqBlock interface is `vector<Stmt>` only, internal
// decls inside `seq` are NOT modeled in M2's AST shape.
//
// FOLDED M2 SCOPE DECISION: data-model.md §1.5 specifies
// `SeqBlock::items: vector<unique_ptr<Stmt>>`. Internal declarations
// inside `seq` (per `lang.ebnf §8` lines 405–411) cannot be stored
// there without a base-class change. M2 ships the simpler shape:
// internal declarations inside `seq` are accepted by the parser but
// the resulting `*Decl` node is dropped on the floor (a Sema-or-
// later concern; the lexically valid input parses without error,
// and FR-006 / Acceptance Scenario 4 are satisfied for the
// statement-only forms). This is a deliberate M2 simplification —
// the FUTURE expansion (data-model gain a Stmt-or-Decl variant) is
// recorded in the integration concerns of the track-D handoff.

#include "ParserImpl.h"

#include "nsl/AST/AltBlock.h"
#include "nsl/AST/AnyBlock.h"
#include "nsl/AST/BareFinishStmt.h"
#include "nsl/AST/CallExpr.h"
#include "nsl/AST/ControlCallStmt.h"
#include "nsl/AST/DelayTaskStmt.h"
#include "nsl/AST/EmptyStmt.h"
#include "nsl/AST/Expr.h"
#include "nsl/AST/ForBlock.h"
#include "nsl/AST/GotoStmt.h"
#include "nsl/AST/IdentifierExpr.h"
#include "nsl/AST/IfStmt.h"
#include "nsl/AST/InitBlockStmt.h"
#include "nsl/AST/LabeledStmt.h"
#include "nsl/AST/ParallelBlock.h"
#include "nsl/AST/ReturnStmt.h"
#include "nsl/AST/SeqBlock.h"
#include "nsl/AST/Stmt.h"
#include "nsl/AST/StructuralGenerate.h"
#include "nsl/AST/SystemTaskStmt.h"
#include "nsl/AST/TransferStmt.h"
#include "nsl/AST/WhileBlock.h"

#include <memory>
#include <utility>
#include <vector>

namespace nsl::parse {

namespace {

bool isInternalDeclStart(TokenKind k) {
  switch (k) {
  case TokenKind::tk_wire:
  case TokenKind::tk_reg:
  case TokenKind::tk_func_self:
  case TokenKind::tk_proc_name:
  case TokenKind::tk_state_name:
  case TokenKind::tk_first_state:
  case TokenKind::tk_mem:
  case TokenKind::tk_integer:
  case TokenKind::tk_variable:
    return true;
  default:
    return false;
  }
}

bool isLabelNameDecl(TokenKind k) { return k == TokenKind::tk_label_name; }

/// Skip a `label_name id { , id } ;` form. The AST has no node kind
/// for it (it's a Sema/M3 concern); parsing it correctly preserves
/// the surrounding seq_block items.
bool skipLabelNameDecl(Parser &p) {
  p.consume(); // label_name
  Token t;
  if (!p.expect(TokenKind::tk_identifier, "label_name identifier", &t)) {
    return false;
  }
  while (p.check(TokenKind::tk_comma)) {
    p.consume();
    if (!p.expect(TokenKind::tk_identifier, "label_name identifier after ','",
                  &t)) {
      return false;
    }
  }
  return p.expect(TokenKind::tk_semicolon, "';' after label_name declaration");
}

bool isLValueStart(TokenKind k) {
  // identifier-like or `.{` (concat-LHS) or system-function expr
  return k == TokenKind::tk_identifier || k == TokenKind::tk_label ||
         k == TokenKind::tk_dot_lbrace;
}

} // namespace

// ---------- action_statement dispatch ----------

std::unique_ptr<ast::Stmt> Parser::parseActionStatement() {
  TokenKind k = peekKind();
  switch (k) {
  case TokenKind::tk_lbrace:
    return parseParallelBlock();
  case TokenKind::tk_alt:
    return parseAltBlock();
  case TokenKind::tk_any:
    return parseAnyBlock();
  case TokenKind::tk_seq:
    return parseSeqBlock();
  case TokenKind::tk_while_:
    return parseWhileBlock();
  case TokenKind::tk_for_:
    return parseForBlock();
  case TokenKind::tk_if_:
    return parseIfStatement();
  case TokenKind::tk_generate:
    return parseStructuralGenerate();
  case TokenKind::tk_return_:
    return parseReturnStatement();
  case TokenKind::tk_goto_:
    return parseGotoStatement();
  case TokenKind::tk_finish: {
    // bare `finish;` form per `lang.ebnf §9` line 478.
    Token t = consume();
    Token semi;
    if (!expect(TokenKind::tk_semicolon, "';' after 'finish'", &semi)) {
      return nullptr;
    }
    return std::make_unique<ast::BareFinishStmt>(
        rangeFromTo(t.range().begin(), semi.range().end()));
  }
  case TokenKind::tk_system_task:
    return parseSystemTaskStatement();
  case TokenKind::tk_semicolon: {
    Token t = consume();
    return std::make_unique<ast::EmptyStmt>(t.range());
  }
  default:
    if (isLValueStart(k)) {
      return parseLValueLedStatement();
    }
    errorAtPeek("expected action statement");
    return nullptr;
  }
}

// ---------- LValue-led: transfer / control-call / inc-dec / labeled ----------

std::unique_ptr<ast::Stmt> Parser::parseLValueLedStatement() {
  // Special-case: a bare `identifier ":"` is a labeled statement
  // (per `lang.ebnf §8` line 415: `labeled_statement = identifier ":" ;`).
  if ((peekKind() == TokenKind::tk_identifier ||
       peekKind() == TokenKind::tk_label) &&
      peekAhead(1).kind() == TokenKind::tk_colon) {
    Token name_tok = consume();
    if (name_tok.kind() == TokenKind::tk_label) {
      warning(name_tok.range().begin(),
              "'label' is reserved; using as identifier (parser-note N10)");
    }
    Token colon = consume();
    // The labeled-statement node has a `body` member but the EBNF
    // allows the label to stand alone with NO trailing body — the
    // body is the next statement, but the AST groups them as
    // `LabeledStmt(label, body=nullptr)`. Sema/IR ties them together.
    return std::make_unique<ast::LabeledStmt>(
        rangeFromTo(name_tok.range().begin(), colon.range().end()),
        name_tok.spelling(), nullptr);
  }

  // For statement-position LHS forms we parse the head as a scoped
  // identifier so we can detect `target ( args ) ;` (control-call,
  // per N6) directly without needing to lift through CallExpr's
  // const-only accessors. The `.{...}`-LHS form (N3) is handled by
  // the general parseExpr() fallback below.
  if (peekKind() == TokenKind::tk_identifier ||
      peekKind() == TokenKind::tk_label) {
    SourceRange head_range;
    ast::ScopedName head_name = parseScopedName(head_range);
    if (head_name.parts.empty()) {
      return nullptr;
    }
    SourceLocation begin = head_range.begin();

    // Control-terminal call: `target(args);` (per N6).
    if (check(TokenKind::tk_lparen)) {
      consume();
      std::vector<std::unique_ptr<ast::Expr>> args;
      if (!parseArgumentList(args)) {
        return nullptr;
      }
      Token semi;
      if (!expect(TokenKind::tk_semicolon, "';' after control-terminal call",
                  &semi)) {
        return nullptr;
      }
      return std::make_unique<ast::ControlCallStmt>(
          rangeFromTo(begin, semi.range().end()), std::move(head_name),
          std::move(args));
    }

    // Build an LHS expression from the scoped name; postfix `[hi]`,
    // `[hi:lo]` etc. then attach. We synthesize an IdentifierExpr
    // and feed it through parsePostfix to pick up the bit-select tail.
    std::unique_ptr<ast::Expr> lhs = std::make_unique<ast::IdentifierExpr>(
        head_range, std::move(head_name));
    lhs = parsePostfix(std::move(lhs));
    if (!lhs) {
      return nullptr;
    }
    TokenKind nxt = peekKind();
    if (nxt == TokenKind::tk_assign || nxt == TokenKind::tk_assign_seq) {
      ast::TransferStmt::Op op = (nxt == TokenKind::tk_assign_seq)
                                      ? ast::TransferStmt::Op::RegColonEq
                                      : ast::TransferStmt::Op::WireEq;
      consume();
      auto rhs = parseExpr();
      if (!rhs) {
        return nullptr;
      }
      Token semi;
      if (!expect(TokenKind::tk_semicolon, "';' after transfer", &semi)) {
        return nullptr;
      }
      return std::make_unique<ast::TransferStmt>(
          rangeFromTo(begin, semi.range().end()), op, std::move(lhs),
          std::move(rhs));
    }
    errorAtPeek("expected '=' / ':=' or '(' after LHS");
    return nullptr;
  }

  // `.{...}` LHS-concat form (N3). The general expression parser
  // builds a `ConcatExpr` for this.
  auto lhs = parseExpr();
  if (!lhs) {
    return nullptr;
  }
  TokenKind nxt = peekKind();
  if (nxt == TokenKind::tk_assign || nxt == TokenKind::tk_assign_seq) {
    ast::TransferStmt::Op op = (nxt == TokenKind::tk_assign_seq)
                                    ? ast::TransferStmt::Op::RegColonEq
                                    : ast::TransferStmt::Op::WireEq;
    consume();
    auto rhs = parseExpr();
    if (!rhs) {
      return nullptr;
    }
    Token semi;
    if (!expect(TokenKind::tk_semicolon, "';' after transfer", &semi)) {
      return nullptr;
    }
    SourceLocation begin = lhs->loc().begin();
    return std::make_unique<ast::TransferStmt>(
        rangeFromTo(begin, semi.range().end()), op, std::move(lhs),
        std::move(rhs));
  }
  errorAtPeek("expected '=' or ':=' after LHS");
  return nullptr;
}

// ---------- Block forms ----------

std::unique_ptr<ast::Stmt> Parser::parseParallelBlock() {
  Token lbr;
  if (!expect(TokenKind::tk_lbrace, "'{' to begin parallel block", &lbr)) {
    return nullptr;
  }
  std::vector<std::unique_ptr<ast::Stmt>> items;
  for (;;) {
    consumeLineMarkers();
    if (check(TokenKind::tk_rbrace) || check(TokenKind::tk_eof)) {
      break;
    }
    if (isInternalDeclStart(peekKind())) {
      // Per the EBNF parallel_block_item allows internal_declaration.
      // Drop the produced *Decl on the floor at M2 (see file header).
      auto d = parseInternalDecl();
      if (!d) {
        return nullptr;
      }
      continue;
    }
    auto s = parseActionStatement();
    if (!s) {
      return nullptr;
    }
    items.push_back(std::move(s));
  }
  Token rbr;
  if (!expect(TokenKind::tk_rbrace, "'}' to close parallel block", &rbr)) {
    return nullptr;
  }
  return std::make_unique<ast::ParallelBlock>(
      rangeFromTo(lbr.range().begin(), rbr.range().end()), std::move(items));
}

std::unique_ptr<ast::Stmt> Parser::parseAltBlock() {
  Token alt_tok;
  if (!expect(TokenKind::tk_alt, "'alt'", &alt_tok)) {
    return nullptr;
  }
  if (!expect(TokenKind::tk_lbrace, "'{' after 'alt'")) {
    return nullptr;
  }
  std::vector<ast::CondCase> cases;
  std::unique_ptr<ast::Stmt> elseCase;
  for (;;) {
    consumeLineMarkers();
    if (check(TokenKind::tk_rbrace) || check(TokenKind::tk_eof)) {
      break;
    }
    if (check(TokenKind::tk_else_)) {
      consume();
      if (!expect(TokenKind::tk_colon, "':' after 'else'")) {
        return nullptr;
      }
      elseCase = parseActionStatement();
      if (!elseCase) {
        return nullptr;
      }
      continue;
    }
    auto cond = parseExpr();
    if (!cond) {
      return nullptr;
    }
    if (!expect(TokenKind::tk_colon, "':' after alt-case condition")) {
      return nullptr;
    }
    auto body = parseActionStatement();
    if (!body) {
      return nullptr;
    }
    cases.push_back({std::move(cond), std::move(body)});
  }
  Token rbr;
  if (!expect(TokenKind::tk_rbrace, "'}' to close 'alt' block", &rbr)) {
    return nullptr;
  }
  return std::make_unique<ast::AltBlock>(
      rangeFromTo(alt_tok.range().begin(), rbr.range().end()), std::move(cases),
      std::move(elseCase));
}

std::unique_ptr<ast::Stmt> Parser::parseAnyBlock() {
  Token any_tok;
  if (!expect(TokenKind::tk_any, "'any'", &any_tok)) {
    return nullptr;
  }
  if (!expect(TokenKind::tk_lbrace, "'{' after 'any'")) {
    return nullptr;
  }
  std::vector<ast::CondCase> cases;
  std::unique_ptr<ast::Stmt> elseCase;
  for (;;) {
    consumeLineMarkers();
    if (check(TokenKind::tk_rbrace) || check(TokenKind::tk_eof)) {
      break;
    }
    if (check(TokenKind::tk_else_)) {
      consume();
      if (!expect(TokenKind::tk_colon, "':' after 'else'")) {
        return nullptr;
      }
      elseCase = parseActionStatement();
      if (!elseCase) {
        return nullptr;
      }
      continue;
    }
    auto cond = parseExpr();
    if (!cond) {
      return nullptr;
    }
    if (!expect(TokenKind::tk_colon, "':' after any-case condition")) {
      return nullptr;
    }
    auto body = parseActionStatement();
    if (!body) {
      return nullptr;
    }
    cases.push_back({std::move(cond), std::move(body)});
  }
  Token rbr;
  if (!expect(TokenKind::tk_rbrace, "'}' to close 'any' block", &rbr)) {
    return nullptr;
  }
  return std::make_unique<ast::AnyBlock>(
      rangeFromTo(any_tok.range().begin(), rbr.range().end()), std::move(cases),
      std::move(elseCase));
}

std::unique_ptr<ast::Stmt> Parser::parseSeqBlock() {
  Token seq_tok;
  if (!expect(TokenKind::tk_seq, "'seq'", &seq_tok)) {
    return nullptr;
  }
  if (!expect(TokenKind::tk_lbrace, "'{' after 'seq'")) {
    return nullptr;
  }
  std::vector<std::unique_ptr<ast::Stmt>> items;
  for (;;) {
    consumeLineMarkers();
    if (check(TokenKind::tk_rbrace) || check(TokenKind::tk_eof)) {
      break;
    }
    if (isLabelNameDecl(peekKind())) {
      if (!skipLabelNameDecl(*this)) {
        return nullptr;
      }
      continue;
    }
    if (isInternalDeclStart(peekKind())) {
      auto d = parseInternalDecl();
      if (!d) {
        return nullptr;
      }
      continue;
    }
    auto s = parseActionStatement();
    if (!s) {
      return nullptr;
    }
    items.push_back(std::move(s));
  }
  Token rbr;
  if (!expect(TokenKind::tk_rbrace, "'}' to close 'seq' block", &rbr)) {
    return nullptr;
  }
  return std::make_unique<ast::SeqBlock>(
      rangeFromTo(seq_tok.range().begin(), rbr.range().end()), std::move(items));
}

std::unique_ptr<ast::Stmt> Parser::parseWhileBlock() {
  Token while_tok;
  if (!expect(TokenKind::tk_while_, "'while'", &while_tok)) {
    return nullptr;
  }
  if (!expect(TokenKind::tk_lparen, "'(' after 'while'")) {
    return nullptr;
  }
  auto cond = parseExpr();
  if (!cond) {
    return nullptr;
  }
  if (!expect(TokenKind::tk_rparen, "')' after while-condition")) {
    return nullptr;
  }
  if (!expect(TokenKind::tk_lbrace, "'{' to begin 'while' body")) {
    return nullptr;
  }
  std::vector<std::unique_ptr<ast::Stmt>> items;
  for (;;) {
    consumeLineMarkers();
    if (check(TokenKind::tk_rbrace) || check(TokenKind::tk_eof)) {
      break;
    }
    if (isLabelNameDecl(peekKind())) {
      if (!skipLabelNameDecl(*this)) {
        return nullptr;
      }
      continue;
    }
    if (isInternalDeclStart(peekKind())) {
      auto d = parseInternalDecl();
      if (!d) {
        return nullptr;
      }
      continue;
    }
    auto s = parseActionStatement();
    if (!s) {
      return nullptr;
    }
    items.push_back(std::move(s));
  }
  Token rbr;
  if (!expect(TokenKind::tk_rbrace, "'}' to close 'while' body", &rbr)) {
    return nullptr;
  }
  return std::make_unique<ast::WhileBlock>(
      rangeFromTo(while_tok.range().begin(), rbr.range().end()), std::move(cond),
      std::move(items));
}

std::unique_ptr<ast::Stmt> Parser::parseForBlock() {
  Token for_tok;
  if (!expect(TokenKind::tk_for_, "'for'", &for_tok)) {
    return nullptr;
  }
  if (!expect(TokenKind::tk_lparen, "'(' after 'for'")) {
    return nullptr;
  }

  // for_init: simple form `id := expr` OR a parallel_block `{ ... }`.
  ast::ForForm form;
  if (check(TokenKind::tk_lbrace)) {
    auto blk = parseParallelBlock();
    if (!blk) {
      return nullptr;
    }
    form.init = std::move(blk);
  } else {
    Token name_tok;
    if (!expect(TokenKind::tk_identifier, "for-init identifier", &name_tok)) {
      return nullptr;
    }
    if (!expect(TokenKind::tk_assign_seq, "':=' in for-init")) {
      return nullptr;
    }
    auto init_expr = parseExpr();
    if (!init_expr) {
      return nullptr;
    }
    // Synthesize a `TransferStmt` for the init; the for-form stores
    // the init clause as a Stmt regardless of single/compound shape.
    auto target = std::make_unique<ast::IdentifierExpr>(
        name_tok.range(), ast::ScopedName{{name_tok.spelling()}});
    SourceLocation begin = name_tok.range().begin();
    SourceLocation end = init_expr->loc().end();
    form.init = std::make_unique<ast::TransferStmt>(
        rangeFromTo(begin, end), ast::TransferStmt::Op::RegColonEq,
        std::move(target), std::move(init_expr));
  }

  // The for_header has two forms:
  //   ENUM:  for_init "," expression ")"
  //   C:     for_init ";" expression ";" for_step ")"
  if (check(TokenKind::tk_comma)) {
    consume();
    auto end_expr = parseExpr();
    if (!end_expr) {
      return nullptr;
    }
    form.cond = std::move(end_expr);
    // No step in enum form.
  } else if (check(TokenKind::tk_semicolon)) {
    consume();
    auto cond = parseExpr();
    if (!cond) {
      return nullptr;
    }
    form.cond = std::move(cond);
    if (!expect(TokenKind::tk_semicolon, "';' between for-cond and for-step")) {
      return nullptr;
    }
    // for_step
    if (check(TokenKind::tk_lbrace)) {
      auto blk = parseParallelBlock();
      if (!blk) {
        return nullptr;
      }
      form.step = std::move(blk);
    } else {
      // single_for_step: increment_decrement OR `id := expr`
      // Increment_decrement at expression position has multiple forms
      // (++id, --id, id++, id--). For M2 we capture all as
      // IncDecStmt — but the for-step storage is `unique_ptr<Stmt>`.
      // Simplest: parse an expression (Pratt handles ++id at nud),
      // then if followed by `++`/`--` it's a postfix; else the LHS
      // is the head and we look for `:=` for the assignment shape.
      Token name_tok;
      if (!expect(TokenKind::tk_identifier, "for-step identifier", &name_tok)) {
        return nullptr;
      }
      // After the identifier, we have either `++` / `--` or `:=`.
      // The lexer DOES NOT have `tk_increment` / `tk_decrement` —
      // there is no "++" punctuator in Token.h's enum. So inc/dec
      // is not supported in M2's for-step (a known parser-shape gap;
      // see file header). Require `:=`.
      if (!expect(TokenKind::tk_assign_seq, "':=' in for-step")) {
        return nullptr;
      }
      auto step_expr = parseExpr();
      if (!step_expr) {
        return nullptr;
      }
      auto target = std::make_unique<ast::IdentifierExpr>(
          name_tok.range(), ast::ScopedName{{name_tok.spelling()}});
      SourceLocation begin = name_tok.range().begin();
      SourceLocation end = step_expr->loc().end();
      form.step = std::make_unique<ast::TransferStmt>(
          rangeFromTo(begin, end), ast::TransferStmt::Op::RegColonEq,
          std::move(target), std::move(step_expr));
    }
  } else {
    errorAtPeek("expected ',' (enum-form for) or ';' (C-form for)");
    return nullptr;
  }

  if (!expect(TokenKind::tk_rparen, "')' after for-header")) {
    return nullptr;
  }
  if (!expect(TokenKind::tk_lbrace, "'{' to begin 'for' body")) {
    return nullptr;
  }
  std::vector<std::unique_ptr<ast::Stmt>> items;
  for (;;) {
    consumeLineMarkers();
    if (check(TokenKind::tk_rbrace) || check(TokenKind::tk_eof)) {
      break;
    }
    if (isLabelNameDecl(peekKind())) {
      if (!skipLabelNameDecl(*this)) {
        return nullptr;
      }
      continue;
    }
    if (isInternalDeclStart(peekKind())) {
      auto d = parseInternalDecl();
      if (!d) {
        return nullptr;
      }
      continue;
    }
    auto s = parseActionStatement();
    if (!s) {
      return nullptr;
    }
    items.push_back(std::move(s));
  }
  Token rbr;
  if (!expect(TokenKind::tk_rbrace, "'}' to close 'for' body", &rbr)) {
    return nullptr;
  }
  return std::make_unique<ast::ForBlock>(
      rangeFromTo(for_tok.range().begin(), rbr.range().end()), std::move(form),
      std::move(items));
}

// ---------- if / generate / return / goto ----------

std::unique_ptr<ast::Stmt> Parser::parseIfStatement() {
  Token if_tok;
  if (!expect(TokenKind::tk_if_, "'if'", &if_tok)) {
    return nullptr;
  }
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
  auto thenBr = parseActionStatement();
  if (!thenBr) {
    return nullptr;
  }
  std::unique_ptr<ast::Stmt> elseBr;
  SourceLocation end_loc = thenBr->loc().end();
  if (check(TokenKind::tk_else_)) {
    consume();
    elseBr = parseActionStatement();
    if (!elseBr) {
      return nullptr;
    }
    end_loc = elseBr->loc().end();
  }
  return std::make_unique<ast::IfStmt>(
      rangeFromTo(if_tok.range().begin(), end_loc), std::move(cond),
      std::move(thenBr), std::move(elseBr));
}

std::unique_ptr<ast::Stmt> Parser::parseStructuralGenerate() {
  Token gen_tok;
  if (!expect(TokenKind::tk_generate, "'generate'", &gen_tok)) {
    return nullptr;
  }
  if (!expect(TokenKind::tk_lparen, "'(' after 'generate'")) {
    return nullptr;
  }
  Token name_tok;
  if (!expect(TokenKind::tk_identifier, "generate-init identifier", &name_tok)) {
    return nullptr;
  }
  if (!expect(TokenKind::tk_assign, "'=' in generate-init")) {
    return nullptr;
  }
  // generate_init = identifier "=" constant_expression — we need the
  // initializer expression but the AST node `StructuralGenerate` only
  // stores the init identifier (per data-model §1.5 lines 92–93). So
  // we parse and DROP the initializer expression. (The same applies
  // to `step`'s LHS: spec stores only the step expression.)
  auto init_expr = parseExpr();
  if (!init_expr) {
    return nullptr;
  }
  if (!expect(TokenKind::tk_semicolon, "';' between generate-init and cond")) {
    return nullptr;
  }
  auto cond = parseExpr();
  if (!cond) {
    return nullptr;
  }
  if (!expect(TokenKind::tk_semicolon, "';' between generate-cond and step")) {
    return nullptr;
  }
  // generate_step — the EBNF allows `increment_decrement` or
  // `id "=" const_expr`. The lexer has no `++` / `--` punctuator at
  // M1; treat increment_decrement as out-of-scope at parser-shape
  // level. Require `id "=" expr` form.
  Token step_name;
  if (!expect(TokenKind::tk_identifier, "generate-step identifier", &step_name)) {
    return nullptr;
  }
  if (!expect(TokenKind::tk_assign, "'=' in generate-step")) {
    return nullptr;
  }
  auto step_expr = parseExpr();
  if (!step_expr) {
    return nullptr;
  }
  if (!expect(TokenKind::tk_rparen, "')' after generate-header")) {
    return nullptr;
  }
  auto body = parseActionStatement();
  if (!body) {
    return nullptr;
  }
  SourceLocation end_loc = body->loc().end();
  return std::make_unique<ast::StructuralGenerate>(
      rangeFromTo(gen_tok.range().begin(), end_loc), name_tok.spelling(),
      std::move(cond), std::move(step_expr), std::move(body));
}

std::unique_ptr<ast::Stmt> Parser::parseReturnStatement() {
  Token ret_tok;
  if (!expect(TokenKind::tk_return_, "'return'", &ret_tok)) {
    return nullptr;
  }
  std::unique_ptr<ast::Expr> value;
  if (!check(TokenKind::tk_semicolon)) {
    value = parseExpr();
    if (!value) {
      return nullptr;
    }
  }
  Token semi;
  if (!expect(TokenKind::tk_semicolon, "';' after 'return'", &semi)) {
    return nullptr;
  }
  return std::make_unique<ast::ReturnStmt>(
      rangeFromTo(ret_tok.range().begin(), semi.range().end()), std::move(value));
}

std::unique_ptr<ast::Stmt> Parser::parseGotoStatement() {
  Token goto_tok;
  if (!expect(TokenKind::tk_goto_, "'goto'", &goto_tok)) {
    return nullptr;
  }
  Token name_tok;
  if (!expect(TokenKind::tk_identifier, "goto target identifier", &name_tok)) {
    return nullptr;
  }
  Token semi;
  if (!expect(TokenKind::tk_semicolon, "';' after 'goto'", &semi)) {
    return nullptr;
  }
  return std::make_unique<ast::GotoStmt>(
      rangeFromTo(goto_tok.range().begin(), semi.range().end()),
      name_tok.spelling());
}

// ---------- _init / _delay / system_task ----------

std::unique_ptr<ast::Stmt> Parser::parseInitBlock() {
  // Caller has gated on `tk_system_task` whose spelling is `_init`.
  Token init_tok = consume();
  if (!expect(TokenKind::tk_lbrace, "'{' after '_init'")) {
    return nullptr;
  }
  std::vector<std::unique_ptr<ast::Stmt>> items;
  for (;;) {
    consumeLineMarkers();
    if (check(TokenKind::tk_rbrace) || check(TokenKind::tk_eof)) {
      break;
    }
    auto s = parseActionStatement();
    if (!s) {
      return nullptr;
    }
    items.push_back(std::move(s));
  }
  Token rbr;
  if (!expect(TokenKind::tk_rbrace, "'}' to close '_init' block", &rbr)) {
    return nullptr;
  }
  return std::make_unique<ast::InitBlockStmt>(
      rangeFromTo(init_tok.range().begin(), rbr.range().end()),
      std::move(items));
}

std::unique_ptr<ast::Stmt> Parser::parseDelayTask() {
  Token delay_tok = consume(); // `_delay`
  if (!expect(TokenKind::tk_lparen, "'(' after '_delay'")) {
    return nullptr;
  }
  auto count = parseExpr();
  if (!count) {
    return nullptr;
  }
  if (!expect(TokenKind::tk_rparen, "')' after '_delay' argument")) {
    return nullptr;
  }
  Token semi;
  if (!expect(TokenKind::tk_semicolon, "';' after '_delay(...)'", &semi)) {
    return nullptr;
  }
  return std::make_unique<ast::DelayTaskStmt>(
      rangeFromTo(delay_tok.range().begin(), semi.range().end()),
      std::move(count));
}

std::unique_ptr<ast::Stmt> Parser::parseSystemTaskStatement() {
  // peek().spelling() carries the leading-underscore name (`_display`,
  // `_finish`, `_init`, `_delay`, `_readmemh`, …). Per N11(a) all of
  // these are statement-position system tasks. `_init { ... }` and
  // `_delay(n)` get specialized AST nodes; the rest become `SystemTaskStmt`.
  llvm::StringRef sp = peek().spelling();
  if (sp == "_init") {
    return parseInitBlock();
  }
  if (sp == "_delay") {
    return parseDelayTask();
  }
  Token name_tok = consume();
  if (!expect(TokenKind::tk_lparen, "'(' after system task")) {
    return nullptr;
  }
  std::vector<std::unique_ptr<ast::Expr>> args;
  if (!parseArgumentList(args)) {
    return nullptr;
  }
  Token semi;
  if (!expect(TokenKind::tk_semicolon, "';' after system-task call", &semi)) {
    return nullptr;
  }
  return std::make_unique<ast::SystemTaskStmt>(
      rangeFromTo(name_tok.range().begin(), semi.range().end()),
      name_tok.spelling(), std::move(args));
}

} // namespace nsl::parse
