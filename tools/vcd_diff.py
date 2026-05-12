#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""
tools/vcd_diff.py — M7 semantic-equal VCD comparator (FR-021).

Pinned by `specs/011-m7-driver-e2e/contracts/vcd-diff.contract.md`.

Compares two VCD files for semantic equivalence:
  - Ignores `$date` / `$version` / `$timescale` / `$comment`
    declarations (simulator-toolchain metadata, not RTL semantics).
  - Extracts signal sets as (scope-path, name, width) tuples.
  - Applies optional `SIGNAL_MAP.toml` aliases for signals named
    differently between golden and emitted sides.
  - Compares value-change record sequences on the matched-signal
    intersection.

Exit codes:
  0 — semantic-equal
  1 — divergence; report on stderr / `--report`
  2 — parse error in one or both inputs
  3 — bad CLI

Python 3.11+ stdlib only (no PyPI deps). `tomllib` (3.11+) is used
for SIGNAL_MAP.toml parsing per Q2 → B.
"""

from __future__ import annotations

import argparse
import logging
import sys
from dataclasses import dataclass, field
from pathlib import Path

try:
    import tomllib
except ImportError:  # pragma: no cover - Python <3.11
    print(
        "vcd_diff.py: requires Python 3.11+ (tomllib stdlib module)",
        file=sys.stderr,
    )
    sys.exit(3)


# -----------------------------------------------------------------------------
# Parsed VCD data structures
# -----------------------------------------------------------------------------


@dataclass
class VarDecl:
    """A `$var` declaration: scope-path-tuple + name + width + id-char."""

    scope: tuple[str, ...]
    name: str
    width: int
    id_char: str

    def full_path(self) -> str:
        return ".".join((*self.scope, self.name))


@dataclass
class VCDDoc:
    """Parsed VCD: variables keyed by id-char + value-change records."""

    # id-char → VarDecl
    vars_by_id: dict[str, VarDecl] = field(default_factory=dict)
    # (timestamp, id-char) → value-string, recorded in walk order.
    changes: list[tuple[int, str, str]] = field(default_factory=list)


class VCDParseError(Exception):
    """Raised on malformed VCD input."""


# -----------------------------------------------------------------------------
# VCD parser
# -----------------------------------------------------------------------------

# Declarations whose body we consume + ignore (not retained).
_IGNORED_DECLS = frozenset({"$date", "$version", "$timescale", "$comment"})


def parse_vcd(path: Path) -> VCDDoc:
    """Parse a VCD file. Raises VCDParseError on malformed input."""
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError as ex:
        raise VCDParseError(f"cannot read {path}: {ex}") from ex

    doc = VCDDoc()
    scope_stack: list[str] = []
    timestamp = 0
    tokens = text.split()
    i = 0
    in_definitions = True
    while i < len(tokens):
        tok = tokens[i]

        # Ignored declarations: consume everything up to $end.
        if tok in _IGNORED_DECLS:
            end_i = _find_end(tokens, i + 1)
            if end_i is None:
                raise VCDParseError(f"unterminated {tok} declaration")
            i = end_i + 1
            continue

        # $scope <kind> <name> $end
        if tok == "$scope":
            if i + 3 >= len(tokens):
                raise VCDParseError("truncated $scope declaration")
            # i+1 = kind (module/task/function/begin); i+2 = name; i+3 = $end
            scope_stack.append(tokens[i + 2])
            if tokens[i + 3] != "$end":
                raise VCDParseError(
                    f"malformed $scope: expected $end, got {tokens[i + 3]!r}"
                )
            i += 4
            continue

        if tok == "$upscope":
            if i + 1 >= len(tokens) or tokens[i + 1] != "$end":
                raise VCDParseError("malformed $upscope")
            if not scope_stack:
                raise VCDParseError("$upscope with empty scope stack")
            scope_stack.pop()
            i += 2
            continue

        # $var <type> <width> <id-char(s)> <name> [bus-range...] $end
        if tok == "$var":
            end_i = _find_end(tokens, i + 1)
            if end_i is None or end_i - i < 5:
                raise VCDParseError("malformed $var declaration")
            # Layout: $var TYPE WIDTH IDCHAR NAME [bus...] $end
            try:
                width = int(tokens[i + 2])
            except ValueError as ex:
                raise VCDParseError(
                    f"non-integer width in $var: {tokens[i + 2]!r}"
                ) from ex
            id_char = tokens[i + 3]
            name = tokens[i + 4]
            decl = VarDecl(
                scope=tuple(scope_stack),
                name=name,
                width=width,
                id_char=id_char,
            )
            doc.vars_by_id[id_char] = decl
            i = end_i + 1
            continue

        if tok == "$enddefinitions":
            if i + 1 >= len(tokens) or tokens[i + 1] != "$end":
                raise VCDParseError("malformed $enddefinitions")
            in_definitions = False
            i += 2
            continue

        # $dumpvars / $dumpall / $dumpoff / $dumpon — boundary markers;
        # the value-change records inside are processed below.
        if tok in ("$dumpvars", "$dumpall", "$dumpon", "$dumpoff"):
            i += 1
            continue
        if tok == "$end":
            i += 1
            continue

        # Timestamp: #<integer>
        if tok.startswith("#"):
            try:
                timestamp = int(tok[1:])
            except ValueError as ex:
                raise VCDParseError(
                    f"malformed timestamp: {tok!r}"
                ) from ex
            i += 1
            continue

        # In definitions section, any other token is malformed.
        if in_definitions:
            raise VCDParseError(f"unexpected token in definitions: {tok!r}")

        # Value-change records:
        #   single-bit: <0|1|x|X|z|Z><id-char>
        #   vectored:   b<bits> <id-char>     (two tokens)
        #   real:       r<value> <id-char>    (two tokens)
        if tok[:1] in ("0", "1", "x", "X", "z", "Z"):
            value = tok[:1]
            id_char = tok[1:]
            if id_char not in doc.vars_by_id:
                # Stray reference; tolerate (some VCDs include extra IDs).
                i += 1
                continue
            doc.changes.append((timestamp, id_char, value))
            i += 1
            continue
        if tok[:1] in ("b", "B", "r", "R"):
            # Vectored / real value: token + next-token id-char.
            value = tok
            if i + 1 >= len(tokens):
                raise VCDParseError(
                    f"truncated vectored value record at {tok!r}"
                )
            id_char = tokens[i + 1]
            doc.changes.append((timestamp, id_char, value))
            i += 2
            continue

        raise VCDParseError(f"unexpected token: {tok!r}")

    return doc


def _find_end(tokens: list[str], start: int) -> int | None:
    """Find the next `$end` token at or after `start`; return its index."""
    for j in range(start, len(tokens)):
        if tokens[j] == "$end":
            return j
    return None


# -----------------------------------------------------------------------------
# Signal-map loader
# -----------------------------------------------------------------------------


def load_signal_map(path: Path) -> dict[str, str]:
    """Load a SIGNAL_MAP.toml; return golden-name → emitted-name dict."""
    try:
        with open(path, "rb") as f:
            data = tomllib.load(f)
    except (OSError, tomllib.TOMLDecodeError) as ex:
        raise VCDParseError(f"cannot read signal-map {path}: {ex}") from ex
    aliases = data.get("alias", [])
    result: dict[str, str] = {}
    for entry in aliases:
        g = entry.get("golden")
        e = entry.get("emitted")
        if not isinstance(g, str) or not isinstance(e, str):
            raise VCDParseError(
                f"SIGNAL_MAP entry missing golden/emitted strings: {entry!r}"
            )
        result[g] = e
    return result


# -----------------------------------------------------------------------------
# Comparator
# -----------------------------------------------------------------------------


@dataclass
class DivergenceReport:
    """First-divergence report; format pinned by vcd-diff.contract.md §6."""

    signal_path: str
    width_golden: int
    width_emitted: int
    timestamp: int | None
    value_golden: str
    value_emitted: str
    width_mismatch: bool = False


def _changes_by_id(
    doc: VCDDoc,
) -> dict[str, list[tuple[int, str]]]:
    """Group value-change records by id-char (preserving walk order)."""
    out: dict[str, list[tuple[int, str]]] = {}
    for ts, idc, val in doc.changes:
        out.setdefault(idc, []).append((ts, val))
    return out


def compare(
    golden: VCDDoc,
    emitted: VCDDoc,
    signal_map: dict[str, str],
) -> tuple[DivergenceReport | None, int, int, int]:
    """Compare two VCDDocs. Return (first-divergence-or-None, matched, unmatched-golden, unmatched-emitted)."""
    # Build golden full-path → VarDecl + id-char.
    golden_by_path: dict[str, tuple[VarDecl, str]] = {
        decl.full_path(): (decl, idc) for idc, decl in golden.vars_by_id.items()
    }
    emitted_by_path: dict[str, tuple[VarDecl, str]] = {
        decl.full_path(): (decl, idc) for idc, decl in emitted.vars_by_id.items()
    }

    matched: list[tuple[str, VarDecl, str, VarDecl, str]] = []
    unmatched_golden = 0
    for g_path, (g_decl, g_idc) in golden_by_path.items():
        # Apply signal-map alias if present.
        e_path = signal_map.get(g_path, g_path)
        e_entry = emitted_by_path.get(e_path)
        if e_entry is None:
            unmatched_golden += 1
            continue
        e_decl, e_idc = e_entry
        matched.append((g_path, g_decl, g_idc, e_decl, e_idc))

    # Count emitted signals not matched (for the verbose summary).
    matched_emitted_paths = {
        signal_map.get(g_path, g_path) for g_path, _, _, _, _ in matched
    }
    unmatched_emitted = sum(
        1 for path in emitted_by_path if path not in matched_emitted_paths
    )

    golden_changes = _changes_by_id(golden)
    emitted_changes = _changes_by_id(emitted)

    for g_path, g_decl, g_idc, e_decl, e_idc in matched:
        # Width mismatch on a matched pair is a divergence.
        if g_decl.width != e_decl.width:
            return (
                DivergenceReport(
                    signal_path=g_path,
                    width_golden=g_decl.width,
                    width_emitted=e_decl.width,
                    timestamp=None,
                    value_golden=f"{g_decl.width}-bit",
                    value_emitted=f"{e_decl.width}-bit",
                    width_mismatch=True,
                ),
                len(matched),
                unmatched_golden,
                unmatched_emitted,
            )

        g_seq = golden_changes.get(g_idc, [])
        e_seq = emitted_changes.get(e_idc, [])
        for (g_ts, g_val), (e_ts, e_val) in zip(g_seq, e_seq):
            if g_ts != e_ts or g_val != e_val:
                return (
                    DivergenceReport(
                        signal_path=g_path,
                        width_golden=g_decl.width,
                        width_emitted=e_decl.width,
                        timestamp=g_ts,
                        value_golden=g_val,
                        value_emitted=e_val,
                    ),
                    len(matched),
                    unmatched_golden,
                    unmatched_emitted,
                )
        if len(g_seq) != len(e_seq):
            # Trailing-record length mismatch.
            ts = g_seq[len(e_seq)][0] if len(e_seq) < len(g_seq) else e_seq[len(g_seq)][0]
            return (
                DivergenceReport(
                    signal_path=g_path,
                    width_golden=g_decl.width,
                    width_emitted=e_decl.width,
                    timestamp=ts,
                    value_golden=f"{len(g_seq)} change(s)",
                    value_emitted=f"{len(e_seq)} change(s)",
                ),
                len(matched),
                unmatched_golden,
                unmatched_emitted,
            )

    return None, len(matched), unmatched_golden, unmatched_emitted


# -----------------------------------------------------------------------------
# CLI
# -----------------------------------------------------------------------------


def render_divergence(
    report: DivergenceReport,
    golden_path: Path,
    emitted_path: Path,
    matched: int,
    unmatched_golden: int,
    unmatched_emitted: int,
    out,
) -> None:
    """Render the first-divergence report per vcd-diff.contract.md §6."""
    out.write("VCD divergence:\n")
    out.write(f"  Golden:  {golden_path}\n")
    out.write(f"  Emitted: {emitted_path}\n\n")
    if report.width_mismatch:
        out.write("Width mismatch on matched signal pair:\n")
        out.write(f"  Signal:        {report.signal_path}\n")
        out.write(f"  Golden width:  {report.width_golden}\n")
        out.write(f"  Emitted width: {report.width_emitted}\n\n")
    else:
        out.write(f"First divergence at simulation time #{report.timestamp}:\n")
        out.write(
            f"  Signal:  {report.signal_path}[{report.width_golden}]\n"
        )
        out.write(f"  Golden value:  {report.value_golden}\n")
        out.write(f"  Emitted value: {report.value_emitted}\n\n")
    out.write(f"Total signals matched: {matched}\n")
    out.write(f"Signals unmatched on golden side: {unmatched_golden}\n")
    out.write(f"Signals unmatched on emitted side: {unmatched_emitted}\n")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="vcd_diff.py",
        description=(
            "M7 semantic-equal VCD comparator. Compares golden vs emitted "
            "VCD files ignoring simulator metadata; honors per-project "
            "SIGNAL_MAP.toml aliases."
        ),
    )
    parser.add_argument("golden", type=Path, help="Path to the reference golden VCD")
    parser.add_argument(
        "emitted", type=Path, help="Path to the VCD captured from nslc-emitted output"
    )
    parser.add_argument(
        "--signal-map",
        type=Path,
        default=None,
        help="Optional TOML file aliasing upstream-vs-nslc signal names",
    )
    parser.add_argument(
        "--report",
        type=Path,
        default=None,
        help="Write divergence report to this path instead of stderr",
    )
    parser.add_argument(
        "-v", "--verbose", action="store_true", help="Emit INFO logs for unmatched signals"
    )

    try:
        args = parser.parse_args(argv)
    except SystemExit as exc:
        # argparse calls sys.exit(2) on error; we want exit code 3 for bad CLI.
        return 3 if exc.code != 0 else 0

    logging.basicConfig(
        level=logging.INFO if args.verbose else logging.WARNING,
        format="%(message)s",
    )

    try:
        golden = parse_vcd(args.golden)
        emitted = parse_vcd(args.emitted)
    except VCDParseError as ex:
        print(f"vcd_diff: parse error: {ex}", file=sys.stderr)
        return 2

    signal_map: dict[str, str] = {}
    if args.signal_map is not None:
        try:
            signal_map = load_signal_map(args.signal_map)
        except VCDParseError as ex:
            print(f"vcd_diff: {ex}", file=sys.stderr)
            return 2

    report, matched, unmatched_g, unmatched_e = compare(
        golden, emitted, signal_map
    )

    if args.verbose:
        logging.info(
            "matched=%d  unmatched_golden=%d  unmatched_emitted=%d",
            matched,
            unmatched_g,
            unmatched_e,
        )

    if report is None:
        return 0

    out = sys.stderr if args.report is None else open(args.report, "w", encoding="utf-8")
    try:
        render_divergence(
            report,
            args.golden,
            args.emitted,
            matched,
            unmatched_g,
            unmatched_e,
            out,
        )
    finally:
        if args.report is not None:
            out.close()
    return 1


if __name__ == "__main__":
    sys.exit(main())
