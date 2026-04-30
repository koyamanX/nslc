// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Driver/RunNSLPasses.cpp — `Compilation::runNSLPasses` stub
// body (M4; real body lands at M5).
//
// **Specification anchors**:
//   - `specs/007-m4-mlir-dialect/spec.md` FR-004 — at M4 the body
//     is a trivial diagnostic stub returning `mlir::failure()`.
//   - `specs/007-m4-mlir-dialect/research.md` §7 — stub form
//     rationale (parallel to `LowerToNSL.cpp`).
//
// Reachable only from a direct C++ caller; the `nslc` CLI rejects
// `-emit=mlir` at M4 (FR-023). M5's patch replaces this entire file
// with the structural-expansion-pass driver (`NSLResolveParamsPass`,
// `NSLExpandGeneratePass`, `NSLExpandVariablesPass`,
// `NSLExplodeSubmodArrayPass`, `NSLInlineInternalFuncPass`,
// `NSLCheckSemanticsPass` per design §9 line 1075) without touching
// `Compilation.cpp` or any header.

#include "nsl/Driver/Compilation.h"

#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceLocation.h"

namespace nsl::driver {

mlir::LogicalResult Compilation::runNSLPasses(mlir::ModuleOp /*module*/) {
  diag_.report(Severity::Error, SourceLocation{},
               "MLIR lowering not yet implemented; see M5");
  return mlir::failure();
}

} // namespace nsl::driver
