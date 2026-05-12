# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# cmake/audited_corpus.cmake — generates the `check-audited`
# regression target (M7 US4).
#
# Spec anchor: `specs/011-m7-driver-e2e/spec.md` FR-018, FR-019,
# FR-020, FR-023.
# Contract: `specs/011-m7-driver-e2e/contracts/audited-corpus.contract.md`
# §5 (per-cell test shape).
#
# **Status (Phase 1 scaffold)**: declares an empty `check-audited`
# custom target. The per-project enumeration + per-cell lit-fixture
# generation lands at T071 once vcd_diff.py + the lit substitutions
# are in place.

# Empty placeholder target — depends on nothing, runs nothing, so a
# Phase-1 invocation `ninja check-audited` exits 0 trivially. The
# real enumeration body lands at T071.
if(NOT TARGET check-audited)
  add_custom_target(check-audited
    COMMENT "M7 audited-corpus regression (scaffold — body lands at T071)")
endif()
