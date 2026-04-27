# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test_unit/helper_evaluator_test/CMakeLists.txt — gtest suite
# scaffolding for the HelperEvaluator (data-model entity 13). Suite
# populated by Phase 4 (T052): tests over the 22 closed-set helpers
# from include/nsl/Basic/HelperSet.def, edge cases per research §5,
# and the error model per research §10. The guarded-add pattern
# below keeps the build green during Phase 2 (no C++ source files
# yet) while the foreach loop in `test_unit/CMakeLists.txt` already
# lists this suite.

set(_helper_evaluator_sources
  helper_evaluator_test.cpp)

set(_have_sources TRUE)
foreach(_src IN LISTS _helper_evaluator_sources)
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
    set(_have_sources FALSE)
    break()
  endif()
endforeach()

if(_have_sources)
  include(GoogleTest)
  add_executable(helper_evaluator_test ${_helper_evaluator_sources})
  # Phase-4 wiring (T052): HelperEvaluator lives in `nsl-preprocess`.
  # The link is set up here even though `nsl-preprocess` is currently
  # a M0-anchor-TU skeleton (no real symbols) — that's the intended
  # FR-036 RED state per Constitution Principle VIII. The parallel
  # `nsl-frontend-impl` agent's HelperEvaluator.cpp lands the symbols
  # and the suite turns green.
  target_link_libraries(helper_evaluator_test
    PRIVATE
      nsl-basic
      nsl-preprocess
      GTest::gtest_main)
  gtest_discover_tests(helper_evaluator_test
    PROPERTIES TIMEOUT 30)
endif()
