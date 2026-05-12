# Golden VCD regeneration — turboV

## Regeneration command

**Status (Phase 4 — 2026-05-12)**: scaffold. Actual golden VCDs
land at T068 (P-VCD Phase 5). turboV is a CPU project; per
Constitution Principle VI "Reference VCDs" + FR-017, CPU
projects' goldens come from a manually-authored reference trace
or a formal-validation framework export. turboV's upstream ships
a Python reference simulator under `simulator/` — that simulator
is the canonical golden source.

Expected recipe shape (per-instruction-family scenarios):

```sh
# Build the RISC-V test binaries from the vendored tests/ directory:
make -C test/audited/turboV/tests

# For each instruction-family scenario:
for scenario in add load store branch jump csr alu-imm alu-reg; do
  python3 test/audited/turboV/simulator/ref_sim.py \
    --image test/audited/turboV/tests/$scenario.bin \
    --vcd test/audited/turboV/golden/$scenario.vcd \
    --cycles 1000
done
```

## External source

The vendored Python reference simulator at
`test/audited/turboV/simulator/` is the external known-good source.
This simulator is upstream-authored (Chihiro Koyama) and
cross-validated against the RISC-V unprivileged-ISA spec v2.2.
NOT regenerated from `nslc`-emitted output (FR-016).

## Simulator + version

Vendored Python simulator pinned to turboV upstream-SHA
62f4849dd0f0ba39cd82f35dc15ab920d20cec1b (recorded in
`../PROVENANCE.md`). Python 3.11+ (stdlib only).

## Environment / dependencies

- riscv32-unknown-elf-gcc (for compiling the test binaries) —
  provided by the M7 dev container `ghcr.io/koyamanX/nsl-nslc:dev-m7`
  per container-m7.contract.md §2.
- Python 3.11+ stdlib (no PyPI deps).

## Notes

Phase-4 scaffold; T068 fills in the per-scenario `.vcd` files.
Per FR-017 the CPU-project golden source MUST be either
manually-authored hand-traced reference OR formal-validation
framework export; turboV uses the former (vendored Python
reference simulator).
