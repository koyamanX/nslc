<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: P-VEN + P-VCD + audited-corpus regression

**Feature**: 011-m7-driver-e2e
**Owners**: `test/audited/`, `cmake/AuditedCorpusLint.cmake`,
`cmake/audited_corpus.cmake`
**Status**: Frozen at M7
**Related FRs**: FR-009 through FR-023, FR-025, FR-026
**Related contracts**: `driver-emit-verilog.contract.md` (consumed
by per-cell test commands); `vcd-diff.contract.md` (comparison
policy); `container-m7.contract.md` (simulator availability)

---

## §1 Vendored project layout (P-VEN)

Each of the seven audited projects MUST live under
`test/audited/<project>/` with this minimum shape:

```text
test/audited/<project>/
├── PROVENANCE.md                  # required (see §2 below)
├── <project>.nsl                  # primary source — upstream verbatim
├── *.nsl                          # secondary sources — upstream verbatim
├── tb/                            # testbench(es) — upstream-verbatim
│   └── <project>_tb.v             #   OR manually-authored per research.md §9
├── golden/                        # required golden directory
│   ├── REGEN.md                   #   required (see §3)
│   ├── <scenario>.vcd             #   one or more
│   └── SIGNAL_MAP.toml            #   optional (see vcd-diff.contract.md §3)
└── README.md                      # optional — narrative description
```

The project list (cardinality = exactly 4 at M7 acceptance — see
amendment note below):

1. `cpu16`
2. `mips32_single_cycle`
3. `ahb_lite_nsl`
4. `turboV`

**Amendment (2026-05-12)**: the original contract listed 7
projects. The license audit at M7 implementation T046 surfaced
3 incompatible projects (`rv32x_dev`: GPL-3.0; `mmcspi` +
`SDRAM_Controler`: forks with no upstream LICENSE file + no
original-author-grant path). They are dropped from the M7
acceptance gate and may be re-added via routine vendoring PRs
once their upstream licensing is resolved, per §8 below +
[`CONTRIBUTING.md`](../../../CONTRIBUTING.md) §2.1. The single
edit point that adds a new project is
[`cmake/AuditedCorpusLint.cmake`](../../../cmake/AuditedCorpusLint.cmake)'s
`NSL_AUDITED_PROJECTS` list — no other infra changes required.

Sources MUST be vendored as verbatim file copies from the
upstream commit (no patches except those documented in
PROVENANCE.md `Notes:` block). No `.gitmodules` entry. No
`FetchContent_Declare` or `ExternalProject_Add` invocation that
references `test/audited/`.

---

## §2 `PROVENANCE.md` schema

Each project's `PROVENANCE.md` MUST contain (at minimum) these
machine-parseable header lines:

| Key | Format | Required |
|---|---|---|
| `Upstream-URL` | URL | yes |
| `Upstream-SHA` | 40-character hex | yes |
| `Upstream-Tag` | tag string (e.g. `v1.2.0`) | no |
| `License` | SPDX identifier or full name | yes |
| `License-File` | relative path | no |
| `Vendored-At` | ISO-8601 date | yes |
| `Vendored-By` | committer | no |

Plus an optional `## Notes` H2 section for free-form annotations
(license-header insertion, file renames, etc.).

`License` MUST be in the Apache-2.0-WITH-LLVM-exception compatible
set. Per research.md §8, the seven audited projects' licenses
(BSD-2-Clause / MIT / Apache-2.0) are all compatible.

---

## §3 `REGEN.md` schema

Each project's `golden/REGEN.md` MUST contain these H2 sections:

| Section | Content |
|---|---|
| `## Regeneration command` | The exact shell command(s) to regenerate the golden VCDs from external sources |
| `## External source` | Identification of the source (upstream NSL toolchain version, hand-traced reference, formal-validation framework, etc.) |
| `## Simulator + version` | The simulator used to capture the golden (or "N/A" for hand-traced) |
| `## Environment / dependencies` | Runtime deps for regeneration |

Optional sections: `## Notes`, `## Per-scenario` (when project has
multiple scenarios with diverging recipes).

**Hard constraint (FR-016)**: NO `REGEN.md` may contain the
token `nslc` outside of explanatory prose that explicitly says
"NSL toolchain != nslc" / "not regenerated from nslc". CI lint
(`cmake/AuditedCorpusLint.cmake`) does a strict grep that allows
the token only inside code blocks marked with a `# NOT nslc` line
comment OR inside a `## Notes` H2 block — implementation MAY use
either of these escape hatches, but the lint default is "no
mention".

---

## §4 Configure-time lint (`cmake/AuditedCorpusLint.cmake`)

The CMake module runs at configure time (`cmake -B build ...`)
and asserts:

| Check | Failure message |
|---|---|
| Each of the seven directories exists | `Missing vendored project: <name>` |
| Each has `PROVENANCE.md` | `<project>/PROVENANCE.md missing` |
| `PROVENANCE.md` contains required keys | `<project>/PROVENANCE.md missing required key: <key>` |
| `Upstream-SHA` matches `^[0-9a-f]{40}$` | `<project>/PROVENANCE.md Upstream-SHA malformed` |
| `License` is in the compatible set | `<project>/PROVENANCE.md License '<value>' not in compatible set` |
| No `.gitmodules` references `test/audited/` | `Forbidden submodule under test/audited/` |
| No `CMakeLists.txt` under `test/audited/` invokes `FetchContent_Declare` or `ExternalProject_Add` | `Forbidden network-fetch under test/audited/<project>/CMakeLists.txt` |
| Each project has `golden/` directory | `<project>/golden/ missing` |
| Each `golden/` has `REGEN.md` | `<project>/golden/REGEN.md missing` |
| No `REGEN.md` contains a bare `nslc` invocation outside escape hatches | `<project>/golden/REGEN.md contains forbidden nslc invocation` |

Failure mode: `message(FATAL_ERROR "...")` — configure aborts.

---

## §5 `check-audited` CMake target

`cmake/audited_corpus.cmake` defines `check-audited` with:

- Dependencies: `nslc` build product + the seven vendored
  projects + `tools/vcd_diff.py`.
- Behavior: enumerates `test/audited/<project>/` directories,
  enumerates each project's `golden/*.vcd` scenarios, generates
  one lit-test instance per (project × simulator × scenario)
  tuple where `simulator ∈ {iverilog, verilator}`.
- Invocation: `cmake --build build --target check-audited`.
- Inclusion in top-level `check`: yes (`add_dependencies(check
  check-audited)`).

The per-cell test (one lit fixture instance) executes:

1. **Build phase**: `nslc -emit=verilog <project>/<sources> -o build/test/audited/<project>/verilog/`.
2. **Compile phase**: per-simulator:
   - `iverilog -g2012 -o build/test/audited/<project>/<sim>/sim.vvp build/test/audited/<project>/verilog/*.v <project>/tb/*.v`
   - `verilator --binary -Wno-fatal --top-module <top> -o sim ...`
3. **Run phase**: execute the simulator binary, dump VCD to
   `build/test/audited/<project>/<sim>/<scenario>.vcd`.
4. **Compare phase**: `python3 tools/vcd_diff.py [--signal-map=
   <project>/golden/SIGNAL_MAP.toml] <project>/golden/<scenario>.vcd
   build/test/audited/<project>/<sim>/<scenario>.vcd`.

A cell fails if ANY phase exits non-zero. Per-cell log
(`build/test/audited/<project>/<sim>/<scenario>.log`) captures
each phase's stdout+stderr for FR-022 inspectability.

---

## §6 Two-simulator parity rule

Per `research.md` §6: a cell PASSes only if BOTH simulators
(iverilog + verilator) PASS for that (project × scenario)
combination. Per-simulator XFAIL is NOT allowed. If a real-world
divergence cannot be resolved during M7's window, the affected
scenario is *removed* from `golden/` (and the removal is
documented in `golden/REGEN.md`).

**Cell count (post-amendment, 2026-05-12)**: 4 projects × 2
simulators = **8 cells** total at M7 acceptance. The per-cell
multiplier still scales with per-project scenario count: e.g.,
`ahb_lite_nsl` may ship `ahb_read.vcd` + `ahb_write.vcd` →
2 scenarios × 2 simulators = 4 cells for that project alone.
SC-001 names the 8-cell minimum; the actual lit count at green
will be higher if multiple scenarios per project land.

---

## §7 Wall-clock budget

The full `check-audited` target completes in ≤ 15 minutes on a
standard CI runner inside `:dev-m7`. Per-project parallelism via
lit handles per-cell distribution.

A cell that exceeds 5 minutes individually MUST document its
runtime in `golden/REGEN.md` so future maintainers know what to
expect.

Wall-clock measurement: `scripts/ci.sh` records the wall-clock of
the audited-corpus cell separately from other cells; the
measurement is compared against the 15-minute budget in CI's
final summary.

---

## §8 New-project addition path (post-M7)

Adding an 8th audited project after M7 lands is a routine PR
that requires (per SC-006):

1. Vendor `test/audited/<new-project>/` (sources + tb).
2. Author `test/audited/<new-project>/PROVENANCE.md`.
3. Author `test/audited/<new-project>/golden/REGEN.md`.
4. Generate `test/audited/<new-project>/golden/<scenario>.vcd`
   from external source.
5. (Optional) Author `test/audited/<new-project>/golden/SIGNAL_MAP.toml`.

ZERO edits to:
- `lib/Driver/`
- `tools/nslc/`
- `cmake/audited_corpus.cmake` (the per-project enumeration
  auto-discovers via directory glob).
- `cmake/AuditedCorpusLint.cmake` (the lint applies generically).
- `test/lit.cfg.py` (the substitution variables apply
  generically).

The PR's CI run automatically picks up the new project; the
lit fixture instances for (new-project × iverilog × scenario)
and (new-project × verilator × scenario) come into existence
without infra changes.

---

## §9 Forward compatibility

This contract is frozen at M7. Anticipated forward changes:

- M8 (riscv-formal) adds a third per-cell evaluation track for
  `rv32x_dev` (and possibly `turboV`): formal-equivalence check.
  This is an EXTENSION, not a modification — the formal track
  SUPPLEMENTS the golden-VCD track per Constitution Principle VI
  "formal SUPPLEMENTS the golden VCD" clause.
- M9 (release) may require a `--reproducible` audit step that
  verifies each `PROVENANCE.md`'s SHA still resolves on upstream.
  This is anticipated by §2's SHA-pin requirement.
- Future projects beyond the seven add via §8 — never edit §1–§4
  for new projects.

Removing a project from the seven (e.g., upstream goes 404)
requires a /speckit-clarify session and a fresh feature spec.
NOT a routine PR.
