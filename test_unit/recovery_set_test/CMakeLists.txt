# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test_unit/recovery_set_test/CMakeLists.txt — gtest suite for the
# Pratt precedence table (M2 Phase 2: T017) and the parser
# recovery-set primitives (M2 Phase 5: T053–T054). All three sources
# reach into private headers under `lib/Parse/` (PrecedenceTable.h,
# Recovery.h, ParserImpl.h) via the `target_include_directories(...
# PRIVATE ${CMAKE_SOURCE_DIR}/lib/Parse)` line below — no public API
# surface is created prematurely.
#
# Phase 5 / US3 additions (per `specs/005-m2-parser/tasks.md`):
#   - `recovery_partial_ast_test.cpp` (T053) — partial-AST
#     preservation under recovery; exercises the public
#     `nsl::parse::parseCompilationUnit` entry over malformed inputs.
#   - `recovery_set_test.cpp` (T054) — `TokenSet` + `skipUntil()` +
#     `RecoveryGuard` primitive surface (private header
#     `lib/Parse/Recovery.h`, authored by Track L at T056).
#
# **FR-029 RED state**: against parent commit `39cc618`,
# `lib/Parse/Recovery.h` does NOT exist, so the recovery_set_test.cpp
# translation unit FAILS TO COMPILE. The recovery_partial_ast_test.cpp
# COMPILES (the public Parser.h surface is stable) but FAILS at
# runtime — the partial-AST assertions break under the parent's
# single-error-bail behavior.

include(GoogleTest)

add_executable(recovery_set_test
  precedence_table_test.cpp
  recovery_partial_ast_test.cpp
  recovery_set_test.cpp)

target_include_directories(recovery_set_test
  PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/Parse)

# Link order: the M2 Phase 5 additions reference the public AST node
# types (`CompilationUnit`, `ModuleBlock`, `WireDecl`) and the public
# parser entry point. `nsl-preprocess` is pulled in transitively but
# listed for link-edge clarity (mirrors `parse_test`'s pattern).
target_link_libraries(recovery_set_test
  PRIVATE
    nsl-basic
    nsl-lex
    nsl-preprocess
    nsl-ast
    nsl-parse
    GTest::gtest_main)

gtest_discover_tests(recovery_set_test
  PROPERTIES TIMEOUT 30)
