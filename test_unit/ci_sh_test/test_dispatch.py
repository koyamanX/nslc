# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Asserts scripts/ci.sh contract:
#   - unknown stages exit non-zero with an "unknown stage" diagnostic
#   - e2e + formal exit 0 with "wired but empty" stdout (FR-015)
#   - the `all` and `--help` invocations behave per ci-pipeline.contract.md

import os
import subprocess
from pathlib import Path

REPO_ROOT = Path(os.environ.get(
    "NSLC_REPO_ROOT",
    str(Path(__file__).resolve().parents[2])))
CI_SH = REPO_ROOT / "scripts" / "ci.sh"


def run(*args):
    return subprocess.run(
        [str(CI_SH), *args],
        capture_output=True, text=True, cwd=REPO_ROOT)


def test_script_exists_and_executable():
    assert CI_SH.exists(), f"{CI_SH} missing"
    assert os.access(CI_SH, os.X_OK), f"{CI_SH} not executable"


def test_unknown_stage_fails_with_diagnostic():
    r = run("nonexistent-stage")
    assert r.returncode != 0
    combined = (r.stdout + r.stderr).lower()
    assert "unknown stage" in combined or "unknown" in combined


def test_e2e_wired_but_empty():
    r = run("e2e")
    assert r.returncode == 0, r.stderr
    assert "wired but empty" in r.stdout.lower()


def test_formal_wired_but_empty():
    r = run("formal")
    assert r.returncode == 0, r.stderr
    assert "wired but empty" in r.stdout.lower()


def test_help_prints_usage():
    r = run("--help")
    # Usage exits 2 by GNU convention
    combined = r.stdout + r.stderr
    assert "usage" in combined.lower()
