// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/Sema/Sema.h — `Sema` engine + `SemaResult`.
//
// Public surface (per `contracts/sema-api.contract.md` Invariant 1):
// this header is one of three permitted public umbrella headers under
// `include/nsl/Sema/` (Constitution Principle II §3 amended at
// v1.6.0). The other two are `SymbolTable.h` and `TypeSystem.h`.
//
// `Sema::run()` is the single public entry point (Invariant 3); the
// internal pass visitors (`ResolutionPass`, `ConstraintCheckRegistry`,
// the per-`Sn` visitors) are private impl details under `lib/Sema/`
// and are NOT exposed here.

#ifndef NSL_SEMA_SEMA_H
#define NSL_SEMA_SEMA_H

#include "nsl/Sema/SymbolTable.h"
#include "nsl/Sema/TypeSystem.h"

#include <memory>

namespace nsl {
class DiagnosticEngine;
} // namespace nsl

namespace nsl::ast {
class CompilationUnit;
class IdentifierExpr;
} // namespace nsl::ast

namespace nsl::sema {

/// Classification of an `IdentifierExpr` per the `S27` constructive
/// rule (per Q1 Option B; `sema-api.contract.md` Invariant 4).
/// Returned by `Sema::classifyIdentifierExpr` once Sema runs.
enum class ClassifierKind : uint8_t {
  /// Default: the identifier names a value-shaped symbol (port,
  /// reg, wire, variable, integer, mem, struct field) and is used
  /// in expression position as that value.
  Value,

  /// `S27`: the identifier names a control-shaped terminal
  /// (`func_in`/`func_out`/`func_self`/proc) and is used in
  /// expression position as a 1-bit "tap" of its control signal.
  ControlTerminalTap,
};

/// The output of `Sema::run()` — owns the symbol table and type
/// system that downstream stages (`-emit=ast` post-Sema printer at
/// M3; `-emit=mlir` at M5+) consume.
///
/// Ownership transfer (Invariant 6 of `sema-api.contract.md`):
/// after `Sema::run()` returns, the `SymbolTable` and `TypeSystem`
/// `unique_ptr`s are moved into the result. The originating `Sema`
/// instance's internal members are nulled-out; re-invoking `run()`
/// on the same instance asserts in debug builds (Phase 2 ships the
/// assertion site; concrete behavior lands in Phase 3).
struct SemaResult {
  /// Owned symbol table — non-null on success, valid for the
  /// lifetime of the surrounding `CompilationUnit`. Per
  /// `sema-stability.contract.md` Invariant 8: `Symbol*` values
  /// returned from this table MUST NOT be passed to or compared
  /// with `Symbol*` values from a different `Sema::run()`
  /// invocation.
  std::unique_ptr<SymbolTable> symbols;

  /// Owned type system — non-null on success.
  std::unique_ptr<TypeSystem> types;

  /// Mirror of `DiagnosticEngine::hasError()` at the end of the
  /// run. `true` if any error-severity diagnostic was emitted
  /// (warnings do NOT set this flag — they're advisory). The
  /// default-constructed value is `false` so an unfilled
  /// `SemaResult` (e.g., when the driver short-circuits on a parse
  /// failure before invoking `Sema::run`) is observably "no Sema
  /// errors yet".
  bool hasErrors = false;
};

/// Sema engine (data-model §4.2). Single-shot: construct, call
/// `run()` exactly once, consume the `SemaResult`. Re-using a
/// `Sema` instance for a second run is an error (asserted in debug
/// builds).
///
/// Threading: not thread-safe. Each thread should have its own
/// `Sema` + `DiagnosticEngine` pair.
class Sema {
public:
  /// Construct a Sema engine bound to `diag` (the only diagnostic
  /// surface per `sema-api.contract.md` Invariant 7). Lifetime of
  /// `diag` MUST exceed the lifetime of `*this`.
  explicit Sema(DiagnosticEngine &diag);

  /// Out-of-line destructor (anchored in `Sema.cpp`).
  ~Sema();

  Sema(const Sema &)            = delete;
  Sema &operator=(const Sema &) = delete;
  Sema(Sema &&)                 = delete;
  Sema &operator=(Sema &&)      = delete;

  /// Run Sema on `unit`. Executes the resolution pass first
  /// (single top-down `ASTVisitor` walk: open/close scopes,
  /// declare symbols, resolve names, infer widths), then the
  /// constraint passes (one fan-out walk per `Sn` family). On the
  /// first stage's failure it still proceeds to the second so
  /// multi-error reporting works (FR-016).
  ///
  /// Returns a `SemaResult` whose `symbols` / `types` move-own the
  /// internal state. After this call, the `Sema` instance's
  /// internal state is empty; calling `run()` a second time
  /// asserts in debug builds.
  ///
  /// **Phase 2 status**: scaffolding only. The two stage
  /// invocations (`runResolutionPass` / `runConstraintPasses`)
  /// are no-ops; concrete behavior lands at Phase 3 (US1) and
  /// Phase 4 (US2). `SemaResult::symbols` and `SemaResult::types`
  /// are non-null and constructed-empty; `SemaResult::hasErrors`
  /// is `false` unless a pre-existing diagnostic was already on
  /// `diag_`.
  SemaResult run(ast::CompilationUnit &unit);

  /// Classify an `IdentifierExpr` per the `S27` constructive rule
  /// (per Q1 Option B; `sema-api.contract.md` Invariant 4).
  /// Returns `ControlTerminalTap` for any expression-position
  /// identifier resolving to a `FuncInSymbol` / `FuncOutSymbol` /
  /// `FuncSelfSymbol` / `ProcSymbol`; `Value` otherwise.
  ///
  /// **Phase 2 status**: signature stub only — returns `Value`
  /// unconditionally. Concrete behavior lands at Phase 4 (T070,
  /// `S27_ControlTerminalAs1Bit.cpp`).
  [[nodiscard]] ClassifierKind
  classifyIdentifierExpr(const ast::IdentifierExpr &expr) const;

private:
  /// Resolution pass: opens/closes scopes, declares symbols,
  /// resolves names, infers widths. Phase 2 stub: no-op. Phase 3
  /// fills.
  void runResolutionPass(ast::CompilationUnit &unit);

  /// Constraint passes: fan-out of N independent walkers, one per
  /// `Sn` family. Each walker is registered at static-init time
  /// via `NSL_REGISTER_CONSTRAINT(N, ...)`. Phase 2 stub: no-op.
  /// Phase 4 fills (each `Sn` adds one source file under
  /// `lib/Sema/Constraints/`).
  void runConstraintPasses(ast::CompilationUnit &unit);

  DiagnosticEngine             &diag_;
  std::unique_ptr<SymbolTable>  symbols_;
  std::unique_ptr<TypeSystem>   types_;
  bool                          hasRun_ = false;
};

} // namespace nsl::sema

#endif // NSL_SEMA_SEMA_H
