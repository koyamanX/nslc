# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test_unit/ci_sh_test/CMakeLists.txt — TDD seed for scripts/ci.sh
# (FR-015, FR-017, FR-021, SC-006). Lands BEFORE T046.

add_test(
  NAME ci_sh_dispatch
  COMMAND ${Python3_EXECUTABLE} -m pytest -v
    ${CMAKE_CURRENT_SOURCE_DIR}/test_dispatch.py)
set_tests_properties(ci_sh_dispatch PROPERTIES
  ENVIRONMENT "NSLC_REPO_ROOT=${CMAKE_SOURCE_DIR}")
