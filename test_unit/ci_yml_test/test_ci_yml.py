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


def test_triggers_pull_request_and_push_master(workflow):
    # PyYAML 'on' (a boolean keyword) is normalised to True; YAML 1.1
    # quirk. Tolerate either parsed form. Branch name is "master" per
    # commit b75a105 (ci: retarget GitHub Actions to master branch
    # was main); the project's default branch is master.
    on = workflow.get(True) or workflow.get("on")
    assert on is not None, "ci.yml missing top-level 'on:' key"
    pr = on.get("pull_request") or {}
    push = on.get("push") or {}
    assert "master" in (pr.get("branches") or []), "FR-013: PR trigger to master"
    assert "master" in (push.get("branches") or []), "FR-013: push trigger to master"


# -----------------------------------------------------------------------------
# Container alignment with the .docker/ stack
# -----------------------------------------------------------------------------

CONTAINERIZED_JOBS = (
    "build-matrix",
    "static-checks",
    "unit-and-layer-tests",
    "lowering-tests",
)


@pytest.mark.parametrize("job_name", CONTAINERIZED_JOBS)
def test_job_runs_in_nslc_container(workflow, job_name):
    """Each real-work job MUST execute inside the nsl-nslc image so
    the LLVM/MLIR/CIRCT install (staged by .docker/{llvm-mlir,circt})
    is on-PATH without an inline download step (research §2 +
    .docker/). Stages 5/6 are exempt — they're skipped by `if: false`.
    """
    job = workflow["jobs"][job_name]
    container = job.get("container")
    assert container is not None, \
        f"job '{job_name}' must run inside the nsl-nslc container"
    image = container if isinstance(container, str) else container.get("image", "")
    # The image may be a literal "ghcr.io/.../nsl-nslc:tag" or a
    # GitHub-Actions expression "${{ env.NSLC_IMAGE }}" that resolves
    # to one. Resolve via the workflow's top-level `env:` block.
    if image.strip().startswith("${{") and "env.NSLC_IMAGE" in image:
        image = workflow.get("env", {}).get("NSLC_IMAGE", "")
    assert "nsl-nslc" in image, \
        f"job '{job_name}' container must be the nsl-nslc image; got: {image}"


def test_workflow_level_image_pinned_to_ghcr(workflow):
    """The top-level `env.NSLC_IMAGE` must point at the project's ghcr
    image so all containerized jobs share a single source of truth."""
    image = workflow.get("env", {}).get("NSLC_IMAGE", "")
    assert image.startswith("ghcr.io/"), \
        f"NSLC_IMAGE must be a ghcr.io reference; got: {image}"
    assert "nsl-nslc" in image, \
        f"NSLC_IMAGE must reference nsl-nslc; got: {image}"


def test_no_inline_apt_install(workflow):
    """The container ships gcc / clang / cmake / ninja / python3-pytest /
    python3-yaml etc. No CI step should rebuild that surface inline
    (Principle V — env drift between local `docker run` and CI would
    break determinism). If a tool is missing, add it to
    .docker/nslc/Dockerfile, not to ci.yml.
    """
    bad = []
    for job_name, job in workflow["jobs"].items():
        for i, step in enumerate(job.get("steps") or []):
            if not isinstance(step, dict):
                continue
            run = step.get("run") or ""
            if "apt-get install" in run or "apt install" in run:
                bad.append(f"{job_name}.steps[{i}]")
    assert not bad, \
        ("inline apt-get install is forbidden in ci.yml; install in "
         f".docker/nslc/Dockerfile instead. found: {bad}")
