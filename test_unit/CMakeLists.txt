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
    macro_expander_test
    # M2 Phase 2 (005-m2-parser) — gtest suites for the AST and the
    # parser's recovery-set bookkeeping. The per-suite `CMakeLists.txt`
    # + `.cpp` fixtures are authored by the parallel test-author
    # (Track B); registering the suite names here is harmless until
    # those files land thanks to the existence guard below.
    ast_visitor_test
    ast_printer_test
    recovery_set_test
    # M2 Phase 3 / US1 (T021 of `specs/005-m2-parser/tasks.md`) —
    # gtest smoke fixture for `nsl::parse::parseCompilationUnit`.
    # Authored RED before `lib/Parse/` is populated; the binary
    # FAILS TO LINK until Track D lands the `nsl-parse` library.
    parse_test
    # M3 Phase 2 (006-m3-sema, T015) — gtest suites covering the
    # `nsl-sema` scaffolding. `symbol_table_test` exercises
    # `enterScope`/`leaveScope`/`declare`/`lookup` per data-model §2;
    # `type_system_test` exercises interning per Invariant 5 of
    # `sema-api.contract.md`; `sema_lifecycle_test` exercises
    # `Sema::run()` ownership transfer per Invariant 6. The
    # `resolution_pass_test` and `constructive_sn_test` suite names
    # are reserved for Phase 3 (US1) and Phase 4 (US2) respectively;
    # the `EXISTS` guard below skips them at configure time until
    # those phases land per-suite `CMakeLists.txt` files.
    symbol_table_test
    type_system_test
    sema_lifecycle_test
    resolution_pass_test
    constructive_sn_test)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${suite}/CMakeLists.txt")
    add_subdirectory(${suite})
  endif()
endforeach()
