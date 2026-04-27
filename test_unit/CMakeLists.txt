# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test_unit/CMakeLists.txt — root of the GoogleTest + pytest unit
# fixtures (per research.md §1: GoogleTest is the chosen unit
# framework; pytest drives the per-script-CLI fixtures like
# scripts/check_spdx_test.py).
#
# Per-suite subdirectories arrive with US1/US2/US3 implementation
# tasks; this file only declares the shared dependencies.

find_package(GTest REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# -----------------------------------------------------------------------------
# Per-suite subdirectories
# -----------------------------------------------------------------------------
#
# US1 (T015–T040) lands `add_nsl_library_test/`.
# US2 (T041–T053) lands `determinism_test/`, `ci_sh_test/`,
#                       `ci_yml_test/`, `branch_protection_test/`.
# US3 (T054–T067) lands `spdx_check_test/`.

# A future implementer can add an `add_subdirectory(...)` line here
# alongside any new suite. No top-level CMakeLists.txt edit needed.
foreach(suite
    add_nsl_library_test
    determinism_test
    ci_sh_test
    ci_yml_test
    branch_protection_test
    spdx_check_test
    source_manager_test
    diagnostic_engine_test
    macro_table_test
    helper_evaluator_test
    macro_expander_test)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${suite}/CMakeLists.txt")
    add_subdirectory(${suite})
  endif()
endforeach()
