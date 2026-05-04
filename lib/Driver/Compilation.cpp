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
// At M6 the constructor loads the `nsl` dialect (M4) plus the five
// CIRCT dialects (`hw`, `comb`, `seq`, `fsm`, `sv`) per design §11
// lines 1146–1150. The CIRCT dialects are required because the M6
// `Compilation::lowerToCIRCT` member function produces ops in all
// five; loading them into the driver context up-front matches the
// `tools/nsl-opt/main.cpp` registry pattern and guarantees the
// PassManager has the dialects available before pass execution.

#include "nsl/Driver/Compilation.h"

#include "circt/Dialect/Comb/CombDialect.h"
#include "circt/Dialect/FSM/FSMDialect.h"
#include "circt/Dialect/HW/HWDialect.h"
#include "circt/Dialect/SV/SVDialect.h"
#include "circt/Dialect/Seq/SeqDialect.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Dialect/NSL/IR/NSLDialect.h"

namespace nsl::driver {

Compilation::Compilation(DiagnosticEngine &diag) : diag_(diag), mlir_ctx_() {
  // Per FR-004 + design §11 line 1145: load the `nsl` dialect into
  // the driver-side context so the M5 AST→MLIR lowering body has
  // a registered dialect to build ops in.
  mlir_ctx_.loadDialect<nsl::dialect::NSLDialect>();
  // M6: load the five CIRCT dialects so `Compilation::lowerToCIRCT`
  // can emit `hw::*`, `comb::*`, `seq::*`, `fsm::*`, `sv::*` ops.
  mlir_ctx_.loadDialect<circt::hw::HWDialect>();
  mlir_ctx_.loadDialect<circt::comb::CombDialect>();
  mlir_ctx_.loadDialect<circt::seq::SeqDialect>();
  mlir_ctx_.loadDialect<circt::fsm::FSMDialect>();
  mlir_ctx_.loadDialect<circt::sv::SVDialect>();
}

Compilation::~Compilation() = default;

} // namespace nsl::driver
