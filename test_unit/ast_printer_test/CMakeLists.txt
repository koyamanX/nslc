# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test_unit/ast_printer_test/CMakeLists.txt — gtest suite for the
# AST printer's determinism + format invariants (M2 Phase 2: T012).
# Asserts contracts/ast-stability.contract.md Invariants 2/3/6/8.

include(GoogleTest)

add_executable(ast_printer_test
  determinism_test.cpp)

target_link_libraries(ast_printer_test
  PRIVATE
    nsl-basic
    nsl-ast
    GTest::gtest_main)

gtest_discover_tests(ast_printer_test
  PROPERTIES TIMEOUT 30)
