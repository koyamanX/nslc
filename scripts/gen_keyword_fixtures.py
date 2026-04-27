#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""scripts/gen_keyword_fixtures.py — generate per-keyword lex fixtures.

Reads `include/nsl/Lex/KeywordSet.def` (the X-macro single-source-of-
truth for the NSL reserved-keyword set per `lang.ebnf` §15) and emits
one `.test` file per keyword under `test/lex/keywords/`.

Per Constitution Principle V (deterministic): given the same input
`KeywordSet.def`, this script produces byte-identical output across
runs. No hash-derived ordering, no timestamps, no environment-derived
state in the emitted text.

Per Constitution Principle VIII (TDD): the fixtures land BEFORE the
lexer + driver implementation; running lit against the unchanged tree
will observe each fixture FAILING (no `nslc -emit=tokens` flag yet).

Usage:
  python3 scripts/gen_keyword_fixtures.py [--force]

Options:
  --force   Overwrite existing .test files. Without --force the script
            skips files that already exist (safer regeneration).

Output:
  Prints `Generated N fixtures` (or `Generated N fixtures, skipped M
  existing` when some pre-existed) on stdout at exit.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# Match `KEYWORD(<enum_suffix>, "<spelling>")` in KeywordSet.def. The
# def file pads with whitespace for alignment; tolerate it.
KEYWORD_RE = re.compile(
    r'^\s*KEYWORD\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*,\s*"([^"]+)"\s*\)',
)

FIXTURE_TEMPLATE = """\
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Generated from include/nsl/Lex/KeywordSet.def — DO NOT EDIT.
// Regenerate with: python3 scripts/gen_keyword_fixtures.py
//
// Per-keyword lex fixture for `{spelling}` per Constitution Principle
// VIII (TDD). One token + tk_eof; SPDX header per FR-010..012.
//
// RUN: %nslc -emit=tokens %s | FileCheck %s

{spelling}
// CHECK:      tk_{enum_suffix}{TAB}{spelling}{TAB}{{{{[^:]+}}}}:1:1:0{TAB}{{{{[^:]+}}}}:1:1{TAB}[]
// CHECK-NEXT: tk_eof{TAB}{TAB}{{{{[^:]+}}}}:{eof_line}:{eof_col}:{eof_off}{TAB}{{{{[^:]+}}}}:{eof_line}:{eof_col}{TAB}[]
"""


def parse_keyword_set(def_path: Path) -> list[tuple[str, str]]:
    """Return [(enum_suffix, spelling), …] in declaration order."""
    entries: list[tuple[str, str]] = []
    for raw in def_path.read_text(encoding="utf-8").splitlines():
        # Skip comment-only lines (// or block-comment debris). The
        # regex's leading-whitespace tolerance handles indented entries
        # if any are added in future revisions.
        m = KEYWORD_RE.match(raw)
        if m:
            entries.append((m.group(1), m.group(2)))
    return entries


def emit_fixture(spelling: str, enum_suffix: str) -> str:
    """Produce the fixture text for a single keyword.

    The fixture contains the keyword text on line 1 followed by a
    trailing newline. After the lexer scans the keyword, tk_eof is
    reported at the position immediately past the spelling — typically
    line 2, col 1, offset (len + 1) accounting for the newline. The
    spec is silent on whether tk_eof appears at end-of-line-1 or
    line-2-col-1; either is acceptable. We assert line 2 col 1 here
    matching the contract example's pattern (a final newline pushes
    the EOF location to the next line). Should the implementation pin
    a different position, regenerate and commit.
    """
    spelling_len = len(spelling)
    # The fixture file body is `<spelling>\n`; tk_eof sits at offset
    # spelling_len + 1 (past the newline) on line 2, col 1.
    eof_off = spelling_len + 1
    eof_line = 2
    eof_col = 1
    return FIXTURE_TEMPLATE.format(
        spelling=spelling,
        enum_suffix=enum_suffix,
        TAB="\t",
        eof_line=eof_line,
        eof_col=eof_col,
        eof_off=eof_off,
    )


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--force",
        action="store_true",
        help="Overwrite existing fixtures (default: skip them).",
    )
    parser.add_argument(
        "--def-file",
        default=None,
        help="Path to KeywordSet.def (default: include/nsl/Lex/KeywordSet.def).",
    )
    parser.add_argument(
        "--out-dir",
        default=None,
        help="Output directory (default: test/lex/keywords/).",
    )
    args = parser.parse_args(argv)

    repo_root = Path(__file__).resolve().parent.parent
    def_path = (
        Path(args.def_file)
        if args.def_file
        else repo_root / "include" / "nsl" / "Lex" / "KeywordSet.def"
    )
    out_dir = (
        Path(args.out_dir)
        if args.out_dir
        else repo_root / "test" / "lex" / "keywords"
    )

    if not def_path.is_file():
        print(f"error: cannot read {def_path}", file=sys.stderr)
        return 2

    out_dir.mkdir(parents=True, exist_ok=True)

    entries = parse_keyword_set(def_path)
    if not entries:
        print(f"error: no KEYWORD(...) entries found in {def_path}",
              file=sys.stderr)
        return 2

    written = 0
    skipped = 0
    # Iterate in declaration order (Principle V — deterministic).
    for enum_suffix, spelling in entries:
        # Filename uses the SPELLING (so reviewers grep by keyword text)
        # — note that distinct enum_suffix entries always have distinct
        # spellings per `lang.ebnf` §15, so no collisions.
        out_path = out_dir / f"{spelling}.test"
        if out_path.exists() and not args.force:
            skipped += 1
            continue
        out_path.write_text(
            emit_fixture(spelling, enum_suffix),
            encoding="utf-8",
        )
        written += 1

    if skipped:
        print(f"Generated {written} fixtures, skipped {skipped} existing")
    else:
        print(f"Generated {written} fixtures")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
