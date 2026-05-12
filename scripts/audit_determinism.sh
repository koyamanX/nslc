#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# scripts/audit_determinism.sh — M5 US5 / T102 determinism-audit grep.
#
# Greps `lib/Lower/` for source-level patterns that leak non-
# determinism into the visitor + the six structural-expansion passes.
# The pattern set is the canonical "forbidden constructs" table from
# `specs/008-m5-structural-passes/research.md` §13 — pointer-derived
# ordering, time / pid / random sources, address-derived hashes.
#
# This is a CI-blocking static check; it runs in `scripts/ci.sh`
# stage 2 (static-checks) immediately after
# `scripts/audit_lower_fixtures.sh`. The script exits non-zero on the
# first match, prints the offending file/line/pattern to stderr, and
# refers the operator to the alternative (`llvm::DenseMap`,
# `llvm::SmallDenseMap`, AST-source-order iteration, etc.) per
# Constitution Principle V + research.md §13.
#
# Forbidden patterns:
#
#   std::unordered_map / std::unordered_set / std::unordered_multimap
#                       — hash-iteration order is implementation-
#                         defined; replace with `llvm::DenseMap` (or
#                         `llvm::MapVector` if iteration order is
#                         load-bearing).
#   reinterpret_cast<uintptr_t>
#                       — pointer-bits-as-integer is the canonical
#                         non-determinism source; if you need a
#                         stable identity, use the lexical-emission
#                         counter (`nextTempId_++`).
#   std::time(          — wall-clock seconds; never appears in
#                         deterministic compiler output.
#   std::chrono::       — same as above.
#   std::random_device  — entropy source; never appears in lowering.
#   std::random_engine  — same as above.
#   std::mt19937        — same as above.
#   getpid              — process-id-derived hashing.
#   gettimeofday        — wall-clock μsec.
#
# The grep is strict (extended regex, line-anchored where useful)
# and runs only on `*.cpp` / `*.h` files — generated files, build/
# directory, third-party submodules, and `test/` are skipped by
# construction (we never recurse outside `lib/Lower/`).
#
# Usage:
#   bash scripts/audit_determinism.sh
#   bash scripts/audit_determinism.sh --quiet    # only summary line
#   bash scripts/audit_determinism.sh --verbose  # per-pattern decision
#
# Exit codes:
#   0  zero forbidden patterns matched
#   1  one or more forbidden patterns matched
#   2  internal error (missing source tree, etc.)

set -euo pipefail

# -----------------------------------------------------------------------------
# Locate repo root + lower source tree
# -----------------------------------------------------------------------------

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
# M5 + M6 originated this audit on lib/Lower; M7 extends the audit
# surface to lib/Driver (per specs/011-m7-driver-e2e/tasks.md T019)
# because M7 adds new TUs there (EmitVerilog.cpp, RunCIRCTPasses.cpp)
# whose Verilog-bytes output must be byte-deterministic per FR-005.
readonly LOWER_ROOT="${REPO_ROOT}/lib/Lower"
readonly DRIVER_ROOT="${REPO_ROOT}/lib/Driver"
readonly AUDIT_ROOTS=("${LOWER_ROOT}" "${DRIVER_ROOT}")

QUIET=0
VERBOSE=0
case "${1:-}" in
  --quiet)   QUIET=1 ;;
  --verbose) VERBOSE=1 ;;
  "") : ;;
  *)
    printf '[audit_determinism] error: unknown arg %q (expected --quiet | --verbose | <none>)\n' \
      "$1" >&2
    exit 2
    ;;
esac

log()    { (( QUIET )) || printf '[audit_determinism] %s\n' "$*"; }
detail() { (( VERBOSE )) && printf '[audit_determinism]   %s\n' "$*"; return 0; }

for root in "${AUDIT_ROOTS[@]}"; do
  if [[ ! -d "${root}" ]]; then
    printf '[audit_determinism] error: cannot read directory %s\n' "${root}" >&2
    exit 2
  fi
done

# -----------------------------------------------------------------------------
# Forbidden patterns table (research.md §13)
# -----------------------------------------------------------------------------
#
# Each row: <pattern> <one-line rationale shown on match>.
# The `rationale` field is appended to the per-line stderr output to
# steer the operator to the canonical replacement.

declare -a FORBIDDEN=(
  'std::unordered_map'
  'std::unordered_set'
  'std::unordered_multimap'
  'std::unordered_multiset'
  'reinterpret_cast<uintptr_t>'
  'std::time\('
  'std::chrono::'
  'std::random_device'
  'std::random_engine'
  'std::mt19937'
  'getpid\(\)'
  'gettimeofday\('
)

declare -A RATIONALE=(
  ['std::unordered_map']='hash-iteration order undefined; use llvm::DenseMap'
  ['std::unordered_set']='hash-iteration order undefined; use llvm::DenseSet'
  ['std::unordered_multimap']='hash-iteration order undefined; use llvm::DenseMap<K, SmallVector<V>>'
  ['std::unordered_multiset']='hash-iteration order undefined; use llvm::DenseMap<K, unsigned>'
  ['reinterpret_cast<uintptr_t>']='pointer-bits-as-integer non-determinism; use lexical-emission counter (nextTempId_++)'
  ['std::time\(']='wall-clock; never appears in deterministic compiler output'
  ['std::chrono::']='wall-clock; never appears in deterministic compiler output'
  ['std::random_device']='entropy source; lowering must be reproducible'
  ['std::random_engine']='entropy source; lowering must be reproducible'
  ['std::mt19937']='entropy source; lowering must be reproducible'
  ['getpid\(\)']='process-id-derived non-determinism'
  ['gettimeofday\(']='wall-clock μsec non-determinism'
)

# -----------------------------------------------------------------------------
# Per-pattern grep (line-anchored stderr report)
# -----------------------------------------------------------------------------

FAIL=0
TOTAL_MATCHES=0

for pattern in "${FORBIDDEN[@]}"; do
  detail "scanning lib/Lower/ + lib/Driver/ for: ${pattern}"
  # `--include` keeps us on translation-unit and header source only;
  # `--exclude-dir build` defends against an accidental in-tree build.
  # M7 (T019): scan both lib/Lower (M5/M6 origin) and lib/Driver
  # (M7 new TUs: EmitVerilog.cpp, RunCIRCTPasses.cpp).
  if matches="$(grep -rEn \
      --include='*.cpp' --include='*.h' \
      --include='*.cc' --include='*.hpp' \
      --exclude-dir=build \
      "${pattern}" "${AUDIT_ROOTS[@]}" 2>/dev/null)"; then
    if [[ -n "${matches}" ]]; then
      printf '[audit_determinism] FORBIDDEN PATTERN: %s\n' "${pattern}" >&2
      printf '%s\n' "${matches}" >&2
      printf '  rationale: %s\n' "${RATIONALE[${pattern}]}" >&2
      printf '\n' >&2
      n="$(printf '%s\n' "${matches}" | grep -c .)"
      TOTAL_MATCHES=$(( TOTAL_MATCHES + n ))
      FAIL=1
    fi
  fi
done

# -----------------------------------------------------------------------------
# Report + exit
# -----------------------------------------------------------------------------

if (( FAIL == 0 )); then
  log "OK: zero forbidden patterns in lib/Lower/ + lib/Driver/ (${#FORBIDDEN[@]} pattern(s) scanned)"
  exit 0
fi

printf '[audit_determinism] FAIL: %d match(es) across %d pattern category/categories\n' \
  "${TOTAL_MATCHES}" "${#FORBIDDEN[@]}" >&2
printf '\nfix:\n' >&2
printf '  1. replace std::unordered_* with llvm::DenseMap (or llvm::MapVector if order is load-bearing).\n' >&2
printf '  2. for stable identity, use the lexical-emission counter (nextTempId_++) — NOT pointer bits.\n' >&2
printf '  3. for time/random/pid sources: lowering MUST be reproducible — no entropy permitted.\n' >&2
printf '  see: specs/008-m5-structural-passes/research.md §13.\n' >&2
exit 1
