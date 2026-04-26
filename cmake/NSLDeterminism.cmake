# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# NSLDeterminism.cmake
# ====================
#
# Operationalizes Constitution Principle V (Inspectable, Deterministic
# Pipeline) at the build-system layer. Every flag here exists to make
# the produced .a archives and the nslc binary byte-identical between
# two builds at the same git ref.
#
# Spec: FR-018, FR-022, SC-005.
# Research: specs/001-m0-build-ci-scaffolding/research.md §4.
#
# What this module does:
#   1. Forces GNU `ar` deterministic mode (`D` flag) so static-archive
#      members carry zeroed mtime/uid/gid metadata.
#   2. Suppresses ELF build-id, which is otherwise hashed from input
#      timestamps / paths.
#   3. Maps source paths and `__FILE__`-derived strings to relative form
#      so the build directory does not leak into object files.
#   4. Seeds template-instantiation pointer ordering deterministically
#      via `-frandom-seed=$<TARGET_OBJECTS:>`.
#   5. Refuses time-sensitive built-in macros (`__DATE__`, `__TIME__`,
#      `__TIMESTAMP__`) — also enforced via .clang-tidy as a backstop.
#
# Two-build determinism gate: see scripts/check_determinism.sh and
# the `Release × gcc` step of the CI build-matrix stage.

include_guard(GLOBAL)

# -----------------------------------------------------------------------------
# 1. Static-archive determinism (`ar Drcs` instead of the default `ar qc`).
# -----------------------------------------------------------------------------
#
# CMake constructs the archive command from CMAKE_<LANG>_ARCHIVE_CREATE
# and CMAKE_<LANG>_ARCHIVE_APPEND. We override the `r` flag with `Drcs`
# (Deterministic, replace, create-if-missing, write-symbol-table).

set(CMAKE_C_ARCHIVE_CREATE   "<CMAKE_AR> Dqcs <TARGET> <LINK_FLAGS> <OBJECTS>" CACHE STRING "" FORCE)
set(CMAKE_C_ARCHIVE_APPEND   "<CMAKE_AR> Dq <TARGET> <LINK_FLAGS> <OBJECTS>"  CACHE STRING "" FORCE)
set(CMAKE_C_ARCHIVE_FINISH   "<CMAKE_RANLIB> -D <TARGET>"                      CACHE STRING "" FORCE)
set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> Dqcs <TARGET> <LINK_FLAGS> <OBJECTS>" CACHE STRING "" FORCE)
set(CMAKE_CXX_ARCHIVE_APPEND "<CMAKE_AR> Dq <TARGET> <LINK_FLAGS> <OBJECTS>"  CACHE STRING "" FORCE)
set(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> -D <TARGET>"                      CACHE STRING "" FORCE)

# -----------------------------------------------------------------------------
# 2. Link-time determinism: suppress ELF build-id.
# -----------------------------------------------------------------------------

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_link_options("-Wl,--build-id=none")
endif()

# -----------------------------------------------------------------------------
# 3. Source-path normalisation.
# -----------------------------------------------------------------------------

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(
    "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=."
    "-fmacro-prefix-map=${CMAKE_SOURCE_DIR}=."
    "-fdebug-prefix-map=${CMAKE_SOURCE_DIR}=.")
endif()

# -----------------------------------------------------------------------------
# 4. Template-instantiation ordering.
# -----------------------------------------------------------------------------

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options("-frandom-seed=$<TARGET_OBJECTS:>")
endif()

# -----------------------------------------------------------------------------
# 5. Forbid time-sensitive built-in macros.
# -----------------------------------------------------------------------------
#
# Compiler-level enforcement on top of the .clang-tidy rule. Any source
# file that uses __DATE__/__TIME__/__TIMESTAMP__ produces a hard error,
# not a warning. (The macros are still defined, but referencing them
# triggers -Werror=date-time on GCC and -Werror,-Wdate-time on Clang.)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options("-Werror=date-time")
endif()
