// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/Dialect/NSL/IR/NSLDialect.h — single public umbrella
// header for the `nsl-dialect` library (M4, layer 7).
//
// **Specification anchors**:
//   - `specs/007-m4-mlir-dialect/spec.md` FR-001 / FR-006: single
//     public umbrella header per Constitution Principle II §3.
//   - `specs/007-m4-mlir-dialect/contracts/dialect-api.contract.md`
//     §1 — public include path; §2 — symbol export table; §3 —
//     registration entry-point contract (idempotent, void-returning).
//   - `specs/007-m4-mlir-dialect/data-model.md` §9 — header-surface
//     invariant.
//
// Consumers (`nsl-opt`, `nsl-driver`, future M5 lowering) include
// THIS header only — never the TableGen-generated `.h.inc` files
// directly. Per Principle II §3, `nsl-dialect` is NOT an exception
// to the single-public-header rule (unlike `nsl-ast` and `nsl-sema`,
// which have explicit two-header carve-outs).
//
// At Phase 2 this header re-exports the dialect class only; op +
// type class re-exports land in Phase 3 US1 once the TableGen
// records ship.

#ifndef NSL_DIALECT_NSL_IR_NSLDIALECT_H
#define NSL_DIALECT_NSL_IR_NSLDIALECT_H

#include "mlir/IR/Dialect.h"
#include "mlir/IR/DialectRegistry.h"

// TableGen-generated dialect class declaration. Private build
// artifact emitted by `add_mlir_dialect(NSLOps nsl)` (see
// `lib/Dialect/NSL/IR/CMakeLists.txt`); this umbrella is its only
// consumer surface. The tablegen output lives in
// `${CMAKE_CURRENT_BINARY_DIR}` per upstream `mlir_tablegen`'s
// fixed-output convention; the dialect's CMakeLists adds that
// directory to `nsl-dialect`'s PUBLIC include path, so a
// bare-basename include resolves for every consumer.
#include "NSLOpsDialect.h.inc"

// Phase 3 US1 (T070–T072 + T073–T083): once the type and op
// records ship, uncomment the type-decls and op-decls includes:
//
//   #define GET_TYPEDEF_CLASSES
//   #include "NSLOpsTypes.h.inc"
//
//   #define GET_OP_CLASSES
//   #include "NSLOps.h.inc"
//
// At Phase 2 the generated headers contain no records to declare;
// including them is harmless but adds no symbols to the public
// surface.

namespace nsl::dialect {

/// Register the `nsl` MLIR dialect on the given registry.
///
/// Behavior (per
/// `contracts/dialect-stability.contract.md` §7 and
/// `contracts/dialect-api.contract.md` §3):
///   - Inserts `NSLDialect` into `registry`.
///   - Idempotent: calling twice on the same registry is a no-op
///     (MLIR's `DialectRegistry::insert<T>` already de-duplicates
///     by `TypeID`).
///   - Thread-safe at the call site (upstream MLIR contract).
///   - Returns `void`; failures (out of memory, etc.) propagate as
///     standard upstream MLIR exceptions.
///
/// Called once at startup by `nsl-opt`'s `main` (per FR-014) and by
/// the driver's `Compilation` ctor (per FR-004 + design §11 line
/// 1145; the driver-side stub bodies land at M5).
void registerNSLDialect(mlir::DialectRegistry &registry);

} // namespace nsl::dialect

#endif // NSL_DIALECT_NSL_IR_NSLDIALECT_H
