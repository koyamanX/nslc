# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test_unit/spdx_check_test/CMakeLists.txt — registers the pytest
# suite for scripts/check_spdx.py per contract `spdx-check.contract.md`.
# The fixture inputs live under fixtures/ in this directory; the test
# functions live in scripts/check_spdx_test.py (alongside the script
# they exercise).

add_test(
  NAME spdx_check_tests
  COMMAND ${Python3_EXECUTABLE} -m pytest -v
    ${CMAKE_SOURCE_DIR}/scripts/check_spdx_test.py)
set_tests_properties(spdx_check_tests PROPERTIES
  ENVIRONMENT "NSLC_REPO_ROOT=${CMAKE_SOURCE_DIR}")
