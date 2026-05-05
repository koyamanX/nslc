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
# library's first real source.
#
# Phase 2b (T016–T020) adds CSTBuilder.{h,cpp} — the concrete
# `parse::CSTSink` implementation that records the parser's CST-mode
# event stream. NslFmt now links nsl-parse for the CSTSink interface
# (declared in include/nsl/Parse/Parser.h per T017).
#
# Phase 2c adds Doc.cpp / LayoutRenderer.cpp / Format.cpp; Phase 3
# adds LayoutPlanner.cpp; etc.

add_library(NslFmt STATIC
  DirectiveSplitter.cpp
  CSTBuilder.cpp
  Doc.cpp                # T2 Phase 2c — Wadler-Leijen IR (T024)
  LayoutRenderer.cpp     # T2 Phase 2c — ribbon-fitter (T025)
  LayoutPlanner.cpp      # T2 Phase 3-skeleton — AST → Doc IR visitor
  Config.cpp             # T2 Phase 2c → 6 — Configuration + parse_config_file (T088/T089/T087/T103/T104/T105)
  ConfigDiscovery.cpp    # T2 Phase 6 — discover_config upward walk (T106)
  Format.cpp             # T2 Phase 2c — format_buffer skeleton (T026/T027)
  Diff.cpp               # T2 Phase 3b — LCS-based unified-diff emitter (T076)
)

target_include_directories(NslFmt
  PUBLIC
    "${CMAKE_SOURCE_DIR}/include")

# Eventual deps (uncommented per task as each layer's API gets used):
#   nsl-ast       # CST node-kind taxonomy mirrors AST (Phase 3)
#   tomlpp        # TOML config parsing (Phase 6 — T103)
target_link_libraries(NslFmt
  PUBLIC
    nsl-basic
    nsl-lex      # T2 Phase 2c — Lexer + Token (used by future Format.cpp paths)
    nsl-parse    # T2 Phase 2b — CSTSink interface in Parser.h
    nsl-ast      # T2 Phase 3-skeleton — AST visitor in LayoutPlanner
  PRIVATE
    tomlpp)      # T2 Phase 6 (T103) — TOML config parsing in Config.cpp

# T2 Phase 5 (T087) — bake the configure-time version string into
# `version_string()`. `NSLC_GIT_DESCRIBE` is set by
# `cmake/NSLVersion.cmake` (the same source as nslc's own version
# banner). The "nsl-fmt " prefix is added here so the macro is
# fully-formed at the call site.
target_compile_definitions(NslFmt
  PRIVATE
    NSL_FMT_VERSION_STRING="nsl-fmt ${NSLC_GIT_DESCRIBE}")

target_compile_features(NslFmt PUBLIC cxx_std_17)
