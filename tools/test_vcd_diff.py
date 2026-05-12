#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# tools/test_vcd_diff.py — unittest suite for tools/vcd_diff.py
# (M7 US3 T057).
#
# **Test discipline (Constitution Principle VIII; TDD)**: this
# file lands BEFORE tools/vcd_diff.py's implementation body
# (T058 verify red → T059 implement → T060 verify green). The 8
# test cases pin the public-facing behavior of vcd_diff.py per
# vcd-diff.contract.md §7.
#
# Invocation: `python3 -m unittest tools/test_vcd_diff.py` from
# repo root. Wired into scripts/ci.sh at T061.

"""Unit tests for vcd_diff.py — the M7 semantic-equal VCD comparator."""

from __future__ import annotations

import subprocess
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
VCD_DIFF = REPO_ROOT / "tools" / "vcd_diff.py"


def run(args: list[str], stdin: str | None = None) -> subprocess.CompletedProcess:
    """Invoke vcd_diff.py with the given args; capture stdout/stderr."""
    return subprocess.run(
        [sys.executable, str(VCD_DIFF), *args],
        input=stdin,
        capture_output=True,
        text=True,
        timeout=30,
    )


# -----------------------------------------------------------------------------
# Test fixtures (inline VCD strings)
# -----------------------------------------------------------------------------

VCD_IDENTICAL_A = textwrap.dedent(
    """\
    $date Mon May 12 00:00:00 2026 $end
    $version vcd_diff_test_v1 $end
    $timescale 1ns $end
    $scope module top $end
    $var wire 8 ! q [7:0] $end
    $var reg 8 # r [7:0] $end
    $upscope $end
    $enddefinitions $end
    #0
    $dumpvars
    b00000000 !
    b00000000 #
    $end
    #10
    b00000001 #
    b00000001 !
    #20
    b00000010 #
    b00000010 !
    """
)

# Byte-different in `$date` ($version) only.
VCD_HEADER_DRIFT = textwrap.dedent(
    """\
    $date Tue Jun 01 12:00:00 2027 $end
    $version other_simulator_v2.3 $end
    $timescale 100ps $end
    $scope module top $end
    $var wire 8 ! q [7:0] $end
    $var reg 8 # r [7:0] $end
    $upscope $end
    $enddefinitions $end
    #0
    $dumpvars
    b00000000 !
    b00000000 #
    $end
    #10
    b00000001 #
    b00000001 !
    #20
    b00000010 #
    b00000010 !
    """
)

# Differs in value: r becomes b00000011 at #20 (vs b00000010).
VCD_VALUE_DIFFERS = textwrap.dedent(
    """\
    $date Mon May 12 00:00:00 2026 $end
    $version vcd_diff_test_v1 $end
    $timescale 1ns $end
    $scope module top $end
    $var wire 8 ! q [7:0] $end
    $var reg 8 # r [7:0] $end
    $upscope $end
    $enddefinitions $end
    #0
    $dumpvars
    b00000000 !
    b00000000 #
    $end
    #10
    b00000001 #
    b00000001 !
    #20
    b00000011 #
    b00000010 !
    """
)

# Missing one signal (no `r`) on the emitted side; intersection-only
# comparison should pass on signal `q`.
VCD_MISSING_SIGNAL = textwrap.dedent(
    """\
    $date Mon May 12 00:00:00 2026 $end
    $version vcd_diff_test_v1 $end
    $timescale 1ns $end
    $scope module top $end
    $var wire 8 ! q [7:0] $end
    $upscope $end
    $enddefinitions $end
    #0
    $dumpvars
    b00000000 !
    $end
    #10
    b00000001 !
    #20
    b00000010 !
    """
)

# Differently-named: r→r_alt on emitted side. Needs SIGNAL_MAP.
VCD_SIGNAL_RENAMED = textwrap.dedent(
    """\
    $date Mon May 12 00:00:00 2026 $end
    $version vcd_diff_test_v1 $end
    $timescale 1ns $end
    $scope module top $end
    $var wire 8 ! q [7:0] $end
    $var reg 8 # r_alt [7:0] $end
    $upscope $end
    $enddefinitions $end
    #0
    $dumpvars
    b00000000 !
    b00000000 #
    $end
    #10
    b00000001 #
    b00000001 !
    #20
    b00000010 #
    b00000010 !
    """
)

# r has different width (16 instead of 8). Should be a divergence
# on the matched-signal pair.
VCD_WIDTH_MISMATCH = textwrap.dedent(
    """\
    $date Mon May 12 00:00:00 2026 $end
    $version vcd_diff_test_v1 $end
    $timescale 1ns $end
    $scope module top $end
    $var wire 8 ! q [7:0] $end
    $var reg 16 # r [15:0] $end
    $upscope $end
    $enddefinitions $end
    #0
    $dumpvars
    b00000000 !
    b0000000000000000 #
    $end
    """
)

# Malformed: $var line missing $end.
VCD_MALFORMED = "$date foo\n$version bar $end\n$scope module top $end\n$var wire 8 ! q [7:0]\n"

# Signal-map TOML for the rename case.
SIGNAL_MAP_RENAME = textwrap.dedent(
    """\
    [[alias]]
    golden  = "top.r"
    emitted = "top.r_alt"
    """
)


# -----------------------------------------------------------------------------
# Helpers
# -----------------------------------------------------------------------------


def write_temp(content: str, suffix: str) -> Path:
    """Write `content` to a unique temp file; return its path."""
    fd, path = tempfile.mkstemp(suffix=suffix, prefix="vcd_diff_test_")
    p = Path(path)
    with open(fd, "w", encoding="utf-8") as f:
        f.write(content)
    return p


# -----------------------------------------------------------------------------
# Tests (8 cases per vcd-diff.contract.md §7)
# -----------------------------------------------------------------------------


class VCDDiffTests(unittest.TestCase):
    """Behavioural tests against the M7 vcd_diff.py comparator."""

    def test_identical_vcds(self):
        """Two byte-identical VCDs → exit 0."""
        a = write_temp(VCD_IDENTICAL_A, ".vcd")
        b = write_temp(VCD_IDENTICAL_A, ".vcd")
        r = run([str(a), str(b)])
        self.assertEqual(r.returncode, 0, msg=f"stderr: {r.stderr}")

    def test_header_only_differ(self):
        """Differing $date/$version/$timescale → still exit 0 (ignored)."""
        a = write_temp(VCD_IDENTICAL_A, ".vcd")
        b = write_temp(VCD_HEADER_DRIFT, ".vcd")
        r = run([str(a), str(b)])
        self.assertEqual(r.returncode, 0, msg=f"stderr: {r.stderr}")

    def test_one_value_differs(self):
        """One value-change record differs → exit 1, report names signal+time."""
        a = write_temp(VCD_IDENTICAL_A, ".vcd")
        b = write_temp(VCD_VALUE_DIFFERS, ".vcd")
        r = run([str(a), str(b)])
        self.assertEqual(r.returncode, 1, msg=f"stderr: {r.stderr}")
        # Report mentions the divergent timestamp + signal name.
        self.assertIn("20", r.stderr)  # timestamp
        self.assertIn("r", r.stderr)  # signal

    def test_missing_signal_on_emitted(self):
        """Golden has `r`, emitted does not → exit 0 (intersection-only)."""
        a = write_temp(VCD_IDENTICAL_A, ".vcd")
        b = write_temp(VCD_MISSING_SIGNAL, ".vcd")
        r = run([str(a), str(b)])
        self.assertEqual(r.returncode, 0, msg=f"stderr: {r.stderr}")

    def test_signal_map_alias(self):
        """Signal renamed (r↔r_alt) with SIGNAL_MAP → exit 0."""
        a = write_temp(VCD_IDENTICAL_A, ".vcd")
        b = write_temp(VCD_SIGNAL_RENAMED, ".vcd")
        m = write_temp(SIGNAL_MAP_RENAME, ".toml")
        r = run([f"--signal-map={m}", str(a), str(b)])
        self.assertEqual(r.returncode, 0, msg=f"stderr: {r.stderr}")

    def test_width_mismatch_on_matched_pair(self):
        """Matched signal differs in width → exit 1."""
        a = write_temp(VCD_IDENTICAL_A, ".vcd")
        b = write_temp(VCD_WIDTH_MISMATCH, ".vcd")
        r = run([str(a), str(b)])
        self.assertEqual(r.returncode, 1, msg=f"stderr: {r.stderr}")

    def test_malformed_vcd(self):
        """Corrupt input → exit 2."""
        a = write_temp(VCD_IDENTICAL_A, ".vcd")
        b = write_temp(VCD_MALFORMED, ".vcd")
        r = run([str(a), str(b)])
        self.assertEqual(r.returncode, 2, msg=f"stderr: {r.stderr}")

    def test_bad_cli(self):
        """Missing args / help → exit 3."""
        r = run([])
        self.assertEqual(r.returncode, 3, msg=f"stderr: {r.stderr}")


if __name__ == "__main__":
    unittest.main()
