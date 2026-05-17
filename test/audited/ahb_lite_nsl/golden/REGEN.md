# Golden VCD regeneration — ahb_lite_nsl

## Regeneration command

**Status (Phase 4 — 2026-05-12)**: scaffold. Actual golden VCDs
land at T064 (P-VCD Phase 5). Expected recipe shape (per-scenario:
`ahb_read.vcd` + `ahb_write.vcd`):

```sh
make -C test/audited/ahb_lite_nsl/sim
for scenario in ahb_read ahb_write; do
  test/audited/ahb_lite_nsl/sim/sim_binary +SCENARIO=$scenario \
    +VCDFILE=test/audited/ahb_lite_nsl/golden/$scenario.vcd
done
```

## External source

Upstream NSL toolchain simulator. No self-referential regeneration
per FR-016.

## Simulator + version

(To be populated at T064.)

## Environment / dependencies

(To be populated at T064.)

## Notes

Phase-4 scaffold; T064 fills in the recipe + per-scenario `.vcd`
files.
