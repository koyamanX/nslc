// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Driver/Compilation.cpp — `Compilation` constructor + destructor.
//
// **Specification anchors**:
//   - `specs/007-m4-mlir-dialect/spec.md` FR-004 — the constructor
//     must load the `nsl` dialect into `mlir_ctx_` per design §11
//     line 1145.
//   - `docs/design/nsl_compiler_design.md` §11 lines 1140–1151 —
//     the constructor body.
//   - `specs/007-m4-mlir-dialect/data-model.md` §5 — driver dialect-
//     load surface.
//
// At M4 the constructor loads only the `nsl` dialect; the CIRCT
// dialects (`hw`, `comb`, `seq`, `fsm`, `sv`) per design §11 lines
// 1146–1150 land in `nsl-opt`'s registry (FR-014; see
// `tools/nsl-opt/main.cpp`) but NOT in the driver's context, because
// at M4 the driver never reaches a stage that emits CIRCT (FR-023
// rejects `-emit=hw`/`-emit=verilog`). The CIRCT-dialect-load lines
// land in this constructor at M6 alongside the AST → CIRCT
// lowering.

#include "nsl/Driver/Compilation.h"

#include "nsl/Basic/Diagnostic.h"
#include "nsl/Dialect/NSL/IR/NSLDialect.h"

namespace nsl::driver {

Compilation::Compilation(DiagnosticEngine &diag) : diag_(diag), mlir_ctx_() {
  // Per FR-004 + design §11 line 1145: load the `nsl` dialect into
  // the driver-side context so the M5 AST→MLIR lowering body has
  // a registered dialect to build ops in.
  mlir_ctx_.loadDialect<nsl::dialect::NSLDialect>();
}

Compilation::~Compilation() = default;

} // namespace nsl::driver
