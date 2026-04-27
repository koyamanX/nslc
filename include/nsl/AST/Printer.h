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

namespace llvm {
class raw_ostream;
} // namespace llvm

namespace nsl {
class SourceManager;
} // namespace nsl

namespace nsl::ast {

class CompilationUnit;

/// Walk `cu` in declaration order and write its text-only
/// S-expression-style dump to `os`. Resolves every `SourceLocation`
/// through `sm` to obtain the virtual (post-`#line`) `path:line:col`
/// per `nslc-emit-ast.contract.md`. Output terminates in `\n` per
/// the contract's "Trailing newline" rule.
void print(const CompilationUnit &cu, const SourceManager &sm,
           llvm::raw_ostream &os);

} // namespace nsl::ast

#endif // NSL_AST_PRINTER_H
