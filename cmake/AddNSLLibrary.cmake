# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# AddNSLLibrary.cmake
# ===================
#
# **STUB.** Real macro lands at task T026 (US1). This file exists so
# that the top-level CMakeLists.txt's `include(AddNSLLibrary)` resolves
# during Phase-2 configure even before Phase-3 (US1) implementation.
#
# Once T026 lands, this entire file is replaced by the per-contract
# implementation in
#   specs/001-m0-build-ci-scaffolding/contracts/add_nsl_library.contract.md
#
# Calling `add_nsl_library(...)` against this stub will succeed but
# produce nothing — the configure-time validation, dependency-direction
# guard, and aggregate-property registration arrive with T026.

include_guard(GLOBAL)

function(add_nsl_library name)
  message(WARNING
    "add_nsl_library(${name}) called against the Phase-2 stub. "
    "Real macro lands at T026 (specs/001-m0-build-ci-scaffolding/"
    "contracts/add_nsl_library.contract.md).")
endfunction()
