---
name: "nsl-formal"
description: "Wire rv32x_dev to riscv-formal for ISA-compliance verification; integrate formal results into CI as a supplement (not replacement) for the golden VCD — gates M8."
argument-hint: "Formal task (e.g., 'wire rv32x_dev unit-pc check')"
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

Owns the M8 deliverable: integrating [riscv-formal](https://github.com/YosysHQ/riscv-formal) into the `rv32x_dev` audited project for ISA-compliance verification. Constitution **Principle VI's formal clause** is explicit: *formal SUPPLEMENTS the golden VCD; it does not replace it.* The golden VCD remains the canonical reference for `rv32x_dev`; formal adds a stronger correctness signal alongside it.

This skill also covers the formal CI stage in Principle IX (stage 6), which is gated identically to the formal clause of Principle VI.

## Outline

1. **Confirm the prerequisites.**
   - The `rv32x_dev` project is already vendored under `test/audited/rv32x_dev/` with `PROVENANCE.md` (P-VEN, M7 dep).
   - The golden VCD for `rv32x_dev` already exists at `test/audited/rv32x_dev/golden/<scenario>.vcd` (P-VCD, M7 dep).
   - The compiler can already lower `rv32x_dev` end-to-end to Verilog (M7 complete).
   - If any of these are missing, defer M8 work and route to `/nsl-driver-e2e`.

2. **Configure riscv-formal for `rv32x_dev`.**
   - Place formal harness under `test/audited/rv32x_dev/formal/`
   - Author SymbiYosys (`.sby`) check files for the riscv-formal property suite (PC behavior, register file, memory ops, CSR, …)
   - Wire the harness to consume the Verilog produced by `nslc -emit=verilog rv32x_dev/...`

3. **Document regeneration.**
   - Add `test/audited/rv32x_dev/formal/REGEN.md` documenting:
     - The exact riscv-formal commit SHA used (deterministic build environment, Principle V)
     - The SymbiYosys + Yosys versions
     - The command to run all formal checks locally
   - License-compatibility check: riscv-formal license MUST be compatible with Apache-2.0 WITH LLVM-exception (Apache or BSD-style is fine; GPL would be a blocker)

4. **Wire the CI stage (Principle IX stage 6).**
   - Coordinate with `/nsl-build-ci` to add the formal stage to the GitHub Actions matrix
   - Stage runs only when the lowering supports it (gated identically to Principle VI's formal clause)
   - Result MUST be published in CI (Principle IX merge gate signal)

5. **Verify the SUPPLEMENT (not replace) discipline.**
   - Golden VCD test for `rv32x_dev` MUST still run and pass — formal does not replace it
   - If formal slips, the project may ship a `0.x` series with the formal gate explicitly disabled in release notes; the canonical `1.0.0` (M9) requires M8 done

6. **Verify.** Confirm:
   - [ ] `test/audited/rv32x_dev/formal/` exists with `.sby` files for the riscv-formal property suite
   - [ ] `REGEN.md` records riscv-formal SHA + tool versions + command
   - [ ] License audit complete (riscv-formal terms compatible)
   - [ ] CI publishes formal-pass result on PR + push to `main` (Principle IX)
   - [ ] Golden VCD test for `rv32x_dev` still runs and still passes (formal SUPPLEMENTS, does not replace)
   - [ ] `README.md` §Roadmap M8 row test gate ("`rv32x_dev` passes the riscv-formal ISA-compliance suite; result published in CI") satisfied

## Constitutional anchors

- **Principle V** — Inspectable, Deterministic Pipeline (formal harness MUST be deterministic; pin all tool versions)
- **Principle VI** — Layered Test Discipline; **formal clause** ("Formal SUPPLEMENTS the golden VCD; it does not replace it")
- **Principle IX** — Continuous Integration & Delivery, **stage 6 Formal**
