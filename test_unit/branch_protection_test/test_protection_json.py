# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Asserts .github/branch-protection.json contract per spec FR-016 +
# Q3 (named-reason override is the ONLY allowed bypass).

import json
import os
from pathlib import Path

import pytest

REPO_ROOT = Path(os.environ.get(
    "NSLC_REPO_ROOT",
    str(Path(__file__).resolve().parents[2])))
PROTECTION = REPO_ROOT / ".github" / "branch-protection.json"

EXPECTED_CONTEXTS = {
    "build-matrix (Debug, gcc)",
    "build-matrix (Debug, clang)",
    "build-matrix (Release, gcc)",
    "build-matrix (Release, clang)",
    "static-checks",
    "unit-and-layer-tests",
    "lowering-tests",
}


@pytest.fixture(scope="module")
def protection():
    with PROTECTION.open() as f:
        return json.load(f)


def test_protection_file_exists():
    assert PROTECTION.exists(), f"{PROTECTION} missing"


def test_enforce_admins(protection):
    assert protection["enforce_admins"] is True, \
        "FR-016 spec Q3: branch protection MUST apply to repo admins too"


def test_force_pushes_disabled(protection):
    assert protection["allow_force_pushes"] is False


def test_deletions_disabled(protection):
    assert protection["allow_deletions"] is False


def test_required_contexts_exact(protection):
    contexts = set(protection["required_status_checks"]["contexts"])
    missing = EXPECTED_CONTEXTS - contexts
    assert not missing, f"missing required contexts: {missing}"


def test_no_extra_skipped_stages_in_required(protection):
    # Stages 5/6 (end-to-end, formal) ship `if: false` and MUST NOT be
    # in `required_status_checks.contexts` until M7/M8 land — otherwise
    # GitHub blocks every PR on a never-firing check (research §8).
    contexts = set(protection["required_status_checks"]["contexts"])
    forbidden = {"end-to-end", "formal"}
    leakage = contexts & forbidden
    assert not leakage, \
        f"required-checks must not include skipped stages: {leakage}"
