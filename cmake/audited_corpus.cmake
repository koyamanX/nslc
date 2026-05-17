# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# cmake/audited_corpus.cmake — generates the `check-audited`
# regression target (M7 US4).
#
# Spec anchor: `specs/011-m7-driver-e2e/spec.md` FR-018, FR-019,
# FR-020, FR-023.
# Contract: `specs/011-m7-driver-e2e/contracts/audited-corpus.contract.md`
# §5 (per-cell test shape).
#
# **State (Phase 6 scaffold, 2026-05-12)**: per-cell .test files
# under `test/audited/*.test` (8 files: 4 projects × 2 simulators)
# are picked up by lit's default auto-discovery. Each cell uses
# `REQUIRES: iverilog` or `REQUIRES: verilator` to UNSUPPORTED out
# pre-`:dev-m7` (the container that ships the simulators per
# specs/011-m7-driver-e2e/contracts/container-m7.contract.md §2).
#
# The `check-audited` target is a thin wrapper that invokes lit
# scoped to `test/audited/`. It depends on:
#   - `nslc` (to run `nslc -emit=verilog` on the vendored sources)
#   - `tools/vcd_diff.py` (the semantic-equal VCD comparator;
#     no separate build dep — Python script)
#
# **Forward-looking dependencies (NOT yet wired)**:
#   - Phase 5 final (T062-T068): golden .vcd files under
#     test/audited/<project>/golden/. The per-cell tests
#     reference these via `%vcd-diff` at RUN: time; missing
#     goldens cause cells to FAIL after iverilog/verilator
#     materialize via :dev-m7. Until Phase 5 final lands, the
#     REQUIRES directive alone is the cells' UNSUPPORTED gate.
#   - Phase 2A (T006-T011): :dev-m7 container with iverilog +
#     verilator on PATH. Until that lands, all 8 cells
#     UNSUPPORTED out.

# Project list reused from AuditedCorpusLint.cmake — single source
# of truth.
if(NOT DEFINED NSL_AUDITED_PROJECTS)
  include(${CMAKE_CURRENT_LIST_DIR}/AuditedCorpusLint.cmake)
endif()

# Sanity check: each project should have a corresponding pair of
# *.test cells at the top of test/audited/. If any are missing,
# warn at configure time (not FATAL — allows partial-corpus
# development).
set(_missing_cells "")
foreach(project IN LISTS NSL_AUDITED_PROJECTS)
  foreach(sim IN ITEMS iverilog verilator)
    set(cell_path "${CMAKE_SOURCE_DIR}/test/audited/${project}_${sim}.test")
    if(NOT EXISTS "${cell_path}")
      list(APPEND _missing_cells "${project}_${sim}.test")
    endif()
  endforeach()
endforeach()
if(_missing_cells)
  message(WARNING
    "audited_corpus: missing per-cell .test fixtures: ${_missing_cells} "
    "(should be 4 projects × 2 simulators = 8 cells)")
endif()
unset(_missing_cells)

# `check-audited` invokes lit scoped to the build-tree audited
# subdirectory. Mirrors the `check-nslc` pattern from test/CMakeLists.txt.
#
# lit auto-discovers the 8 .test files (excluding the 4 vendored
# project subdirs per test/audited/lit.local.cfg). Cells with
# REQUIRES iverilog/verilator UNSUPPORTED out until those binaries
# are on PATH.
# `add_lit_testsuite` is the LLVM-provided helper that wires a lit
# invocation as a CMake target. Brought in transitively by
# find_package(MLIR ... CONFIG) at the top-level. The helper sets
# up the lit-site-config + invokes lit with the build-tree paths
# pre-configured.
#
# The per-cell .test files at the top of test/audited/ are picked
# up by lit auto-discovery; cells with `REQUIRES: iverilog` or
# `REQUIRES: verilator` UNSUPPORTED-out when those binaries aren't
# on PATH (pre-`:dev-m7`).
if(NOT TARGET check-audited)
  add_lit_testsuite(check-audited
    "Running M7 audited-corpus regression"
    ${CMAKE_BINARY_DIR}/test
    PARAMS "audited"
    DEPENDS nslc FileCheck)
endif()
