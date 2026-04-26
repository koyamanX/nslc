# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test_unit/ci_yml_test/CMakeLists.txt — TDD seed for the GitHub
# Actions workflow shape (FR-013, FR-014, FR-015, spec Q2, research §7).
# Lands BEFORE T050 (.github/workflows/ci.yml).

add_test(
  NAME ci_yml_shape
  COMMAND ${Python3_EXECUTABLE} -m pytest -v
    ${CMAKE_CURRENT_SOURCE_DIR}/test_ci_yml.py)
set_tests_properties(ci_yml_shape PROPERTIES
  ENVIRONMENT "NSLC_REPO_ROOT=${CMAKE_SOURCE_DIR}")
