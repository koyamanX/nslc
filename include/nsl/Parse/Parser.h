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
// Recovery: full clangd-style multi-error recovery is wired (M2
// Phase 5 / US3, per /speckit-clarify Q1). Per-rule recovery sets
// at every `parseFoo()` site emit a diagnostic, `skipUntil()` to a
// resync token, and continue iterating; multiple independent syntax
// errors in one source surface in a single parse run. The returned
// `CompilationUnit` is non-null on the well-formed prefix even when
// some children fail to parse — well-formed siblings between/after
// error sites are preserved, malformed sub-parses are simply absent
// from their parent's vector. `nullptr` is returned only if recovery
// exhausts to EOF (a true unrecoverable failure). Callers consult
// `diag.hasError()` to decide whether to print the partial AST; the
// `nslc -emit=ast` driver suppresses stdout when any error fired
// (per `nslc-emit-ast.contract.md` §Behavior step 5).
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
