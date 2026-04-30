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
//     utilities (`emitParentMismatch`, `isRegLikeValue`, etc.).
//   - `specs/007-m4-mlir-dialect/research.md` §4 — verifier-impl
//     language (hand-written C++ in this file).
//
// At Phase 2 this file is empty scaffolding — the TableGen records
// have not been authored yet (Phase 3 US1, T073–T083), so the
// generated `NSLOps.cpp.inc` is empty and there is nothing to pull
// in via `#define GET_OP_CLASSES`. Once T073–T083 land, the verifier
// bodies (Phase 4 US2, T100–T118) populate the namespace below.

#include "nsl/Dialect/NSL/IR/NSLDialect.h"

namespace {

// Verifier helpers (Phase 4 US2, T098). Reserved for the ~5
// transitive-parent verifiers per Q2 Option B and the reg-like-value
// helper used by `nsl.clocked_transfer` / `nsl.incdec`. F9 carry-over
// (Pass 4 of `/speckit-analyze`): `op->getParentOfType<T>()` is the
// upstream MLIR helper that subsumes any `findAncestorOfKind<T>`
// candidate; Phase 4 verifiers MUST use the upstream helper, NOT a
// hand-rolled equivalent.
//
//   mlir::LogicalResult emitParentMismatch(
//       mlir::Operation *op, llvm::StringRef expectedKind);
//   bool isRegLikeValue(mlir::Value v);

} // namespace

// Phase 3 US1 (T073–T083): once op records exist, uncomment the
// op-class definitions include. The generated file is
// `NSLOps.cpp.inc` (the .td stem matches the `add_mlir_dialect`
// dialect-file argument):
//
//   #define GET_OP_CLASSES
//   #include "NSLOps.cpp.inc"
//
// Bare-basename resolves via the `${CMAKE_CURRENT_BINARY_DIR}`
// PUBLIC include path.
