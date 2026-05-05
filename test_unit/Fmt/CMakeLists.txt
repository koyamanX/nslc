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

# -----------------------------------------------------------------------------
# Phase 3b — LCS-based unified-diff emitter (T073, T074, T075)
# -----------------------------------------------------------------------------
add_executable(diff_emitter_test
  diff_emitter_test.cpp)

target_link_libraries(diff_emitter_test
  PRIVATE
    NslFmt
    GTest::gtest_main)

target_include_directories(diff_emitter_test
  PRIVATE
    "${CMAKE_SOURCE_DIR}/lib/Fmt")  # private header Diff.h

gtest_discover_tests(diff_emitter_test
  PROPERTIES
    TIMEOUT 30
    LABELS Fmt)

set_property(GLOBAL APPEND PROPERTY _NSL_FMT_GTEST_BINARIES
  diff_emitter_test)

# -----------------------------------------------------------------------------
# Phase 3c — SC-003 perf-budget gate (T118)
# -----------------------------------------------------------------------------
add_executable(perf_smoke_test
  perf_smoke_test.cpp)

target_link_libraries(perf_smoke_test
  PRIVATE
    nsl-basic
    NslFmt
    GTest::gtest_main)

gtest_discover_tests(perf_smoke_test
  PROPERTIES
    TIMEOUT 60
    LABELS Fmt)

set_property(GLOBAL APPEND PROPERTY _NSL_FMT_GTEST_BINARIES
  perf_smoke_test)

# -----------------------------------------------------------------------------
# Phase 6 — Configuration parser (T094, T095, T096, T097 + bonuses)
# -----------------------------------------------------------------------------
add_executable(config_parser_test
  config_parser_test.cpp)

target_link_libraries(config_parser_test
  PRIVATE
    nsl-basic
    NslFmt
    GTest::gtest_main)

gtest_discover_tests(config_parser_test
  PROPERTIES
    TIMEOUT 30
    LABELS Fmt)

set_property(GLOBAL APPEND PROPERTY _NSL_FMT_GTEST_BINARIES
  config_parser_test)

# -----------------------------------------------------------------------------
# Phase 5 — CLI ↔ library parity + external-link smoke (T081, T082, T083)
# -----------------------------------------------------------------------------
add_executable(format_parity_test
  format_parity_test.cpp)

target_link_libraries(format_parity_test
  PRIVATE
    nsl-basic
    NslFmt
    GTest::gtest_main)

# T081 needs to invoke the built nsl-fmt binary via popen. The path
# is injected at compile time via a macro (matches the project's
# NSLC_VERSION_STRING pattern). The binary is built into
# `${CMAKE_BINARY_DIR}/bin/nsl-fmt` per tools/nsl-fmt/CMakeLists.txt.
target_compile_definitions(format_parity_test
  PRIVATE
    NSL_FMT_BINARY_PATH="${CMAKE_BINARY_DIR}/bin/nsl-fmt")

# Build dependency: format_parity_test needs the nsl-fmt binary to
# exist BEFORE the test runs (the CLIMatchesLibrary case popen()s
# it). Standard ctest doesn't auto-build referenced binaries, so we
# add an explicit add_dependencies edge.
add_dependencies(format_parity_test nsl-fmt)

gtest_discover_tests(format_parity_test
  PROPERTIES
    TIMEOUT 30
    LABELS Fmt)

set_property(GLOBAL APPEND PROPERTY _NSL_FMT_GTEST_BINARIES
  format_parity_test)
