# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test/audited/CMakeLists.txt — wires the M7 audited-corpus
# regression target into the build (M7 P-VEN + P-VCD + US4).
#
# Spec anchor: `specs/011-m7-driver-e2e/spec.md` FR-018.
# Contract: `specs/011-m7-driver-e2e/contracts/audited-corpus.contract.md`
# §5.
#
# Includes the two cmake modules:
#   - cmake/AuditedCorpusLint.cmake — configure-time structural lint
#   - cmake/audited_corpus.cmake     — generates the check-audited
#                                       custom target

include(${CMAKE_SOURCE_DIR}/cmake/AuditedCorpusLint.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/audited_corpus.cmake)
