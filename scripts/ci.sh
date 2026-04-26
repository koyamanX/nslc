#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# scripts/ci.sh — single authoritative entry point for local CI
# reproduction (FR-017, FR-021). The .github/workflows/ci.yml file
# calls into the same stage-name dispatch so divergence between local
# and remote runs is impossible.
#
# Usage:
#   ./scripts/ci.sh <stage> [args...]
#   ./scripts/ci.sh all
#
# Stages mirror Constitution Principle IX exactly and the contract
# in specs/001-m0-build-ci-scaffolding/contracts/ci-pipeline.contract.md.

set -euo pipefail

# -----------------------------------------------------------------------------
# Constants
# -----------------------------------------------------------------------------

readonly REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly DEFAULT_BUILD_DIR="${REPO_ROOT}/build"
readonly DETERMINISM_BUILD_DIR="${REPO_ROOT}/build2"

# -----------------------------------------------------------------------------
# Helpers
# -----------------------------------------------------------------------------

log()  { printf '[ci.sh] %s\n' "$*"; }
die()  { printf '[ci.sh] error: %s\n' "$*" >&2; exit 1; }

usage() {
  cat >&2 <<'USAGE'
nslc-ci: usage: ./scripts/ci.sh <stage> [args]

stages (Constitution Principle IX order):
  build-matrix [TYPE] [CC]   stage 1; build one cell (default: Release × host)
  build-matrix --matrix      stage 1; fan out all 4 cells
  static-checks              stage 2 (clang-format + clang-tidy + SPDX header)
  unit-tests                 stage 3 (ctest)
  lowering-tests             stage 4 (lit)
  e2e                        stage 5 (wired but empty until M7)
  formal                     stage 6 (wired but empty until M8)
  all                        stages 1..4 in order; first failure stops

  -h | --help                this message
USAGE
}

# -----------------------------------------------------------------------------
# Stage 1: build-matrix
# -----------------------------------------------------------------------------

_one_cell() {
  local build_type="${1:-Release}"
  local cxx="${2:-}"
  local cell_dir="${DEFAULT_BUILD_DIR}-${build_type}-${cxx:-host}"

  log "build-matrix cell: ${build_type} × ${cxx:-host}  →  ${cell_dir}"

  local cmake_extra=()
  case "${cxx}" in
    gcc)   cmake_extra+=("-DCMAKE_C_COMPILER=gcc"   "-DCMAKE_CXX_COMPILER=g++");;
    clang) cmake_extra+=("-DCMAKE_C_COMPILER=clang" "-DCMAKE_CXX_COMPILER=clang++");;
    "")    : ;;
    *)     die "unknown compiler '${cxx}' (gcc|clang)";;
  esac

  cmake -S "${REPO_ROOT}" -B "${cell_dir}" -G Ninja \
    "-DCMAKE_BUILD_TYPE=${build_type}" \
    "${cmake_extra[@]}" \
    "${MLIR_DIR:+-DMLIR_DIR=${MLIR_DIR}}" \
    "${CIRCT_DIR:+-DCIRCT_DIR=${CIRCT_DIR}}"
  cmake --build "${cell_dir}"

  log "smoke: ${cell_dir}/bin/nslc --version"
  "${cell_dir}/bin/nslc" --version

  if [[ "${build_type}" == "Release" && "${cxx}" == "gcc" ]]; then
    log "determinism gate (Release × gcc): rebuilding into ${DETERMINISM_BUILD_DIR}"
    cmake -S "${REPO_ROOT}" -B "${DETERMINISM_BUILD_DIR}" -G Ninja \
      "-DCMAKE_BUILD_TYPE=Release" \
      "-DCMAKE_C_COMPILER=gcc" "-DCMAKE_CXX_COMPILER=g++" \
      "${MLIR_DIR:+-DMLIR_DIR=${MLIR_DIR}}" \
      "${CIRCT_DIR:+-DCIRCT_DIR=${CIRCT_DIR}}"
    cmake --build "${DETERMINISM_BUILD_DIR}"
    "${REPO_ROOT}/scripts/check_determinism.sh" \
      "${cell_dir}" "${DETERMINISM_BUILD_DIR}"
  fi
}

stage_build_matrix() {
  if [[ "${1:-}" == "--matrix" ]]; then
    for build_type in Debug Release; do
      for cxx in gcc clang; do
        _one_cell "${build_type}" "${cxx}"
      done
    done
  else
    _one_cell "${1:-Release}" "${2:-}"
  fi
}

# -----------------------------------------------------------------------------
# Stage 2: static-checks
# -----------------------------------------------------------------------------

stage_static_checks() {
  log "stage 2: static-checks"
  local rc=0

  # 1. clang-format dry-run (FR-009).
  log "  clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.h' '*.cc' '*.hpp' | wc -l) files"
  local format_files
  format_files="$(git ls-files '*.cpp' '*.h' '*.cc' '*.hpp' || true)"
  if [[ -n "${format_files}" ]]; then
    # shellcheck disable=SC2086
    clang-format --dry-run --Werror ${format_files} || rc=$?
  fi

  # 2. clang-tidy (FR-008). Reuses compile_commands.json from a build dir.
  if [[ -f "${DEFAULT_BUILD_DIR}/compile_commands.json" ]]; then
    local tidy_files
    tidy_files="$(git ls-files '*.cpp' || true)"
    if [[ -n "${tidy_files}" ]]; then
      log "  clang-tidy -p ${DEFAULT_BUILD_DIR}"
      # shellcheck disable=SC2086
      clang-tidy -p "${DEFAULT_BUILD_DIR}" ${tidy_files} || rc=$?
    fi
  else
    log "  (skipping clang-tidy: ${DEFAULT_BUILD_DIR}/compile_commands.json absent)"
  fi

  # 3. SPDX-header presence check, full-repo scan (FR-010, spec Q4).
  if [[ -x "${REPO_ROOT}/scripts/check_spdx.py" ]]; then
    log "  python3 scripts/check_spdx.py --all"
    python3 "${REPO_ROOT}/scripts/check_spdx.py" --all || rc=$?
  else
    log "  (skipping SPDX check: scripts/check_spdx.py not yet present — lands at T065)"
  fi

  return "${rc}"
}

# -----------------------------------------------------------------------------
# Stage 3: unit-tests
# -----------------------------------------------------------------------------

stage_unit_tests() {
  log "stage 3: unit-and-layer-tests"
  local build_dir="${1:-${DEFAULT_BUILD_DIR}}"
  ctest --test-dir "${build_dir}" --output-on-failure
}

# -----------------------------------------------------------------------------
# Stage 4: lowering-tests
# -----------------------------------------------------------------------------

stage_lowering_tests() {
  log "stage 4: lowering-tests (lit)"
  local build_dir="${1:-${DEFAULT_BUILD_DIR}}"
  cmake --build "${build_dir}" --target check-nslc
}

# -----------------------------------------------------------------------------
# Stages 5 and 6: wired-but-empty
# -----------------------------------------------------------------------------

stage_e2e() {
  log "stage 5 (end-to-end): wired but empty until M7 — see roadmap M7."
  exit 0
}

stage_formal() {
  log "stage 6 (formal): wired but empty until M8 — see roadmap M8 and Principle VI."
  exit 0
}

# -----------------------------------------------------------------------------
# stage_all
# -----------------------------------------------------------------------------

stage_all() {
  stage_build_matrix
  stage_static_checks
  stage_unit_tests
  stage_lowering_tests
  log "all stages green"
}

# -----------------------------------------------------------------------------
# Dispatcher
# -----------------------------------------------------------------------------

case "${1:-}" in
  build-matrix)     shift; stage_build_matrix    "$@" ;;
  static-checks)    stage_static_checks ;;
  unit-tests)       shift; stage_unit_tests      "$@" ;;
  lowering-tests)   shift; stage_lowering_tests  "$@" ;;
  e2e)              stage_e2e ;;
  formal)           stage_formal ;;
  all)              stage_all ;;
  -h|--help)        usage; exit 0 ;;
  "")               usage; exit 2 ;;
  *)
    printf '[ci.sh] unknown stage %q\n' "$1" >&2
    usage
    exit 2
    ;;
esac
