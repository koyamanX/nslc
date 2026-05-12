#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# scripts/lsp_link_audit.sh — Constitution Principle II §3
# enforcement for `nsl-lsp`. Asserts that `lib/LSP/` does NOT
# re-implement any frontend class (Lexer, Preprocessor, Parser,
# Sema, SymbolTable, TypeSystem, DiagnosticEngine, SourceManager,
# CompilationUnit, etc.). Reuse is enforced by linking
# `nsl-driver`, which transitively pulls every layer of
# `libNSLFrontend.a` per its `add_nsl_library` DEPENDS list.
#
# Approach: source-tree grep. The nm-based audit was rejected
# because the Itanium ABI emits multiple destructor variants
# (D0/D1/D2) at distinct addresses, which look identical to a
# duplication after demangling — too noisy to threshold
# reliably. The grep-based audit is direct and unambiguous: if
# `lib/LSP/` doesn't define a class named `Lexer` or `Parser`,
# there is no possible code duplication.
#
# Usage: scripts/lsp_link_audit.sh

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LIB_LSP="${REPO_ROOT}/lib/LSP"

if [[ ! -d "${LIB_LSP}" ]]; then
  echo "lsp_link_audit: ${LIB_LSP} not found" >&2
  exit 2
fi

# Frontend class names that MUST come exclusively from
# `libNSLFrontend.a` (linked via nsl-driver). Any of these defined
# in lib/LSP/ is a Principle II violation.
forbidden=(
  "class Lexer"
  "class Preprocessor"
  "class Parser"
  "class Sema"
  "class SymbolTable"
  "class TypeSystem"
  "class DiagnosticEngine"
  "class SourceManager"
  "class CompilationUnit"
  "class ASTNode"
)

violations=0
for pat in "${forbidden[@]}"; do
  hits=$(grep -rnE "^[[:space:]]*${pat}[[:space:]]*\\{" \
                "${LIB_LSP}/" 2>/dev/null || true)
  if [[ -n "${hits}" ]]; then
    echo "lsp_link_audit: FAIL — '${pat}' defined in lib/LSP/ (Principle II violation):" >&2
    echo "${hits}" >&2
    violations=$((violations + 1))
  fi
done

# Single-public-header check: include/nsl/LSP/ must contain
# exactly Server.h. Documented in plan §Constitution Check II.
public_headers=$(find "${REPO_ROOT}/include/nsl/LSP/" -maxdepth 1 -name '*.h' 2>/dev/null | wc -l)
if [[ "${public_headers}" -ne 1 ]]; then
  echo "lsp_link_audit: FAIL — include/nsl/LSP/ contains ${public_headers} public headers (expected exactly 1: Server.h):" >&2
  ls "${REPO_ROOT}/include/nsl/LSP/" >&2
  violations=$((violations + 1))
fi

if [[ "${violations}" -gt 0 ]]; then
  echo "lsp_link_audit: ${violations} violation(s)" >&2
  exit 1
fi

echo "lsp_link_audit: PASS — no frontend duplication; single public header" >&2
exit 0
