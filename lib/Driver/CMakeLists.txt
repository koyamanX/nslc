# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# nsl-driver — layer 9. CLI orchestration consumed by tools/nslc.
# Aggregates every layer below; the top-level `nslc` binary links
# only against nsl-driver and inherits everything transitively
# (Principle II).
#
# At M2 (Phase 3) nsl-driver gains the `-emit=ast` code path in
# `EmitAST.cpp` alongside M1's `EmitTokens.cpp`.

add_nsl_library(nsl-driver
  EmitTokens.cpp
  EmitAST.cpp
  EmitMLIR.cpp
  EmitHW.cpp
  EmitVerilog.cpp      # M7 (FR-001, FR-006): emitVerilog() free function
  Sema.cpp
  Compilation.cpp
  LowerToNSL.cpp
  RunNSLPasses.cpp
  LowerToCIRCT.cpp
  RunCIRCTPasses.cpp   # M7 (FR-002): Compilation::runCIRCTPasses body
  HEADERS
    ${CMAKE_SOURCE_DIR}/include/nsl/Driver/EmitTokens.h
    ${CMAKE_SOURCE_DIR}/include/nsl/Driver/EmitAST.h
    ${CMAKE_SOURCE_DIR}/include/nsl/Driver/EmitMLIR.h
    ${CMAKE_SOURCE_DIR}/include/nsl/Driver/EmitHW.h
    ${CMAKE_SOURCE_DIR}/include/nsl/Driver/EmitVerilog.h  # M7 (FR-006)
    ${CMAKE_SOURCE_DIR}/include/nsl/Driver/Sema.h
    ${CMAKE_SOURCE_DIR}/include/nsl/Driver/Compilation.h
  DEPENDS
    nsl-basic
    nsl-preprocess
    nsl-lex
    nsl-ast
    nsl-parse
    nsl-sema
    # M4 (FR-004 / FR-019): nsl-driver gains a build-time edge into
    # nsl-dialect so the `Compilation` ctor can call
    # `mlirCtx_.loadDialect<nsl::dialect::NSLDialect>()` per design
    # §11 line 1145, and so `LowerToNSL.cpp` / `RunNSLPasses.cpp`
    # can reference dialect types in their forward-looking M5
    # bodies. The reverse edge (nsl-dialect → nsl-driver) is
    # forbidden by Principle II + AddNSLLibrary's layer-index check.
    nsl-dialect
    nsl-lower
  LINK_LIBS
    # M7 (FR-002 + FR-003): three new CIRCT link libs for the stock-
    # CIRCT post-processing pipeline + ExportVerilog dispatch per
    # specs/011-m7-driver-e2e/contracts/driver-emit-verilog.contract.md
    # §6 and specs/011-m7-driver-e2e/contracts/circt-passes.contract.md
    # §1. Each is non-substitutable. Library names are upstream-
    # canonical CIRCTTargets.cmake exports verified 2026-05-12 against
    # the vendored :dev container.
    #
    # Library → provided symbols:
    #   - CIRCTExportVerilog → circt::exportVerilog,
    #                          circt::exportSplitVerilog
    #                          (PrepareForEmission runs internally;
    #                          not invoked explicitly per
    #                          circt/Conversion/Passes.td:76)
    #   - CIRCTFSMToSV       → circt::createConvertFSMToSVPass
    #   - CIRCTSeqToSV       → circt::createLowerSeqToSVPass
    #
    # (Spec named CIRCTSeqTransforms / CIRCTSVTransforms /
    # CIRCTFSMTransforms; these provide internal Seq/SV/FSM
    # transforms but NOT the to-SV conversion passes we want here.
    # The "-To-SV" conversion libs are separate by upstream design.)
    CIRCTExportVerilog
    CIRCTFSMToSV
    CIRCTSeqToSV)

# nsl-driver is the first M-track library to consume MLIR headers
# (Compilation.cpp pulls in mlir::MLIRContext + the dialect-load call
# site per design §11 line 1145; LowerToNSL.cpp / RunNSLPasses.cpp
# reference mlir::ModuleOp / mlir::LogicalResult). MLIR upstream is
# built `-fno-rtti`, so consumers must match to avoid vtable-typeinfo
# link errors. The other nsl-driver TUs (EmitTokens.cpp / EmitAST.cpp
# / Sema.cpp) don't include any MLIR header but compile fine
# `-fno-rtti` (no dynamic_cast / typeid / exception-handling RTTI
# usage in the project per Build/Code/Licensing standards).
target_compile_options(nsl-driver PRIVATE -fno-rtti)
