# PROVENANCE — mips32_single_cycle

Upstream-URL: https://github.com/koyamanX/mips32_single_cycle
Upstream-SHA: 135cc05c1306978113663c009b9b2bae90e6a1a7
License: Apache-2.0 WITH LLVM-exception (original-author grant)
Vendored-At: 2026-05-12
Vendored-By: ckoyama

## Notes

Original-author grant: the upstream repository at
`https://github.com/koyamanX/mips32_single_cycle` has no LICENSE
file at the vendored SHA. The original author (Chihiro Koyama,
ckoyama.1996@gmail.com) grants Apache-2.0 WITH LLVM-exception
licensing for the vendored copy under this directory, for
inclusion in the nslc M7 audited corpus.

Upstream layout (preserved verbatim):
  - src/  — NSL sources: alu32, alu_ctl, cla32, ctl_unit, inc32,
            mips32, reg32 + headers (mips32_single_cycle.h,
            opcode.h)
  - sim/  — upstream simulation harness (preserved for
            upstream-flow reproducibility)

No vendor-time modifications.
