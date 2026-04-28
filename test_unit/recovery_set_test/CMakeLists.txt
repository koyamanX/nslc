# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test_unit/recovery_set_test/CMakeLists.txt — gtest suite for the
# Pratt precedence table (M2 Phase 2: T017). The table is a private
# header `lib/Parse/PrecedenceTable.h` consumed only by Phase-3's
# ParseExpr.cpp; the test reaches into the private header directly
# via `target_include_directories(... PRIVATE ${CMAKE_SOURCE_DIR}/lib/Parse)`
# so no public-API surface is created prematurely. Track B's test
# integration concern #4 is satisfied here (NOT in test_unit/CMakeLists.txt
# at the parent level).

include(GoogleTest)

add_executable(recovery_set_test
  precedence_table_test.cpp)

target_include_directories(recovery_set_test
  PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/Parse)

target_link_libraries(recovery_set_test
  PRIVATE
    nsl-basic
    nsl-lex
    GTest::gtest_main)

gtest_discover_tests(recovery_set_test
  PROPERTIES TIMEOUT 30)
