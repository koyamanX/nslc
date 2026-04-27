# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test_unit/source_manager_test/CMakeLists.txt — gtest suite for
# data-model entities 1–5 (SourceLocation, SourceRange, FileID,
# Buffer, SourceManager) plus the M1 Phase 3 KeywordSet recognizer
# (T024). Tests written first per Constitution Principle VIII (T006
# / T008 / T024 of M1 tasks.md).

include(GoogleTest)

add_executable(source_manager_test
  source_location_test.cpp
  source_manager_test.cpp
  keyword_set_test.cpp)

# `keyword_set_test.cpp` includes `nsl/Lex/KeywordSet.h` + `Token.h`
# and references `nsl::classifyKeyword(...)` from `nsl-lex`. Until
# the parallel nsl-frontend-impl agent's lexer headers + library
# land, this binary FAILS TO LINK — that is the Constitution
# Principle VIII RED state (FR-036).
target_link_libraries(source_manager_test
  PRIVATE
    nsl-basic
    nsl-lex
    GTest::gtest_main)

# Discover individual TEST cases as separate ctest entries so a
# failed assertion names the offending fixture without requiring
# scrollback inspection.
gtest_discover_tests(source_manager_test
  PROPERTIES TIMEOUT 30)
