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

#include "llvm/ADT/StringRef.h"

#include <memory>

namespace nsl {
class Lexer;
class DiagnosticEngine;
class SourceLocation;
class Token;
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

// -----------------------------------------------------------------------------
// CST-mode parsing — observer hook for tools (T2 milestone)
// -----------------------------------------------------------------------------
//
// Per Constitution Principle II (single public header for `nsl-parse`)
// and `specs/010-t2-formatter-v0/contracts/cst-shape.contract.md` §6,
// the formatter (`nsl-fmt`) needs to walk the parser's token stream
// while the parser also constructs the AST. The mechanism: the
// caller supplies a `CSTSink` to the 3-arg overload below; the
// parser invokes its hooks at every grammar production boundary AND
// every token consumption.
//
// `CSTSink` is an abstract interface (no implementation in
// `nsl-parse`); concrete sinks live ABOVE the layer boundary
// (e.g. `nsl::fmt::CSTBuilder` in `lib/Fmt/`). This keeps
// `nsl-parse`'s downward-only dependency direction intact (Principle
// II layer-table rule); `nsl-parse` neither knows nor depends on any
// upper-layer type.
//
// At T2, only `parseCompilationUnit` instruments the sink at the
// production boundary level (single top-level beginNode/endNode);
// every consumed token routes through `recordToken`. Phase 3+
// extends sink coverage to internal productions (`parseModuleItem`,
// `parseAltBlock`, etc.) as the LayoutPlanner requires structural
// context for layout decisions.
//
// **Spec / contract anchors**:
//   - `specs/010-t2-formatter-v0/research.md` §2 (CST-mode parser
//     extension shape — minimal-blast-radius design with one new
//     abstract interface + one new free-function overload).
//   - `specs/010-t2-formatter-v0/contracts/cst-shape.contract.md` §6
//     (per-production wrapping; tokens routed through recordToken).
//   - Constitution Principle II — single public header retained.

/// Abstract sink for CST-mode parsing. Implementations live above
/// the `nsl-parse` layer (e.g., `nsl::fmt::CSTBuilder`). Methods
/// are virtual; expect them to be invoked once per token (heavy
/// hot path) — store and defer rather than allocate per call.
class CSTSink {
public:
  virtual ~CSTSink() = default;

  /// Called when the parser begins a grammar production. `kindName`
  /// is a stable string identifying the production (e.g.,
  /// `"CompilationUnit"`, `"AltBlock"`); pointer must outlive the
  /// sink call (use string literals or interned storage). `start`
  /// is the source location of the production's first byte.
  virtual void beginNode(llvm::StringRef kindName,
                         const ::nsl::SourceLocation &start) = 0;

  /// Called for every token consumed inside an open production.
  virtual void recordToken(const ::nsl::Token &tok) = 0;

  /// Called when a production's last token has been consumed.
  /// `end` is the source location one-past the production's last
  /// byte (i.e., the begin of the next token, or EOF for the last).
  virtual void endNode(const ::nsl::SourceLocation &end) = 0;
};

/// CST-mode overload: when `sink` is non-null, the parser invokes
/// its hooks alongside the AST construction. When `sink` is null
/// the behavior is identical to the 2-arg overload above.
///
/// The pre-existing 2-arg overload is preserved verbatim — every
/// existing call site of `parseCompilationUnit(lex, diag)` retains
/// its current behavior (no implicit AST/CST cost regression).
std::unique_ptr<ast::CompilationUnit>
parseCompilationUnit(Lexer &lex, DiagnosticEngine &diag, CSTSink *sink);

} // namespace nsl::parse

#endif // NSL_PARSE_PARSER_H
