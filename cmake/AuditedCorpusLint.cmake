# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# cmake/AuditedCorpusLint.cmake — configure-time lint for the
# audited NSL corpus (M7 P-VEN + P-VCD).
#
# Spec anchor: `specs/011-m7-driver-e2e/spec.md` FR-013, FR-016.
# Contract: `specs/011-m7-driver-e2e/contracts/audited-corpus.contract.md`
# §4 (checks enumerated).
#
# This module runs at `cmake -B build ...` configure time. Each
# violation aborts configure with `message(FATAL_ERROR …)` — Principle
# IX no-bypass.
#
# **Status (Phase 1 scaffold)**: skeleton only — emits an
# informational STATUS message. The actual check body lands at T045
# once the seven projects + their PROVENANCE.md / golden/REGEN.md
# files exist under test/audited/.

# Single source of truth for the seven canonical audited projects.
# Pinning the list here (rather than via directory glob) makes the
# error case "the cpu16 directory is missing" produce a precise
# diagnostic rather than a silent count mismatch. Adding an 8th
# project post-M7 is a routine PR per audited-corpus.contract.md §8
# — that PR appends to this list.
set(NSL_AUDITED_PROJECTS
  cpu16
  mips32_single_cycle
  ahb_lite_nsl
  mmcspi
  SDRAM_Controler
  rv32x_dev
  turboV)

# Apache-2.0 WITH LLVM-exception compatible SPDX identifiers (FR-011).
# Maintained centrally to avoid drift across audited PROVENANCE.md
# files. Per research.md §8 the seven projects' upstream licenses
# (BSD-2-Clause / MIT / Apache-2.0) are all in this set.
set(NSL_LICENSE_COMPATIBLE
  "BSD-2-Clause"
  "BSD-3-Clause"
  "MIT"
  "Apache-2.0"
  "Apache-2.0 WITH LLVM-exception")

function(_nsl_audited_corpus_lint)
  message(STATUS "AuditedCorpusLint: scaffold (M7 Phase 1); "
                 "full check body lands at T045 once P-VEN structure "
                 "is filled. Configured projects: ${NSL_AUDITED_PROJECTS}")
endfunction()

_nsl_audited_corpus_lint()
