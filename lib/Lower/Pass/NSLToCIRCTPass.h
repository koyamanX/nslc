// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Lower/Pass/NSLToCIRCTPass.h — internal header for the M6
// nsl→CIRCT conversion pass.
//
// **Specification anchors**:
//   - `specs/010-m6-circt-lowering/data-model.md` §1 — class shape.
//   - `specs/010-m6-circt-lowering/contracts/lower-api.contract.md`
//     §4 — public-surface freeze for `createNSLToCIRCTPass`.
//   - `specs/010-m6-circt-lowering/contracts/circt-lowering.contract.md`
//     §1 — per-op mapping freeze.
//   - `specs/010-m6-circt-lowering/research.md` §1, §11 —
//     full-conversion mode + per-family populate-helpers.
//
// PRIVATE: this header is consumed only by `lib/Lower/Pass/`
// translation units (the pass body, the family-pattern files, and
// `lib/Lower/Lower.cpp` for the public-surface forwarding). No
// public consumer includes it; public consumers go through
// `include/nsl/Lower/Lower.h`.

#ifndef NSL_LOWER_PASS_NSL_TO_CIRCT_PASS_H
#define NSL_LOWER_PASS_NSL_TO_CIRCT_PASS_H

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/DialectConversion.h"

#include <memory>

namespace nsl::lower {

class CIRCTTypeConverter;

// Per-family populate helpers. Each family file under
// `lib/Lower/Pass/CIRCTPatterns/` exposes one. Called in
// alphabetic order by `NSLToCIRCTPass::runOnOperation` per
// research.md §11 (deterministic registration).
void populateArithPatterns(mlir::RewritePatternSet &patterns,
                           CIRCTTypeConverter &type_converter);
void populateBitOpPatterns(mlir::RewritePatternSet &patterns,
                           CIRCTTypeConverter &type_converter);
void populateControlPatterns(mlir::RewritePatternSet &patterns,
                             CIRCTTypeConverter &type_converter);
void populateFSMPatterns(mlir::RewritePatternSet &patterns,
                         CIRCTTypeConverter &type_converter);
void populateModulePatterns(mlir::RewritePatternSet &patterns,
                            CIRCTTypeConverter &type_converter);
void populateParamPatterns(mlir::RewritePatternSet &patterns,
                           CIRCTTypeConverter &type_converter);
void populatePortPatterns(mlir::RewritePatternSet &patterns,
                          CIRCTTypeConverter &type_converter);
void populateSimPatterns(mlir::RewritePatternSet &patterns,
                         CIRCTTypeConverter &type_converter);
void populateStatePatterns(mlir::RewritePatternSet &patterns,
                           CIRCTTypeConverter &type_converter);

// M6 Phase 4 (US2): structural rewrite that materialises every
// `nsl::dialect::ModuleOp` into a `circt::hw::HWModuleOp` with the
// port list derived from the paired `nsl::dialect::DeclareOp`.
// Implemented in `CIRCTPatterns/ModulePatterns.cpp`; called from
// `NSLToCIRCTPass::runOnOperation` BEFORE `applyFullConversion`.
//
// Per `specs/010-m6-circt-lowering/contracts/circt-lowering.contract.md`
// §3 the rewrite is non-pattern-driven (a manual walk) because the
// dual-placement port-info-op design (M4 amendment #9) requires
// coordinated in-module + in-declare consumption that the standard
// DialectConversion worklist would interleave incorrectly with
// attempts to legalize individual port-info ops.
mlir::LogicalResult lowerNSLModulesToHWModules(mlir::ModuleOp parentModule);

// M6 Phase 5 (US3): FSM-lowering pre-pass. Every `nsl::dialect::ProcOp`
// (with its child `StateOp`s + `FirstStateOp` + body-region
// `GotoOp` / `FinishOp` / `CallOp` ops) becomes a top-level
// `circt::fsm::MachineOp` sibling of the enclosing `hw::HWModuleOp`.
// Every `nsl::dialect::FuncOp` containing a single `nsl::dialect::SeqOp`
// becomes a `fsm::MachineOp` with auto-generated `seq_N` states.
// Implemented in `CIRCTPatterns/FSMPatterns.cpp`; called from
// `NSLToCIRCTPass::runOnOperation` BEFORE `applyFullConversion`,
// AFTER `lowerNSLModulesToHWModules`.
//
// Per `specs/010-m6-circt-lowering/contracts/circt-lowering.contract.md`
// §6 the rewrite is non-pattern-driven (manual walk) because the
// proc → state → goto hierarchy requires coordinated visit-
// children-before-parent semantics that the standard
// DialectConversion worklist would interleave incorrectly with
// attempts to legalize state / goto ops independently.
mlir::LogicalResult lowerNSLProcsToFSMMachines(mlir::ModuleOp parentModule);

} // namespace nsl::lower

#endif // NSL_LOWER_PASS_NSL_TO_CIRCT_PASS_H
