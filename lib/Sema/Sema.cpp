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

#include "ConstraintCheckRegistry.h"
#include "ResolutionPass.h"

#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/IdentifierExpr.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Sema/SymbolTable.h"
#include "nsl/Sema/TypeSystem.h"

#include <cassert>
#include <memory>
#include <utility>

namespace nsl::sema {

namespace {

/// Storage for the most-recent run's `ResolutionMap`, kept alive
/// for the duration of the post-Sema printer invocation. The
/// Printer reads this via `currentResolutionMap()`. Single-threaded
/// compiler so a thread-local is sufficient; LSP-tier callers MUST
/// drive Sema and the printer on the same thread (per
/// `sema-stability.contract.md` Invariant 8).
thread_local std::unique_ptr<ResolutionMap> g_lastRunMap;

} // namespace

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
  // Phase 4b T070 implementation. Resolve `expr.name()` via the
  // resolution map (set by the Phase 3 ResolutionPass) and return
  // `ControlTerminalTap` when the resolved Symbol's kind is one of
  // FuncIn / FuncOut / FuncSelf / Proc; otherwise `Value`.
  const ResolutionMap *map = currentResolutionMap();
  if (map != nullptr) {
    auto it = map->exprToSymbol.find(&expr);
    if (it != map->exprToSymbol.end() && it->second != nullptr) {
      const Symbol *sym = it->second;
      switch (sym->kind()) {
      case SymbolKind::SK_FuncIn:
      case SymbolKind::SK_FuncOut:
      case SymbolKind::SK_FuncSelf:
      case SymbolKind::SK_Proc:
        return ClassifierKind::ControlTerminalTap;
      default:
        return ClassifierKind::Value;
      }
    }
  }
  // Fallback: probe the symbol table directly. Useful for tests that
  // construct an ad-hoc IdentifierExpr without running ResolutionPass.
  if (symbols_) {
    if (!expr.name().parts.empty()) {
      Symbol *sym = symbols_->lookup(expr.name().parts.front());
      if (sym != nullptr) {
        switch (sym->kind()) {
        case SymbolKind::SK_FuncIn:
        case SymbolKind::SK_FuncOut:
        case SymbolKind::SK_FuncSelf:
        case SymbolKind::SK_Proc:
          return ClassifierKind::ControlTerminalTap;
        default:
          return ClassifierKind::Value;
        }
      }
    }
  }
  return ClassifierKind::Value;
}

void Sema::runResolutionPass(ast::CompilationUnit &unit) {
  // Phase 3 (T026-T030): invoke the top-down ASTVisitor walker
  // that opens scopes, declares symbols, resolves names, and
  // infers widths. The result map is stashed in the thread-local
  // `g_lastRunMap` so the post-Sema `-emit=ast` printer can
  // consume it via `currentResolutionMap()`.
  assert(symbols_ && types_ &&
         "runResolutionPass after ownership transfer");
  auto map = std::make_unique<ResolutionMap>(
      runResolutionPassImpl(unit, *symbols_, *types_, diag_));
  g_lastRunMap = std::move(map);
  setCurrentResolutionMap(g_lastRunMap.get());
}

void Sema::runConstraintPasses(ast::CompilationUnit &unit) {
  // Fan out over every per-`Sn` walker registered at static-init
  // time via `NSL_REGISTER_CONSTRAINT`. The registry iterates in
  // Sn-numeric order (deterministic per Principle V); each walker
  // reads `ctx` (immutable post-resolution view) and emits any
  // S<NN> diagnostics into `ctx.diag`.
  ConstraintContext ctx;
  ctx.unit        = &unit;
  ctx.symbols     = symbols_.get();
  ctx.types       = types_.get();
  ctx.resolutions = g_lastRunMap.get();
  ctx.diag        = &diag_;
  runAllConstraints(ctx);
}

} // namespace nsl::sema
