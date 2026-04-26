---
name: "nsl-driver-e2e"
description: "Wire nsl-driver, run the audited-project regression, manage P-VEN vendoring + P-VCD golden VCDs, and verify byte-stable end-to-end NSL → Verilog — gates M7."
argument-hint: "Driver flag, vendoring task, or audited-project regression issue"
metadata:
  author: "nslc-project"
user-invocable: true
disable-model-invocation: false
---

## User Input

```text
$ARGUMENTS
```

You **MUST** consider the user input before proceeding (if not empty).

## Role

Owns the M7 demonstration moment: the first end-to-end NSL → Verilog pipeline running against the seven audited open-source NSL projects. This skill also covers the two compiler-related project-enablement deliverables that gate M7: **P-VEN** (vendoring) and **P-VCD** (golden VCDs).

| Library | Milestone | Scope |
|---|---|---|
| `nsl-driver` (9) | M7 | `Compilation` object, `-emit=*` flags, end-to-end `nslc -emit=verilog` |
| `P-VEN` | M7 dep | Vendor seven audited projects under `test/audited/<project>/` |
| `P-VCD` | M7 dep | Golden VCDs at `test/audited/<project>/golden/` from external sources |

## Outline

1. **Identify the work item.** M7 has three orthogonal sub-tasks; pick one:
   - **Driver wiring** — wire a new `-emit=*` flag, plumb a `CompileOptions` field, integrate a new pass into the `Compilation` run loop
   - **P-VEN (vendoring)** — vendor or update an audited project
   - **P-VCD (golden VCDs)** — generate or regenerate a golden VCD from an external source
   - **Regression run** — execute `nslc -emit=verilog` over the audited corpus, simulate, diff against golden VCDs

2. **Driver wiring (Compilation object).**
   - Read `docs/design/nsl_compiler_design.md` §11 (lines **1102–1157**) — `CompileOptions`, run loop
   - Add the new flag in the driver entry (`tools/nslc/main.cpp` stays ~60 lines; logic goes in `lib/Driver/`)
   - Every new stage MUST add its own `-emit=*` per Principle V; output MUST be byte-stable across two builds
   - Hand off to `/nsl-test-author` for FileCheck cases on the new flag's output

3. **P-VEN (vendoring).**
   - Vendor under `test/audited/<project>/` — copy the upstream NSL source files in directly. **No git submodules. No configure-time fetches.** Principle V demands a deterministic build environment; submodules and fetches break that.
   - Create `test/audited/<project>/PROVENANCE.md` recording:
     - Upstream URL
     - Commit SHA at time of vendoring
     - License (must be compatible with Apache-2.0 WITH LLVM-exception)
   - Updating an audited project is a fresh re-vendoring commit, not a submodule bump

4. **P-VCD (golden VCDs).**
   - Goldens live at `test/audited/<project>/golden/<scenario>.vcd`
   - **Source rule (Principle VI "Reference VCDs"):** Goldens MUST come from an external known-good source — the upstream NSL toolchain output for non-CPU projects, or a manually-authored / formal-validated reference for CPU projects. **Self-referential VCDs (regenerated from `nslc`'s own emitted Verilog at test time) are NOT acceptable** — they only catch `nslc`-vs-previous-`nslc` regressions, not NSL-correctness.
   - Each project carries `test/audited/<project>/golden/REGEN.md` documenting the regeneration command. If the testbench is amended, the maintainer can reconstitute the golden using `REGEN.md`.
   - Once `/nsl-formal` lands the riscv-formal clause for `rv32x_dev`, formal verification SUPPLEMENTS the golden VCD for that project — it does not replace it.

5. **Regression run.** All seven audited projects (`cpu16`, `mips32_single_cycle`, `ahb_lite_nsl`, `mmcspi`, `SDRAM_Controler`, `rv32x_dev`, `turboV`) MUST compile and simulate equivalently to their `golden/*.vcd` under Icarus Verilog and Verilator. **A change that breaks any of these does not land** (Principle VI, NON-NEGOTIABLE).

6. **Determinism gate (Principle V).** For every `-emit=*` flag, two builds with identical inputs and flags MUST produce byte-identical output. Verify with a determinism FileCheck case.

7. **Verify.** Confirm:
   - [ ] If new `-emit=*` flag: per-flag FileCheck cases land in the same PR
   - [ ] All seven audited projects still pass the regression
   - [ ] No git submodule introduced under `test/audited/`
   - [ ] `PROVENANCE.md` present and complete for any newly vendored project
   - [ ] `REGEN.md` present for any new golden VCD; golden came from an external source (not from `nslc`)
   - [ ] Byte-stability verified across two builds (Principle V)
   - [ ] Principle IX no-bypass rule observed (`--no-verify` etc. not used)

## Constitutional anchors

- **Principle V** — Inspectable, Deterministic Pipeline (every stage has `-emit=*`; byte-stable)
- **Principle VI** — Layered Test Discipline; "Delivery" sub-bullet (vendoring); "Reference VCDs" sub-bullet (golden VCDs); end-to-end clause (NON-NEGOTIABLE)
- **Principle IX** — Continuous Integration & Delivery (no bypass; no human-built artifacts at release)
