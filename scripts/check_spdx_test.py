# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""pytest suite for scripts/check_spdx.py.

TDD seed for US3 — per Principle VIII these tests land BEFORE
scripts/check_spdx.py implementation. Each test function maps 1:1 to
a row in the contract's §Test contract table
(specs/001-m0-build-ci-scaffolding/contracts/spdx-check.contract.md).

Bad-SPDX inputs are generated under pytest's tmp_path so no committed
file ever lacks a header — only the three "valid" fixture files
under test_unit/spdx_check_test/fixtures/ are checked into the repo.
"""

import os
import subprocess
import sys
from pathlib import Path

import pytest

REPO_ROOT = Path(os.environ.get(
    "NSLC_REPO_ROOT",
    str(Path(__file__).resolve().parents[1])))
SCRIPT = REPO_ROOT / "scripts" / "check_spdx.py"
FIXTURES = REPO_ROOT / "test_unit" / "spdx_check_test" / "fixtures"

VALID_CPP_HEADER = "// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception"
VALID_PY_HEADER = "# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception"


def run(*args, exceptions=None, cwd=None):
    cmd = [sys.executable, str(SCRIPT)]
    if exceptions is not None:
        cmd += ["--exceptions", str(exceptions)]
    cmd += [str(a) for a in args]
    return subprocess.run(
        cmd, capture_output=True, text=True, cwd=cwd or REPO_ROOT)


@pytest.fixture
def empty_exceptions(tmp_path):
    p = tmp_path / "exceptions.txt"
    p.write_text("# empty\n")
    return p


# -----------------------------------------------------------------------------
# Contract row 1: valid_md_passes
# -----------------------------------------------------------------------------
def test_valid_md_passes(empty_exceptions):
    r = run(FIXTURES / "valid_md.md", exceptions=empty_exceptions)
    assert r.returncode == 0, f"stdout: {r.stdout}\nstderr: {r.stderr}"


# -----------------------------------------------------------------------------
# Contract row 2: valid_cpp_passes
# -----------------------------------------------------------------------------
def test_valid_cpp_passes(empty_exceptions):
    r = run(FIXTURES / "valid_cpp.cpp", exceptions=empty_exceptions)
    assert r.returncode == 0, f"stdout: {r.stdout}\nstderr: {r.stderr}"


# -----------------------------------------------------------------------------
# Contract row 3: valid_py_with_shebang_passes
# -----------------------------------------------------------------------------
def test_valid_py_with_shebang_passes(empty_exceptions):
    r = run(FIXTURES / "valid_py_with_shebang.py", exceptions=empty_exceptions)
    assert r.returncode == 0, f"stdout: {r.stdout}\nstderr: {r.stderr}"


# -----------------------------------------------------------------------------
# Contract row 4: missing_header_fails
# -----------------------------------------------------------------------------
def test_missing_header_fails(tmp_path, empty_exceptions):
    bad = tmp_path / "missing_header.cpp"
    bad.write_text("int main() { return 0; }\n")
    r = run(bad, exceptions=empty_exceptions)
    assert r.returncode == 1, r.stdout
    assert f"{bad}:1" in r.stdout, f"diagnostic must name file:1 — got: {r.stdout}"


# -----------------------------------------------------------------------------
# Contract row 5: wrong_identifier_fails
# -----------------------------------------------------------------------------
def test_wrong_identifier_fails(tmp_path, empty_exceptions):
    bad = tmp_path / "wrong_identifier.cpp"
    # Note: bare Apache-2.0, missing the LLVM-exception clause
    bad.write_text("// SPDX-License-Identifier: Apache-2.0\n"
                   "int main() {}\n")
    r = run(bad, exceptions=empty_exceptions)
    assert r.returncode == 1
    text = r.stdout.lower()
    assert "expected" in text and "observed" in text, \
        f"diagnostic must name BOTH expected and observed — got: {r.stdout}"


# -----------------------------------------------------------------------------
# Contract row 6: unknown_extension_fails_loudly
# -----------------------------------------------------------------------------
def test_unknown_extension_fails_loudly(tmp_path, empty_exceptions):
    bad = tmp_path / "unknown.xyz"
    bad.write_text("hello\n")
    r = run(bad, exceptions=empty_exceptions)
    assert r.returncode == 1
    assert "no recipe" in r.stdout.lower(), \
        f"FR-010: silent skip is forbidden — got: {r.stdout}"


# -----------------------------------------------------------------------------
# Contract row 7: exempt_path_skipped
# -----------------------------------------------------------------------------
def test_exempt_path_skipped(tmp_path):
    target = tmp_path / "no_header.cpp"
    target.write_text("int main() {}\n")
    exc = tmp_path / "exceptions.txt"
    exc.write_text(f"{target}\n")
    r = run(target, exceptions=exc)
    assert r.returncode == 0, r.stdout
    assert "exempt" in r.stdout.lower()


# -----------------------------------------------------------------------------
# Contract row 8: stale_exception_fails
# -----------------------------------------------------------------------------
def test_stale_exception_fails(tmp_path):
    exc = tmp_path / "exceptions.txt"
    exc.write_text(f"{tmp_path}/nonexistent_path\n")
    target = tmp_path / "any.cpp"
    target.write_text(VALID_CPP_HEADER + "\n")
    r = run(target, exceptions=exc)
    assert r.returncode == 1
    assert "stale" in r.stdout.lower()


# -----------------------------------------------------------------------------
# Contract row 9: mixed_results_summary_correct
# -----------------------------------------------------------------------------
def test_mixed_results_summary_correct(tmp_path):
    paths = []
    for i in range(3):
        p = tmp_path / f"good{i}.cpp"
        p.write_text(VALID_CPP_HEADER + "\n")
        paths.append(p)
    bad1 = tmp_path / "bad1.cpp"
    bad1.write_text("int main() {}\n")
    bad2 = tmp_path / "bad2.cpp"
    bad2.write_text("// SPDX-License-Identifier: Apache-2.0\n")
    paths += [bad1, bad2]
    exempt = tmp_path / "exempt.cpp"
    exempt.write_text("int main() {}\n")
    paths.append(exempt)

    exc = tmp_path / "exceptions.txt"
    exc.write_text(f"{exempt}\n")

    r = run(*paths, exceptions=exc)
    assert r.returncode == 1
    assert "3 passed" in r.stdout, r.stdout
    assert "2 failed" in r.stdout, r.stdout
    assert "1 exempt" in r.stdout, r.stdout
    assert "out of 6 files" in r.stdout, r.stdout


# -----------------------------------------------------------------------------
# Contract row 10: git_ls_files_mode_works
# -----------------------------------------------------------------------------
def test_git_ls_files_mode_works():
    """`--all` should be equivalent to passing `git ls-files` output.

    Verifies the equivalence (same exit code), regardless of whether
    the current tree passes or fails. This decouples the test from
    the production exception list.
    """
    if not (REPO_ROOT / ".git").exists():
        pytest.skip("not a git repository")

    r_all = subprocess.run(
        [sys.executable, str(SCRIPT), "--all"],
        capture_output=True, text=True, cwd=REPO_ROOT)
    files = subprocess.check_output(
        ["git", "ls-files"], cwd=REPO_ROOT, text=True).splitlines()
    if not files:
        pytest.skip("git ls-files returned nothing")
    r_explicit = subprocess.run(
        [sys.executable, str(SCRIPT), *files],
        capture_output=True, text=True, cwd=REPO_ROOT)
    assert r_all.returncode == r_explicit.returncode, \
        "--all and explicit file list must produce the same exit code"
