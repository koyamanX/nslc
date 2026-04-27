# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test_unit/diagnostic_engine_test/CMakeLists.txt — gtest suite for
# data-model entity 6 (DiagnosticEngine): canonical text format
# (T010), smoke-only NDJSON shape (T011), sort-on-render determinism
# (T012), include-stack notes (T013). Tests written first per
# Constitution Principle VIII.

include(GoogleTest)

add_executable(diagnostic_engine_test
  diagnostic_engine_text_test.cpp
  diagnostic_engine_json_test.cpp
  diagnostic_engine_sort_test.cpp
  include_stack_test.cpp)

target_link_libraries(diagnostic_engine_test
  PRIVATE
    nsl-basic
    GTest::gtest_main)

gtest_discover_tests(diagnostic_engine_test
  PROPERTIES TIMEOUT 30)
