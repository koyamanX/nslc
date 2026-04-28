# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test_unit/symbol_table_test/CMakeLists.txt — gtest suite for the
# `SymbolTable` scope-stack invariants (M3 Phase 2: T005 + T007).
# Asserts the `enterScope`/`leaveScope`/`declare`/`lookup`/
# `lookupScoped` API surface defined by `data-model.md` §2 and
# `contracts/sema-api.contract.md` Invariant 1.

include(GoogleTest)

add_executable(symbol_table_test
  scope_stack_test.cpp)

target_link_libraries(symbol_table_test
  PRIVATE
    nsl-basic
    nsl-ast
    nsl-sema
    GTest::gtest_main)

gtest_discover_tests(symbol_table_test
  PROPERTIES TIMEOUT 30)
