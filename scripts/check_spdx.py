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
    ".js":     ("//",   None),  # T8 hand-authored grammar.js + generated mirror
    ".ts":     ("//",   None),  # T8 VS Code extension TypeScript shell
    ".scm":    (";",    None),  # T8 tree-sitter highlight queries (Scheme)
    ".ebnf":   ("(*",   "*)"),
    ".nsl":    ("//",   None),
    ".nslh":   ("//",   None),
    ".yml":    ("#",    None),
    ".yaml":   ("#",    None),
    ".toml":   ("#",    None),
    ".test":   ("//",   None),  # lit fixtures
    ".lock":   ("#",    None),  # cmake/deps.lock and friends
    ".td":     ("//",   None),  # MLIR/LLVM TableGen
    ".def":    ("//",   None),  # X-macro single-source-of-truth headers
    ".mlir":   ("//",   None),  # MLIR text format (M4+)
    ".txt":    ("#",    None),  # plain-text config (spdx_exceptions.txt etc.)
    ".cfg":    ("#",    None),  # config files (.lit configs etc.)
    # JSON has no comment syntax — the SPDX header rides on a
    # top-level `_comment_top` JSON key per
    # specs/009-t1-textmate-grammar/research.md §2 (precedent:
    # .github/branch-protection.json). The recipe pseudo-tuple
    # below carries the literal opener `"_comment_top": "` so the
    # first-line match logic finds it.
    ".json":   ('"_comment_top": "', None),
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
    "SPDX.NOTICE",      # T8 — companion NOTICE files documenting
                        # license inheritance for generator-output
                        # artefacts that cannot carry per-file SPDX
                        # headers (per
                        # specs/010-t8-tree-sitter-grammar/research.md
                        # §9). The NOTICE itself MAY carry an SPDX
                        # line on line 1 by convention but is not
                        # subject to header validation here.
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


def _is_syntax_test_marker(line: str) -> bool:
    """`vscode-tmgrammar-test` test fixtures REQUIRE
    `// SYNTAX TEST "<scope>"` as the literal first line (see
    test/tooling/textmate/node_modules/vscode-tmgrammar-test/README.md
    §"Unit tests"). Treat it like a shebang — allowed on line 1
    before the SPDX header."""
    return line.startswith('// SYNTAX TEST "')


def find_recipe(path: Path) -> Optional[tuple[str, Optional[str]]]:
    name = path.name
    if name in RECIPES_BY_BASENAME:
        return RECIPES_BY_BASENAME[name]
    # Strip template suffix: Version.h.in → look up `.h`;
    # grammar.js.template → look up `.js`. Both `.in` and
    # `.template` are project-wide template-suffix conventions
    # (T8 introduces `.template`).
    suffixes = path.suffixes
    if len(suffixes) >= 2 and suffixes[-1] in (".in", ".template"):
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
    # JSON exception: the `_comment_top` JSON-key convention puts
    # the SPDX header on line 2 (after the opening `{`); so for
    # `.json` files we match against any line whose stripped form
    # STARTS WITH `"_comment_top": "SPDX-License-Identifier: <id>`.
    is_json = path.suffix == ".json"
    try:
        with open(path, encoding="utf-8", errors="replace") as fh:
            for raw in fh:
                line = raw.rstrip("\r\n")
                stripped = line.strip()
                if not stripped:
                    continue
                if _is_shebang(line):
                    continue
                if _is_syntax_test_marker(line):
                    continue
                if is_json:
                    # Skip the JSON object opener `{` (and any
                    # plain wrapper line) so the SPDX match falls
                    # on the first content line of the object.
                    if stripped == "{":
                        continue
                    json_want_prefix = (
                        f'"_comment_top": "SPDX-License-Identifier: '
                        f'{EXPECTED_ID}'
                    )
                    if stripped.startswith(json_want_prefix):
                        return Result(spath, "PASS")
                    return Result(
                        spath, "FAIL",
                        expected=(
                            f'(JSON) line starting with '
                            f'"_comment_top": "SPDX-License-Identifier: '
                            f'{EXPECTED_ID}'
                        ),
                        observed=stripped,
                    )
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
    # `-c safe.directory=*` tolerates the dubious-ownership case that
    # arises when the source tree is mounted into a Docker container
    # (the volume's uid/gid mismatch the running user). Same workaround
    # used in cmake/NSLVersion.cmake.
    try:
        out = subprocess.check_output(
            ["git", "-c", "safe.directory=*", "ls-files"],
            cwd=repo_root, text=True)
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
