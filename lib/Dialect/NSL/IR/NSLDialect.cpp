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
// At Phase 3 (US1, T084) `initialize()` registers the 41 ops + 3
// types + the `IncDecKind` enum-attr via the TableGen-generated
// `GET_OP_LIST` / `GET_TYPEDEF_LIST` / `GET_ATTRDEF_LIST` macros.

#include "nsl/Dialect/NSL/IR/NSLDialect.h"

#include "mlir/IR/Builders.h"
#include "mlir/IR/DialectImplementation.h"
#include "mlir/IR/DialectRegistry.h"
#include "mlir/IR/OpImplementation.h"
#include "llvm/ADT/TypeSwitch.h"

// TableGen-generated dialect-class definitions (the
// `NSLDialect::Impl` accessors and the dialect-namespace metadata).
#include "NSLOpsDialect.cpp.inc"

// Enum-attr definitions (`IncDecKind` for `nsl.incdec`).
#include "NSLOpsEnums.cpp.inc"

// Attr-class definitions (`IncDecKindAttr` storage + member fns).
// MUST be in this TU (and NOT in NSLOps.cpp) so `addAttributes<>()`
// in `initialize()` has the complete `IncDecKindAttrStorage` type
// for its template instantiation.
#define GET_ATTRDEF_CLASSES
#include "NSLOpsAttrDefs.cpp.inc"

// Type-class definitions (`!nsl.bits<N>`, `!nsl.struct<@T>`,
// `!nsl.mem<[D x T]>` storage + member fns). Same rationale as
// the attr block above — `addTypes<>()` in `initialize()` needs
// complete Storage classes.
#define GET_TYPEDEF_CLASSES
#include "NSLOpsTypes.cpp.inc"

// Op-class definitions (constructors, accessors, parser, printer
// for the 41 named ops). `verify()` member functions are
// hand-written stubs in NSLOps.cpp (Phase 4 US2 fills them with
// real structural-invariant checks).
#define GET_OP_CLASSES
#include "NSLOps.cpp.inc"

namespace nsl::dialect {

void NSLDialect::initialize() {
  addOperations<
#define GET_OP_LIST
#include "NSLOps.cpp.inc"
      >();
  addTypes<
#define GET_TYPEDEF_LIST
#include "NSLOpsTypes.cpp.inc"
      >();
  addAttributes<
#define GET_ATTRDEF_LIST
#include "NSLOpsAttrDefs.cpp.inc"
      >();
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
