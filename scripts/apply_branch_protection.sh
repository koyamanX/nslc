#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# scripts/apply_branch_protection.sh — apply the canonical
# branch-protection settings to `main` via the GitHub REST API.
#
# Idempotent: running it on an already-applied configuration produces
# no changes. Requires `gh auth login` and repo-admin privileges.
#
# Authoritative settings live in `.github/branch-protection.json`.
# The accompanying `.github/branch-protection.md` documents WHY each
# setting is what it is; that file is the human-readable source of
# truth (FR-016, spec Q3).

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROTECTION_JSON="${REPO_ROOT}/.github/branch-protection.json"

if [[ ! -f "${PROTECTION_JSON}" ]]; then
  echo "apply_branch_protection.sh: ${PROTECTION_JSON} not found" >&2
  exit 1
fi

if ! command -v gh >/dev/null 2>&1; then
  echo "apply_branch_protection.sh: gh CLI not on PATH" >&2
  exit 1
fi

# Discover owner/repo from the current `gh repo view` context unless
# overridden by env. This avoids hard-coding the upstream owner.
OWNER_REPO="${GITHUB_OWNER_REPO:-$(gh repo view --json nameWithOwner -q .nameWithOwner)}"
BRANCH="${GITHUB_BRANCH:-main}"

echo "applying branch protection: ${OWNER_REPO}@${BRANCH}"
gh api \
  "repos/${OWNER_REPO}/branches/${BRANCH}/protection" \
  --method PUT \
  --input "${PROTECTION_JSON}" \
  -H "Accept: application/vnd.github+json"

echo "done."
