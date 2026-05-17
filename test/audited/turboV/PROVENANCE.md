# PROVENANCE — turboV

Upstream-URL: https://github.com/koyamanX/turboV
Upstream-SHA: 62f4849dd0f0ba39cd82f35dc15ab920d20cec1b
License: Apache-2.0 WITH LLVM-exception (original-author grant)
Vendored-At: 2026-05-12
Vendored-By: ckoyama

## Notes

Original-author grant: the upstream repository at
`https://github.com/koyamanX/turboV` has no LICENSE file at the
vendored SHA. The original author (Chihiro Koyama,
ckoyama.1996@gmail.com) grants Apache-2.0 WITH LLVM-exception
licensing for the vendored copy under this directory, for
inclusion in the nslc M7 audited corpus.

Upstream layout (preserved verbatim — 144 files, ~1.5 MB):
  - src/         — turboV CPU core (NSL: core + wishbone +
                   integration; ~56 .nsl/.h files)
  - simulator/   — upstream Python instruction-set reference
                   simulator (cross-referenced from
                   golden/REGEN.md as the external golden-VCD
                   source per Q4-implicit resolution in
                   research.md §9)
  - tests/       — RISC-V instruction-stream test inputs
  - software/    — companion C software for FPGA bring-up
  - tools/       — upstream toolchain glue
  - image/       — boot images (binary blobs; preserved as
                   upstream ships)
  - CMakeLists.txt + run.sh + version.h.in — upstream build
                   orchestration

No vendor-time modifications. The audited-corpus regression's
golden VCDs for turboV come from the vendored Python reference
simulator at `simulator/` — see `golden/REGEN.md` for the
regeneration recipe. The upstream Python simulator is the
"external known-good source" per Constitution Principle VI
"Reference VCDs" sub-bullet.
