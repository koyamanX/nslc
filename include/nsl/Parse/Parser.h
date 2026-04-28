// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/Parse/Parser.h — public API for `nsl-parse`
// (`specs/005-m2-parser/data-model.md` §2; FR-002, FR-006).
//
// The parser consumes a `nsl::Lexer`'s pull-model token stream and
// produces a `nsl::ast::CompilationUnit` AST per `lang.ebnf §§1–11`.
// Diagnostics route through the M1 `DiagnosticEngine` (FR-019); the
// parser MUST NOT write to `stderr` directly.
//
// Recovery is OUT of M2 Phase 3 scope — at this milestone the parser
// uses single-error bail (return nullptr / propagate) when it detects
// a syntax error. The Phase 5 (US3) track adds full multi-error
// recovery on top of the same `parseFoo()` shape.
//
// Layered position: layer 5. Depends on `nsl-basic` (SourceLocation /
// DiagnosticEngine), `nsl-lex` (Lexer / Token), and `nsl-ast` (the AST
// types it constructs). MUST NOT depend on `nsl-sema` or any later
// layer (FR-003 / SC-009).

#ifndef NSL_PARSE_PARSER_H
#define NSL_PARSE_PARSER_H

#include <memory>

namespace nsl {
class Lexer;
class DiagnosticEngine;
} // namespace nsl

namespace nsl::ast {
class CompilationUnit;
} // namespace nsl::ast

namespace nsl::parse {

/// Parse `lex`'s token stream into a `CompilationUnit`. Diagnostics
/// raised during parse route through `diag`. On success returns the
/// constructed AST root; on a fatal-after-recovery-exhaustion error
/// returns `nullptr` (callers should consult `diag.hasError()` after
/// this call to decide whether to print the AST).
///
/// The function consumes tokens from `lex` until it sees `tk_eof`.
std::unique_ptr<ast::CompilationUnit>
parseCompilationUnit(Lexer &lex, DiagnosticEngine &diag);

} // namespace nsl::parse

#endif // NSL_PARSE_PARSER_H
