// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Dialect/NSL/IR/NSLDialect.cpp — dialect-init code for
// `nsl-dialect` (M4, layer 7).
//
// **Specification anchors**:
//   - `specs/007-m4-mlir-dialect/spec.md` FR-006 — registration
//     entry-point exported in the umbrella.
//   - `specs/007-m4-mlir-dialect/contracts/dialect-api.contract.md`
//     §3 — `registerNSLDialect` is idempotent and void-returning.
//   - `specs/007-m4-mlir-dialect/contracts/dialect-stability.contract.md`
//     §7 — calling twice on the same registry is a no-op.
//   - `specs/007-m4-mlir-dialect/data-model.md` §1 — entity catalog.
//
// At Phase 2 the dialect's `addOperations<>` and `addTypes<>` lists
// are empty; Phase 3 US1 (T084) extends `initialize()` to register
// the 40 ops + 3 types via the TableGen-generated `GET_OP_LIST` /
// `GET_TYPEDEF_LIST` macros.

#include "nsl/Dialect/NSL/IR/NSLDialect.h"

#include "mlir/IR/DialectRegistry.h"

// TableGen-generated dialect-class definitions (the
// `NSLDialect::Impl` accessors and the dialect-namespace metadata).
// Private build artifact emitted by `add_mlir_dialect(NSLOps nsl)`.
// Bare-basename include resolves via the
// `${CMAKE_CURRENT_BINARY_DIR}` PUBLIC include path on `nsl-dialect`.
#include "NSLOpsDialect.cpp.inc"

namespace nsl::dialect {

void NSLDialect::initialize() {
  // Phase 3 US1 (T084): addOperations<...>() over the 40 ops via
  // `#define GET_OP_LIST` from `NSLOps.cpp.inc`.
  // Phase 3 US1 (T084): addTypes<...>() over the 3 types via
  // `#define GET_TYPEDEF_LIST` from `NSLTypes.cpp.inc`.
}

void registerNSLDialect(mlir::DialectRegistry &registry) {
  // `DialectRegistry::insert<T>` is idempotent by TypeID — calling
  // twice on the same registry is a no-op (the second call observes
  // an existing entry and returns silently). This satisfies the
  // `dialect-stability.contract.md` §7 invariant exercised by
  // `test_unit/dialect_register_test/idempotency_test.cc`.
  registry.insert<NSLDialect>();
}

} // namespace nsl::dialect
