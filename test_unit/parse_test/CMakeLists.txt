# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test_unit/parse_test/CMakeLists.txt — gtest suite for the
# `nsl-parse` library's public entry point
# `nsl::parse::parseCompilationUnit` (M2 Phase 3 / US1; T021 of
# `specs/005-m2-parser/tasks.md`). Tests authored first per
# Constitution Principle VIII (FR-029); against the unchanged tree
# at `7bc1aea` this binary FAILS TO LINK because Track D has not
# yet authored `lib/Parse/Parser.cpp`.

include(GoogleTest)

add_executable(parse_test
  parser_smoke_test.cpp)

# Link against every layer the smoke fixture exercises:
#   - `nsl-basic`  — SourceManager, DiagnosticEngine
#   - `nsl-lex`    — Lexer (M1)
#   - `nsl-preprocess` — pulled in transitively but listed for
#                  link-edge clarity (M1 Preprocessor surface)
#   - `nsl-ast`    — CompilationUnit / ModuleBlock / RegDecl /
#                  NodeKind types referenced by the assertions
#   - `nsl-parse`  — parseCompilationUnit() — TARGET DOES NOT EXIST
#                  on the unchanged tree at `7bc1aea`. Once Track D
#                  lands `lib/Parse/CMakeLists.txt`'s
#                  `add_nsl_library(nsl-parse ...)` invocation, this
#                  link line resolves and the binary builds.
target_link_libraries(parse_test
  PRIVATE
    nsl-basic
    nsl-lex
    nsl-preprocess
    nsl-ast
    nsl-parse
    GTest::gtest_main)

gtest_discover_tests(parse_test
  PROPERTIES TIMEOUT 30)
