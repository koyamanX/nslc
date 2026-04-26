# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# AddNSLLibrary.cmake — single declaration mechanism for the nine
# compiler-track layer libraries (FR-002).
#
# Contract:
#   specs/001-m0-build-ci-scaffolding/contracts/add_nsl_library.contract.md
#
# Constitution Principle II is operationalized here: the macro refuses
# at configure time to wire a dependency that violates the §3 layer
# table's downward-only direction (FR-004). The diagnostic format
# matches Principle IV ("diagnostics point back to the spec").
#
# Skeleton-library handling: M0 ships zero source code in the layer
# libs, but FR-001 demands that nine static-archive `.a` files exist
# in the build output. When `add_nsl_library` is called with no
# positional source arguments the macro auto-generates a tiny anchor
# TU so the resulting STATIC library has at least one object file.
# Real sources arrive milestone-by-milestone (M1+) and replace the
# anchor.

include_guard(GLOBAL)

# -----------------------------------------------------------------------------
# Layer table
# -----------------------------------------------------------------------------
#
# Mirrors `docs/design/nsl_compiler_design.md` §3. Lower index = lower
# layer = no upward deps allowed. Order is dependency-correct:
# nsl-ast precedes nsl-parse so that the existing `Parse DEPENDS AST`
# wiring satisfies the strictly-lower rule.

function(_nsl_layer_index out_var name)
  set(_idx -1)
  if(name STREQUAL "nsl-basic")          set(_idx 1)
  elseif(name STREQUAL "nsl-preprocess") set(_idx 2)
  elseif(name STREQUAL "nsl-lex")        set(_idx 3)
  elseif(name STREQUAL "nsl-ast")        set(_idx 4)
  elseif(name STREQUAL "nsl-parse")      set(_idx 5)
  elseif(name STREQUAL "nsl-sema")       set(_idx 6)
  elseif(name STREQUAL "nsl-dialect")    set(_idx 7)
  elseif(name STREQUAL "nsl-lower")      set(_idx 8)
  elseif(name STREQUAL "nsl-driver")     set(_idx 9)
  endif()
  set(${out_var} ${_idx} PARENT_SCOPE)
endfunction()

set(_NSL_VALID_LAYERS
  "nsl-basic, nsl-preprocess, nsl-lex, nsl-ast, nsl-parse, "
  "nsl-sema, nsl-dialect, nsl-lower, nsl-driver")

# -----------------------------------------------------------------------------
# add_nsl_library(<name>
#   [<source files>...]
#   [HEADERS <header files>...]
#   [DEPENDS <intra-project lib targets>...]
#   [LINK_LIBS <external targets>...]
#   [EXCLUDE_FROM_LIBNSLFRONTEND])
# -----------------------------------------------------------------------------

function(add_nsl_library name)
  cmake_parse_arguments(NSL
    "EXCLUDE_FROM_LIBNSLFRONTEND"
    ""
    "HEADERS;DEPENDS;LINK_LIBS"
    ${ARGN})
  set(_sources ${NSL_UNPARSED_ARGUMENTS})

  # ---------- 1. Layer-name validation ----------
  _nsl_layer_index(_idx ${name})
  if(_idx EQUAL -1)
    message(FATAL_ERROR
      "add_nsl_library: '${name}' is not in the §3 layer table; "
      "the 9 valid names are ${_NSL_VALID_LAYERS} — see "
      "docs/design/nsl_compiler_design.md §3.")
  endif()

  # ---------- 2. Dependency-direction validation ----------
  foreach(_dep IN LISTS NSL_DEPENDS)
    _nsl_layer_index(_dep_idx ${_dep})
    if(_dep_idx EQUAL -1)
      message(FATAL_ERROR
        "add_nsl_library(${name}): DEPENDS '${_dep}' is not a known "
        "nsl-layer; valid names are ${_NSL_VALID_LAYERS} — see "
        "docs/design/nsl_compiler_design.md §3. (External libraries "
        "belong in LINK_LIBS, not DEPENDS.)")
    endif()
    if(NOT _dep_idx LESS _idx)
      message(FATAL_ERROR
        "add_nsl_library: layer '${name}' (index ${_idx}) cannot "
        "depend on '${_dep}' (index ${_dep_idx} ≥ ${_idx}) — see "
        "docs/design/nsl_compiler_design.md §3.")
    endif()
  endforeach()

  # ---------- 3. Auto-generate anchor TU when sources are empty ----------
  if(NOT _sources)
    string(REPLACE "-" "_" _safe_name ${name})
    set(_anchor "${CMAKE_CURRENT_BINARY_DIR}/${name}_anchor.cpp")
    if(NOT EXISTS "${_anchor}")
      file(WRITE "${_anchor}"
"// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Auto-generated anchor TU for the M0 skeleton library '${name}'.
// Replaced piecewise as later milestones land real sources; once the
// library has any real TU this file is harmless dead weight.
namespace nsl::detail {
void ${_safe_name}_anchor() {}
}
")
    endif()
    set(_sources "${_anchor}")
  endif()

  # ---------- 4. Library creation ----------
  add_library(${name} STATIC ${_sources})
  target_compile_features(${name} PUBLIC cxx_std_17)
  set_target_properties(${name} PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME ${name})

  # ---------- 5. Public include path ----------
  target_include_directories(${name} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:include>)

  # ---------- 6. Header install via FILE_SET ----------
  if(NSL_HEADERS)
    target_sources(${name} PUBLIC
      FILE_SET nsl_public_headers
      TYPE HEADERS
      BASE_DIRS ${CMAKE_SOURCE_DIR}/include
      FILES ${NSL_HEADERS})
  endif()

  # ---------- 7. Link plumbing ----------
  if(NSL_DEPENDS OR NSL_LINK_LIBS)
    target_link_libraries(${name} PUBLIC ${NSL_DEPENDS} ${NSL_LINK_LIBS})
  endif()

  # ---------- 8. Aggregate-target registration ----------
  if(NOT NSL_EXCLUDE_FROM_LIBNSLFRONTEND)
    set_property(GLOBAL APPEND PROPERTY NSL_FRONTEND_LIBS ${name})
  endif()

  # ---------- 9. clang-tidy / clang-format hook ----------
  if(_sources)
    set_property(GLOBAL APPEND PROPERTY NSL_FORMAT_TARGETS ${_sources})
  endif()
endfunction()
