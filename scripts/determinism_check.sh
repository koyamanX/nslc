#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# scripts/determinism_check.sh — M5 US5 / T101 cross-host-path
# determinism check for `nslc -emit=mlir`.
#
# Operationalizes Constitution Principle V at the M5 driver layer
# per FR-025 + FR-026 + FR-029 and the determinism contract in
# `specs/008-m5-structural-passes/contracts/driver-emit-mlir.contract.md`
# §3 (Output format frozen by Q2 → Option A).
#
# Strategy:
#
#   1. Copy the repo into two distinct host paths
#      ($TMPDIR/nslc-det-a-$$ + $TMPDIR/nslc-det-b-$$).
#   2. Build `nslc` + `nsl-opt` in each (Release, ASan-off, Ninja).
#   3. Run `nslc -emit=mlir` against the canonical-smoke fixture
#      (`test/Lower/determinism/canonical_smoke.nsl` — see T100) in
#      each tree.
#   4. `diff -q` the two outputs. Any byte difference is a
#      determinism failure.
#   5. Grep BOTH outputs for the forbidden host-path patterns from
#      driver-emit-mlir.contract.md §3
#      (`/build|/home|/tmp/|\$TMPDIR|0x[0-9a-fA-F]{8,}|[0-9]{10,}T[0-9]{6,}`).
#      Any match is a host-path leakage failure.
#
# Cost: the cross-path build pair is expensive (~minutes inside the
# dev container; it builds the full toolchain twice). Local
# developers should run this gate manually before submitting US5
# changes. CI runs it on PR-validation runs only, NOT on every push,
# matching the sibling `scripts/check_determinism.sh` build-time
# binary-equality gate which also runs once per build-matrix cell.
#
# Usage:
#   bash scripts/determinism_check.sh
#   bash scripts/determinism_check.sh --skip-build   # reuse $WORK_A / $WORK_B if present
#
# Exit codes:
#   0  byte-identical output across both host paths AND zero
#      forbidden host-path patterns in either output
#   1  cross-path output diff OR forbidden-pattern match
#   2  internal error (build failure, missing fixture, etc.)

set -euo pipefail

# -----------------------------------------------------------------------------
# Locate repo root + canonical fixture
# -----------------------------------------------------------------------------

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
readonly FIXTURE_REL="test/Lower/determinism/canonical_smoke.nsl"

if [[ ! -f "${REPO_ROOT}/${FIXTURE_REL}" ]]; then
  printf '[determinism_check] error: missing fixture %s\n' \
    "${REPO_ROOT}/${FIXTURE_REL}" >&2
  exit 2
fi

SKIP_BUILD=0
case "${1:-}" in
  --skip-build) SKIP_BUILD=1 ;;
  "") : ;;
  *)
    printf '[determinism_check] error: unknown arg %q\n' "$1" >&2
    exit 2
    ;;
esac

# -----------------------------------------------------------------------------
# Two distinct host paths
# -----------------------------------------------------------------------------
#
# The two work paths MUST differ in length AND bytes; if one is a
# prefix of the other, an `__FILE__` leak might still produce
# byte-identical output by coincidence. Use stable-ish names rooted
# in $TMPDIR with the parent PID so two concurrent invocations don't
# collide.

readonly TMPROOT="${TMPDIR:-/tmp}"
readonly WORK_A="${TMPROOT}/nslc-det-a-$$"
readonly WORK_B="${TMPROOT}/nslc-det-bbb-$$"
readonly OUT_A="${TMPROOT}/nslc-det-out-a-$$.mlir"
readonly OUT_B="${TMPROOT}/nslc-det-out-b-$$.mlir"

cleanup() {
  if (( SKIP_BUILD == 0 )); then
    rm -rf "${WORK_A}" "${WORK_B}"
  fi
  rm -f "${OUT_A}" "${OUT_B}"
}
trap cleanup EXIT

log() { printf '[determinism_check] %s\n' "$*"; }

# -----------------------------------------------------------------------------
# Step 1+2: copy + build twice
# -----------------------------------------------------------------------------

build_one() {
  local dest="$1"
  log "  preparing ${dest}"
  rm -rf "${dest}"
  mkdir -p "${dest}"
  # Copy only the source tree, not pre-existing build dirs. The
  # dev-container image does not have rsync; use tar with explicit
  # excludes for portability. `git ls-files` would miss any
  # SPDX-bearing CMake files that are untracked at this commit;
  # tar with explicit excludes is safer than a `git`-driven enum.
  ( cd "${REPO_ROOT}" && tar -cf - \
      --exclude='./build*' \
      --exclude='./.git' \
      --exclude='./.cache' \
      --exclude='./.idea' \
      --exclude='./.vscode' \
      --exclude='./node_modules' \
      . ) | ( cd "${dest}" && tar -xf - )
  log "  configuring ${dest}/build-det"
  # ASan is intentionally disabled here: (a) GCC ASan globals
  # instrumentation embeds build-tree paths in object files (cf. M4
  # commit `3326eb6` "determinism fix"), defeating the cross-host-
  # path check at the binary layer; (b) the M5 lower path is known
  # ASan-broken upstream of this gate (see offload constraints).
  # We're testing nslc OUTPUT determinism, not nslc binary
  # determinism — that is `scripts/check_determinism.sh`'s job.
  cmake -S "${dest}" -B "${dest}/build-det" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DNSL_ENABLE_ASAN=OFF \
    -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
    "${MLIR_DIR:+-DMLIR_DIR=${MLIR_DIR}}" \
    "${CIRCT_DIR:+-DCIRCT_DIR=${CIRCT_DIR}}" \
    > "${dest}/cmake.log" 2>&1
  log "  building ${dest}/build-det"
  if ! cmake --build "${dest}/build-det" \
       --target nslc nsl-opt > "${dest}/build.log" 2>&1; then
    printf '[determinism_check] build failed in %s — see %s\n' \
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
  printf '[determinism_check] --skip-build but %s or %s missing nslc\n' \
    "${WORK_A}" "${WORK_B}" >&2
  exit 2
else
  log "step 1/4: --skip-build — reusing existing trees"
fi

# -----------------------------------------------------------------------------
# Step 3: emit MLIR in each tree
# -----------------------------------------------------------------------------

log "step 2/4: running nslc -emit=mlir in each tree"
"${WORK_A}/build-det/bin/nslc" -emit=mlir "${WORK_A}/${FIXTURE_REL}" > "${OUT_A}"
"${WORK_B}/build-det/bin/nslc" -emit=mlir "${WORK_B}/${FIXTURE_REL}" > "${OUT_B}"

# -----------------------------------------------------------------------------
# Step 4: byte-for-byte diff
# -----------------------------------------------------------------------------

log "step 3/4: byte-for-byte diff of the two outputs"
if ! diff -q "${OUT_A}" "${OUT_B}" > /dev/null; then
  printf '[determinism_check] DETERMINISM FAILURE: output differs across host paths\n' >&2
  printf '  %s vs %s\n' "${OUT_A}" "${OUT_B}" >&2
  diff "${OUT_A}" "${OUT_B}" | head -40 >&2
  exit 1
fi

# -----------------------------------------------------------------------------
# Step 5: forbidden-pattern grep on either output
# -----------------------------------------------------------------------------
#
# The regex follows driver-emit-mlir.contract.md §3 verbatim. Each
# alternative is a separate failure-class:
#
#   /build, /home, /tmp/, $TMPDIR  → host-path leakage
#   0x[0-9a-fA-F]{8,}              → pointer-address-derived suffix
#   [0-9]{10,}T[0-9]{6,}           → ISO-8601 wall-clock stamp
#
# We grep both outputs (they're identical at this point — the diff
# above confirmed it — but checking both is defence-in-depth).

readonly FORBIDDEN_RE='/build|/home|/tmp/|\$TMPDIR|0x[0-9a-fA-F]{8,}|[0-9]{10,}T[0-9]{6,}'

log "step 4/4: forbidden-pattern grep (driver-emit-mlir.contract.md §3)"
if grep -EH "${FORBIDDEN_RE}" "${OUT_A}" "${OUT_B}" >&2; then
  printf '[determinism_check] FAIL: forbidden host-path/timestamp/pointer-address pattern in -emit=mlir output\n' >&2
  printf '  pattern: %s\n' "${FORBIDDEN_RE}" >&2
  printf '  see: specs/008-m5-structural-passes/contracts/driver-emit-mlir.contract.md §3\n' >&2
  exit 1
fi

log "PASS: byte-identical output across two host paths; zero forbidden patterns."
exit 0
