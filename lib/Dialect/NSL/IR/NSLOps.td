// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Dialect/NSL/IR/NSLOps.td — TableGen records for the 40
// `nsl.*` ops (M4, layer 7).
//
// **Specification anchors**:
//   - `specs/007-m4-mlir-dialect/spec.md` FR-009 / FR-010 — full op
//     set table (40 named ops + auto-generated terminators).
//   - `specs/007-m4-mlir-dialect/data-model.md` §2 — per-op trait set
//     and verifier-implementation style (TableGen-trait-only vs
//     hand-written body per Q2 Option B).
//   - `docs/design/nsl_compiler_design.md` §§7–10 — design-doc
//     anchor for every op record.
//
// At Phase 2 this file is scaffolding only — the 40 op records land
// in Phase 3 US1 (T073–T083) per the TDD ordering.

#ifndef NSL_OPS_TD
#define NSL_OPS_TD

include "NSLDialect.td"
include "NSLTypes.td"

// Phase 3 US1 (T073): module-level ops (NSL_ModuleOp, NSL_StructOp,
//                     NSL_SubmoduleOp, NSL_ConnectOp).
// Phase 3 US1 (T074): storage ops (NSL_RegOp, NSL_WireOp,
//                     NSL_VariableOp, NSL_MemOp).
// Phase 3 US1 (T075): control-terminal ops (NSL_FuncInOp,
//                     NSL_FuncOutOp, NSL_FuncSelfOp).
// Phase 3 US1 (T076): action-block ops (NSL_AltOp, NSL_AnyOp,
//                     NSL_IfOp, NSL_ParallelOp, NSL_SeqOp,
//                     NSL_WhileOp, NSL_ForOp).
// Phase 3 US1 (T077): action-helper ops (NSL_CaseOp, NSL_DefaultOp).
// Phase 3 US1 (T078): atomic ops (NSL_TransferOp,
//                     NSL_ClockedTransferOp, NSL_IncDecOp,
//                     NSL_CallOp, NSL_FinishOp, NSL_FinishMethodOp,
//                     NSL_InvokeMethodOp).
// Phase 3 US1 (T079): procedure ops (NSL_ProcOp, NSL_FirstStateOp,
//                     NSL_StateOp, NSL_FuncOp).
// Phase 3 US1 (T080): procedure-helper op (NSL_GotoOp).
// Phase 3 US1 (T081): system-task ops (NSL_SimDisplayOp,
//                     NSL_SimFinishOp, NSL_SimInitOp,
//                     NSL_SimDelayOp).
// Phase 3 US1 (T082): marker ops (NSL_FireProbeOp,
//                     NSL_StructCastOp, NSL_FieldOp).
// Phase 3 US1 (T083): expansion-only op (NSL_StructuralGenerateOp).

#endif // NSL_OPS_TD
