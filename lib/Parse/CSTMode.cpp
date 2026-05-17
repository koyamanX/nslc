// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Parse/CSTMode.cpp — PRIVATE impl of the 3-arg CST-mode
// overload of `parseCompilationUnit` declared in
// `include/nsl/Parse/Parser.h` (T2 Phase 2b — T017/T018).
//
// **Why a separate translation unit**: the existing
// `Parser.cpp::parseCompilationUnit(Lexer&, DiagnosticEngine&)` is a
// load-bearing entry point used by every nslc invocation; the
// CST-mode wiring is purely additive (an opt-in observer hook). Per
// Constitution Principle II's no-duplication rule the new overload
// reuses the same `Parser::parseCompilationUnit()` body — it does
// NOT duplicate the parse loop. The only logic added here is the
// top-level `beginNode`/`endNode` bracketing.
//
// Sub-production (`parseModuleItem`, `parseAltBlock`, etc.) sink
// instrumentation is deferred to T2 Phase 3+ when the
// LayoutPlanner needs the structural context to emit `Doc::group`
// boundaries.
//
// **Spec / contract anchors**:
//   - `specs/010-t2-formatter-v0/research.md` §2 (CST-mode parser
//     extension shape).
//   - `specs/010-t2-formatter-v0/contracts/cst-shape.contract.md` §6
//     (top-level production wraps with begin/endNode; tokens via
//     recordToken).
//   - Constitution Principle II — no second public header on
//     nsl-parse; this file is private to lib/Parse/.

#include "ParserImpl.h"
#include "nsl/AST/CompilationUnit.h"
#include "nsl/Basic/SourceLocation.h"
#include "nsl/Lex/Lexer.h"
#include "nsl/Parse/Parser.h"

#include "llvm/ADT/StringRef.h"

#include <memory>

namespace nsl::parse {

std::unique_ptr<ast::CompilationUnit>
parseCompilationUnit(Lexer &lex, DiagnosticEngine &diag, CSTSink *sink) {
  // Fast path: when no sink is attached, behave identically to the
  // 2-arg overload (zero overhead vs the AST-only hot path).
  if (sink == nullptr) {
    return parseCompilationUnit(lex, diag);
  }

  Parser p(lex, diag);
  p.setCSTSink(sink);

  // Top-level production wrap. `peek(0)` forces the lexer to lex the
  // first token; its `range().begin()` is the first byte of the
  // compilation unit (offset 0 of the input FileID, even for empty
  // input — `tk_eof.range().begin()` == offset 0 in that case).
  ::nsl::SourceLocation begin = lex.peek(0).range().begin();
  sink->beginNode(::llvm::StringRef("CompilationUnit"), begin);

  std::unique_ptr<ast::CompilationUnit> cu = p.parseCompilationUnit();

  // After parseCompilationUnit returns, the lexer is parked at
  // `tk_eof`; its `range().begin()` is the EOF cursor — the
  // one-past-end byte of the source. This is the canonical "end" of
  // the top-level production (per `Parser::parseCompilationUnit`'s
  // own `end` computation in `Parser.cpp`).
  ::nsl::SourceLocation end = lex.peek(0).range().begin();
  sink->endNode(end);

  return cu;
}

} // namespace nsl::parse
