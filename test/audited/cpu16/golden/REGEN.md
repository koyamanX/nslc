# Golden VCD regeneration — cpu16

## Regeneration command

**Status (Phase 4 — 2026-05-12)**: scaffold. Actual golden VCDs
land at T062 (P-VCD Phase 5). The expected recipe shape:

```sh
# 1. Use the upstream `Makefile` to compile the testbench
#    (cpu_sim.nsl + cpu.nsl + opcode.h):
make -C test/audited/cpu16 sim

# 2. Run the simulation under upstream NSL toolchain's bundled
#    simulator; dump VCD via $dumpfile/$dumpvars directives:
test/audited/cpu16/sim_binary +VCDFILE=test/audited/cpu16/golden/cpu16_basic.vcd
```

## External source

Upstream NSL toolchain simulator (NSL Studio bundled simulator;
version recorded at golden generation time). Self-referential
regeneration via `nslc` itself is forbidden per Constitution
Principle VI "Reference VCDs" and FR-016.

## Simulator + version

(To be populated at T062 with the exact upstream simulator
version used to capture the golden.)

## Environment / dependencies

(To be populated at T062. Expected: upstream NSL toolchain on
`PATH`, accessible to the maintainer running the regeneration.)

## Notes

- This file is a Phase-4 scaffold. The lint at `cmake/AuditedCorpusLint.cmake`
  requires `REGEN.md` present (FR-016 — no bare-`nslc` invocation
  in the recipe; the scaffold mentions `nslc` only in prose, never
  as a command).
- Phase 5 (T062) fills in: (a) the recipe with actual environment
  pins, (b) the captured `.vcd` file(s).
