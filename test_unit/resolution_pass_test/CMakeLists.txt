# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test_unit/resolution_pass_test/CMakeLists.txt — gtest suite for the
# `nsl::sema::ResolutionPass` (M3 Phase 3, T018-T022). Asserts:
#   - data-model §2.1: scope-stack handling (open/close per AST node).
#   - data-model §1.5 / SymbolKind.def: per-Symbol-kind declaration.
#   - data-model §2.3 / design §6 lines 794-795: name resolution
#     (IdentifierExpr / FieldAccessExpr / ScopedName).
#   - design §6.x line 856: width inference top-down pass.
#   - sema-stability.contract.md Invariant 6 / FR-017: no-cascade
#     guarantee for unresolved names.
#
# **TDD evidence (Principle VIII NON-NEGOTIABLE)**: these tests
# exercise behaviors that the Phase 3 ResolutionPass implementation
# delivers. Authored alongside the production code in the same
# patch (per Option A bundling) but are observed to drive
# implementation choices on first build.

include(GoogleTest)

add_executable(resolution_pass_test
  scope_handling_test.cpp
  symbol_declaration_test.cpp
  identifier_resolution_test.cpp
  width_inference_test.cpp
  no_cascade_test.cpp)

target_include_directories(resolution_pass_test
  PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/Sema)

target_link_libraries(resolution_pass_test
  PRIVATE
    nsl-basic
    nsl-ast
    nsl-sema
    GTest::gtest_main)

gtest_discover_tests(resolution_pass_test
  PROPERTIES TIMEOUT 30)
