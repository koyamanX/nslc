# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test_unit/dialect_register_test/CMakeLists.txt — gtest suite for
# the `nsl::dialect::registerNSLDialect` entry-point idempotency
# invariant (M4 Phase 2, T020 + T021). Asserts the registration
# contract from `specs/007-m4-mlir-dialect/contracts/dialect-stability.contract.md`
# §7: calling the entry-point twice on the same registry is a no-op.

include(GoogleTest)

add_executable(dialect_register_test
  idempotency_test.cc)

target_link_libraries(dialect_register_test
  PRIVATE
    nsl-dialect
    MLIRIR
    MLIRSupport
    GTest::gtest_main)

gtest_discover_tests(dialect_register_test
  PROPERTIES TIMEOUT 30)
