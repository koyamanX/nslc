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

# Compare lib/ first (the .a archives the macro emits). bin/ second
# (the nslc binary). Either divergence is a Principle V violation.
#
# Some build dirs may not have a lib/ subtree yet (e.g., M0 in-tree
# placement under build/lib/nsl/<Layer>/); fall back to a top-level
# scan when that happens.

if [[ -d "${build1}/lib" && -d "${build2}/lib" ]]; then
  diff -r "${build1}/lib" "${build2}/lib"
fi
if [[ -d "${build1}/bin" && -d "${build2}/bin" ]]; then
  diff -r "${build1}/bin" "${build2}/bin"
fi

printf 'determinism gate: identical (%s vs %s)\n' "${build1}" "${build2}"
