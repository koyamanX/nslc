# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test/Lower/circt/coverage_guard.cmake — M6 conversion-pattern ↔
# fixture bijection guard (FR-033 enforcement; spec.md / tasks.md
# T028 fleshes out the bijection logic).
#
# At Phase 1 (T004) this file is intentionally a SKELETON — empty
# enforcement until T028 implements the regex-walk over
# `lib/Lower/CIRCTPatterns/*.cpp` and bijection check against
# `test/Lower/circt/<family>/*.nsl`. The file's mere presence at
# Phase 1 establishes the location + include path; T028 adds the
# logic. The configure-time include below from the top-level
# CMakeLists.txt (added at T028) wires this into the build.
#
# Per Constitution Principle VIII, this guard mechanises the TDD
# discipline at the lowering layer: any new conversion pattern in
# `lib/Lower/CIRCTPatterns/` must come with a matching fixture in
# `test/Lower/circt/<family>/`, and vice versa. CI configure fails
# on bijection violation.
#
# References:
#   - specs/010-m6-circt-lowering/spec.md FR-033
#   - specs/010-m6-circt-lowering/tasks.md T004 (skeleton — this
#     file) and T028 (logic — fleshes out the body)
#   - specs/010-m6-circt-lowering/research.md §14
#   - specs/010-m6-circt-lowering/data-model.md §8

# At Phase 1, only an informational message — no enforcement yet.
# Replace the `message(STATUS …)` below with the real bijection
# walker at T028.
message(STATUS
  "[nslc] M6 coverage_guard.cmake: skeleton (Phase 1 — T028 will "
  "implement the conversion-pattern ↔ fixture bijection check; "
  "no enforcement at this checkpoint)")
