<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: `tools/vcd_diff.py` — semantic-equal VCD comparator

**Feature**: 011-m7-driver-e2e
**Owners**: `tools/vcd_diff.py`, `tools/test_vcd_diff.py`
**Status**: Frozen at M7
**Related FRs**: FR-021, FR-022
**Related contracts**: `audited-corpus.contract.md` (the consumer)

---

## §1 CLI surface

```text
Usage: vcd_diff.py [OPTIONS] <golden.vcd> <emitted.vcd>

Compare two VCD files for semantic equivalence per the M7 policy
(Clarifications Q2 → B).

Positional arguments:
  golden.vcd       Path to the reference golden VCD.
  emitted.vcd      Path to the VCD captured from nslc-emitted output.

Options:
  --signal-map=<path>    Optional TOML file aliasing upstream-vs-nslc
                         signal names (see §3).
  --report=<path>        Write divergence report to <path> instead of
                         stderr. Used by FR-022 per-scenario logging.
  -v, --verbose          Emit INFO-level logs about unmatched signals
                         (signals present in one file but not the
                         other). Default: silent.
  -h, --help             Show usage and exit 3.
```

---

## §2 Exit codes

| Exit code | Meaning |
|---|---|
| 0 | Semantic-equal: every matched signal's value-change-record sequence is identical between golden and emitted, at every simulation timestamp |
| 1 | Divergence detected: report written to stderr (or `--report=<path>` if specified) |
| 2 | Parse error in one or both input files; report on stderr |
| 3 | Bad CLI (missing arguments, unreadable files, help requested) |

---

## §3 `SIGNAL_MAP.toml` schema

When `--signal-map=<path>` is provided, the file is loaded via
Python stdlib `tomllib` (3.11+ requirement). Schema:

```toml
# Optional top-level metadata (ignored by the comparator).
project = "..."
maintained_by = "..."

# One or more alias entries. Each entry is a separate [[alias]]
# table inside an array-of-tables.
[[alias]]
golden  = "top.cpu.regfile.r5"        # signal as named in golden VCD
emitted = "top.cpu.r_regfile_5"       # signal as named in emitted VCD

[[alias]]
golden  = "top.dut.alu.result_o"
emitted = "top.dut.alu.alu_result"
```

Semantics:
- Each `[[alias]]` entry creates a bidirectional name mapping.
- A signal in `golden` matches its mapped name in `emitted` (and
  vice-versa).
- Unmapped signal names match verbatim (the default — most signals
  do not need aliasing).
- Aliases apply ONLY to scope-path-tuple matching, NOT to widths
  or values.

Width mismatch on a matched-signal pair (e.g., golden has
`r5[31:0]` but emitted has `r_regfile_5[63:0]`) is treated as a
divergence (exit 1) with a clear report.

---

## §4 Parse policy

The VCD parser MUST:

- **Ignore** `$date`, `$version`, `$timescale`, `$comment`
  declarations entirely. These are simulator-toolchain metadata
  and do not participate in equivalence.
- **Track** scope-path via `$scope <module|task|function|begin> <name>
  $end` / `$upscope $end`.
- **Record** every `$var <type> <width> <id-char(s)> <name> $end`
  as a `(scope-path-tuple, name, width)` entry keyed by
  `id-char(s)`. The `id-char(s)` is the VCD identifier code (1–7
  printable ASCII chars excluding space, per IEEE 1800-2009
  §21.7.1).
- **Recognize** value-change records:
  - `0<id>`, `1<id>`, `X<id>`, `Z<id>`, `x<id>`, `z<id>` (single-bit).
  - `b<bits> <id>` (vectored).
  - `r<value> <id>` (real — extremely rare in audited corpus;
    treated as opaque string comparison).
- **Track** simulation timestamp via `#<integer>` records.
- **Stop** on `$enddefinitions $end` for the var-block; continue
  parsing value-change records until EOF or `$dumpoff` / `$dumpon`
  / `$dumpvars` boundaries (these boundaries are honored — values
  inside `$dumpoff` reset; values inside `$dumpvars` set initial
  values).

Parse error mode: malformed records produce exit 2 with a report
identifying the offending line number + the parser state at that
line.

---

## §5 Comparison policy

After parsing both files:

1. Build the matched-signal set:
   - For each signal `s` in golden's `(scope-path, name, width)`
     space, find the corresponding `(scope-path, name, width)` in
     emitted's space (after applying any SIGNAL_MAP aliases).
   - If found → matched.
   - If not found → unmatched on golden side; emit INFO log if
     `--verbose`; otherwise silent.
   - Symmetrically: for each emitted signal not in golden, log
     INFO under `--verbose`.

2. For each matched signal pair:
   - Walk value-change records in timestamp order.
   - At each timestamp, the matched signal's value MUST be equal
     between golden and emitted.
   - First divergence → exit 1 with the report (§6).
   - If both signals end with identical full value-change-record
     sequences → pass.

3. After all matched signals pass:
   - If `--verbose`, emit a final summary: matched count,
     unmatched-on-golden count, unmatched-on-emitted count.
   - Exit 0.

---

## §6 Divergence-report format

When exit 1, write to stderr (or `--report=<path>`):

```text
VCD divergence:
  Golden:  <golden.vcd>
  Emitted: <emitted.vcd>

First divergence at simulation time #<timestamp>:
  Signal:  <scope.path.signal>[<width>]
  Golden value:  <value-string>
  Emitted value: <value-string>

Context (last 3 value changes on this signal):
  #<t-3>  golden=<v-3>  emitted=<v-3>
  #<t-2>  golden=<v-2>  emitted=<v-2>
  #<t-1>  golden=<v-1>  emitted=<v-1>
  #<t>    golden=<value-string>  emitted=<value-string>  ← divergence

Total signals matched: <N>
Signals unmatched on golden side: <M>
Signals unmatched on emitted side: <K>
```

The report is human-readable; format is stable for downstream
log scraping (`scripts/ci.sh` may grep `First divergence at` to
extract the first-divergence timestamp).

---

## §7 Test suite

Required Python `unittest` cases in `tools/test_vcd_diff.py`:

| Test name | Scenario | Expected outcome |
|---|---|---|
| `test_identical_vcds` | Two byte-identical VCDs | exit 0 |
| `test_header_only_differ` | Differ in `$date` lines only | exit 0 |
| `test_one_value_differs` | One value-change record differs | exit 1; report names signal + timestamp |
| `test_missing_signal_on_emitted` | Golden has signal `x`; emitted does not | exit 0 (intersection rule); INFO when verbose |
| `test_signal_map_alias` | Names differ, aliased | exit 0 |
| `test_width_mismatch_on_matched_pair` | Matched signal differs in width | exit 1 |
| `test_malformed_vcd` | Corrupt input | exit 2 |
| `test_bad_cli` | Missing args | exit 3 |

These tests land BEFORE the implementation (TDD per Constitution
Principle VIII). Invocation: `python3 -m unittest tools/test_vcd_diff.py`.

---

## §8 No third-party dependencies

`vcd_diff.py` SHALL import from Python 3.11+ stdlib ONLY. The
permitted modules are: `argparse`, `enum`, `io`, `logging`,
`pathlib`, `re`, `sys`, `tomllib`, `typing`, `unittest` (in the
test peer only).

Any addition of a PyPI / third-party module requires a /speckit-clarify
session + contract amendment.

---

## §9 Forward compatibility

Anticipated future enhancements:

- `--strict` flag for byte-equal comparison (bypass §4's
  ignored-declarations rule). Not in M7 scope.
- `--tolerance=<n>` flag for cycle-window tolerance (allow up to
  `n` cycles of timing drift). Not in M7 scope; would erode
  Principle VI's reference-VCD guarantee.

The CLI in §1 is the stable contract surface; flag additions
require contract amendment.
