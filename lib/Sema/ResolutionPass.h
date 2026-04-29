// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/ResolutionPass.h — private impl header for the
// resolution + width-inference pass. NOT a public header; lives
// under `lib/Sema/` per Phase 3's "no new public headers"
// constraint (per /tasks.md T026 task description) and the
// `sema-api.contract.md` Invariant 1 freeze on
// `include/nsl/Sema/`.
//
// The pass is the single top-down `ASTVisitor` walk that:
//   (a) opens/closes `Scope`s at AST node entry/exit;
//   (b) constructs and `declare()`s one `Symbol` per declaration
//       site;
//   (c) resolves every `IdentifierExpr` / `FieldAccessExpr` /
//       `ScopedName` to a `Symbol*` (writing it into a side-table
//       — the AST nodes themselves are immutable in M3);
//   (d) infers widths for every `Expr` (writing through
//       `Expr::setInferredType()`);
//   (e) emits exactly one "unresolved name 'X'" diagnostic per
//       distinct `X` per FR-017.
//
// The result of the walk is a `ResolutionMap` that the post-Sema
// printer consumes. The map is owned by `Sema` (private state) so
// it lives for the duration of the `CompilationUnit`.

#ifndef NSL_SEMA_RESOLUTION_PASS_H
#define NSL_SEMA_RESOLUTION_PASS_H

#include "nsl/Basic/SourceLocation.h"

#include "llvm/ADT/DenseMap.h"

namespace nsl {
class DiagnosticEngine;
} // namespace nsl

namespace nsl::ast {
class CompilationUnit;
class Expr;
} // namespace nsl::ast

namespace nsl::sema {

class Symbol;
class SymbolTable;
class TypeSystem;

/// Side-table mapping resolved name-references (`IdentifierExpr`,
/// `FieldAccessExpr` head, `ScopedName` head) to their declaring
/// `Symbol*`. Populated by `runResolutionPass`; consumed by the
/// post-Sema `-emit=ast` printer to render the
/// `→ decl@<file>:<line>:<col>` suffix.
///
/// Pointer keys are stable for the lifetime of the surrounding
/// `CompilationUnit` (AST nodes are owned by `unique_ptr<>` and
/// never relocated). The map is iteration-order-deterministic in
/// practice because the printer never iterates it — it only does
/// per-key lookups during the deterministic AST walk.
struct ResolutionMap {
  /// `IdentifierExpr*` / `FieldAccessExpr*` keys mapped to the
  /// resolved declaration's `Symbol*`. Null entries are not
  /// inserted (lookup returning end() means "unresolved" — the
  /// printer prints `Unresolved` and omits the decl-loc suffix).
  llvm::DenseMap<const ast::Expr *, const Symbol *> exprToSymbol;
};

/// Driver for the resolution + width-inference pass.
///
/// Inputs: `unit` (the AST root); `symbols` (target symbol table —
/// the pass calls `enterScope` / `declare` / `lookup` on it);
/// `types` (type interner — the pass calls `bit()` / `bitVector(N)`
/// / `unresolved()` on it); `diag` (diagnostic surface for
/// "unresolved name" / "duplicate name" reports).
///
/// Output: a populated `ResolutionMap` for printer consumption.
///
/// Side effects on the AST: `Expr::setInferredType(...)` is called
/// on every `Expr` reached during the walk. Other AST slots are
/// not mutated.
ResolutionMap runResolutionPassImpl(const ast::CompilationUnit &unit,
                                    SymbolTable &symbols, TypeSystem &types,
                                    DiagnosticEngine &diag);

/// Thread-local accessor used by the printer (`lib/AST/Printer.cpp`)
/// to look up the resolution data for a given AST node. Set by
/// `Sema::run()` before the printer is invoked; cleared after.
///
/// Why TLS rather than a parameter: the public `nsl::ast::print`
/// signature in `include/nsl/AST/Printer.h` is M2-frozen; the
/// post-Sema enrichments are additive and the printer detects
/// them dynamically. The TLS pointer is null in the pre-Sema
/// (M2) print mode, non-null post-Sema. Single-threaded compiler
/// → no contention.
const ResolutionMap *currentResolutionMap() noexcept;
void setCurrentResolutionMap(const ResolutionMap *map) noexcept;

} // namespace nsl::sema

#endif // NSL_SEMA_RESOLUTION_PASS_H
