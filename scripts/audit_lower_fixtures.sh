#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# scripts/audit_lower_fixtures.sh — M5 US1 fixture-coverage CI guard
# enforcing FR-027 + SC-001 from
# `specs/008-m5-structural-passes/spec.md`. For every concrete
# `visit()` override on `ASTToMLIR` in `lib/Lower/ASTToMLIR.cpp`
# (excluding the no-op `STUB(...)` macro slots), assert a paired
# fixture exists somewhere under `test/Lower/` whose filename
# contains the lowercased AST-node-kind name as a substring.
#
# Authoritative data sources:
#   - lib/Lower/ASTToMLIR.cpp — visit() overrides (real impls + STUBs)
#   - test/Lower/**            — fixture inventory
#
# Coverage rule (FR-027):
#   For every line `void ASTToMLIR::visit(const ast::<Name> & ...) {`
#   in ASTToMLIR.cpp that is NOT a STUB(<Name>) macro expansion,
#   there MUST exist at least one regular file under test/Lower/
#   whose lowercased basename contains the lowercased <Name> as a
#   substring — OR the <Name> MUST appear on the in-script ALLOWLIST
#   below with a cited rationale.
#
# The script's enumeration is deterministic: visitors are sorted in
# lexical order, fixture lookup is sorted in lexical order, output
# byte-stable across two runs (Constitution Principle V).
#
# Exit codes:
#   0  every concrete visitor has a fixture (or is allow-listed)
#   1  one or more concrete visitors lack both a fixture and an
#      allow-list entry — real coverage gap
#   2  script-internal error (missing source file, unreadable test
#      directory, etc.)
#
# Usage:
#   bash scripts/audit_lower_fixtures.sh
#   bash scripts/audit_lower_fixtures.sh --quiet   # only summary line + exit code
#   bash scripts/audit_lower_fixtures.sh --verbose # per-visitor decision

set -euo pipefail

# -----------------------------------------------------------------------------
# Locate repo root + sources
# -----------------------------------------------------------------------------

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
readonly VISITOR_SRC="${REPO_ROOT}/lib/Lower/ASTToMLIR.cpp"
readonly FIXTURE_ROOT="${REPO_ROOT}/test/Lower"

QUIET=0
VERBOSE=0
case "${1:-}" in
  --quiet)   QUIET=1 ;;
  --verbose) VERBOSE=1 ;;
  "") : ;;
  *)
    printf '[audit_lower_fixtures] error: unknown arg %q (expected --quiet | --verbose | <none>)\n' \
      "$1" >&2
    exit 2
    ;;
esac

log()    { (( QUIET )) || printf '[audit_lower_fixtures] %s\n' "$*"; }
detail() { (( VERBOSE )) && printf '[audit_lower_fixtures]   %s\n' "$*"; return 0; }

if [[ ! -f "${VISITOR_SRC}" ]]; then
  printf '[audit_lower_fixtures] error: cannot read %s\n' "${VISITOR_SRC}" >&2
  exit 2
fi
if [[ ! -d "${FIXTURE_ROOT}" ]]; then
  printf '[audit_lower_fixtures] error: cannot read directory %s\n' "${FIXTURE_ROOT}" >&2
  exit 2
fi

# -----------------------------------------------------------------------------
# Allow-list: visitors that intentionally have no fixture matching
# their full lowercased name as a substring of the basename.
#
# Each entry is the AST-node-kind name (case-sensitive, as it appears
# in `ast::<Name>`) -> cited rationale. Adding a new entry MUST cite
# a commit hash, design doc anchor, or sibling fixture path that
# justifies the omission. The allow-list is intentionally minimal —
# the default disposition for a real-impl visitor is "ship a fixture
# whose basename contains the node-kind name as a substring".
#
# Cited at HEAD 3b3f3cd (US1 close-out commit on branch
# 008-m5-structural-passes).
# -----------------------------------------------------------------------------

declare -A ALLOWLIST=(
  # ----- synthetic top-level dispatch ---------------------------------
  # Body is a for-loop over `node.items()` calling `accept(*this)`.
  # Every fixture under test/Lower/ implicitly exercises it (entry
  # point of every `nslc -emit=mlir` run). A dedicated fixture would
  # add no signal beyond what the per-item fixtures already give.
  # Cited: lib/Lower/ASTToMLIR.cpp:161-165 (pass-through visitor body).
  ["CompilationUnit"]="dispatch-only; covered transitively by every test/Lower/ fixture"

  # ----- expression-position lowerExpr delegators ---------------------
  # `(void)lowerExpr(&node);` one-liners. They emit no IR at the
  # current insertion point — the actual lowering happens when an
  # enclosing statement-position visitor (`TransferStmt`, `IfStmt`,
  # etc.) calls `lowerExpr(...)` and consumes the resulting Value.
  # Coverage is transitive through every fixture using the matching
  # NSL syntax (e.g., `+`/`-` for BinaryExpr, `#` for SignExtendExpr).
  # Cited: lib/Lower/ASTToMLIR.cpp:889-942.
  ["LiteralExpr"]="pure-value delegator to lowerExpr; covered transitively"
  ["IdentifierExpr"]="pure-value delegator to lowerExpr; covered transitively"
  ["SignExtendExpr"]="pure-value delegator to lowerExpr; covered transitively"
  ["ZeroExtendExpr"]="pure-value delegator to lowerExpr; covered transitively"
  ["BinaryExpr"]="pure-value delegator to lowerExpr; covered transitively"
  ["UnaryExpr"]="pure-value delegator to lowerExpr; covered transitively"
  ["ConditionalExpr"]="pure-value delegator to lowerExpr; covered transitively"
  ["SliceExpr"]="pure-value delegator to lowerExpr; covered transitively"
  ["ConcatExpr"]="pure-value delegator to lowerExpr; covered transitively"
  ["StructCastExpr"]="pure-value delegator to lowerExpr; covered transitively"
  ["FieldAccessExpr"]="pure-value delegator to lowerExpr; covered transitively"

  # ----- statement / decl visitors with sibling-fixture coverage ------
  # These visitors have real IR-emitting bodies but their dedicated
  # fixture's basename does not contain the full lowercased node name
  # as a substring. The fuzzy-suffix-strip pass (Step 5b) catches most
  # of them; the rest are covered by sibling fixtures cited below.

  # `state s0 { }` body is exercised by procdefn_emit_mlir.nsl. A
  # standalone fixture would duplicate the CHECK lines verbatim.
  # Cited: test/Lower/decl/procdefn_emit_mlir.nsl:13-23.
  ["StateDefn"]="covered by test/Lower/decl/procdefn_emit_mlir.nsl"

  # FuncSelfDecl + PortDecl(FuncIn/FuncOut/FuncSelf) lower to
  # nsl.func_self / nsl.func_in / nsl.func_out. Both are exercised by
  # the fire_probe fixture which declares `func_self fire;`.
  # Data-port flavours (Input/Output/Inout) are deferred to M6 per
  # lib/Lower/ASTToMLIR.cpp:1110-1145.
  # Cited: test/Lower/marker/fire_probe_emit_mlir.nsl:24-36.
  ["FuncSelfDecl"]="covered by test/Lower/marker/fire_probe_emit_mlir.nsl"
  ["PortDecl"]="covered by test/Lower/marker/fire_probe_emit_mlir.nsl (control-terminal flavour)"

  # StructDecl (top-level) and StructInstDecl (Reg form) are both
  # exercised by the structcastexpr / fieldaccessexpr fixtures, which
  # declare `struct MyRec { ... }` and `MyRec reg slot;`. The Wire-
  # form of StructInstDecl deliberately soft-fails per FR-013 (struct
  # wires rejected by dialect verifier). Cited:
  # test/Lower/expr/structcastexpr_emit_mlir.nsl:26-37,
  # test/Lower/expr/fieldaccessexpr_emit_mlir.nsl:26-37,
  # lib/Lower/ASTToMLIR.cpp:1008-1013.
  ["StructDecl"]="covered by test/Lower/expr/structcastexpr_emit_mlir.nsl"
  ["StructInstDecl"]="covered by test/Lower/expr/fieldaccessexpr_emit_mlir.nsl (Reg form); Wire form soft-fails per FR-013"
)

# -----------------------------------------------------------------------------
# Step 1: enumerate concrete visit() overrides
# -----------------------------------------------------------------------------
#
# Match `void ASTToMLIR::visit(const ast::<Name> &` and capture <Name>.
# The `&` (potentially followed by an identifier or `/*...*/`)
# disambiguates from forward declarations / member-function pointers.

mapfile -t ALL_VISITORS < <(
  # `[^\\]$` excludes the macro-definition body line
  # `  void ASTToMLIR::visit(const ast::EnumName & /*node*/) {}` which
  # ends in a backslash continuation when expanded by the preprocessor
  # but appears verbatim in the source. We also exclude lines whose
  # captured name is the literal string `EnumName` (the macro
  # parameter), defence-in-depth against future formatting changes.
  grep -E '^[[:space:]]*void[[:space:]]+ASTToMLIR::visit[[:space:]]*\(const[[:space:]]+ast::[A-Za-z_][A-Za-z0-9_]*[[:space:]]*&' \
    "${VISITOR_SRC}" \
  | grep -v '\\$' \
  | sed -E 's/^[[:space:]]*void[[:space:]]+ASTToMLIR::visit[[:space:]]*\(const[[:space:]]+ast::([A-Za-z_][A-Za-z0-9_]*).*$/\1/' \
  | grep -v '^EnumName$' \
  | sort -u
)

# -----------------------------------------------------------------------------
# Step 2: enumerate STUB(...) macro slots (no-op visitors)
# -----------------------------------------------------------------------------
#
# Lines look like `STUB(EnumName)`. These expand to single-line empty
# bodies: `void ASTToMLIR::visit(const ast::EnumName & /*node*/) {}`.
# The grep in Step 1 does NOT pick them up because the macro form
# doesn't contain the textual `void ASTToMLIR::visit(const ast::` —
# but a future refactor might unroll them. We compute the STUB set
# explicitly so the audit is robust to either representation.

mapfile -t STUB_VISITORS < <(
  grep -E '^STUB\([A-Za-z_][A-Za-z0-9_]*\)' "${VISITOR_SRC}" \
  | sed -E 's/^STUB\(([A-Za-z_][A-Za-z0-9_]*)\).*$/\1/' \
  | sort -u
)

# -----------------------------------------------------------------------------
# Step 3: subtract STUBs from ALL_VISITORS (defence-in-depth)
# -----------------------------------------------------------------------------

declare -A STUB_SET=()
for s in "${STUB_VISITORS[@]}"; do
  STUB_SET["${s}"]=1
done

CONCRETE_VISITORS=()
for v in "${ALL_VISITORS[@]}"; do
  if [[ -z "${STUB_SET[${v}]:-}" ]]; then
    CONCRETE_VISITORS+=("${v}")
  fi
done

# -----------------------------------------------------------------------------
# Step 4: enumerate fixture filenames (basename only, lowercased)
# -----------------------------------------------------------------------------

mapfile -t FIXTURE_BASENAMES < <(
  find "${FIXTURE_ROOT}" -type f \( -name '*.nsl' -o -name '*.mlir' \) -printf '%f\n' \
  | tr '[:upper:]' '[:lower:]' \
  | sort -u
)

# -----------------------------------------------------------------------------
# Step 5: per-visitor coverage check
# -----------------------------------------------------------------------------

MISSING=()
COVERED_BY_FIXTURE=0
COVERED_BY_ALLOWLIST=0

# For each visitor, build a small ordered list of candidate substrings
# to probe against fixture basenames:
#   1. lowered                           (e.g., RegDecl       -> regdecl)
#   2. lowered with one trailing kind-suffix stripped
#                                        (e.g., FirstStateDecl -> firststate
#                                               ModuleBlock    -> module
#                                               DelayTaskStmt  -> delaytask
#                                               InitBlockStmt  -> initblock)
# The stripped suffix set is the standard MLIR/AST node-kind tail set:
# `decl`, `stmt`, `block`, `expr`, `defn`. The strip runs once (a
# single trailing token) — multi-pass stripping would be too lossy.
#
# Special-cased: `DelayTaskStmt` -> `delay`, `InitBlockStmt` -> `init`
# (the second strip is needed because their fixtures live under
# `systemtaskstmt_<flavour>_emit_mlir.nsl`). We probe a third
# candidate by stripping `task` / `block` after the standard one.

readonly KIND_SUFFIX_RE='(decl|stmt|block|expr|defn)$'

for visitor in "${CONCRETE_VISITORS[@]}"; do
  lowered="$(printf '%s' "${visitor}" | tr '[:upper:]' '[:lower:]')"
  candidates=("${lowered}")
  stripped="$(printf '%s' "${lowered}" | sed -E "s/${KIND_SUFFIX_RE}//")"
  if [[ "${stripped}" != "${lowered}" && -n "${stripped}" ]]; then
    candidates+=("${stripped}")
    # Second-pass strip: e.g. `delaytask` -> `delay`,
    # `initblock` -> `init`. Same suffix set.
    stripped2="$(printf '%s' "${stripped}" | sed -E 's/(task|block)$//')"
    if [[ "${stripped2}" != "${stripped}" && -n "${stripped2}" ]]; then
      candidates+=("${stripped2}")
    fi
  fi
  found=""
  matched_candidate=""
  for cand in "${candidates[@]}"; do
    for fixture in "${FIXTURE_BASENAMES[@]}"; do
      if [[ "${fixture}" == *"${cand}"* ]]; then
        found="${fixture}"
        matched_candidate="${cand}"
        break 2
      fi
    done
  done
  if [[ -n "${found}" ]]; then
    if [[ "${matched_candidate}" == "${lowered}" ]]; then
      detail "OK    ${visitor} -> ${found}"
    else
      detail "OK*   ${visitor} -> ${found} (via fuzzy stem '${matched_candidate}')"
    fi
    COVERED_BY_FIXTURE=$(( COVERED_BY_FIXTURE + 1 ))
    continue
  fi
  if [[ -n "${ALLOWLIST[${visitor}]:-}" ]]; then
    detail "ALLOW ${visitor} (${ALLOWLIST[${visitor}]})"
    COVERED_BY_ALLOWLIST=$(( COVERED_BY_ALLOWLIST + 1 ))
    continue
  fi
  detail "MISS  ${visitor} (no fixture, no allow-list)"
  MISSING+=("${visitor}")
done

# -----------------------------------------------------------------------------
# Step 6: report + exit
# -----------------------------------------------------------------------------

readonly TOTAL_CONCRETE="${#CONCRETE_VISITORS[@]}"
readonly TOTAL_STUB="${#STUB_VISITORS[@]}"
readonly TOTAL_FIXTURES="${#FIXTURE_BASENAMES[@]}"

if (( ${#MISSING[@]} == 0 )); then
  log "OK: ${TOTAL_CONCRETE} concrete visitor(s) covered (${COVERED_BY_FIXTURE} via fixture, ${COVERED_BY_ALLOWLIST} via allow-list); ${TOTAL_STUB} STUB; ${TOTAL_FIXTURES} fixture(s) under test/Lower/"
  exit 0
fi

printf '[audit_lower_fixtures] FAIL: %d concrete visitor(s) without a fixture and without an allow-list entry:\n' \
  "${#MISSING[@]}" >&2
for v in "${MISSING[@]}"; do
  printf '  - ast::%s (expected fixture matching `*%s*` somewhere under test/Lower/)\n' \
    "${v}" "$(printf '%s' "${v}" | tr '[:upper:]' '[:lower:]')" >&2
done
printf '\nfix:\n' >&2
printf '  1. add test/Lower/<category>/<node-snake-case>_emit_mlir.nsl with a RUN line that pipes nslc -emit=mlir into FileCheck, OR\n' >&2
printf '  2. add an entry to the ALLOWLIST in scripts/audit_lower_fixtures.sh with a cited rationale.\n' >&2
exit 1
