#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# scripts/check_determinism.sh — two-build determinism gate.
#
# Compares the lib/ and bin/ payloads of two build trees built at the
# same git ref. Exits non-zero on any byte-level divergence. This is
# the operational enforcement of FR-018 + SC-005 + Constitution
# Principle V at the build-system layer.
#
# Usage:
#   ./scripts/check_determinism.sh <build_dir_1> <build_dir_2>
#
# Used by:
#   - the last step of stage 1 (build-matrix) Release × gcc cell
#     in scripts/ci.sh and .github/workflows/ci.yml
#   - the determinism_smoke CTest fixture (smoke against a single
#     build dir, which always passes)

set -euo pipefail

build1="${1:?usage: $0 <build_dir_1> <build_dir_2>}"
build2="${2:?usage: $0 <build_dir_1> <build_dir_2>}"

if [[ ! -d "${build1}" ]]; then
  printf 'check_determinism.sh: %s does not exist\n' "${build1}" >&2
  exit 2
fi
if [[ ! -d "${build2}" ]]; then
  printf 'check_determinism.sh: %s does not exist\n' "${build2}" >&2
  exit 2
fi

# Principle V "byte-identical artifacts" means the .a archives the
# macro emits AND the bin/ executables — NOT the cmake-generated
# install scripts (`cmake_install.cmake` legitimately bakes in the
# absolute build-dir path) or the build-graph glue (CMakeFiles/,
# CTestTestfile.cmake, build.ninja, etc.). Restrict the comparison
# to actual deliverable artifacts.

failed=0

# Enumerate from BOTH build dirs so an artifact present in one tree
# but missing from the other is flagged. Enumerating only build1 (an
# earlier draft) would let an extra .a in build2 silently pass the
# gate, which violates the byte-identical artifact contract.
_artifacts_in() {
  local root="$1"
  ( cd "${root}" 2>/dev/null && \
      { find lib -type f -name '*.a' 2>/dev/null
        find bin -type f          2>/dev/null; } | sort )
}

mapfile -t artifacts1 < <(_artifacts_in "${build1}")
mapfile -t artifacts2 < <(_artifacts_in "${build2}")

if [[ "$(printf '%s\n' "${artifacts1[@]}")" \
   != "$(printf '%s\n' "${artifacts2[@]}")" ]]; then
  printf 'determinism gate: artifact set mismatch (%s vs %s)\n' \
    "${build1}" "${build2}" >&2
  diff <(printf '%s\n' "${artifacts1[@]}") \
       <(printf '%s\n' "${artifacts2[@]}") >&2 || true
  failed=1
fi

# Compare every artifact that appears in either set, byte-for-byte.
declare -A seen=()
for rel in "${artifacts1[@]}" "${artifacts2[@]}"; do
  [[ -z "${rel}" || -n "${seen[${rel}]:-}" ]] && continue
  seen[${rel}]=1
  if [[ ! -f "${build1}/${rel}" || ! -f "${build2}/${rel}" ]]; then
    printf 'asymmetric: %s exists in only one tree\n' "${rel}" >&2
    failed=1
  elif ! cmp -s "${build1}/${rel}" "${build2}/${rel}"; then
    printf 'non-deterministic: %s\n' "${rel}" >&2
    failed=1
  fi
done

if [[ ${failed} -ne 0 ]]; then
  printf 'determinism gate: FAILED (%s vs %s)\n' "${build1}" "${build2}" >&2
  exit 1
fi

printf 'determinism gate: identical — %d artifact(s) byte-equal (%s vs %s)\n' \
  "${#seen[@]}" "${build1}" "${build2}"
