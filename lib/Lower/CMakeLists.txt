# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# nsl-lower — layer 8. Two lowering pipelines: AST → nsl::* dialect
# (M5 structural-expansion passes; THIS milestone) and nsl::* → CIRCT
# (M6 hw / comb / seq / sv emission; CIRCT link-libs declared but
# kept inert at M5 per spec §Assumptions).
#
# **M5 contract amendment** (per `specs/008-m5-structural-passes/contracts/lower-api.contract.md`
# §5): LINK_LIBS extended with `MLIRIR`, `MLIRPass`, `MLIRSupport`
# (mirrors the M4 nsl-dialect precedent at `lib/Dialect/NSL/IR/CMakeLists.txt`
# lines 68–70 — the M5 pass classes inherit from
# `mlir::PassWrapper<...>` so MLIRPass is a direct link dependency).
# The CIRCT libs (`CIRCTHW`, `CIRCTComb`, `CIRCTSeq`, `CIRCTSV`)
# stay in LINK_LIBS but inert at M5 (no circt::* symbol referenced
# from any TU here); M6 activates them.
#
# M5 source list:
#   - Lower.cpp                       — public umbrella implementations
#                                       (astToMLIR + registerNSLLowerPasses)
#   - ASTToMLIR.cpp                   — AST → nsl::* dialect visitor
#   - Pass/Common/DiagnosticBridge.cpp — RAII handler forwarding MLIR
#                                       diagnostics to nsl::DiagnosticEngine
#   - Pass/NSL{X}Pass.cpp × 6         — six structural-expansion pass slots

add_nsl_library(nsl-lower
  Lower.cpp
  ASTToMLIR.cpp
  Pass/Common/DiagnosticBridge.cpp
  Pass/NSLResolveParamsPass.cpp
  Pass/NSLExpandGeneratePass.cpp
  Pass/NSLExpandVariablesPass.cpp
  Pass/NSLExplodeSubmodArrayPass.cpp
  Pass/NSLInlineInternalFuncPass.cpp
  Pass/NSLCheckSemanticsPass.cpp
  HEADERS
    ${CMAKE_SOURCE_DIR}/include/nsl/Lower/Lower.h
  DEPENDS nsl-sema nsl-dialect
  LINK_LIBS
    MLIRIR
    MLIRPass
    MLIRSupport
    CIRCTHW
    CIRCTComb
    CIRCTSeq
    CIRCTSV)

# Match MLIR upstream's `-fno-rtti` convention. Pass classes derive
# from `mlir::Pass` (via `mlir::PassWrapper<...>`); building this
# library WITH RTTI emits a vtable referencing `typeinfo for
# mlir::Pass`, which MLIR doesn't ship — fails at link time. Mirrors
# the precedent established at M4 in `lib/Dialect/NSL/IR/CMakeLists.txt`
# line 88.
target_compile_options(nsl-lower PRIVATE -fno-rtti)
