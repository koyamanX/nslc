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

  # PR #14 review #17 (coverage-guard refinement): the original guard
  # grep'd for `OpConversionPattern<` in family files to detect "active"
  # families. M6's actual lowering architecture (Phase 4–6) places the
  # lowering bodies inline inside ModulePatterns.cpp + FSMPatterns.cpp
  # via custom structural pre-passes; the family files (Arith/BitOp/
  # State/Control/Sim/Param/Port/PatternsCpp) are documentation only —
  # their populate*Patterns bodies are intentionally empty. A grep for
  # `OpConversionPattern<` therefore reports ZERO hits for every
  # family, and the original bijection rule trivially passed even when
  # a family's lowering was missing.
  #
  # The refined rule: every family that owns design-§10 mapping rows
  # MUST have at least one matching fixture under its mapped
  # test/Lower/circt/<dir>/. The "ownership" is hard-coded in the
  # _NSLC_M6_FAMILY_DIRS list above (frozen by data-model.md §3 +
  # tasks.md §Phase 6 grouping); we no longer infer it from the file
  # content. This is a STRICTER check than the previous grep because
  # every family is now unconditionally subject to the fixture-
  # presence requirement (no false-pass via empty populator body).
  #
  # Future hardening (Round-2+ work, post-PR-#14): add a per-family
  # helper-function regex (lowerArithOp / lowerBitOp / lowerSimDisplayOp
  # / etc.) over ModulePatterns.cpp + FSMPatterns.cpp to assert the
  # actual lowering helper exists. The helper-function naming
  # convention is documented in each family-file header; mechanizing
  # it requires a per-family regex map.
  file(GLOB _fixtures
       "${_NSLC_M6_FIXTURE_BASEDIR}/${_dir}/*.nsl"
       "${_NSLC_M6_FIXTURE_BASEDIR}/${_dir}/*.mlir")
  list(LENGTH _fixtures _fixture_count)
  if(_fixture_count EQUAL 0)
    list(APPEND _NSLC_M6_GAPS
         "${_family} owns design-§10 rows mapped to "
         "${_NSLC_M6_FIXTURE_BASEDIR}/${_dir}/ but that directory has zero "
         "*.nsl or *.mlir fixtures")
  endif()
endforeach()

list(LENGTH _NSLC_M6_GAPS _gap_count)
if(_gap_count GREATER 0)
  string(REPLACE ";" "\n  - " _gap_msg "${_NSLC_M6_GAPS}")
  message(FATAL_ERROR
    "[nslc] M6 coverage_guard: family↔fixture-directory bijection "
    "violation:\n  - ${_gap_msg}\n\n"
    "Per FR-033 (Principle VIII TDD; refined by PR #14 review-#17): "
    "every CIRCTPatterns family in the 9-row family→fixture-directory "
    "map UNCONDITIONALLY requires at least one *.nsl OR *.mlir fixture "
    "under its mapped test/Lower/circt/<dir>/. Add the fixture(s) — "
    "or, if the family has been retired wholesale, remove its row "
    "from the _NSLC_M6_FAMILY_DIRS list at the top of this file.")
else()
  message(STATUS
    "[nslc] M6 coverage_guard: OK (9 families scanned; per-family "
    "fixture-directory presence rule holds at this configure run)")
endif()

unset(_NSLC_M6_FAMILY_DIRS)
unset(_NSLC_M6_PATTERN_BASEDIR)
unset(_NSLC_M6_FIXTURE_BASEDIR)
unset(_NSLC_M6_GAPS)
