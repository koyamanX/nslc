# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test_unit/branch_protection_test/CMakeLists.txt — TDD seed for the
# branch-protection JSON shape (FR-016, spec Q3). Lands BEFORE T051.

add_test(
  NAME branch_protection_shape
  COMMAND ${Python3_EXECUTABLE} -m pytest -v
    ${CMAKE_CURRENT_SOURCE_DIR}/test_protection_json.py)
set_tests_properties(branch_protection_shape PROPERTIES
  ENVIRONMENT "NSLC_REPO_ROOT=${CMAKE_SOURCE_DIR}")
