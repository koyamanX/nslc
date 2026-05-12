#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# scripts/audit_fmt_api.sh — T2 Phase 2c (T029) public-API freeze gate.
#
# Asserts that the SOLE public header `include/nsl/Fmt/Fmt.h`
# contains EXACTLY the 10 declarations frozen by
# `specs/010-t2-formatter-v0/contracts/format-api.contract.md` §5:
#
#   3 types:    Configuration, LineRange, FormatResult
#   7 fns:      format_buffer, parse_config_file, discover_config,
#               emit_unified_diff, default_configuration,
#               config_key_names, version_string
#
# **Detection strategy**: rather than parse C++, we look for each
# expected symbol by its declaration anchor. Each anchor is a regex
# matching a top-level declaration of the named symbol; missing
# anchors fail the audit. We also enumerate the top-level
# `struct`/`class` declarations to detect surface ADDITIONS — if a
# new top-level struct/class appears that's not in the expected list,
# the script fails with the unexpected name.
#
# **Why a CI grep**: per Constitution Principle II's single-public-
# header rule, `nsl-fmt` (a tool library) exposes one umbrella
# header. The format-api contract additionally freezes the SYMBOL
# COUNT to guard against accidental surface expansion. Adding an
# 11th symbol = contract change; this script fails CI until the
# contract is amended in the same PR.
#
# Usage:
#   bash scripts/audit_fmt_api.sh
#   bash scripts/audit_fmt_api.sh --quiet
#   bash scripts/audit_fmt_api.sh --verbose
#
# Exit codes:
#   0  exactly the expected 10 symbols present, no extras
#   1  one or more symbols missing, OR an unexpected top-level
#      struct/class found
#   2  internal error (Fmt.h missing, etc.)

set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
readonly FMT_HEADER="${REPO_ROOT}/include/nsl/Fmt/Fmt.h"

QUIET=0
VERBOSE=0
case "${1:-}" in
  --quiet)   QUIET=1 ;;
  --verbose) VERBOSE=1 ;;
  "") : ;;
  *)
    printf '[audit_fmt_api] error: unknown arg %q (expected --quiet | --verbose | <none>)\n' \
      "$1" >&2
    exit 2
    ;;
esac

log()    { (( QUIET )) || printf '[audit_fmt_api] %s\n' "$*"; }
detail() { (( VERBOSE )) && printf '[audit_fmt_api]   %s\n' "$*"; return 0; }

if [[ ! -f "${FMT_HEADER}" ]]; then
  printf '[audit_fmt_api] error: cannot read %s\n' "${FMT_HEADER}" >&2
  exit 2
fi

# -----------------------------------------------------------------------------
# Step 1 — strip C++ comments so phantom matches inside `//` and
# `/* … */` blocks don't pollute the detection.
# -----------------------------------------------------------------------------
#
# Comment-stripping via sed:
#   * line comments: `s|//.*||`
#   * block comments: handled with a multi-line sed script.

stripped="$(sed -e 's|//.*||' \
                -e '/\/\*/,/\*\//d' \
                "${FMT_HEADER}")"

# -----------------------------------------------------------------------------
# Step 2 — assert each of the 10 expected symbols by anchor regex.
# -----------------------------------------------------------------------------

declare -a EXPECTED_TYPE_NAMES=(
  "Configuration"
  "LineRange"
  "FormatResult"
)

# Each entry: "<symbol-name> <regex-anchor-without-wrapping-^/$>".
# Anchors are LINE patterns (the symbol's declaration sits on its
# own line in Fmt.h per project formatting convention).
declare -A EXPECTED_FUNC_ANCHORS=(
  ["format_buffer"]="^FormatResult format_buffer\("
  ["parse_config_file"]="^FormatResult parse_config_file\("
  ["discover_config"]="^std::optional<std::string> discover_config\("
  ["emit_unified_diff"]="^std::string emit_unified_diff\("
  ["default_configuration"]="^Configuration default_configuration\("
  ["config_key_names"]="^llvm::ArrayRef<llvm::StringRef> config_key_names\("
  ["version_string"]="^llvm::StringRef version_string\("
)

MISSING=()
EXTRAS=()

for type_name in "${EXPECTED_TYPE_NAMES[@]}"; do
  anchor="^struct ${type_name}( |\$|\{)"
  if printf '%s\n' "${stripped}" | grep -Eq "${anchor}"; then
    detail "found type: ${type_name}"
  else
    MISSING+=("type:${type_name}")
  fi
done

for fn_name in "${!EXPECTED_FUNC_ANCHORS[@]}"; do
  if printf '%s\n' "${stripped}" | grep -Eq "${EXPECTED_FUNC_ANCHORS[${fn_name}]}"; then
    detail "found fn: ${fn_name}"
  else
    MISSING+=("fn:${fn_name}")
  fi
done

# -----------------------------------------------------------------------------
# Step 3 — detect UNEXPECTED top-level struct/class additions.
# -----------------------------------------------------------------------------
#
# Anything matching `^struct <Name>` or `^class <Name>` whose Name
# is not in EXPECTED_TYPE_NAMES (and not one of the well-known
# nested types — though those are typically indented and so won't
# match `^struct`).

while IFS= read -r found; do
  if [[ -z "${found}" ]]; then continue; fi
  # Strip trailing brace / newline noise.
  name="${found%% *}"
  name="${name%%[\{]*}"
  expected=0
  for t in "${EXPECTED_TYPE_NAMES[@]}"; do
    if [[ "${name}" == "${t}" ]]; then
      expected=1
      break
    fi
  done
  if (( expected == 0 )); then
    EXTRAS+=("type:${name}")
  fi
done < <(printf '%s\n' "${stripped}" | sed -nE 's/^(struct|class) ([A-Za-z_][A-Za-z0-9_]*).*/\2/p')

# Detect UNEXPECTED top-level free-function declarations. Anything
# matching `<return-type> <Name>(` at column 0 whose Name is not in
# EXPECTED_FUNC_ANCHORS is flagged. The regex captures one return-
# type token followed by the function name plus an opening `(`; it
# deliberately stays loose on the return type (it can contain `::`,
# `<`, `>`, `&`, `*`) so any non-trivial return type still parses.
EXPECTED_FUNC_NAMES=(
  "format_buffer"
  "parse_config_file"
  "discover_config"
  "emit_unified_diff"
  "default_configuration"
  "config_key_names"
  "version_string"
)
while IFS= read -r fn; do
  if [[ -z "${fn}" ]]; then continue; fi
  expected=0
  for f in "${EXPECTED_FUNC_NAMES[@]}"; do
    if [[ "${fn}" == "${f}" ]]; then
      expected=1
      break
    fi
  done
  if (( expected == 0 )); then
    EXTRAS+=("fn:${fn}")
  fi
done < <(printf '%s\n' "${stripped}" \
  | sed -nE 's/^[A-Za-z_:<>,&* ]+[[:space:]]+([A-Za-z_][A-Za-z0-9_]*)\(.*/\1/p')

# -----------------------------------------------------------------------------
# Report + exit
# -----------------------------------------------------------------------------

found_count=$(( 10 - ${#MISSING[@]} ))
extra_count=${#EXTRAS[@]}

if [[ ${#MISSING[@]} -eq 0 && ${extra_count} -eq 0 ]]; then
  log "OK: 10 of 10 expected public symbols present in include/nsl/Fmt/Fmt.h (no extras)"
  exit 0
fi

printf '[audit_fmt_api] FAIL: public-API drift in include/nsl/Fmt/Fmt.h\n' >&2
printf '  expected: 10 symbols (3 types + 7 fns)\n' >&2
printf '  found:    %d expected\n' "${found_count}" >&2
if (( ${#MISSING[@]} > 0 )); then
  printf '  missing (%d):\n' "${#MISSING[@]}" >&2
  for s in "${MISSING[@]}"; do printf '    - %s\n' "${s}" >&2; done
fi
if (( extra_count > 0 )); then
  printf '  unexpected (%d):\n' "${extra_count}" >&2
  for s in "${EXTRAS[@]}"; do printf '    - %s\n' "${s}" >&2; done
fi
printf '\nfix:\n' >&2
printf '  EITHER revert the public-surface change\n' >&2
printf '  OR amend specs/010-t2-formatter-v0/contracts/format-api.contract.md §5\n' >&2
printf '     in the same PR (the contract freezes the count at 10).\n' >&2
exit 1
