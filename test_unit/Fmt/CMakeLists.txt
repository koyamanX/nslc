# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test_unit/Fmt/CMakeLists.txt — gtest suites for `libNslFmt.a`.
#
# Per Constitution Principle VIII (TDD), each suite's test fixtures
# are committed RED before the matching implementation lands. The
# `EXISTS` guard in `test_unit/CMakeLists.txt` permits the suite
# directory to appear in the foreach list before any tests exist.
#
# Suite layout: each test binary lives directly under `test_unit/Fmt/`
# (no per-binary sub-directory), declared by an `add_executable()`
# below. This deviates from the per-suite-directory convention
# (`test_unit/source_manager_test/`) used by the M0 layers because
# the `nsl-fmt` library will ship multiple small focused test
# binaries (directive-splitter, doc-layout, format-parity, diff-
# emitter, config-parser, perf-smoke); grouping them under one
# `Fmt/` umbrella keeps the test_unit tree shallow.
#
# Phase 2a (T011–T015): `directive_splitter_test` exercises the
# `DirectiveSplitter` pre-pass. Authored RED before
# `lib/Fmt/DirectiveSplitter.cpp` lands its real body — the binary
# FAILS TO LINK or produces FAILED test output until that source
# file is implemented.
#
# Phase 2b/2c (T016–T029): adds `doc_layout_test`. Phases 4–6 add
# `diff_emitter_test`, `format_parity_test`, `config_parser_test`.
# Phase 7 adds `perf_smoke_test`. Each follows the same shape.

include(GoogleTest)

# -----------------------------------------------------------------------------
# Phase 2a — Directive splitter (T011, T012, T013)
# -----------------------------------------------------------------------------
add_executable(directive_splitter_test
  directive_splitter_test.cpp)

target_link_libraries(directive_splitter_test
  PRIVATE
    nsl-basic
    nsl-lex     # T2 Phase 2b — Lexer + Token used by CSTRoundTrip / CSTInvariants tests
    nsl-ast     # T2 Phase 2b — CompilationUnit return type
    nsl-parse   # T2 Phase 2b — parseCompilationUnit + CSTSink interface
    NslFmt
    GTest::gtest_main)

target_include_directories(directive_splitter_test
  PRIVATE
    "${CMAKE_SOURCE_DIR}/lib/Fmt")  # private header DirectiveSplitter.h

gtest_discover_tests(directive_splitter_test
  PROPERTIES
    TIMEOUT 30
    LABELS Fmt)

# Roll-up: when a future contributor adds more Fmt gtest binaries
# alongside `directive_splitter_test`, append them to the
# `_NSL_FMT_GTEST_BINARIES` global property below; the
# `check-fmt-unit` target in the project-root CMakeLists.txt picks
# them up via that property.
set_property(GLOBAL APPEND PROPERTY _NSL_FMT_GTEST_BINARIES
  directive_splitter_test)

# -----------------------------------------------------------------------------
# Phase 2c — Wadler-Leijen Doc IR + LayoutRenderer (T021, T022, T023)
# -----------------------------------------------------------------------------
add_executable(doc_layout_test
  doc_layout_test.cpp)

target_link_libraries(doc_layout_test
  PRIVATE
    NslFmt
    GTest::gtest_main)

target_include_directories(doc_layout_test
  PRIVATE
    "${CMAKE_SOURCE_DIR}/lib/Fmt")  # private headers Doc.h, LayoutRenderer.h

gtest_discover_tests(doc_layout_test
  PROPERTIES
    TIMEOUT 30
    LABELS Fmt)

set_property(GLOBAL APPEND PROPERTY _NSL_FMT_GTEST_BINARIES
  doc_layout_test)
