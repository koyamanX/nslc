// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/Printer.h
//
// AST printer entry point — emits a compilation unit as a text-only
// S-expression-style dump matching `contracts/nslc-emit-ast.contract.md`
// §"Stdout schema". Format frozen by `test/Driver/emit-ast.test`'s
// golden at M2; subsequent milestones re-cut the golden in the same
// patch as a format-bumping change (Invariant 7 in
// `contracts/ast-stability.contract.md`).
//
// Determinism guarantees (FR-030 / FR-031 / FR-032; Invariants 2–4):
//   - Byte-identical output across two runs over the same AST.
//   - No raw pointers; cross-references serialize as
//     `ref=path:line:col` of the target's `SourceRange::begin()`.
//   - Every collection iterated in declaration order
//     (`std::vector`); no `std::unordered_*` in the print path.
//
// Why a `SourceManager` parameter: the `SourceLocation`s carried by
// AST nodes are opaque (FileID + offset). The printer resolves them
// to virtual `(path, line, col)` per the contract, which requires
// the `SourceManager` that owns the buffers. The `data-model.md` §3
// signature was `print(CompilationUnit, raw_ostream)`; this header
// extends it minimally — both M2 tracks read the same surface, and
// the test corpus already constructs nodes through a live
// `SourceManager` for the round-trip check (Invariant 8).

#ifndef NSL_AST_PRINTER_H
#define NSL_AST_PRINTER_H

#include "nsl/Basic/SourceLocation.h"

namespace llvm {
class raw_ostream;
} // namespace llvm

namespace nsl {
class SourceManager;
} // namespace nsl

namespace nsl::ast {

class CompilationUnit;
class Expr;

/// Decl-loc lookup callback invoked by the post-Sema printer for
/// every `IdentifierExpr` / `FieldAccessExpr` / `ScopedName` head.
/// Returns the `SourceRange` of the resolved declaration (whose
/// `start` the printer renders as `→ decl@<file>:<line>:<col>`),
/// or an invalid range if the name is unresolved.
///
/// The callback type is opaque — `nsl-ast` does not depend on
/// `nsl-sema`. Phase 3 wires the callback in `nsl::sema` to a
/// thread-local resolution map; tooling layers (M3+ LSP) MAY wire
/// their own callbacks for incremental introspection.
using DeclLocLookupFn = SourceRange (*)(const Expr *);

/// Walk `cu` in declaration order and write its text-only
/// S-expression-style dump to `os`. Resolves every `SourceLocation`
/// through `sm` to obtain the virtual (post-`#line`) `path:line:col`
/// per `nslc-emit-ast.contract.md`. Output terminates in `\n` per
/// the contract's "Trailing newline" rule.
///
/// Mode detection: when ANY `Expr` reached during the walk has
/// `Expr::inferredType() != nullptr` (i.e., M3 Sema has run), the
/// printer enters post-Sema mode and emits the additive ` : <Type>`
/// type-suffix and the optional ` → decl@<file>:<line>:<col>`
/// decl-loc-suffix per `emit-ast-format.contract.md` Invariants 2
/// + 3. Otherwise it emits the M2 format unchanged (Invariant 4).
void print(const CompilationUnit &cu, const SourceManager &sm,
           llvm::raw_ostream &os);

/// Post-Sema variant accepting a decl-loc lookup callback per the
/// `emit-ast-format.contract.md` Invariant 3 ` → decl@…` suffix.
/// `decl_lookup` MAY be null — in that case the type suffix is
/// emitted (when `inferredType() != nullptr`) but the decl-loc
/// suffix is omitted.
void print(const CompilationUnit &cu, const SourceManager &sm,
           llvm::raw_ostream &os, DeclLocLookupFn decl_lookup);

} // namespace nsl::ast

#endif // NSL_AST_PRINTER_H
