# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test/CMakeLists.txt — root of the lit + FileCheck regression tree.
#
# Per Constitution Principle VI, lit + FileCheck is mandatory for the
# lowering and end-to-end stages; it is also the smoke-fixture driver
# for every layer's `smoke.test` (FR-007).
#
# Layout per research.md §9: one root config (test/lit.cfg.py) + an
# auto-generated lit.site.cfg.py via configure_file. Per-layer
# `lit.local.cfg` files arrive only when a layer needs layer-specific
# substitutions (e.g. test/Lower/lit.local.cfg.py once M5 lands).

# Resolve `llvm-lit` and `FileCheck` from the LLVM install discovered
# by find_package(MLIR ...) in the top-level CMakeLists.txt.
set(NSLC_LLVM_TOOLS_BINARY_DIR "${LLVM_TOOLS_BINARY_DIR}")
set(NSLC_BINARY_DIR "${CMAKE_BINARY_DIR}")
set(NSLC_SOURCE_DIR "${CMAKE_SOURCE_DIR}")

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.py.in"
  "${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg.py"
  @ONLY)

# `add_lit_testsuite` ships with LLVM's CMake helpers and is brought in
# transitively by find_package(MLIR ... CONFIG).
add_lit_testsuite(check-nslc
  "Running the NSLC regression tests"
  ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS nslc FileCheck)
