// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Parse/Parser.cpp — top-level driver for the NSL recursive-descent
// parser (FR-006 / `lang.ebnf §1`). Implements the public free function
// `nsl::parse::parseCompilationUnit` and the `Parser` class methods
// shared across the per-file parsers (token-buffer plumbing, scoped-
// identifier helper, expect()).
//
// Per `data-model.md` §1.2 the `CompilationUnit` AST holds
// `std::vector<std::unique_ptr<Decl>> items` populated in declaration
// order. `line_marker` tokens are consumed without producing AST
// nodes (FR-015 / N14).

#include "ParserImpl.h"

#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/Decl.h"
#include "nsl/Parse/Parser.h"

#include "llvm/ADT/StringRef.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace nsl::parse {

// ---------- Parser class out-of-line members ----------

bool Parser::expect(TokenKind k, llvm::StringRef what, Token *out) {
  if (check(k)) {
    Token t = consume();
    if (out != nullptr) {
      *out = t;
    }
    return true;
  }
  std::string msg = "expected ";
  msg += what.str();
  errorAtPeek(std::move(msg));
  return false;
}

bool Parser::consumeIdentifierLike(ast::Identifier &out_name,
                                   SourceRange &out_range) {
  TokenKind k = peekKind();
  if (k == TokenKind::tk_identifier) {
    Token t = consume();
    out_name = t.spelling();
    out_range = t.range();
    return true;
  }
  // N10: `label` reserved-but-warned. The token name uses the
  // KeywordSet.def suffix; in this codebase that is `tk_label` (raw,
  // not `tk_label_`). Warn and accept as identifier.
  if (k == TokenKind::tk_label) {
    Token t = consume();
    warning(t.range().begin(),
            "'label' is reserved; using as identifier (parser-note N10)");
    out_name = t.spelling();
    out_range = t.range();
    return true;
  }
  return false;
}

bool Parser::expectIdentifierAllowLabel(llvm::StringRef what, Token *out) {
  TokenKind k = peekKind();
  if (k == TokenKind::tk_identifier) {
    Token t = consume();
    if (out != nullptr) {
      *out = t;
    }
    return true;
  }
  if (k == TokenKind::tk_label) {
    Token t = consume();
    // FR-027 locked-text warning per N10. Treat `label` as an
    // identifier and continue; the parse is non-blocking.
    warning(t.range().begin(),
            "'label' is reserved; using as identifier (parser-note N10)");
    if (out != nullptr) {
      *out = t;
    }
    return true;
  }
  std::string msg = "expected ";
  msg += what.str();
  errorAtPeek(std::move(msg));
  return false;
}

ast::ScopedName Parser::parseScopedName(SourceRange &out_range) {
  ast::ScopedName name;
  ast::Identifier first;
  SourceRange first_range;
  if (!consumeIdentifierLike(first, first_range)) {
    errorAtPeek("expected identifier");
    out_range = SourceRange();
    return name;
  }
  name.parts.push_back(first);
  SourceLocation begin = first_range.begin();
  SourceLocation end = first_range.end();
  while (check(TokenKind::tk_dot)) {
    // Look ahead: only consume the dot if followed by an identifier-
    // like token. Per N6 (`docs/spec/nsl_lang.ebnf:1051-1059`),
    // proc-instance method access reserves `invoke` and `finish` as
    // method names — accept them on the RHS of `.` so
    // `inst.invoke()` / `inst.finish()` parse as a `ControlCallStmt`
    // with `target=inst.invoke` / `target=inst.finish`. The spelling
    // of the keyword token IS the method name (Sema-side S21
    // validates the proc-instance kind separately).
    Token next = peekAhead(1);
    TokenKind nk = next.kind();
    bool accept = (nk == TokenKind::tk_identifier ||
                   nk == TokenKind::tk_label ||
                   nk == TokenKind::tk_invoke ||
                   nk == TokenKind::tk_finish);
    if (!accept) {
      break;
    }
    consume(); // consume `.`
    Token part_tok = consume();
    if (part_tok.kind() == TokenKind::tk_label) {
      // N10 warn-and-accept; mirrors `consumeIdentifierLike`.
      warning(part_tok.range().begin(),
              "'label' is reserved; using as identifier (parser-note N10)");
    }
    name.parts.push_back(part_tok.spelling());
    end = part_tok.range().end();
  }
  out_range = rangeFromTo(begin, end);
  return name;
}

// ---------- Top-level parser ----------

std::unique_ptr<ast::CompilationUnit> Parser::parseCompilationUnit() {
  // Snapshot the location at which the compilation unit begins. We
  // peek (which forces lex of the first token) and capture its
  // begin() — even for an empty file, peek() returns `tk_eof` whose
  // range begins at offset 0 of the input FileID.
  SourceLocation begin = peek().range().begin();

  std::vector<std::unique_ptr<ast::Decl>> items;
  for (;;) {
    consumeLineMarkers();
    TokenKind k = peekKind();
    if (k == TokenKind::tk_eof) {
      break;
    }
    std::unique_ptr<ast::Decl> item;
    switch (k) {
    case TokenKind::tk_struct_:
      item = parseStructDecl();
      break;
    case TokenKind::tk_param_int:
    case TokenKind::tk_param_str:
      item = parseTopLevelParam();
      break;
    case TokenKind::tk_declare:
      item = parseDeclareBlock();
      break;
    case TokenKind::tk_module:
      item = parseModuleBlock();
      break;
    default:
      // Unexpected top-level token. Emit and bail (no Phase-5
      // recovery yet).
      errorAtPeek(
          "expected top-level item (struct, declare, module, or parameter)");
      return nullptr;
    }
    if (!item) {
      // The per-rule parser already raised a diagnostic. Bail.
      return nullptr;
    }
    items.push_back(std::move(item));
  }
  // CompilationUnit's range spans from the first byte of input to the
  // begin() of `tk_eof` (which is the EOF cursor — for an empty file
  // both endpoints coincide, yielding a zero-length valid range).
  SourceLocation end = peek().range().begin();
  return std::make_unique<ast::CompilationUnit>(rangeFromTo(begin, end),
                                                std::move(items));
}

// ---------- Public API ----------

std::unique_ptr<ast::CompilationUnit>
parseCompilationUnit(Lexer &lex, DiagnosticEngine &diag) {
  Parser p(lex, diag);
  return p.parseCompilationUnit();
}

} // namespace nsl::parse
