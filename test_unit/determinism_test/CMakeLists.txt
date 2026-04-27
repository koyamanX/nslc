# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test_unit/determinism_test/CMakeLists.txt — TDD seed for the
# scripts/check_determinism.sh smoke (FR-018, SC-005, Principle V).
#
# Per Principle VIII this fixture lands BEFORE T047
# (scripts/check_determinism.sh). Run as a configure-time-registered
# CTest case via `ctest --test-dir build`.
#
# At M0 the smoke compares ${CMAKE_BINARY_DIR} against itself — a
# diff-of-self always succeeds and the exit-0 path proves the script
# CLI is wired. The real two-build determinism gate (build vs build2)
# runs as the last step of the CI build-matrix stage; that is where
# byte-identical artifacts are actually enforced.

add_test(
  NAME determinism_smoke
  COMMAND ${CMAKE_SOURCE_DIR}/scripts/check_determinism.sh
    ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR})
