#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""scripts/check_dialect_coverage.py — M4 dialect fixture-coverage CI guard.

Per `specs/007-m4-mlir-dialect/spec.md` FR-021 + research.md §9. Runs in
CI's static-checks stage (Constitution Principle IX stage 2) to enforce
per-op + per-invariant fixture existence under `test/Dialect/`.

Authoritative data sources:
  - `lib/Dialect/NSL/IR/NSLOps.td` — registered op set (greps
    `def NSL_*Op : NSL_Op<"<name>", ...>` records).
  - `.specify/m4_invariant_table.json` — per-op invariant list
    (machine-readable mirror of spec FR-013 maintained in sync with
    the spec at PR-author time). Schema in `data-model.md` §8.

Coverage checks:
  1. For every op `nsl.<name>` in the op set, assert at least one
     `<name>_roundtrip.mlir` fixture exists under
     `test/Dialect/<some-category>/`.
  2. For every cell in `m4_invariant_table.json` with `invariants` ≥ 1,
     assert at least one `<name>_invalid_*.mlir` fixture exists under
     `test/Dialect/<some-category>/`.

Exit codes:
  0  every coverage check passed (or vacuous because op set is empty)
  1  one or more coverage checks failed
  2  script-internal error (bad arguments, malformed JSON, etc.)

Usage:
  python3 scripts/check_dialect_coverage.py
  python3 scripts/check_dialect_coverage.py --repo-root <path>

At Phase 2 of M4 the op set is empty and the JSON's `ops` array is
empty, so this script passes vacuously. Phase 3 (T085) populates the
op list and the round-trip-fixture check goes live; Phase 4 (T119)
populates the invariants and the invalid-fixture check goes live.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path

# Match `def NSL_FooOp : NSL_Op<"<mnemonic>", ...>` lines in NSLOps.td.
# The `<mnemonic>` is the bare op name (e.g., `module`, `proc`) — what
# we use in fixture filenames as `<mnemonic>_roundtrip.mlir`.
_OP_RECORD_RE = re.compile(
    r'^\s*def\s+NSL_\w+Op\s*:\s*NSL_Op<\s*"([^"]+)"',
)


@dataclass
class CoverageResult:
    op_name: str
    kind: str  # "roundtrip" or "invalid"
    found: bool


def _find_repo_root() -> Path:
    """Locate the repo root by walking upward from the script location."""
    here = Path(__file__).resolve().parent
    candidate = here.parent
    if (candidate / "CMakeLists.txt").is_file():
        return candidate
    return Path.cwd()


def parse_registered_ops(td_path: Path) -> list[str]:
    """Return the ordered list of `nsl.<name>` mnemonics from NSLOps.td."""
    if not td_path.is_file():
        # Phase 2 vacuous case: NSLOps.td may be scaffolding-only.
        return []
    names: list[str] = []
    with td_path.open(encoding="utf-8") as fh:
        for line in fh:
            m = _OP_RECORD_RE.match(line)
            if m:
                names.append(m.group(1))
    return names


def load_invariant_table(json_path: Path) -> dict:
    """Return the parsed `.specify/m4_invariant_table.json` payload."""
    if not json_path.is_file():
        return {"ops": []}
    with json_path.open(encoding="utf-8") as fh:
        return json.load(fh)


def find_fixtures(dialect_root: Path, op: str, kind: str) -> list[Path]:
    """Locate fixtures matching `<op>_<kind>.mlir` or `<op>_invalid_*.mlir`.

    `kind == "roundtrip"`: looks for `<op>_roundtrip.mlir` under any
    `test/Dialect/<category>/` subdirectory.

    `kind == "invalid"`: looks for any `<op>_invalid_<reason>.mlir`
    matching the prefix.
    """
    if not dialect_root.is_dir():
        return []
    if kind == "roundtrip":
        pattern = f"{op}_roundtrip.mlir"
    elif kind == "invalid":
        pattern = f"{op}_invalid_*.mlir"
    else:
        raise ValueError(f"unknown kind: {kind!r}")
    return list(dialect_root.rglob(pattern))


def check_coverage(
    repo_root: Path,
) -> tuple[list[CoverageResult], list[str]]:
    """Run the two coverage checks. Returns (results, errors)."""
    td_path = repo_root / "lib" / "Dialect" / "NSL" / "IR" / "NSLOps.td"
    json_path = repo_root / ".specify" / "m4_invariant_table.json"
    dialect_root = repo_root / "test" / "Dialect"

    op_names = parse_registered_ops(td_path)
    invariant_table = load_invariant_table(json_path)
    invariant_ops = {entry["name"]: entry for entry in invariant_table.get("ops", [])}

    results: list[CoverageResult] = []
    errors: list[str] = []

    # Check 1: per-registered-op round-trip fixture.
    for op in op_names:
        fixtures = find_fixtures(dialect_root, op, "roundtrip")
        found = bool(fixtures)
        results.append(CoverageResult(op, "roundtrip", found))
        if not found:
            errors.append(
                f"FR-021: op 'nsl.{op}' has no round-trip fixture; "
                f"expected `test/Dialect/<category>/{op}_roundtrip.mlir`."
            )

    # Check 2: per-invariant invalid fixture (only ops with ≥ 1 invariant).
    for entry in invariant_table.get("ops", []):
        op_full = entry.get("name", "")
        # Strip the `nsl.` prefix if present in the JSON (data-model §8
        # canonical form is `nsl.<name>`).
        op = op_full[len("nsl.") :] if op_full.startswith("nsl.") else op_full
        invariants = entry.get("invariants", []) or []
        if not invariants:
            continue
        fixtures = find_fixtures(dialect_root, op, "invalid")
        found = bool(fixtures)
        results.append(CoverageResult(op, "invalid", found))
        if not found:
            errors.append(
                f"FR-021: op 'nsl.{op}' has {len(invariants)} structural "
                f"invariant(s) but no invalid fixture; expected at least one "
                f"`test/Dialect/<category>/{op}_invalid_<reason>.mlir`."
            )

    return results, errors


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        description="M4 dialect fixture-coverage CI guard "
        "(spec FR-021 + research.md §9)."
    )
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=None,
        help="Repository root (auto-detected from script location).",
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="Suppress the per-op pass log; print errors only.",
    )
    args = parser.parse_args(argv)

    repo_root = args.repo_root or _find_repo_root()
    if not repo_root.is_dir():
        print(f"error: repo-root {repo_root} not a directory", file=sys.stderr)
        return 2

    try:
        results, errors = check_coverage(repo_root)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    if not args.quiet:
        if not results:
            print(
                "[check_dialect_coverage] op set is empty (Phase 2 "
                "vacuous case); coverage check passes."
            )
        else:
            for r in results:
                status = "OK" if r.found else "MISS"
                print(f"[check_dialect_coverage] {r.kind:<10} nsl.{r.op_name:<30} {status}")

    if errors:
        for err in errors:
            print(f"error: {err}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
