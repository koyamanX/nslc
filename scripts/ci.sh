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
  static-checks              stage 2 (clang-format + clang-tidy + SPDX header
                                       + tooling-grammar regen + mirror byte-eq)
  unit-tests                 stage 3 (ctest + tooling-textmate scope tests
                                       + tooling-treesitter smoke/golden)
  tooling-textmate           stage 3 sub-step (TextMate scope tests only;
                                       T1: specs/009-t1-textmate-grammar)
  tooling-treesitter         stage 3 sub-step (tree-sitter smoke + golden
                                       highlight tests; T8:
                                       specs/010-t8-tree-sitter-grammar)
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
  # `grammars/treesitter/src/` is the tree-sitter generator output
  # (parser.c plus the bundled `tree_sitter/{alloc,array,parser}.h`
  # ABI headers — third-party content); never hand-edited and
  # therefore exempt from project clang-format style. The companion
  # `treesitter-grammar-regen-diff` sub-step (below in stage 2)
  # gates byte-stability of this directory instead.
  local format_files
  format_files="$("${_git[@]}" ls-files '*.cpp' '*.h' '*.cc' '*.hpp' \
                   ':!grammars/treesitter/src/**' || true)"
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

  # 9. T1 / FR-001 + SC-003 — tooling-grammar regen-and-diff
  # (specs/009-t1-textmate-grammar/data-model.md §3 / §4).
  # Re-runs `gen_textmate_grammar.py` and `gen_textmate_fixtures.py`,
  # then `git diff --exit-code` over the generated artefacts.
  # A spec-side `KeywordSet.def` edit that lands without a parallel
  # grammar regenerate fails CI here. Constitution Principle VII
  # spec-↔-grammar coupling becomes mechanical.
  if [[ -x "${REPO_ROOT}/scripts/gen_textmate_grammar.py" \
        && -x "${REPO_ROOT}/scripts/gen_textmate_fixtures.py" ]]; then
    log "  python3 scripts/gen_textmate_grammar.py --check"
    python3 "${REPO_ROOT}/scripts/gen_textmate_grammar.py" --check \
      || rc=$?
    log "  python3 scripts/gen_textmate_fixtures.py --check"
    python3 "${REPO_ROOT}/scripts/gen_textmate_fixtures.py" --check \
      || rc=$?
  else
    log "  (skipping tooling-grammar regen check: generators not yet present)"
  fi

  # 9b. T8 / FR-006 — treesitter-spdx
  # (specs/010-t8-tree-sitter-grammar/spec.md FR-006). Validates SPDX
  # headers on the focused T8 path-set (grammar.js, highlights.scm,
  # gen_treesitter_grammar.py, corpus.txt, fixtures). The full-repo
  # check_spdx --all gate above already covers these; this focused
  # sub-step exists for fast-iteration debugging when a T8 author
  # lands a new path. Phase-2 soft-skip for paths that don't exist
  # yet (highlights.scm + corpus.txt + fixtures arrive in US1/US2).
  local ts_paths=()
  for p in \
    "scripts/templates/grammar.js.template" \
    "scripts/gen_treesitter_grammar.py" \
    "grammars/treesitter/grammar.js" \
    "grammars/treesitter/queries/highlights.scm" \
    "grammars/treesitter/queries/locals.scm" \
    "test/tooling/treesitter/smoke/corpus.txt"; do
    [[ -f "${REPO_ROOT}/${p}" ]] && ts_paths+=("${p}")
  done
  if (( ${#ts_paths[@]} > 0 )); then
    log "  python3 scripts/check_spdx.py [T8 focused subset]"
    ( cd "${REPO_ROOT}" && python3 scripts/check_spdx.py "${ts_paths[@]}" ) \
      || rc=$?
  else
    log "  (skipping treesitter-spdx: no T8 source paths present yet)"
  fi

  # 9c. T8 / FR-017 — treesitter-grammar-regen-diff
  # (specs/010-t8-tree-sitter-grammar/research.md §12). Two-stage:
  # first the Python generator's --check (template + KeywordSet.def
  # → grammar.js byte-stable), then `tree-sitter generate` + git
  # diff over parser.c, grammar.json, node-types.json. The second
  # stage soft-skips when npm/npx are unavailable; CI's GitHub
  # Actions run inside a node-bearing image, so the gate fires
  # there.
  if [[ -x "${REPO_ROOT}/scripts/gen_treesitter_grammar.py" ]]; then
    log "  python3 scripts/gen_treesitter_grammar.py --check"
    python3 "${REPO_ROOT}/scripts/gen_treesitter_grammar.py" --check \
      || rc=$?
  else
    log "  (skipping treesitter Python regen-check: generator not yet present)"
  fi
  local ts_grammar_dir="${REPO_ROOT}/grammars/treesitter"
  if [[ ! -f "${ts_grammar_dir}/grammar.js" ]]; then
    log "  (skipping treesitter-grammar-regen-diff: grammar.js not yet present)"
  elif [[ ! -d "${ts_grammar_dir}/node_modules/tree-sitter-cli" ]]; then
    log "  (skipping treesitter-grammar-regen-diff: node_modules/tree-sitter-cli"
    log "   not present; run \`cd ${ts_grammar_dir} && npm ci\`)"
  elif ! command -v npx >/dev/null 2>&1; then
    log "  (skipping treesitter-grammar-regen-diff: npx not found on PATH)"
  else
    log "  cd grammars/treesitter && npx tree-sitter generate --no-bindings && git diff --exit-code"
    ( cd "${ts_grammar_dir}" && npx --no-install tree-sitter generate --no-bindings ) \
      || rc=$?
    ( cd "${REPO_ROOT}" && git -c safe.directory=* diff --exit-code -- \
        grammars/treesitter/src/parser.c \
        grammars/treesitter/src/grammar.json \
        grammars/treesitter/src/node-types.json ) \
      || rc=$?
  fi

  # 9d. T8 / SC-008 — treesitter-wasm-determinism
  # (specs/010-t8-tree-sitter-grammar/spec.md Q2 → Option C; build
  # the WASM artefact twice under `--docker` and sha256sum-compare).
  # Soft-skip on environments without the Docker-in-Docker support
  # tree-sitter's WASM builder needs (research.md §1 / §7).
  if [[ ! -f "${ts_grammar_dir}/grammar.js" ]]; then
    log "  (skipping treesitter-wasm-determinism: grammar.js not yet present)"
  elif [[ ! -d "${ts_grammar_dir}/node_modules/tree-sitter-cli" ]]; then
    log "  (skipping treesitter-wasm-determinism: node_modules/tree-sitter-cli"
    log "   not present; run \`cd ${ts_grammar_dir} && npm ci\`)"
  elif ! command -v npx >/dev/null 2>&1; then
    log "  (skipping treesitter-wasm-determinism: npx not found on PATH)"
  elif ! command -v sha256sum >/dev/null 2>&1; then
    log "  (skipping treesitter-wasm-determinism: sha256sum not found on PATH)"
  else
    log "  cd grammars/treesitter && npx tree-sitter build-wasm --docker (×2)"
    ( cd "${ts_grammar_dir}" && npx --no-install tree-sitter build-wasm --docker ) \
      || rc=$?
    local sum1=""
    [[ -f "${ts_grammar_dir}/tree-sitter-nsl.wasm" ]] \
      && sum1="$(sha256sum "${ts_grammar_dir}/tree-sitter-nsl.wasm" | awk '{print $1}')"
    ( cd "${ts_grammar_dir}" && npx --no-install tree-sitter build-wasm --docker ) \
      || rc=$?
    local sum2=""
    [[ -f "${ts_grammar_dir}/tree-sitter-nsl.wasm" ]] \
      && sum2="$(sha256sum "${ts_grammar_dir}/tree-sitter-nsl.wasm" | awk '{print $1}')"
    if [[ -n "${sum1}" && -n "${sum2}" && "${sum1}" != "${sum2}" ]]; then
      log "  ERROR: tree-sitter-nsl.wasm is non-deterministic across two builds"
      log "         run #1 sha256: ${sum1}"
      log "         run #2 sha256: ${sum2}"
      rc=1
    fi
  fi

  # 10. T1 / FR-013 — tooling-grammar-mirror byte-equality.
  # Per specs/009-t1-textmate-grammar/research.md §5 (as amended by
  # the PR #13 CodeRabbit review): the canonical artefact lives at
  # `grammars/textmate/nsl.tmLanguage.json` and a materialised copy
  # at `editors/vscode/syntaxes/nsl.tmLanguage.json` is written in
  # lockstep by `scripts/gen_textmate_grammar.py`. The materialised-
  # copy approach replaces an earlier symlink design which broke on
  # Windows / zip-archive extraction (the symlink became a literal
  # path string). The byte-equality check below catches drift OR a
  # missing mirror.
  local canonical="${REPO_ROOT}/grammars/textmate/nsl.tmLanguage.json"
  local mirror="${REPO_ROOT}/editors/vscode/syntaxes/nsl.tmLanguage.json"
  if [[ ! -f "${canonical}" ]]; then
    log "  (skipping tooling-grammar-mirror: canonical not yet present)"
  elif [[ ! -f "${mirror}" ]]; then
    log "  ERROR: editors/vscode/syntaxes/nsl.tmLanguage.json is"
    log "         missing while the canonical grammar exists."
    log "         Run: python3 scripts/gen_textmate_grammar.py"
    log "         (the generator writes both paths in lockstep)."
    rc=1
  else
    log "  cmp ${canonical} ${mirror}"
    if ! cmp -s "${canonical}" "${mirror}"; then
      log "  ERROR: editors/vscode/syntaxes/nsl.tmLanguage.json"
      log "         is not byte-equal to the canonical grammar."
      log "         Run: python3 scripts/gen_textmate_grammar.py"
      log "         (the generator writes both paths in lockstep)."
      rc=1
    fi
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
  local rc=0
  ctest --test-dir "${build_dir}" --output-on-failure || rc=$?

  # T1 — tooling-track TextMate scope tests
  # (specs/009-t1-textmate-grammar/contracts/scope-test-format.contract.md §3).
  # Layer test for the TextMate grammar; runs on a clean checkout
  # without `nslc` (FR-019). No-op skip when Node / npm / the
  # vscode-tmgrammar-test package are absent so a partial dev
  # container does not block the rest of stage 3.
  stage_tooling_textmate || rc=$?

  # T8 — tooling-track tree-sitter smoke + golden-highlight tests
  # (specs/010-t8-tree-sitter-grammar/contracts/grammar-coverage.contract.md §4
  # for smoke; contracts/highlights-coverage.contract.md §5 for goldens).
  # Same soft-skip discipline as tooling-textmate: a partial dev
  # container without npx / node_modules does not block stage 3.
  stage_tooling_treesitter || rc=$?

  return "${rc}"
}

# -----------------------------------------------------------------------------
# Stage 3 sub-step: tooling-textmate
# -----------------------------------------------------------------------------
# Runs the vscode-tmgrammar-test scope-test gate against the
# canonical grammar `grammars/textmate/nsl.tmLanguage.json` and the
# fixtures under `test/tooling/textmate/fixtures/`. Per
# `contracts/scope-test-format.contract.md §4` the runner has zero
# compiler dependency — Node + npm + the cached package suffice.
#
# Note: the actual `vscode-tmgrammar-test` runner does NOT consume
# the YAML `.spec` files described in contract §2; assertions live
# inside the fixture files themselves (`// SYNTAX TEST` line-1
# header + `// <-` / `// ^^^` inline assertions). This is a
# contract amendment proposed in T1 close-out; the runner format
# rules.

stage_tooling_textmate() {
  log "stage 3 sub-step: tooling-textmate"
  local fixture_dir="${REPO_ROOT}/test/tooling/textmate"
  local grammar="${REPO_ROOT}/grammars/textmate/nsl.tmLanguage.json"

  if [[ ! -f "${grammar}" ]]; then
    log "  (skipping tooling-textmate: ${grammar} not present)"
    return 0
  fi
  if [[ ! -d "${fixture_dir}/node_modules/vscode-tmgrammar-test" ]]; then
    log "  (skipping tooling-textmate: node_modules/vscode-tmgrammar-test"
    log "   not present; run \`cd ${fixture_dir} && npm install\`)"
    return 0
  fi
  if ! command -v npx >/dev/null 2>&1; then
    log "  (skipping tooling-textmate: npx not found on PATH)"
    return 0
  fi

  log "  npx vscode-tmgrammar-test --grammar ../../../grammars/textmate/nsl.tmLanguage.json 'fixtures/*.nsl'"
  ( cd "${fixture_dir}" && \
    npx --no-install vscode-tmgrammar-test \
      --grammar ../../../grammars/textmate/nsl.tmLanguage.json \
      'fixtures/*.nsl' )
}

# -----------------------------------------------------------------------------
# Stage 3 sub-step: tooling-treesitter
# -----------------------------------------------------------------------------
# Runs two tree-sitter gates back-to-back:
#
#   (a) treesitter-smoke           — read corpus.txt, parse every
#                                    file with `tree-sitter parse`,
#                                    fail on any (ERROR) or (MISSING)
#                                    node (FR-014).
#   (b) treesitter-highlights-golden — `tree-sitter test` against the
#                                    inline-assertion fixtures under
#                                    test/tooling/treesitter/highlights/
#                                    (per FR-007 / FR-009 / FR-010).
#
# Both sub-steps soft-skip when their inputs (corpus.txt or fixtures)
# don't yet exist — Phase 2 leaves them as XFAIL placeholders;
# US1/US2 (T035, T036) lights them up.

stage_tooling_treesitter() {
  log "stage 3 sub-step: tooling-treesitter"
  local rc=0
  local ts_grammar_dir="${REPO_ROOT}/grammars/treesitter"
  local corpus_file="${REPO_ROOT}/test/tooling/treesitter/smoke/corpus.txt"
  local fixtures_dir="${REPO_ROOT}/test/tooling/treesitter/highlights"

  if [[ ! -f "${ts_grammar_dir}/grammar.js" ]]; then
    log "  (skipping tooling-treesitter: ${ts_grammar_dir}/grammar.js not present)"
    return 0
  fi
  if [[ ! -d "${ts_grammar_dir}/node_modules/tree-sitter-cli" ]]; then
    log "  (skipping tooling-treesitter: node_modules/tree-sitter-cli"
    log "   not present; run \`cd ${ts_grammar_dir} && npm ci\`)"
    return 0
  fi
  if ! command -v npx >/dev/null 2>&1; then
    log "  (skipping tooling-treesitter: npx not found on PATH)"
    return 0
  fi

  # (a) treesitter-smoke — corpus parse gate.
  if [[ ! -f "${corpus_file}" ]]; then
    log "  (skipping treesitter-smoke: ${corpus_file} not present)"
  else
    log "  treesitter-smoke: parsing corpus listed in ${corpus_file}"
    local n_smoke=0
    while IFS= read -r raw; do
      local entry="${raw%%#*}"
      entry="${entry#"${entry%%[![:space:]]*}"}"
      entry="${entry%"${entry##*[![:space:]]}"}"
      [[ -z "${entry}" ]] && continue
      n_smoke=$((n_smoke + 1))
      ( cd "${ts_grammar_dir}" && \
        npx --no-install tree-sitter parse --quiet --stat "${REPO_ROOT}/${entry}" \
        >/dev/null ) || rc=$?
    done <"${corpus_file}"
    log "  treesitter-smoke: parsed ${n_smoke} files"
  fi

  # (b) treesitter-highlights-golden — `tree-sitter test`.
  local has_fixtures=0
  if [[ -d "${fixtures_dir}" ]]; then
    if compgen -G "${fixtures_dir}/*.nsl" >/dev/null; then
      has_fixtures=1
    fi
  fi
  if (( has_fixtures == 0 )); then
    log "  (skipping treesitter-highlights-golden: no fixtures in ${fixtures_dir})"
  else
    log "  cd grammars/treesitter && npx tree-sitter test"
    ( cd "${ts_grammar_dir}" && npx --no-install tree-sitter test ) \
      || rc=$?
  fi

  return "${rc}"
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
  build-matrix)      shift; stage_build_matrix    "$@" ;;
  static-checks)     stage_static_checks ;;
  unit-tests)        shift; stage_unit_tests      "$@" ;;
  tooling-textmate)  stage_tooling_textmate ;;
  tooling-treesitter) stage_tooling_treesitter ;;
  lowering-tests)    shift; stage_lowering_tests  "$@" ;;
  e2e)               stage_e2e ;;
  formal)            stage_formal ;;
  all)               stage_all ;;
  -h|--help)         usage; exit 0 ;;
  "")                usage; exit 2 ;;
  *)
    printf '[ci.sh] unknown stage %q\n' "$1" >&2
    usage
    exit 2
    ;;
esac
