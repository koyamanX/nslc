# Golden VCD regeneration — mips32_single_cycle

## Regeneration command

**Status (Phase 4 — 2026-05-12)**: scaffold. Actual golden VCDs
land at T063 (P-VCD Phase 5). The expected recipe shape:

```sh
# Use the upstream sim/ harness to compile + simulate +
# capture VCD:
make -C test/audited/mips32_single_cycle/sim
test/audited/mips32_single_cycle/sim/sim_binary \
  +VCDFILE=test/audited/mips32_single_cycle/golden/mips_hello.vcd
```

## External source

Upstream NSL toolchain simulator. No self-referential regeneration
per FR-016.

## Simulator + version

(To be populated at T063.)

## Environment / dependencies

(To be populated at T063.)

## Notes

Phase-4 scaffold; T063 fills in the recipe + the `.vcd` file(s).
