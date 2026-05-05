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

  # Optional: opt-out of ASan for this cell. The lowering-tests
  # job sets NSL_ENABLE_ASAN=OFF because lit invokes nsl-opt /
  # nslc, which load MLIR's BuiltinDialect during MLIRContext
  # construction. The vendored libMLIR.so under /opt/llvm is built
  # WITHOUT ASan, which clashes with the instrumented binary's
  # SmallVector container annotations and fires use-after-poison
  # on every test. Other cells (build-matrix, unit-tests) keep
  # ASan on, so the project still gets full sanitizer coverage.
  local asan_opt=()
  if [[ -n "${NSL_ENABLE_ASAN:-}" ]]; then
    asan_opt+=("-DNSL_ENABLE_ASAN=${NSL_ENABLE_ASAN}")
  fi

  cmake -S "${REPO_ROOT}" -B "${cell_dir}" -G Ninja \
    "-DCMAKE_BUILD_TYPE=${build_type}" \
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON" \
    "${cmake_extra[@]}" \
    "${asan_opt[@]}" \
    "${MLIR_DIR:+-DMLIR_DIR=${MLIR_DIR}}" \
    "${CIRCT_DIR:+-DCIRCT_DIR=${CIRCT_DIR}}"
  cmake --build "${cell_dir}"

  log "smoke: ${cell_dir}/bin/nslc --version"
  "${cell_dir}/bin/nslc" --version

  if [[ "${build_type}" == "Release" && "${cxx}" == "gcc" ]]; then
    log "determinism gate (Release × gcc): rebuilding into ${DETERMINISM_BUILD_DIR}"
    cmake -S "${REPO_ROOT}" -B "${DETERMINISM_BUILD_DIR}" -G Ninja \
      "-DCMAKE_BUILD_TYPE=Release" \
      "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON" \
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

  # `git ls-files` inside a Docker bind-mount fails with "dubious
  # ownership" when the host uid/gid differ from the in-container
  # uid; `-c safe.directory=*` is the standard workaround.
  local _git=(git -c safe.directory=*)

  # 1. clang-format dry-run (FR-009).
  local format_files
  format_files="$("${_git[@]}" ls-files '*.cpp' '*.h' '*.cc' '*.hpp' || true)"
  local format_count
  format_count="$(printf '%s\n' "${format_files}" | grep -c .)"
  log "  clang-format --dry-run --Werror ${format_count} files"
  if [[ -n "${format_files}" ]]; then
    # shellcheck disable=SC2086
    clang-format --dry-run --Werror ${format_files} || rc=$?
  fi

  # 2. clang-tidy (FR-008). Reuses compile_commands.json from a build
  # dir. Look at the conventional name first; fall back to whichever
  # build-Release-* dir was produced by `./scripts/ci.sh build-matrix`.
  local tidy_build_dir=""
  for candidate in "${DEFAULT_BUILD_DIR}" \
                   "${REPO_ROOT}/build-Release-gcc" \
                   "${REPO_ROOT}/build-Release-clang" \
                   "${REPO_ROOT}/build-Release-host"; do
    if [[ -f "${candidate}/compile_commands.json" ]]; then
      tidy_build_dir="${candidate}"
      break
    fi
  done
  if [[ -z "${tidy_build_dir}" ]]; then
    die "clang-tidy needs compile_commands.json in build-*; run" \
        "\`./scripts/ci.sh build-matrix\` first"
  fi
  local tidy_files
  tidy_files="$("${_git[@]}" ls-files '*.cpp' || true)"
  if [[ -n "${tidy_files}" ]]; then
    log "  clang-tidy --warnings-as-errors=* -p ${tidy_build_dir}"
    # shellcheck disable=SC2086
    clang-tidy --warnings-as-errors='*' -p "${tidy_build_dir}" ${tidy_files} \
      || rc=$?
  fi

  # 3. SPDX-header presence check, full-repo scan (FR-010, spec Q4).
  if [[ -x "${REPO_ROOT}/scripts/check_spdx.py" ]]; then
    log "  python3 scripts/check_spdx.py --all"
    python3 "${REPO_ROOT}/scripts/check_spdx.py" --all || rc=$?
  else
    log "  (skipping SPDX check: scripts/check_spdx.py not yet present — lands at T065)"
  fi

  # 4. M4 dialect fixture-coverage guard (spec FR-021 + research.md §9).
  # Vacuous at Phase 2 (op set empty); goes live as Phase 3 / 4
  # populate `lib/Dialect/NSL/IR/NSLOps.td` and
  # `.specify/m4_invariant_table.json`. The script self-locates the
  # repo root so it runs identically inside and outside the dev
  # container.
  if [[ -x "${REPO_ROOT}/scripts/check_dialect_coverage.py" ]]; then
    log "  python3 scripts/check_dialect_coverage.py"
    python3 "${REPO_ROOT}/scripts/check_dialect_coverage.py" --quiet \
      || rc=$?
  else
    log "  (skipping dialect-coverage check: scripts/check_dialect_coverage.py not yet present)"
  fi

  # 5. M5 lower-fixture coverage guard (M5 spec FR-027 + SC-001).
  # For every concrete `visit()` override on `ASTToMLIR` in
  # `lib/Lower/ASTToMLIR.cpp`, assert a paired fixture exists under
  # `test/Lower/`. Goes live with US1 close-out (T057 + T058); the
  # script self-locates the repo root + carries an in-script ALLOWLIST
  # for visitors that intentionally have no dedicated fixture (e.g.,
  # expression-position lowerExpr delegators). Missing-fixture cases
  # without an allow-list entry MUST fail CI per Principle IX.
  if [[ -x "${REPO_ROOT}/scripts/audit_lower_fixtures.sh" ]]; then
    log "  bash scripts/audit_lower_fixtures.sh"
    bash "${REPO_ROOT}/scripts/audit_lower_fixtures.sh" \
      || rc=$?
  else
    log "  (skipping lower-fixture audit: scripts/audit_lower_fixtures.sh not yet present)"
  fi

  # 6. M5 US5 / T102 determinism source-audit (FR-025 + research §13).
  # Greps `lib/Lower/` for forbidden patterns that leak non-
  # determinism into the visitor + the six structural-expansion
  # passes (std::unordered_*, reinterpret_cast<uintptr_t>,
  # std::time, std::chrono, std::random_*, std::mt19937, getpid,
  # gettimeofday). Goes live with US5 close-out (T102 + T103);
  # CI-blocking on match per Constitution Principle V + Principle IX.
  if [[ -x "${REPO_ROOT}/scripts/audit_determinism.sh" ]]; then
    log "  bash scripts/audit_determinism.sh"
    bash "${REPO_ROOT}/scripts/audit_determinism.sh" \
      || rc=$?
  else
    log "  (skipping determinism source-audit: scripts/audit_determinism.sh not yet present)"
  fi

  # 7. M5 US5 / T101 cross-host-path determinism check
  # (FR-025 + FR-026 + FR-029 + driver-emit-mlir.contract.md §3).
  # Builds the toolchain twice in distinct host paths, runs
  # `nslc -emit=mlir` on `test/Lower/determinism/canonical_smoke.nsl`
  # in each tree, byte-compares outputs, greps for forbidden host-
  # path / wall-clock / pointer-address patterns. Expensive
  # (~minutes — full toolchain build × 2); opt-in via the
  # `NSLC_RUN_DETERMINISM_CHECK=1` env var so PR-validation runs
  # exercise it while local fast iterations skip it. Goes live with
  # US5 close-out (T103).
  if [[ -x "${REPO_ROOT}/scripts/determinism_check.sh" \
        && "${NSLC_RUN_DETERMINISM_CHECK:-0}" == "1" ]]; then
    log "  bash scripts/determinism_check.sh (NSLC_RUN_DETERMINISM_CHECK=1)"
    bash "${REPO_ROOT}/scripts/determinism_check.sh" \
      || rc=$?
  elif [[ -x "${REPO_ROOT}/scripts/determinism_check.sh" ]]; then
    log "  (skipping cross-host-path determinism check; set NSLC_RUN_DETERMINISM_CHECK=1 to opt in)"
  else
    log "  (skipping cross-host-path determinism check: scripts/determinism_check.sh not yet present)"
  fi

  # 7b. M6 US5 / T132 cross-host-path determinism check for `-emit=hw`
  # (FR-022 + driver-emit-hw.contract.md §6). Sibling of step 7;
  # extends the M5 mechanism to the M6 driver layer per Phase-7 of
  # `specs/010-m6-circt-lowering/tasks.md`. Same opt-in env var as
  # the M5 sibling — both gates run together when
  # NSLC_RUN_DETERMINISM_CHECK=1 is set.
  if [[ -x "${REPO_ROOT}/scripts/determinism_check_emit_hw.sh" \
        && "${NSLC_RUN_DETERMINISM_CHECK:-0}" == "1" ]]; then
    log "  bash scripts/determinism_check_emit_hw.sh (NSLC_RUN_DETERMINISM_CHECK=1)"
    bash "${REPO_ROOT}/scripts/determinism_check_emit_hw.sh" \
      || rc=$?
  elif [[ -x "${REPO_ROOT}/scripts/determinism_check_emit_hw.sh" ]]; then
    log "  (skipping cross-host-path -emit=hw determinism check; set NSLC_RUN_DETERMINISM_CHECK=1 to opt in)"
  else
    log "  (skipping cross-host-path -emit=hw determinism check: scripts/determinism_check_emit_hw.sh not yet present)"
  fi

  # 8. M5 T110 / FR-008 + SC-009 op-location audit
  # (closes /speckit-analyze 2026-04-30 findings A3 + A4).
  # Verifies every emitted nsl::* op carries a non-trivial
  # mlir::Location (FileLineColLoc or FusedLoc — never UnknownLoc).
  # **DEFERRED at M5 ship**: visitor uses builder_.getUnknownLoc()
  # universally pending the SourceManager <-> mlir::MLIRContext
  # location-translator adapter (post-M5 amendment). Until that
  # lands, the audit short-circuits with a deferred-status banner
  # (see scripts/audit_op_locations.sh header). Opt in to run the
  # real enforcement via NSLC_RUN_LOCATION_AUDIT=1.
  if [[ -x "${REPO_ROOT}/scripts/audit_op_locations.sh" ]]; then
    log "  bash scripts/audit_op_locations.sh"
    bash "${REPO_ROOT}/scripts/audit_op_locations.sh" \
      || rc=$?
  else
    log "  (skipping op-location audit: scripts/audit_op_locations.sh not yet present)"
  fi

  return "${rc}"
}

# -----------------------------------------------------------------------------
# Stage 3: unit-tests
# -----------------------------------------------------------------------------

_resolve_build_dir() {
  if [[ -n "${1:-}" && -d "${1}" ]]; then
    printf '%s' "${1}"
    return 0
  fi
  for candidate in "${DEFAULT_BUILD_DIR}" \
                   "${REPO_ROOT}/build-Release-gcc" \
                   "${REPO_ROOT}/build-Release-clang" \
                   "${REPO_ROOT}/build-Release-host" \
                   "${REPO_ROOT}/build-Debug-gcc" \
                   "${REPO_ROOT}/build-Debug-clang"; do
    if [[ -f "${candidate}/CMakeCache.txt" ]]; then
      printf '%s' "${candidate}"
      return 0
    fi
  done
  return 1
}

stage_unit_tests() {
  log "stage 3: unit-and-layer-tests"
  local build_dir
  build_dir="$(_resolve_build_dir "${1:-}")" || \
    die "no configured build dir; run \`./scripts/ci.sh build-matrix\` first"
  ctest --test-dir "${build_dir}" --output-on-failure
}

# -----------------------------------------------------------------------------
# Stage 4: lowering-tests
# -----------------------------------------------------------------------------

stage_lowering_tests() {
  log "stage 4: lowering-tests (lit)"
  local build_dir
  build_dir="$(_resolve_build_dir "${1:-}")" || \
    die "no configured build dir; run \`./scripts/ci.sh build-matrix\` first"
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
