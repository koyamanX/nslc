#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# scripts/determinism_check_emit_hw.sh — M6 US5 / T132 cross-host-path
# determinism check for `nslc -emit=hw`.
#
# Sibling of `scripts/determinism_check.sh` (which covers `-emit=mlir`
# at the M5 driver layer); this script extends the same two-host-path
# pattern to `-emit=hw` outputs at the M6 driver layer per spec FR-022
# + `driver-emit-hw.contract.md` §6 (Determinism contract).
#
# Strategy (mirrors determinism_check.sh):
#
#   1. Copy the repo into two distinct host paths
#      ($TMPDIR/nslc-det-hw-a-$$ + $TMPDIR/nslc-det-hw-bbb-$$).
#   2. Build `nslc` in each (Release, ASan-off, Ninja).
#   3. For each fixture in test/Lower/circt/round_trip/*.nsl, run
#      `nslc -emit=hw <fixture>` in each tree.
#   4. `diff -q` the per-fixture outputs. Any byte difference is a
#      determinism failure.
#   5. Grep BOTH outputs for the forbidden host-path / wall-clock /
#      pointer-address patterns (same regex as the M5 sibling).
#
# Cost: full-toolchain build × 2 ≈ minutes inside the dev container.
# CI runs it on PR-validation runs only (gated by
# NSLC_RUN_DETERMINISM_CHECK=1, same pattern as the M5 sibling).
#
# Usage:
#   bash scripts/determinism_check_emit_hw.sh
#   bash scripts/determinism_check_emit_hw.sh --skip-build
#
# Exit codes:
#   0  byte-identical output across both host paths AND zero
#      forbidden host-path patterns in any fixture's output
#   1  cross-path output diff OR forbidden-pattern match
#   2  internal error (build failure, missing fixture-dir, etc.)

set -euo pipefail

# -----------------------------------------------------------------------------
# Locate repo root + canonical fixture set
# -----------------------------------------------------------------------------

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
readonly FIXTURE_DIR_REL="test/Lower/circt/round_trip"

# T132 fixture coverage: every `.nsl` round-trip fixture under
# `test/Lower/circt/round_trip/` whose `RUN: %nslc -emit=hw` line is
# the first form in the file (i.e., not `not %nslc` / not `XFAIL`).
# Listing the basenames explicitly here (rather than `find`-glob) keeps
# the check deterministic in a single layer — no FS-iteration order
# leakage even on filesystems whose readdir is unsorted. The list MUST
# be lexicographically sorted (Constitution Principle V).
readonly -a FIXTURES_REL=(
  "${FIXTURE_DIR_REL}/full_module_combination.nsl"
  "${FIXTURE_DIR_REL}/handshake_pattern.nsl"
  "${FIXTURE_DIR_REL}/loc_plumbing.nsl"
  "${FIXTURE_DIR_REL}/memory_array.nsl"
  "${FIXTURE_DIR_REL}/sim_only.nsl"
  "${FIXTURE_DIR_REL}/small_cpu_subset.nsl"
  "${FIXTURE_DIR_REL}/zero_nsl_ops.nsl"
)

for f in "${FIXTURES_REL[@]}"; do
  if [[ ! -f "${REPO_ROOT}/${f}" ]]; then
    printf '[determinism_check_emit_hw] error: missing fixture %s\n' \
      "${REPO_ROOT}/${f}" >&2
    exit 2
  fi
done

SKIP_BUILD=0
case "${1:-}" in
  --skip-build) SKIP_BUILD=1 ;;
  "") : ;;
  *)
    printf '[determinism_check_emit_hw] error: unknown arg %q\n' "$1" >&2
    exit 2
    ;;
esac

# -----------------------------------------------------------------------------
# Two distinct host paths (same convention as the M5 sibling)
# -----------------------------------------------------------------------------

readonly TMPROOT="${TMPDIR:-/tmp}"
readonly WORK_A="${TMPROOT}/nslc-det-hw-a-$$"
readonly WORK_B="${TMPROOT}/nslc-det-hw-bbb-$$"
readonly OUT_DIR="${TMPROOT}/nslc-det-hw-outs-$$"

cleanup() {
  if (( SKIP_BUILD == 0 )); then
    rm -rf "${WORK_A}" "${WORK_B}"
  fi
  rm -rf "${OUT_DIR}"
}
trap cleanup EXIT

log() { printf '[determinism_check_emit_hw] %s\n' "$*"; }

# -----------------------------------------------------------------------------
# Step 1+2: copy + build twice
# -----------------------------------------------------------------------------

build_one() {
  local dest="$1"
  log "  preparing ${dest}"
  rm -rf "${dest}"
  mkdir -p "${dest}"
  ( cd "${REPO_ROOT}" && tar -cf - \
      --exclude='./build*' \
      --exclude='./.git' \
      --exclude='./.cache' \
      --exclude='./.idea' \
      --exclude='./.vscode' \
      --exclude='./node_modules' \
      . ) | ( cd "${dest}" && tar -xf - )
  log "  configuring ${dest}/build-det"
  # ASan disabled here for the same reasons documented in the M5
  # sibling (determinism_check.sh): instrumented globals embed build
  # paths in object files, defeating the cross-host-path check at the
  # binary layer; the M5/M6 lower path is also ASan-broken upstream
  # of this gate. We test nslc OUTPUT determinism, not nslc binary
  # determinism — that's `scripts/check_determinism.sh`'s job.
  cmake -S "${dest}" -B "${dest}/build-det" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DNSL_ENABLE_ASAN=OFF \
    -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
    "${MLIR_DIR:+-DMLIR_DIR=${MLIR_DIR}}" \
    "${CIRCT_DIR:+-DCIRCT_DIR=${CIRCT_DIR}}" \
    > "${dest}/cmake.log" 2>&1
  log "  building ${dest}/build-det"
  if ! cmake --build "${dest}/build-det" \
       --target nslc > "${dest}/build.log" 2>&1; then
    printf '[determinism_check_emit_hw] build failed in %s — see %s\n' \
      "${dest}" "${dest}/build.log" >&2
    tail -40 "${dest}/build.log" >&2
    exit 2
  fi
}

if (( SKIP_BUILD == 0 )); then
  log "step 1/4: building two trees in distinct host paths"
  build_one "${WORK_A}"
  build_one "${WORK_B}"
elif [[ ! -x "${WORK_A}/build-det/bin/nslc" || ! -x "${WORK_B}/build-det/bin/nslc" ]]; then
  printf '[determinism_check_emit_hw] --skip-build but %s or %s missing nslc\n' \
    "${WORK_A}" "${WORK_B}" >&2
  exit 2
else
  log "step 1/4: --skip-build — reusing existing trees"
fi

# -----------------------------------------------------------------------------
# Step 3: emit HW for each fixture in each tree
# -----------------------------------------------------------------------------

log "step 2/4: running nslc -emit=hw on ${#FIXTURES_REL[@]} fixture(s) in each tree"
mkdir -p "${OUT_DIR}"

for fix in "${FIXTURES_REL[@]}"; do
  base="$(basename "${fix}" .nsl)"
  out_a="${OUT_DIR}/${base}.a.hw"
  out_b="${OUT_DIR}/${base}.b.hw"
  if ! "${WORK_A}/build-det/bin/nslc" -emit=hw "${WORK_A}/${fix}" \
        > "${out_a}" 2> "${out_a}.err"; then
    printf '[determinism_check_emit_hw] nslc -emit=hw failed for %s in %s\n' \
      "${fix}" "${WORK_A}" >&2
    cat "${out_a}.err" >&2
    exit 2
  fi
  if ! "${WORK_B}/build-det/bin/nslc" -emit=hw "${WORK_B}/${fix}" \
        > "${out_b}" 2> "${out_b}.err"; then
    printf '[determinism_check_emit_hw] nslc -emit=hw failed for %s in %s\n' \
      "${fix}" "${WORK_B}" >&2
    cat "${out_b}.err" >&2
    exit 2
  fi
done

# -----------------------------------------------------------------------------
# Step 4: byte-for-byte diff per fixture
# -----------------------------------------------------------------------------

log "step 3/4: byte-for-byte diff of each fixture's two outputs"
fail=0
for fix in "${FIXTURES_REL[@]}"; do
  base="$(basename "${fix}" .nsl)"
  out_a="${OUT_DIR}/${base}.a.hw"
  out_b="${OUT_DIR}/${base}.b.hw"
  if ! diff -q "${out_a}" "${out_b}" > /dev/null; then
    printf '[determinism_check_emit_hw] DIVERGE: %s\n' "${fix}" >&2
    diff "${out_a}" "${out_b}" | head -20 >&2
    fail=1
  fi
done

if (( fail )); then
  printf '[determinism_check_emit_hw] DETERMINISM FAILURE — output differs across host paths\n' >&2
  exit 1
fi

# -----------------------------------------------------------------------------
# Step 5: forbidden-pattern grep on every output
# -----------------------------------------------------------------------------
#
# Same regex as the M5 sibling (driver-emit-mlir.contract.md §3 +
# driver-emit-hw.contract.md §6). M6's `-emit=hw` inherits the M5
# determinism guarantees verbatim.

readonly FORBIDDEN_RE='/build|/home|/tmp/|\$TMPDIR|0x[0-9a-fA-F]{8,}|[0-9]{10,}T[0-9]{6,}'

log "step 4/4: forbidden-pattern grep on every emitted .hw output"
hits=0
for fix in "${FIXTURES_REL[@]}"; do
  base="$(basename "${fix}" .nsl)"
  out_a="${OUT_DIR}/${base}.a.hw"
  out_b="${OUT_DIR}/${base}.b.hw"
  if grep -EH "${FORBIDDEN_RE}" "${out_a}" "${out_b}" >&2; then
    hits=1
  fi
done

if (( hits )); then
  printf '[determinism_check_emit_hw] FAIL: forbidden host-path/timestamp/pointer-address pattern in -emit=hw output\n' >&2
  printf '  pattern: %s\n' "${FORBIDDEN_RE}" >&2
  printf '  see: specs/010-m6-circt-lowering/contracts/driver-emit-hw.contract.md §6\n' >&2
  exit 1
fi

log "PASS: byte-identical -emit=hw output across two host paths for ${#FIXTURES_REL[@]} fixture(s); zero forbidden patterns."
exit 0
