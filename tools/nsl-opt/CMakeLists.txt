# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# tools/nsl-opt/CMakeLists.txt — `nsl-opt` developer/test binary
# (M4). Per Constitution Principle II §4, `nsl-opt` is a developer/
# test tool — NOT a user-facing T-track deliverable; release
# tarballs do not bundle it (FR-016).
#
# **Specification anchors**:
#   - `specs/007-m4-mlir-dialect/spec.md` FR-014 — link nsl-dialect,
#     MLIRIR, MLIRSupport, MLIROptLib, plus the 5 CIRCT dialect
#     libs.
#   - `specs/007-m4-mlir-dialect/research.md` §6 — vanilla
#     `MlirOptMain`-style binary (~50 lines).

# Optional install hook per FR-016. By default the binary lives in
# the build tree only; opt-in via `-DNSL_INSTALL_DEV_TOOLS=ON`.
option(NSL_INSTALL_DEV_TOOLS
       "Install developer/test tools (nsl-opt) into the install prefix"
       OFF)

add_executable(nsl-opt main.cpp)

# `nsl-dialect` carries the dialect surface (M4 layer 7). The CIRCT
# dialect libs are pulled in via their conventional library names
# (CIRCTHW etc.); MLIROptLib is upstream's `MlirOptMain` driver.
# MLIRIR + MLIRSupport are foundational and pulled in transitively
# through nsl-dialect, but listed explicitly for clarity.
target_link_libraries(nsl-opt
  PRIVATE
    nsl-dialect
    nsl-lower      # M5: structural-expansion passes registered via
                   # `nsl::lower::registerNSLLowerPasses()`.
    MLIRIR
    MLIRSupport
    MLIROptLib
    CIRCTHW
    CIRCTComb
    CIRCTSeq
    CIRCTFSM
    CIRCTSV)

# Place alongside `nslc` in `bin/` so contributors invoking from
# `build/bin/` find both binaries side-by-side.
set_target_properties(nsl-opt PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  CXX_EXTENSIONS OFF)

target_compile_features(nsl-opt PRIVATE cxx_std_17)

# Match MLIR's `-fno-rtti` build (LLVM-ecosystem convention). nsl-opt's
# main.cpp instantiates an mlir::DialectRegistry and inserts CIRCT
# dialects; the polymorphic types involved emit vtable references to
# `typeinfo for mlir::Dialect` if compiled WITH RTTI, which the
# upstream MLIR libs don't ship. Stay consistent with `nsl-dialect`'s
# RTTI-off build.
target_compile_options(nsl-opt PRIVATE -fno-rtti)

# Optional install rule, gated by the option above per FR-016.
if(NSL_INSTALL_DEV_TOOLS)
  install(TARGETS nsl-opt RUNTIME DESTINATION bin)
endif()
