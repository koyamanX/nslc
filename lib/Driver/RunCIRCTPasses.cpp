// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Driver/RunCIRCTPasses.cpp — `Compilation::runCIRCTPasses` body
// (M7).
//
// **Specification anchors**:
//   - `specs/011-m7-driver-e2e/spec.md` FR-002, FR-004.
//   - `specs/011-m7-driver-e2e/data-model.md` §2 — driver member-
//     function shape.
//   - `specs/011-m7-driver-e2e/contracts/circt-passes.contract.md`
//     §1 (pass identity + ordering) + §3 (PassManager config).
//   - `specs/011-m7-driver-e2e/research.md` §1 (naming-drift
//     retrospective: `convertFSMToSV` is the vendored-canonical
//     name; design doc's `convertFSMToSeq` is the historical name
//     pending docs retro at T094).
//
// The body assembles a `mlir::PassManager` rooted at the supplied
// `mlir::ModuleOp`, registers the three stock-CIRCT passes in the
// pinned order, runs the pipeline. Diagnostics route through the
// shared `DiagnosticBridge` (Constitution Principle IV), reused
// verbatim from M6's `lib/Driver/LowerToCIRCT.cpp`.
//
// **Determinism (Principle V; FR-005)**: parallel mode is explicitly
// disabled and IR verification is enabled between passes so any
// inter-pass shape violation surfaces immediately.

#include "../Lower/Pass/Common/DiagnosticBridge.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h" // full def of mlir::Pass (unique_ptr dtor)
#include "mlir/Pass/PassManager.h"
#include "mlir/Support/LogicalResult.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Driver/Compilation.h"

// Both M7 conversion passes live in the `circt::` namespace (not in
// `circt::fsm::` or `circt::seq::` subspaces) per vendored CIRCT
// exports verified 2026-05-12.
//
// `PrepareForEmission` is intentionally NOT invoked explicitly here:
// per upstream `circt/Conversion/Passes.td:76` ("ExportVerilog
// internally runs PrepareForEmission"), `circt::exportVerilog`
// + `circt::exportSplitVerilog` schedule it themselves. Explicit
// invocation also fails because the pass declares `Pass<"prepare-
// for-emission">` (no root-op binding) and the vendored MLIR
// rejects scheduling it on `builtin.module`.
#include "circt/Conversion/FSMToSV.h" // declares createConvertFSMToSVPass
#include "circt/Conversion/SeqToSV.h" // declares createLowerSeqToSVPass

namespace nsl::driver {

mlir::LogicalResult Compilation::runCIRCTPasses(mlir::ModuleOp module) {
  if (diag_.hasError()) {
    return mlir::failure();
  }

  // RAII bridge: every MLIR-internal diagnostic emitted during any of
  // the three stock-CIRCT passes (verifier failures, pass-internal
  // emitError calls, ExportVerilog-prep rejections) routes through
  // `diag_` per Constitution Principle IV.
  nsl::lower::DiagnosticBridge bridge(diag_, mlir_ctx_);

  // Generic (un-anchored) PassManager — required because
  // `circt::createPrepareForEmission()` is declared as a generic-op
  // pass (`Pass<"prepare-for-emission">` in Passes.td, with no
  // root-op binding), not a ModuleOp-anchored one. The two
  // *-To-SV passes ARE ModuleOp-anchored but the un-anchored PM
  // hosts them via implicit nesting.
  mlir::PassManager pm(&mlir_ctx_);

  // Principle V determinism: no parallel pass scheduling.
  mlir_ctx_.disableMultithreading();
  pm.enableVerifier(true);

  // Pass pipeline (two explicit passes; PrepareForEmission runs
  // inside ExportVerilog itself — see header comment above):
  //   1. fsm::* → seq::* + comb::* (state-register / next-state)
  //   2. seq::* → sv::reg + sv::alwaysff
  //
  // Vendored upstream-CIRCT exposes the FSM-to-SV converter under
  // the post-rename name `convertFSMToSV`; design §10's
  // `convertFSMToSeq` is the historical name. Per research.md §1
  // we commit to the vendored reality.
  pm.addPass(circt::createConvertFSMToSVPass());
  pm.addPass(circt::createLowerSeqToSVPass());

  return pm.run(module);
}

} // namespace nsl::driver
