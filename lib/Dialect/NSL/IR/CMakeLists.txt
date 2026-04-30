# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# lib/Dialect/NSL/IR/CMakeLists.txt — `nsl-dialect` library (M4,
# layer 7). The `nsl::*` MLIR dialect (TableGen + ODS).
#
# Per Constitution Principle III, this layer sits ABOVE stock CIRCT;
# nothing here ever edits CIRCT. The library DEPENDS only on
# `nsl-basic` and the upstream MLIR `IR` / `Support` libraries; CI
# guards against any edge into `nsl-ast`, `nsl-sema`, `nsl-parse`,
# `nsl-lex`, or `nsl-preprocess` (FR-001 / FR-003 / FR-005;
# `cmake/AddNSLLibrary.cmake` enforces).
#
# **Specification anchors**:
#   - `specs/007-m4-mlir-dialect/spec.md` FR-002 — `add_nsl_library`
#     for the C++ wrap; `add_mlir_dialect` for tablegen.
#   - `specs/007-m4-mlir-dialect/research.md` §15 — both helpers,
#     complementary not overlapping.
#
# `add_mlir_dialect(<TDFileStem> <namespace>)` is the upstream MLIR
# helper. With `<TDFileStem>` = `NSLOps`, the helper runs
# `mlir-tblgen` against `NSLOps.td` (the umbrella that includes the
# other two .td files) and produces six private build artifacts
# under `${CMAKE_BINARY_DIR}/lib/Dialect/NSL/IR/`:
#   - NSLOps.h.inc          / NSLOps.cpp.inc          (op decls/defs)
#   - NSLOpsTypes.h.inc     / NSLOpsTypes.cpp.inc     (typedef decls/defs)
#   - NSLOpsDialect.h.inc   / NSLOpsDialect.cpp.inc   (dialect decls/defs)
# The aggregate tablegen target is `MLIRNSLOpsIncGen` (per
# `add_mlir_dialect_tablegen_target(MLIR${dialect}IncGen)` in
# upstream `AddMLIR.cmake`). The umbrella
# `include/nsl/Dialect/NSL/IR/NSLDialect.h` is the only public
# consumer surface (Principle II §3); the `.h.inc` files live under
# the build tree's include path.
add_mlir_dialect(NSLOps nsl)

# Markdown doc generation (`add_mlir_doc(NSLOps NSLOps Dialect/
# -gen-op-doc)`) is conventional for upstream MLIR/CIRCT dialects but
# is **deferred to M5** — at Phase 2 the op set is empty, so the
# generated `.md` is uninteresting, and the upstream `mlir-doc`
# aggregate target is only available when MLIR is consumed via
# `find_package` from a freshly-installed tree (which is our case in
# the dev container, but vacuous output complicates the build for no
# present benefit). M5 wires it in once real ops exist.

# Project-specific library wrap: SPDX header check, clang-tidy
# profile, and the Principle II layering check via
# `cmake/AddNSLLibrary.cmake`. External MLIR libs go through
# LINK_LIBS (the AddNSLLibrary macro forbids internal nsl-layers in
# LINK_LIBS — see macro lines 117–126).
add_nsl_library(nsl-dialect
  NSLDialect.cpp
  NSLOps.cpp
  NSLTypes.cpp
  HEADERS
    ${CMAKE_SOURCE_DIR}/include/nsl/Dialect/NSL/IR/NSLDialect.h
  DEPENDS
    nsl-basic
  LINK_LIBS
    MLIRIR
    MLIRSupport)

# `add_mlir_dialect` registers the `MLIRNSLOpsIncGen` aggregate
# target carrying every tablegen-generated header. The dialect
# library MUST depend on it so the generated `.h.inc` files exist
# before any TU that includes them is compiled.
add_dependencies(nsl-dialect MLIRNSLOpsIncGen)

# The tablegen-generated `.h.inc` / `.cpp.inc` files land in
# `${CMAKE_CURRENT_BINARY_DIR}` (per upstream `mlir_tablegen`'s
# fixed output convention; see AddMLIR.cmake). The umbrella
# `include/nsl/Dialect/NSL/IR/NSLDialect.h` re-exports them via
# bare-basename `#include "NSLOpsDialect.h.inc"` so we add the
# generated-header directory to `nsl-dialect`'s PUBLIC include path.
# Marking it PUBLIC is correct because the umbrella header itself
# pulls these includes in transitively for every consumer
# (`nsl-opt`, `test_unit/dialect_register_test`, future M5
# `nsl-lower`). Marked `BUILD_INTERFACE` only — install-tree
# consumers see the headers via the umbrella, never the raw `.h.inc`.
target_include_directories(nsl-dialect PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
