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
// At Phase 3 (T070–T072) the three type records ship in NSLTypes.td;
// the TableGen-generated storage/accessor classes are pulled in here
// via `GET_TYPEDEF_CLASSES`. The default printer/parser is wired by
// `useDefaultTypePrinterParser = 1` on the dialect (per FR-008 +
// research §5) — no hand-written `Type::print` / `Type::parse`
// needed.

// Type-class definitions (`!nsl.bits<N>` / `!nsl.struct<@T>` /
// `!nsl.mem<[D x T]>` storage + member fns) MOVED to
// `NSLDialect.cpp` so `addTypes<>()` has complete Storage classes
// for its template instantiation. This file is intentionally
// empty (kept as a build-target sentinel; `add_nsl_library` lists
// it).
#include "nsl/Dialect/NSL/IR/NSLDialect.h"
