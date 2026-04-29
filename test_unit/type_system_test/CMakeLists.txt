# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test_unit/type_system_test/CMakeLists.txt — gtest suite for the
# `TypeSystem` interning contract (M3 Phase 2: T008 + T010). Asserts
# `contracts/sema-api.contract.md` Invariant 5 (pointer equality
# implies type equality) + `sema-stability.contract.md` Invariant 3
# (interning is stable across calls).

include(GoogleTest)

add_executable(type_system_test
  interning_test.cpp)

target_link_libraries(type_system_test
  PRIVATE
    nsl-basic
    nsl-ast
    nsl-sema
    GTest::gtest_main)

gtest_discover_tests(type_system_test
  PROPERTIES TIMEOUT 30)
