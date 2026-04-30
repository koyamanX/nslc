// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Dialect/NSL/IR/NSLTypes.td — TableGen records for the three
// `!nsl.*` dialect types (M4, layer 7).
//
// **Specification anchors**:
//   - `specs/007-m4-mlir-dialect/spec.md` FR-007 / FR-008: three
//     dialect types with default printer/parser symmetric round-trip.
//   - `specs/007-m4-mlir-dialect/data-model.md` §3 — !nsl.bits<N>,
//     !nsl.struct<@T>, !nsl.mem<[D x T]>.
//   - `specs/007-m4-mlir-dialect/research.md` §5 — interning via
//     MLIR's standard `TypeStorage`.
//
// At Phase 2 this file is scaffolding only — the three type records
// land in Phase 3 US1 (T070–T072) per the TDD ordering (round-trip
// fixtures gate the type definitions).

#ifndef NSL_TYPES_TD
#define NSL_TYPES_TD

include "NSLDialect.td"

// Phase 3 US1 (T070): NSL_BitsType — `!nsl.bits<N>`.
// Phase 3 US1 (T071): NSL_StructType — `!nsl.struct<@T>`.
// Phase 3 US1 (T072): NSL_MemType — `!nsl.mem<[D x T]>`.

#endif // NSL_TYPES_TD
