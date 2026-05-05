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

# Per-suite targets — added by Phase 2+ tasks. Listed here so a
# future implementer can `grep` for the full Phase 1 + Phase 2+
# layout.
#
#   Phase 2: LspSession (test library), lifecycle_test
#   Phase 3 (US1): diagnostics_test
#   Phase 5 (US3): folding_test, cancellation_test
#
# Skeleton-only at this milestone marker; suite directories are
# added piecewise.
