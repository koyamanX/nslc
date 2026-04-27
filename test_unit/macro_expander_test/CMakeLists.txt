# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test_unit/macro_expander_test/CMakeLists.txt — gtest suite
# scaffolding for the MacroExpander (data-model entity 1 of
# specs/003-macro-textual-concat/data-model.md). Suite populated
# by Phase 3 (T006) — basic textual-substitution cases — and Phase
# 5 (T021) — recursion + cycle detection cases. The guarded-add
# pattern below keeps the build green during Phase 1+2 (no C++
# source files yet) while the foreach loop in
# `test_unit/CMakeLists.txt` already lists this suite.

set(_macro_expander_sources
  macro_expander_test.cpp)

set(_have_sources TRUE)
foreach(_src IN LISTS _macro_expander_sources)
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
    set(_have_sources FALSE)
    break()
  endif()
endforeach()

if(_have_sources)
  include(GoogleTest)
  add_executable(macro_expander_test ${_macro_expander_sources})
  # MacroExpander lives in `nsl-preprocess`. Link it.
  target_link_libraries(macro_expander_test
    PRIVATE
      nsl-basic
      nsl-preprocess
      GTest::gtest_main)
  gtest_discover_tests(macro_expander_test
    PROPERTIES TIMEOUT 30)
endif()
