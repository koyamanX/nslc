// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Driver/LowerToNSL.cpp — `Compilation::lowerToNSL` stub body
// (M4; real body lands at M5).
//
// **Specification anchors**:
//   - `specs/007-m4-mlir-dialect/spec.md` FR-004 — at M4 the body
//     is a trivial diagnostic stub that emits "MLIR lowering not
//     yet implemented; see M5".
//   - `specs/007-m4-mlir-dialect/research.md` §7 — stub form
//     rationale (forward-declaration alone fails to link; trivial
//     diagnostic stub keeps the binary linkable on the public
//     `-emit=tokens` / `-emit=ast` paths).
//   - `specs/007-m4-mlir-dialect/data-model.md` §5 — driver dialect-
//     load surface entity catalog.
//
// Reachable only from a direct C++ caller; the `nslc` CLI rejects
// `-emit=mlir` at M4 (FR-023), so this stub is unreachable from the
// public surface. M5's patch replaces this entire file with the real
// AST→MLIR lowering body without touching `Compilation.cpp` or any
// header.

#include "nsl/Driver/Compilation.h"

#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"

namespace nsl::driver {

mlir::OwningOpRef<mlir::ModuleOp> Compilation::lowerToNSL(
    ast::CompilationUnit & /*unit*/, sema::SemaResult & /*sema_result*/) {
  diag_.report(Severity::Error, SourceLocation{},
               "MLIR lowering not yet implemented; see M5");
  return {};
}

} // namespace nsl::driver
