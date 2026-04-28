# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test_unit/ast_visitor_test/CMakeLists.txt — gtest suite for the
# AST node base and visitor exhaustiveness (M2 Phase 2: T005, T007).
# Tests authored first per Constitution Principle VIII; Track B
# committed them in TDD-RED state, Track A's nsl-ast library makes
# them green.

include(GoogleTest)

add_executable(ast_visitor_test
  ast_node_construct_test.cpp
  visitor_exhaustiveness_test.cpp)

target_link_libraries(ast_visitor_test
  PRIVATE
    nsl-basic
    nsl-ast
    GTest::gtest_main)

gtest_discover_tests(ast_visitor_test
  PROPERTIES TIMEOUT 30)
