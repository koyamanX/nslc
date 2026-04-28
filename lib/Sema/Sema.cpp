// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/Sema.cpp — Sema engine scaffolding.
//
// **Phase 2 status**: scaffolding only. The two stage invocations
// (`runResolutionPass` / `runConstraintPasses`) are no-ops; concrete
// behavior lands at Phase 3 (US1, T026–T030: ResolutionPass + width
// inference + no-cascade) and Phase 4 (US2, T042–T070: per-`Sn`
// checkers self-registering via `NSL_REGISTER_CONSTRAINT`).
//
// The `ConstraintCheckRegistry` mentioned in `data-model.md` §4.3
// and `tasks.md` T013 is reserved for Phase 4 — it lives here as
// a private impl detail when it lands. Phase 2 ships only the
// public entry points.

#include "nsl/Sema/Sema.h"

#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/IdentifierExpr.h"
#include "nsl/Basic/Diagnostic.h"

#include <cassert>
#include <memory>
#include <utility>

namespace nsl::sema {

Sema::Sema(DiagnosticEngine &diag)
    : diag_(diag),
      symbols_(std::make_unique<SymbolTable>()),
      types_(std::make_unique<TypeSystem>()) {}

Sema::~Sema() = default;

SemaResult Sema::run(ast::CompilationUnit &unit) {
  assert(!hasRun_ &&
         "Sema::run() called twice on the same instance; per "
         "sema-api.contract.md Invariant 6, ownership transferred to "
         "SemaResult on the first call");
  hasRun_ = true;

  // Phase 2 stub orchestration: invoke the two stages so the layout
  // is in place; Phase 3 / Phase 4 fill the concrete bodies. Both
  // stubs are no-ops here.
  runResolutionPass(unit);
  runConstraintPasses(unit);

  // Snapshot diagnostic state (Invariant 7). `hasError()` returns
  // true iff any error-severity diagnostic was emitted; warnings
  // do NOT set this flag. `diag_` is bound to the caller's
  // `DiagnosticEngine` so messages remain visible after this
  // returns.
  bool const errs = diag_.hasError();

  // Transfer ownership of the symbol table and type system into
  // the result. After this, `*this` no longer owns either. Calling
  // `run()` a second time would assert at the top of this method.
  SemaResult result;
  result.symbols   = std::move(symbols_);
  result.types     = std::move(types_);
  result.hasErrors = errs;
  return result;
}

ClassifierKind
Sema::classifyIdentifierExpr(const ast::IdentifierExpr &expr) const {
  // Phase 2 stub — Phase 4's `S27_ControlTerminalAs1Bit.cpp`
  // (T070) replaces this body with the actual classifier (look up
  // `expr.name()` in the symbol table; if the resolved kind is
  // `FuncIn`/`FuncOut`/`FuncSelf`/`Proc`, return
  // `ControlTerminalTap`). At Phase 2 we return `Value` so callers
  // compile and link cleanly.
  (void)expr;
  return ClassifierKind::Value;
}

void Sema::runResolutionPass(ast::CompilationUnit &unit) {
  // Phase 2 stub — Phase 3 (T026–T030) replaces this with the
  // top-down ASTVisitor walk that opens scopes, declares symbols,
  // resolves names, and infers widths.
  (void)unit;
}

void Sema::runConstraintPasses(ast::CompilationUnit &unit) {
  // Phase 2 stub — Phase 4 (T042–T070) replaces this with a
  // fan-out over every per-`Sn` walker registered at static-init
  // time via `NSL_REGISTER_CONSTRAINT`.
  (void)unit;
}

} // namespace nsl::sema
