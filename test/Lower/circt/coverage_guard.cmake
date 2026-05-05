# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test/Lower/circt/coverage_guard.cmake — M6 conversion-pattern ↔
# fixture bijection guard (FR-033 enforcement; spec.md / tasks.md
# T028 implementation per /speckit-implement Phase 3).
#
# **Bijection model** (directory-level, intentionally less granular
# than per-op):
#
#   For each `lib/Lower/Pass/CIRCTPatterns/<Family>Patterns.cpp`,
#   if the file declares at least one `OpConversionPattern<...>`
#   subclass, then the corresponding `test/Lower/circt/<dir>/`
#   MUST contain at least one `*.nsl` fixture. Empty pattern files
#   (Phase 2 scaffolds) require zero fixtures.
#
# Family → fixture-directory mapping (frozen by data-model.md §3
# + tasks.md §Phase 6 grouping):
#
#   ArithPatterns.cpp    → test/Lower/circt/arith/
#   BitOpPatterns.cpp    → test/Lower/circt/arith/   (shared)
#   ControlPatterns.cpp  → test/Lower/circt/control/
#   FSMPatterns.cpp      → test/Lower/circt/fsm/
#   ModulePatterns.cpp   → test/Lower/circt/module/
#   ParamPatterns.cpp    → test/Lower/circt/module/  (shared)
#   PortPatterns.cpp     → test/Lower/circt/module/  (shared)
#   SimPatterns.cpp      → test/Lower/circt/sim/
#   StatePatterns.cpp    → test/Lower/circt/state/
#
# This is more practical than per-op enumeration: a single fixture
# (e.g., `port_mixed.nsl`) can exercise multiple ops; the bijection
# at directory level guarantees per-family coverage without forcing
# 1:1 file-to-op naming.
#
# **Failure mode**: configure-time `message(FATAL_ERROR …)` listing
# the family file + missing fixture-directory pair.
#
# References:
#   - specs/010-m6-circt-lowering/spec.md FR-033
#   - specs/010-m6-circt-lowering/research.md §14
#   - specs/010-m6-circt-lowering/data-model.md §8

set(_NSLC_M6_FAMILY_DIRS
    "ArithPatterns:arith"
    "BitOpPatterns:arith"
    "ControlPatterns:control"
    "FSMPatterns:fsm"
    "ModulePatterns:module"
    "ParamPatterns:module"
    "PortPatterns:module"
    "SimPatterns:sim"
    "StatePatterns:state")

set(_NSLC_M6_PATTERN_BASEDIR
    "${CMAKE_SOURCE_DIR}/lib/Lower/Pass/CIRCTPatterns")
set(_NSLC_M6_FIXTURE_BASEDIR
    "${CMAKE_SOURCE_DIR}/test/Lower/circt")

set(_NSLC_M6_GAPS "")

foreach(_pair ${_NSLC_M6_FAMILY_DIRS})
  string(REPLACE ":" ";" _split "${_pair}")
  list(GET _split 0 _family)
  list(GET _split 1 _dir)

  set(_pattern_file "${_NSLC_M6_PATTERN_BASEDIR}/${_family}.cpp")
  if(NOT EXISTS "${_pattern_file}")
    list(APPEND _NSLC_M6_GAPS
         "missing pattern file: ${_pattern_file}")
    continue()
  endif()

  # A pattern file is "active" if it contains at least one
  # `OpConversionPattern<` token. Empty-populator scaffolds (Phase 2)
  # do not.
  file(STRINGS "${_pattern_file}" _hits REGEX "OpConversionPattern<")
  list(LENGTH _hits _hit_count)
  if(_hit_count GREATER 0)
    file(GLOB _fixtures "${_NSLC_M6_FIXTURE_BASEDIR}/${_dir}/*.nsl")
    list(LENGTH _fixtures _fixture_count)
    if(_fixture_count EQUAL 0)
      list(APPEND _NSLC_M6_GAPS
           "${_family} declares ${_hit_count} OpConversionPattern(s) "
           "but ${_NSLC_M6_FIXTURE_BASEDIR}/${_dir}/ has zero *.nsl fixtures")
    endif()
  endif()
endforeach()

list(LENGTH _NSLC_M6_GAPS _gap_count)
if(_gap_count GREATER 0)
  string(REPLACE ";" "\n  - " _gap_msg "${_NSLC_M6_GAPS}")
  message(FATAL_ERROR
    "[nslc] M6 coverage_guard: pattern↔fixture bijection violation:\n"
    "  - ${_gap_msg}\n\n"
    "Per FR-033 (Principle VIII TDD): every CIRCTPatterns family "
    "with at least one OpConversionPattern<...> MUST have at least "
    "one matching .nsl fixture under test/Lower/circt/<dir>/. Add "
    "the fixture(s) or remove the pattern declaration.")
else()
  message(STATUS
    "[nslc] M6 coverage_guard: OK (9 families scanned; per-family "
    "pattern↔fixture bijection holds at this configure run)")
endif()

unset(_NSLC_M6_FAMILY_DIRS)
unset(_NSLC_M6_PATTERN_BASEDIR)
unset(_NSLC_M6_FIXTURE_BASEDIR)
unset(_NSLC_M6_GAPS)
