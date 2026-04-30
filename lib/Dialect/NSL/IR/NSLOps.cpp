// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Dialect/NSL/IR/NSLOps.cpp — dialect op-class glue and
// hand-written verifier bodies for `nsl-dialect` (M4, layer 7).
//
// **Specification anchors**:
//   - `specs/007-m4-mlir-dialect/spec.md` FR-009–FR-013 — full op
//     set + structural-invariant verifier table (Q1 Option A:
//     structural-only; Q2 Option B: hand-walk for transitive parents).
//   - `specs/007-m4-mlir-dialect/data-model.md` §2 — per-op trait
//     set and verifier-implementation style.
//   - `specs/007-m4-mlir-dialect/data-model.md` §4 — verifier-helper
//     utilities (subsumed by upstream `op->getParentOfType<T>()` per
//     F9 carry-over from Pass 4 of `/speckit-analyze`).
//   - `specs/007-m4-mlir-dialect/research.md` §4 — verifier-impl
//     language (hand-written C++ in this file).
//
// At Phase 3 (US1) the verifier bodies are TRIVIAL STUBS that return
// `success()` — the round-trip fixtures don't exercise them. Phase 4
// (US2, T100–T118) fills the bodies with structural-invariant checks.

#include "nsl/Dialect/NSL/IR/NSLDialect.h"

#include "mlir/IR/Builders.h"
#include "mlir/IR/DialectImplementation.h"
#include "mlir/IR/OpImplementation.h"

#include "llvm/ADT/TypeSwitch.h"

// Pull in the TableGen-generated `IncDecKind` enum-attr definitions
// before the op-class definitions, since `IncDecOp` references the
// enum in its argument list.
#include "NSLOpsEnums.cpp.inc"

#define GET_ATTRDEF_CLASSES
#include "NSLOpsAttrDefs.cpp.inc"

#define GET_OP_CLASSES
#include "NSLOps.cpp.inc"

// ---------------------------------------------------------------------------
// Phase 3 US1 verifier stubs — every op's `verify()` returns success.
// Phase 4 US2 (T100–T118) replaces these with the real structural-
// invariant checks (parent-op kind, region count + kind, attribute
// presence/type, operand-result type relations per FR-013).
// ---------------------------------------------------------------------------

namespace nsl::dialect {

mlir::LogicalResult ModuleOp::verify() { return mlir::success(); }
mlir::LogicalResult StructOp::verify() { return mlir::success(); }
mlir::LogicalResult ConnectOp::verify() { return mlir::success(); }
mlir::LogicalResult RegOp::verify() { return mlir::success(); }
mlir::LogicalResult WireOp::verify() { return mlir::success(); }
mlir::LogicalResult VariableOp::verify() { return mlir::success(); }
mlir::LogicalResult MemOp::verify() { return mlir::success(); }
mlir::LogicalResult AltOp::verify() { return mlir::success(); }
mlir::LogicalResult AnyOp::verify() { return mlir::success(); }
mlir::LogicalResult WhileOp::verify() { return mlir::success(); }
mlir::LogicalResult ForOp::verify() { return mlir::success(); }
mlir::LogicalResult ClockedTransferOp::verify() { return mlir::success(); }
mlir::LogicalResult IncDecOp::verify() { return mlir::success(); }
mlir::LogicalResult CallOp::verify() { return mlir::success(); }
mlir::LogicalResult FinishOp::verify() { return mlir::success(); }
mlir::LogicalResult ProcOp::verify() { return mlir::success(); }
mlir::LogicalResult FirstStateOp::verify() { return mlir::success(); }
mlir::LogicalResult GotoOp::verify() { return mlir::success(); }
mlir::LogicalResult FireProbeOp::verify() { return mlir::success(); }
mlir::LogicalResult StructCastOp::verify() { return mlir::success(); }
mlir::LogicalResult FieldOp::verify() { return mlir::success(); }
mlir::LogicalResult StructuralGenerateOp::verify() { return mlir::success(); }

} // namespace nsl::dialect
