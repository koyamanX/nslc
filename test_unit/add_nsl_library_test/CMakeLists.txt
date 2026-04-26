# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test_unit/add_nsl_library_test/CMakeLists.txt
#
# Each fixture is a self-contained sub-project; the test runs
# `cmake -S <fixture> -B <out>` and inspects the configure-time exit
# code. WILL_FAIL inverts the expected pass/fail relation, so a
# fixture that exercises the macro's FATAL_ERROR path passes the test
# when configure fails.
#
# Per Principle VIII, this file lands BEFORE cmake/AddNSLLibrary.cmake's
# real implementation (T026). Until T026 lands the macro is the
# Phase-2 stub that always succeeds without registering anything; that
# means the four expected-failure fixtures (unknown_layer_name,
# upward_dep, sibling_bypass) will configure cleanly against the stub
# and the WILL_FAIL flag will (correctly) flag the tests as failing.
# Confirming this red state IS the point of the test-first commit.

function(add_nsllibrary_fixture NAME)
  cmake_parse_arguments(ARG "WILL_FAIL" "" "" ${ARGN})
  set(_fixture_src "${CMAKE_CURRENT_SOURCE_DIR}/${NAME}")
  set(_fixture_bin "${CMAKE_CURRENT_BINARY_DIR}/${NAME}-build")
  add_test(
    NAME add_nsl_library/${NAME}
    COMMAND
      ${CMAKE_COMMAND}
        -S "${_fixture_src}"
        -B "${_fixture_bin}"
        -DNSLC_CMAKE_DIR=${CMAKE_SOURCE_DIR}/cmake)
  if(ARG_WILL_FAIL)
    set_tests_properties(add_nsl_library/${NAME}
      PROPERTIES WILL_FAIL TRUE)
  endif()
endfunction()

# Pass cases — macro accepts the call, configure exits 0.
add_nsllibrary_fixture(valid_layer_name)
add_nsllibrary_fixture(downward_dep)
add_nsllibrary_fixture(multi_header_basic)
add_nsllibrary_fixture(per_node_headers_ast)

# Fail cases — macro emits FATAL_ERROR, configure exits non-zero.
add_nsllibrary_fixture(unknown_layer_name      WILL_FAIL)
add_nsllibrary_fixture(upward_dep              WILL_FAIL)
add_nsllibrary_fixture(sibling_bypass          WILL_FAIL)
