// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Parse/ParserImpl.h — PRIVATE implementation header for `nsl-parse`.
//
// Shared by `Parser.cpp`, `ParseDecl.cpp`, `ParseStmt.cpp`,
// `ParseExpr.cpp`. Holds the `Parser` class — the recursive-descent
// driver — plus token-buffer / diagnostic-emit helpers used by every
// `parseFoo()` site.
//
// NOT exposed via `include/nsl/Parse/`: external consumers see only
// the free function `parseCompilationUnit` declared in
// `nsl/Parse/Parser.h`. Adding methods here is a private refactor,
// not a public API change.
//
// Recovery (US3, Phase 5) is NOT implemented here yet. The current
// shape uses single-error bail: when a syntax check fails, the
// `parseFoo()` function emits a diagnostic via `diag()` and returns
// `nullptr` / a default-constructed value. The caller propagates.
// Phase 5 will add `RecoveryGuard` + `skipUntil()` primitives without
// changing the surface of this class.
//
// Token buffer model: we delegate to `Lexer::peek(n)` / `Lexer::next()`
// directly. The lexer maintains its own peek-cache deque per
// `include/nsl/Lex/Lexer.h`, so cumulative `peek()` + `next()` is
// O(1) amortized. The parser does not maintain a parallel buffer.

#ifndef NSL_LIB_PARSE_PARSERIMPL_H
#define NSL_LIB_PARSE_PARSERIMPL_H

#include "nsl/AST/ASTNode.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Lex/Lexer.h"
#include "nsl/Lex/Token.h"

#include "llvm/ADT/StringRef.h"

#include <memory>
#include <string>
#include <vector>

namespace nsl::parse {

/// Recursive-descent driver. Holds the lexer cursor + diagnostic sink.
///
/// Construction: cheap (the lexer reference is stored; no eager peek).
/// Method invocation order is up to the caller — `parseCompilationUnit`
/// drives the canonical top-level loop.
class Parser {
public:
  Parser(Lexer &lex, DiagnosticEngine &diag) noexcept
      : lex_(lex), diag_(diag) {}

  Parser(const Parser &) = delete;
  Parser &operator=(const Parser &) = delete;

  // ----- Token-buffer primitives -----

  /// Look at the next-to-be-returned token without consuming.
  Token peek() { return lex_.peek(0); }

  /// Look at the token `n` positions ahead. `peekAhead(0)` ≡ `peek()`.
  Token peekAhead(int n) { return lex_.peek(n); }

  /// Convenience: kind of next-to-be-returned token.
  TokenKind peekKind() { return lex_.peek(0).kind(); }

  /// Consume the next token and return it.
  Token consume() { return lex_.next(); }

  /// True if the next token is `k`.
  bool check(TokenKind k) { return peekKind() == k; }

  /// If the next token is `k`, consume and return it via `out`. Else
  /// leave the buffer alone and return false.
  bool match(TokenKind k, Token *out = nullptr) {
    if (!check(k)) {
      return false;
    }
    Token t = consume();
    if (out != nullptr) {
      *out = t;
    }
    return true;
  }

  /// Expect the next token to be `k`. On match, consume and return
  /// true (and store via `out`). On mismatch, emit a parser-error
  /// diagnostic citing the unexpected-token location and return false.
  /// `what` names the syntactic construct in the diagnostic message
  /// (e.g., `"';' after register declaration"`).
  bool expect(TokenKind k, llvm::StringRef what, Token *out = nullptr);

  // ----- Diagnostic helpers -----

  /// Emit a parser error at the given location.
  void error(SourceLocation loc, std::string msg) {
    diag_.report(Severity::Error, loc, std::move(msg));
  }

  /// Emit a parser error at the start of `peek()`.
  void errorAtPeek(std::string msg) {
    diag_.report(Severity::Error, peek().range().begin(), std::move(msg));
  }

  /// Emit a warning (e.g., N10 `label`-as-identifier).
  void warning(SourceLocation loc, std::string msg) {
    diag_.report(Severity::Warning, loc, std::move(msg));
  }

  // ----- Engine accessor -----

  DiagnosticEngine &diag() noexcept { return diag_; }

  // ----- N14: line_marker consumption -----
  //
  // `tk_line_directive` carries the textual `#line N "F"` form the
  // M1 preprocessor emits at the seam. The parser drops them at every
  // item-list position; cursor-bookkeeping is the SourceManager's
  // responsibility (the M1 driver replays the directives onto the
  // synthetic FileID before parsing — see lib/Driver/EmitTokens.cpp).
  /// Consume any contiguous run of `tk_line_directive` tokens. The
  /// SourceManager has already absorbed them at driver pre-pass time,
  /// so the parser's only responsibility is to drop them silently.
  /// Each item-list dispatch loop calls this before peeking for its
  /// item-start keyword (per FR-015 / N14).
  void consumeLineMarkers() {
    while (check(TokenKind::tk_line_directive)) {
      consume();
    }
  }

  // ----- Helpers used across the per-file parsers -----

  /// Build a `SourceRange` from `begin` to `end`. Centralized so
  /// per-rule SourceRange construction stays uniform.
  static SourceRange rangeFromTo(SourceLocation begin, SourceLocation end) {
    return SourceRange(begin, end);
  }

  /// Parse a `scoped_identifier` (`ident { '.' ident }`). Caller has
  /// validated that `peek()` is an identifier-like token. Returns the
  /// dotted `ScopedName` and (out-param) the SourceRange of the whole
  /// scoped name. On error, returns an empty ScopedName and emits a
  /// diagnostic.
  ast::ScopedName parseScopedName(SourceRange &out_range);

  /// Consume an identifier-position token. Accepts `tk_identifier`
  /// and `tk_label` (with N10 warning). Stores spelling and range in
  /// out-params. Returns false on mismatch (no error emitted — caller
  /// decides whether mismatch is fatal).
  bool consumeIdentifierLike(ast::Identifier &out_name, SourceRange &out_range);

  // ----- The five per-grammar entry points (defined per file) -----

  std::unique_ptr<ast::CompilationUnit> parseCompilationUnit();

  // -- ParseDecl.cpp --
  std::unique_ptr<ast::Decl> parseStructDecl();
  std::unique_ptr<ast::Decl> parseTopLevelParam();
  std::unique_ptr<ast::Decl> parseDeclareBlock();
  /// Parse one `declare_item`. On a recognized form, appends to one of
  /// the two output vectors and returns true. On EOF/`}` returns false
  /// (caller's `}` consumption will follow). On hard error returns
  /// false but emits a diagnostic; caller bails.
  bool parseDeclareItem(std::vector<std::unique_ptr<ast::Decl>> &headerParams,
                        std::vector<std::unique_ptr<ast::Decl>> &ports);
  std::unique_ptr<ast::Decl> parseModuleBlock();
  bool parseModuleItem(std::vector<std::unique_ptr<ast::Decl>> &internals,
                       std::vector<std::unique_ptr<ast::Stmt>> &actions,
                       std::vector<std::unique_ptr<ast::Decl>> &funcs,
                       std::vector<std::unique_ptr<ast::Decl>> &procs);

  /// Parse one `internal_declaration` form. Returns nullptr on syntax
  /// error (with diagnostic raised).
  std::unique_ptr<ast::Decl> parseInternalDecl();

  std::unique_ptr<ast::Decl> parseFuncDefn();
  std::unique_ptr<ast::Decl> parseProcDefn();
  std::unique_ptr<ast::Decl> parseStateDefn();

  // -- ParseStmt.cpp --
  std::unique_ptr<ast::Stmt> parseActionStatement();
  std::unique_ptr<ast::Stmt> parseParallelBlock();
  std::unique_ptr<ast::Stmt> parseAltBlock();
  std::unique_ptr<ast::Stmt> parseAnyBlock();
  std::unique_ptr<ast::Stmt> parseSeqBlock();
  std::unique_ptr<ast::Stmt> parseWhileBlock();
  std::unique_ptr<ast::Stmt> parseForBlock();
  std::unique_ptr<ast::Stmt> parseIfStatement();
  std::unique_ptr<ast::Stmt> parseStructuralGenerate();
  std::unique_ptr<ast::Stmt> parseReturnStatement();
  std::unique_ptr<ast::Stmt> parseGotoStatement();
  std::unique_ptr<ast::Stmt> parseInitBlock();
  std::unique_ptr<ast::Stmt> parseDelayTask();
  std::unique_ptr<ast::Stmt> parseSystemTaskStatement();
  /// Parse the body of a parenthesized `argument_list`. Caller has
  /// already consumed the `(`. On success consumes the matching `)`
  /// and returns true.
  bool parseArgumentList(std::vector<std::unique_ptr<ast::Expr>> &out);

  // -- ParseExpr.cpp --
  /// Pratt entry. Returns nullptr on syntax error.
  std::unique_ptr<ast::Expr> parseExpr();
  std::unique_ptr<ast::Expr> parseExprAtPrecedence(int floor);
  /// "nud" dispatch — primary / unary / leaf.
  std::unique_ptr<ast::Expr> parseNudExpr();
  /// Postfix tail: `[hi]`, `[hi:lo]`, `.field`, `(args)`. Wraps `head`.
  std::unique_ptr<ast::Expr> parsePostfix(std::unique_ptr<ast::Expr> head);

private:
  Lexer &lex_;
  DiagnosticEngine &diag_;
};

} // namespace nsl::parse

#endif // NSL_LIB_PARSE_PARSERIMPL_H
