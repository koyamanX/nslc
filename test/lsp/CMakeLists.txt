# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test/lsp/CMakeLists.txt — root of the LSP integration-test layer
# introduced at T3.
#
# **Specification anchors**:
#   - `specs/010-t3-lsp-skeleton/contracts/lsp-test-harness.contract.md`
#   - `specs/010-t3-lsp-skeleton/plan.md` §Project Structure
#
# The four gtest binaries below (added task-by-task in Phase 2
# onwards) drive `nsl-lsp` as a subprocess via the shared
# `LspSession` harness in this directory. Per the harness contract
# §1 they spawn `nsl-lsp`, write JSON-RPC envelopes over stdin,
# read responses + stderr from stdout/stderr pipes, and assert on
# the captured output.
#
# Layer driver: gtest (in-tree, subprocess-based). Per Constitution
# Principle VI's per-layer accepted-driver list, the canonical
# drivers (lit + FileCheck for lowering / E2E; gtest / nsl-opt for
# unit / dialect) cover the compiler track; tooling-track LSP
# integration testing is out of that list's enumerated scope by
# construction (parallel to T1's TextMate scope-test layer).
#
# At T001 (skeleton creation) this file declares no targets —
# Phase 2 (T030–T035) lands `LspSession.{h,cpp}`, T037–T041 land
# `lifecycle_test`, and the per-feature suites land in their
# respective phases.

# Gating: top-level `CMakeLists.txt` already wraps `add_subdirectory(test)`
# in `if(NSL_BUILD_TESTS)`, so this file is only loaded when tests
# are enabled.

find_package(GTest REQUIRED)
include(GoogleTest)

# Shared harness library used by every gtest binary in this
# subtree. Per `contracts/lsp-test-harness.contract.md` §1.
add_library(LspSession STATIC LspSession.cpp)
target_compile_features(LspSession PUBLIC cxx_std_17)
target_include_directories(LspSession PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(LspSession PUBLIC GTest::gtest LLVMSupport)
# Match the LLVM-ecosystem -fno-rtti convention; LspSession
# consumes llvm::json which would otherwise trigger the same
# typeinfo-undefined link errors NSLLSP encountered.
target_compile_options(LspSession PRIVATE -fno-rtti)

# Helper: register a single gtest binary that depends on the
# `nsl-lsp` binary at runtime. The test harness resolves the
# path via $NSL_LSP_BINARY → fallback ./bin/nsl-lsp.
function(nsl_add_lsp_test name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE LspSession GTest::gtest GTest::gtest_main)
  target_compile_options(${name} PRIVATE -fno-rtti)
  set_target_properties(${name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    CXX_EXTENSIONS OFF)
  add_dependencies(${name} nsl-lsp)
  gtest_discover_tests(${name}
    TEST_PREFIX "lsp_"
    PROPERTIES
      ENVIRONMENT "NSL_LSP_BINARY=${CMAKE_BINARY_DIR}/bin/nsl-lsp")
endfunction()

# Phase 2 lifecycle tests (T037–T041).
nsl_add_lsp_test(lifecycle_test lifecycle_test.cpp)

# Phase 3 onwards: diagnostics_test, folding_test, cancellation_test
# add similar `nsl_add_lsp_test(...)` lines as their tasks land.
