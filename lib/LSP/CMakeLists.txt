# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# lib/LSP/CMakeLists.txt — `libNSLLSP.a` implementation library for
# the T3 `nsl-lsp` skeleton (tooling-track milestone T3).
#
# **Specification anchors**:
#   - `specs/010-t3-lsp-skeleton/plan.md` §Project Structure
#   - `specs/010-t3-lsp-skeleton/contracts/lsp-protocol.contract.md`
#   - Constitution Principle II §3 — `nsl-lsp` is a user-facing
#     tooling binary; MUST reuse `libNSLFrontend.a` rather than
#     duplicate any frontend layer.
#
# Why `add_library` and NOT `add_nsl_library`:
#   `add_nsl_library` is hardcoded to the 9 §3 compiler-track layer
#   names (cmake/AddNSLLibrary.cmake line 80–85 — FATAL_ERROR for
#   any name outside the table). NSLLSP is a T-track tooling
#   library, outside that table. Plain `add_library(... STATIC ...)`
#   is the right tool; the no-duplication rule is honored by linking
#   `nsl-driver` (which transitively pulls the entire frontend per
#   its own `add_nsl_library` DEPENDS list — basic, preprocess, lex,
#   ast, parse, sema, dialect, lower).

add_library(NSLLSP STATIC
  Server.cpp)

target_compile_features(NSLLSP PUBLIC cxx_std_17)

set_target_properties(NSLLSP PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  CXX_EXTENSIONS OFF
  OUTPUT_NAME NSLLSP)

target_include_directories(NSLLSP PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
  $<INSTALL_INTERFACE:include>)

# Principle II reuse: nsl-driver transitively links the entire
# frontend (basic + preprocess + lex + ast + parse + sema + dialect
# + lower). The LSP server consumes `nsl::driver::Compilation` for
# parse + sema invocation per data-model.md §3 and contracts.
target_link_libraries(NSLLSP PUBLIC
  nsl-driver)

# Header install via FILE_SET (matches add_nsl_library §6 pattern,
# applied manually here for the T-track library).
target_sources(NSLLSP PUBLIC
  FILE_SET nsl_public_headers
  TYPE HEADERS
  BASE_DIRS ${CMAKE_SOURCE_DIR}/include
  FILES
    ${CMAKE_SOURCE_DIR}/include/nsl/LSP/Server.h)

# Register sources for the project-wide clang-tidy / clang-format
# hook (matches the GLOBAL append pattern in AddNSLLibrary.cmake §9).
get_target_property(_lsp_sources NSLLSP SOURCES)
foreach(_src IN LISTS _lsp_sources)
  if(IS_ABSOLUTE "${_src}")
    set_property(GLOBAL APPEND PROPERTY NSL_FORMAT_TARGETS "${_src}")
  else()
    set_property(GLOBAL APPEND PROPERTY NSL_FORMAT_TARGETS "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
  endif()
endforeach()
