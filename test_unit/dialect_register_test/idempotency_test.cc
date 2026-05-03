// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/dialect_register_test/idempotency_test.cc
//
// TDD fixtures (M4 Phase 2, T020) for `nsl::dialect::registerNSLDialect`.
//
// **Specification anchors**:
//   - `specs/007-m4-mlir-dialect/spec.md` FR-006: registration entry-
//     point exported in the umbrella public header.
//   - `specs/007-m4-mlir-dialect/contracts/dialect-stability.contract.md`
//     §7: calling `registerNSLDialect(registry)` twice on the same
//     `mlir::DialectRegistry` is a no-op.
//   - `specs/007-m4-mlir-dialect/contracts/dialect-api.contract.md`
//     §3: registration entry-point contract — idempotent, thread-safe
//     at the call site, returns void.
//
// **TDD evidence (Principle VIII NON-NEGOTIABLE)**: this file is
// authored before `lib/Dialect/NSL/IR/NSLDialect.cpp` lands a real
// `registerNSLDialect` body. Against the unchanged tree the
// translation unit FAILS TO LINK because `nsl::dialect::registerNSLDialect`
// is an unresolved symbol. The expected red→green observation is:
//   1. Author this file + `CMakeLists.txt` → run `./scripts/ci.sh
//      unit-tests` → observe link error (red).
//   2. T004–T006 land the dialect class + registration body → re-run
//      → observe pass (green).

#include "mlir/IR/DialectRegistry.h"
#include "nsl/Dialect/NSL/IR/NSLDialect.h"

#include <gtest/gtest.h>

namespace {

// ---------------------------------------------------------------
// Single registration succeeds; the registry observes the dialect.
// ---------------------------------------------------------------

TEST(DialectRegisterTest, SingleRegistrationSucceeds) {
  mlir::DialectRegistry registry;
  nsl::dialect::registerNSLDialect(registry);

  // The registry's `getRegisteredDialectNames()` must report the
  // `nsl` dialect.
  bool found = false;
  for (llvm::StringRef name : registry.getRegisteredDialectNames()) {
    if (name == "nsl") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found) << "registerNSLDialect did not insert 'nsl' into "
                        "the dialect registry.";
}

// ---------------------------------------------------------------
// Idempotency: calling registerNSLDialect twice on the same registry
// is a no-op (the dialect set has size 1 after the second call).
// ---------------------------------------------------------------

TEST(DialectRegisterTest, RegistrationIsIdempotent) {
  mlir::DialectRegistry registry;

  nsl::dialect::registerNSLDialect(registry);
  const auto names_after_first = registry.getRegisteredDialectNames();
  const std::size_t size_after_first = names_after_first.size();

  nsl::dialect::registerNSLDialect(registry);
  const auto names_after_second = registry.getRegisteredDialectNames();
  const std::size_t size_after_second = names_after_second.size();

  EXPECT_EQ(size_after_first, size_after_second)
      << "Calling registerNSLDialect twice on the same registry must "
         "be a no-op (idempotency contract; "
         "dialect-stability.contract.md §7).";
}

} // namespace
