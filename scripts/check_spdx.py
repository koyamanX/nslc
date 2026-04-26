#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""scripts/check_spdx.py — SPDX-header presence checker.

Per spec FR-010, FR-011, FR-012 and contract `spdx-check.contract.md`.

Usage:
  python3 scripts/check_spdx.py [--exceptions <path>] <file>...
  python3 scripts/check_spdx.py [--exceptions <path>] --all

Exit codes:
  0  every input file passed (or was exempt)
  1  one or more files failed
  2  script-internal error (bad arguments, unreadable exception list)

CI uses `--all` for the full-repo scan mandated by spec Q4.
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, Optional

EXPECTED_ID = "Apache-2.0 WITH LLVM-exception"

# -----------------------------------------------------------------------------
# Per-extension recipe table — data-model.md §entity 4
# -----------------------------------------------------------------------------

# Each entry: (comment_opener, comment_closer or None for line comments).
RECIPES_BY_EXT: dict[str, tuple[str, Optional[str]]] = {
    ".md":     ("<!--", "-->"),
    ".cpp":    ("//",   None),
    ".cc":     ("//",   None),
    ".cxx":    ("//",   None),
    ".c":      ("//",   None),
    ".h":      ("//",   None),
    ".hpp":    ("//",   None),
    ".hh":     ("//",   None),
    ".cmake":  ("#",    None),
    ".py":     ("#",    None),
    ".sh":     ("#",    None),
    ".bash":   ("#",    None),
    ".ebnf":   ("(*",   "*)"),
    ".nsl":    ("//",   None),
    ".yml":    ("#",    None),
    ".yaml":   ("#",    None),
    ".toml":   ("#",    None),
    ".test":   ("//",   None),  # lit fixtures
    ".lock":   ("#",    None),  # cmake/deps.lock and friends
    ".td":     ("//",   None),  # MLIR/LLVM TableGen
    ".mlir":   ("//",   None),  # MLIR text format (M4+)
}

# Special basenames (no extension or non-canonical extension).
RECIPES_BY_BASENAME: dict[str, tuple[str, Optional[str]]] = {
    "CMakeLists.txt": ("#",  None),
    ".gitignore":     ("#",  None),
    ".clang-format":  ("#",  None),
    ".clang-tidy":    ("#",  None),
    ".dockerignore":  ("#",  None),
    "Dockerfile":     ("#",  None),
    "Makefile":       ("#",  None),
}

# Files auto-exempt by basename (no SPDX header expected or possible).
AUTO_EXEMPT_BASENAMES = {
    "LICENSE",          # the project license itself
    ".keep",            # placeholder marker for empty dirs
    ".gitkeep",
}


@dataclass
class Result:
    path: str
    status: str           # "PASS" | "FAIL" | "EXEMPT" | "NO_RECIPE"
    expected: str = ""
    observed: str = ""
    note: str = ""


# -----------------------------------------------------------------------------
# Helpers
# -----------------------------------------------------------------------------

def _is_shebang(line: str) -> bool:
    return line.startswith("#!")


def find_recipe(path: Path) -> Optional[tuple[str, Optional[str]]]:
    name = path.name
    if name in RECIPES_BY_BASENAME:
        return RECIPES_BY_BASENAME[name]
    # Strip `.in` template suffix: Version.h.in → look up `.h`.
    suffixes = path.suffixes
    if len(suffixes) >= 2 and suffixes[-1] == ".in":
        return RECIPES_BY_EXT.get(suffixes[-2])
    if path.suffix in RECIPES_BY_EXT:
        return RECIPES_BY_EXT[path.suffix]
    return None


def expected_line(opener: str, closer: Optional[str]) -> str:
    line = f"{opener} SPDX-License-Identifier: {EXPECTED_ID}"
    if closer:
        line += f" {closer}"
    return line


def check_one_file(
    path: Path,
    exact_exceptions: set[str],
    dir_exceptions: list[str],
) -> Result:
    spath = str(path)

    # 1. Exception list takes precedence. We pre-normalised exception
    # paths to absolute strings against the repo root in
    # `load_exceptions`, so direct string comparison is correct.
    if spath in exact_exceptions:
        return Result(spath, "EXEMPT")
    for prefix in dir_exceptions:
        if spath == prefix or spath.startswith(prefix + os.sep):
            return Result(spath, "EXEMPT")

    # 2. Auto-exempt basenames.
    if path.name in AUTO_EXEMPT_BASENAMES:
        return Result(spath, "EXEMPT")

    # 3. Empty files are auto-exempt (the .keep convention plus general
    # robustness against zero-byte files).
    try:
        if os.path.getsize(path) == 0:
            return Result(spath, "EXEMPT")
    except OSError as exc:
        return Result(spath, "FAIL", note=f"cannot stat: {exc}")

    # 4. Recipe lookup.
    recipe = find_recipe(path)
    if recipe is None:
        return Result(
            spath, "NO_RECIPE",
            note=(f"no recipe registered for extension '{path.suffix}' "
                  "— add to scripts/check_spdx.py recipe table or to "
                  "scripts/spdx_exceptions.txt"))

    opener, closer = recipe
    want = expected_line(opener, closer)

    # 5. First non-empty, non-shebang line must equal `want`.
    try:
        with open(path, encoding="utf-8", errors="replace") as fh:
            for raw in fh:
                line = raw.rstrip("\r\n")
                stripped = line.strip()
                if not stripped:
                    continue
                if _is_shebang(line):
                    continue
                if line == want:
                    return Result(spath, "PASS")
                return Result(spath, "FAIL", expected=want, observed=line)
    except OSError as exc:
        return Result(spath, "FAIL", note=f"cannot read: {exc}")

    return Result(spath, "FAIL", expected=want, observed="(file has no SPDX header line)")


def load_exceptions(
    path: Path, repo_root: Path
) -> tuple[set[str], list[str], list[str]]:
    """Parse the exception list.

    Each non-blank, non-`#` line is either:
      - a directory prefix (trailing `/`) — every tracked file under
        that directory is EXEMPT.
      - an exact path (no trailing `/`) — that single file is EXEMPT.

    Both relative and absolute paths are accepted; the returned sets
    contain absolute path strings normalised against `repo_root` so
    `check_one_file` can do direct string comparison.

    Returns (exact_paths_abs, dir_prefixes_abs, raw_entries) — the
    third element is the list of original entries (used by stale-entry
    detection to keep the diagnostic readable).
    """
    if not path.exists():
        return set(), [], []
    exact: set[str] = set()
    dirs: list[str] = []
    raw_entries: list[str] = []
    try:
        for raw in path.read_text(encoding="utf-8").splitlines():
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            raw_entries.append(line)
            is_dir = line.endswith("/")
            stripped = line.rstrip("/")
            p = Path(stripped)
            if not p.is_absolute():
                p = repo_root / p
            abs_str = str(p)
            if is_dir:
                dirs.append(abs_str)
            else:
                exact.add(abs_str)
    except OSError as exc:
        print(f"check_spdx.py: cannot read {path}: {exc}", file=sys.stderr)
        sys.exit(2)
    return exact, dirs, raw_entries


def stale_exception_paths(raw_entries: list[str], repo_root: Path) -> list[str]:
    stale = []
    for entry in raw_entries:
        stripped = entry.rstrip("/")
        candidate = Path(stripped) if Path(stripped).is_absolute() \
            else repo_root / stripped
        if not candidate.exists():
            stale.append(entry)
    return stale


def gather_files_from_git(repo_root: Path) -> list[Path]:
    try:
        out = subprocess.check_output(
            ["git", "ls-files"], cwd=repo_root, text=True)
    except (subprocess.CalledProcessError, FileNotFoundError) as exc:
        print(f"check_spdx.py: git ls-files failed: {exc}", file=sys.stderr)
        sys.exit(2)
    return [repo_root / line for line in out.splitlines() if line]


# -----------------------------------------------------------------------------
# Output formatting
# -----------------------------------------------------------------------------

def emit_failure(r: Result) -> None:
    print(f"{r.path}:1: SPDX header missing or malformed")
    if r.expected:
        print(f"  expected: {r.expected}")
    if r.observed:
        print(f"  observed: {r.observed}")
    if r.note:
        print(f"  {r.note}")


# -----------------------------------------------------------------------------
# Main
# -----------------------------------------------------------------------------

def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description="SPDX-header presence checker (FR-010, FR-011, FR-012).")
    parser.add_argument(
        "--exceptions",
        default="scripts/spdx_exceptions.txt",
        help="path to the version-controlled exception list "
             "(default: scripts/spdx_exceptions.txt)")
    parser.add_argument(
        "--all", action="store_true",
        help="run on the output of `git ls-files` (FR-010 / spec Q4)")
    parser.add_argument("files", nargs="*",
                        help="files to check (required unless --all)")
    args = parser.parse_args(argv)

    repo_root = Path.cwd()
    exception_path = Path(args.exceptions)
    if not exception_path.is_absolute():
        exception_path = repo_root / exception_path
    exact_exc, dir_exc, raw_entries = load_exceptions(exception_path, repo_root)

    # Stale-exception detection (contract row 8).
    stale = stale_exception_paths(raw_entries, repo_root)
    if stale:
        for p in stale:
            print(f"FAIL: stale exception list entry: '{p}'")
        print(f"spdx-check: 0 passed, {len(stale)} failed, "
              f"0 exempt (out of 0 files)")
        return 1

    # Gather files.
    if args.all:
        files = gather_files_from_git(repo_root)
    else:
        if not args.files:
            print("check_spdx.py: no input files (use --all or pass paths)",
                  file=sys.stderr)
            return 2
        files = [Path(f) if Path(f).is_absolute() else repo_root / f
                 for f in args.files]

    # Check.
    results = [check_one_file(p, exact_exc, dir_exc) for p in files]
    n_pass   = sum(1 for r in results if r.status == "PASS")
    n_exempt = sum(1 for r in results if r.status == "EXEMPT")
    failures = [r for r in results if r.status in ("FAIL", "NO_RECIPE")]

    for r in failures:
        emit_failure(r)
    print(f"spdx-check: {n_pass} passed, {len(failures)} failed, "
          f"{n_exempt} exempt (out of {len(results)} files)")

    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
