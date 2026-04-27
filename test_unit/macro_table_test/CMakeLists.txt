# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test_unit/macro_table_test/CMakeLists.txt — gtest suite scaffolding
# for the MacroTable (data-model entity 11). Suite populated by
# Phase 4 (T051): tests over insertion-ordered iteration, undef /
# redefine semantics, predefined-macro insertion order. The
# guarded-add pattern below keeps the build green during Phase 2 (no
# C++ source files yet) while the foreach loop in
# `test_unit/CMakeLists.txt` already lists this suite.

set(_macro_table_sources
  macro_table_test.cpp)

set(_have_sources TRUE)
foreach(_src IN LISTS _macro_table_sources)
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
    set(_have_sources FALSE)
    break()
  endif()
endforeach()

if(_have_sources)
  include(GoogleTest)
  add_executable(macro_table_test ${_macro_table_sources})
  # Phase-4 wiring (T051): MacroTable lives in `nsl-preprocess`.
  target_link_libraries(macro_table_test
    PRIVATE
      nsl-basic
      nsl-preprocess
      GTest::gtest_main)
  gtest_discover_tests(macro_table_test
    PROPERTIES TIMEOUT 30)
endif()
