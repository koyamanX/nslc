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
declare -a checked=()

_compare_one() {
  local rel="$1"
  if ! cmp -s "${build1}/${rel}" "${build2}/${rel}"; then
    printf 'non-deterministic: %s\n' "${rel}" >&2
    failed=1
  fi
  checked+=("${rel}")
}

# Static-library archives.
while IFS= read -r rel; do
  _compare_one "${rel}"
done < <(cd "${build1}" && find lib -type f -name '*.a' 2>/dev/null | sort)

# bin/ executables (nslc plus anything later milestones add).
while IFS= read -r rel; do
  _compare_one "${rel}"
done < <(cd "${build1}" && find bin -type f 2>/dev/null | sort)

if [[ ${failed} -ne 0 ]]; then
  printf 'determinism gate: FAILED (%s vs %s)\n' "${build1}" "${build2}" >&2
  exit 1
fi

printf 'determinism gate: identical — %d artifact(s) byte-equal (%s vs %s)\n' \
  "${#checked[@]}" "${build1}" "${build2}"
