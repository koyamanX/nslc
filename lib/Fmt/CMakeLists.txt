# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# lib/Fmt/CMakeLists.txt — `libNslFmt.a` tool library (T2 milestone).
#
# **NOTE on target / output naming**: the layer libraries (lib/Lex,
# lib/Parse, etc.) use `add_nsl_library(nsl-<layer>)` which produces
# `libnsl-<layer>.a` (kebab-case, all-lowercase). For tool libraries
# we use **mixed-case `Nsl<Tool>`** so the resulting archive is
# `libNslFmt.a` (per spec FR-017 verbatim). The kebab-case namespace
# is reserved for the nine Principle-II compiler-track layers; this
# split avoids a target-name collision with the user-facing
# `tools/nsl-fmt/` executable (CMake forbids two targets with the
# same name globally). Future tool libraries follow the same scheme:
# `NslLsp` → `libNslLsp.a` (T3), `NslLint` → `libNslLint.a` (T6).
#
# **NOTE on macro choice**: `add_nsl_library(...)` (cmake/AddNSLLibrary.cmake)
# is hard-coded to the nine compiler-track *layer* libraries listed in
# Constitution Principle II's layer table. `NslFmt` is a TOOL library,
# NOT a layer — so it uses plain `add_library(NslFmt STATIC ...)` per
# the LLVM/CIRCT convention. This is the same shape `tools/nslc/` and
# `tools/nsl-opt/` use for their tool-specific code.
#
# Per Principle II's no-duplication rule, every consumer of the NSL
# front-end (`nsl-fmt`, `nsl-lsp`, `nsl-lint`, `nsl-opt`) MUST link
# against `libNSLFrontend.a`'s underlying layer libraries — never
# duplicate them. T2's only modification to the front-end itself is
# the `CSTSink` virtual interface added to `include/nsl/Parse/Parser.h`
# in T017 (a single new abstract symbol on the existing single public
# header — Principle II preserved).
#
# Phase 2a (T011–T015) lands the directive-splitter pre-pass: CST data
# types (CST.h, header-only) plus DirectiveSplitter.{h,cpp}. The Phase-1
# `Stub.cpp` anchor TU is removed — DirectiveSplitter.cpp is now the
# library's first real source. Phase 2b adds CSTBuilder.cpp; Phase 2c
# adds Doc.cpp / LayoutRenderer.cpp / Format.cpp; Phase 3 adds
# LayoutPlanner.cpp; etc.

add_library(NslFmt STATIC
  DirectiveSplitter.cpp
)

target_include_directories(NslFmt
  PUBLIC
    "${CMAKE_SOURCE_DIR}/include")

# Eventual deps (commented out in Phase 1 — uncommented per task as
# each layer's API gets used):
#   nsl-basic     # DiagnosticEngine, SourceManager (Phase 2)
#   nsl-lex       # Lexer / TokenKind (Phase 2)
#   nsl-parse     # Parser + CSTSink interface (Phase 2 — T017/T018)
#   nsl-ast       # CST node-kind taxonomy mirrors AST (Phase 2)
#   tomlpp        # TOML config parsing (Phase 6 — T103)
#   LLVMSupport   # StringRef, Twine, raw_ostream (Phase 2)
target_link_libraries(NslFmt
  PUBLIC
    nsl-basic)

target_compile_features(NslFmt PUBLIC cxx_std_17)
