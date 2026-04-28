# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test_unit/sema_lifecycle_test/CMakeLists.txt — gtest suite for the
# `Sema` engine lifecycle invariants (M3 Phase 2: T011 + T013).
# Asserts `contracts/sema-api.contract.md` Invariants 3 / 6 / 7
# (single public entry point; ownership transfer; DiagnosticEngine
# is the sole diagnostic surface).

include(GoogleTest)

add_executable(sema_lifecycle_test
  ownership_test.cpp)

target_link_libraries(sema_lifecycle_test
  PRIVATE
    nsl-basic
    nsl-ast
    nsl-sema
    GTest::gtest_main)

gtest_discover_tests(sema_lifecycle_test
  PROPERTIES TIMEOUT 30)
