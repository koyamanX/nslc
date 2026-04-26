# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Asserts .github/workflows/ci.yml contract per
# specs/001-m0-build-ci-scaffolding/contracts/ci-pipeline.contract.md.

import os
from pathlib import Path

import pytest

REPO_ROOT = Path(os.environ.get(
    "NSLC_REPO_ROOT",
    str(Path(__file__).resolve().parents[2])))
CI_YML = REPO_ROOT / ".github" / "workflows" / "ci.yml"

yaml = pytest.importorskip("yaml")


@pytest.fixture(scope="module")
def workflow():
    with CI_YML.open() as f:
        return yaml.safe_load(f)


def test_workflow_present():
    assert CI_YML.exists(), f"{CI_YML} missing"


def test_all_six_stages_declared(workflow):
    jobs = set(workflow["jobs"].keys())
    expected = {
        "build-matrix",
        "static-checks",
        "unit-and-layer-tests",
        "lowering-tests",
        "end-to-end",
        "formal",
    }
    assert expected.issubset(jobs), f"missing: {expected - jobs}"


def test_build_matrix_pinned_to_ubuntu_22_04(workflow):
    runs_on = workflow["jobs"]["build-matrix"]["runs-on"]
    assert runs_on == "ubuntu-22.04", \
        "research §7: pin runner image, do not use ubuntu-latest"


def test_build_matrix_is_4_cells(workflow):
    matrix = workflow["jobs"]["build-matrix"]["strategy"]["matrix"]
    assert set(matrix["build_type"]) == {"Debug", "Release"}
    assert set(matrix["compiler"]) == {"gcc", "clang"}


def test_e2e_stage_skipped(workflow):
    # PyYAML parses `if: false` to Python False; tolerate string form.
    cond = workflow["jobs"]["end-to-end"].get("if")
    assert cond is False or str(cond).lower() == "false", \
        "FR-015: end-to-end stage must be wired but skipped pre-M7"


def test_formal_stage_skipped(workflow):
    cond = workflow["jobs"]["formal"].get("if")
    assert cond is False or str(cond).lower() == "false", \
        "FR-015: formal stage must be wired but skipped pre-M8"


def test_triggers_pull_request_and_push_main(workflow):
    # PyYAML 'on' (a boolean keyword) is normalised to True; YAML 1.1
    # quirk. Tolerate either parsed form.
    on = workflow.get(True) or workflow.get("on")
    assert on is not None, "ci.yml missing top-level 'on:' key"
    pr = on.get("pull_request") or {}
    push = on.get("push") or {}
    assert "main" in (pr.get("branches") or []), "FR-013: PR trigger to main"
    assert "main" in (push.get("branches") or []), "FR-013: push trigger to main"
