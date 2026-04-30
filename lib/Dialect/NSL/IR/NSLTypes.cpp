// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Dialect/NSL/IR/NSLTypes.cpp — dialect type-class glue for
// `nsl-dialect` (M4, layer 7).
//
// **Specification anchors**:
//   - `specs/007-m4-mlir-dialect/spec.md` FR-007 / FR-008 — three
//     dialect types with default printer/parser symmetric round-trip.
//   - `specs/007-m4-mlir-dialect/data-model.md` §3 — !nsl.bits<N>,
//     !nsl.struct<@T>, !nsl.mem<[D x T]>.
//
// At Phase 2 this file is empty scaffolding — the TableGen records
// have not been authored yet (Phase 3 US1, T070–T072), so the
// generated `NSLOpsTypes.cpp.inc` is empty and there is nothing to
// pull in via `#define GET_TYPEDEF_CLASSES`. Once T070–T072 land,
// extend this file to include the generated definitions.

#include "nsl/Dialect/NSL/IR/NSLDialect.h"

// Phase 3 US1 (T070–T072): once type records exist, uncomment:
//
//   #define GET_TYPEDEF_CLASSES
//   #include "NSLOpsTypes.cpp.inc"
//
// to pull in the storage-class definitions. Bare-basename resolves
// via the `${CMAKE_CURRENT_BINARY_DIR}` PUBLIC include path.
