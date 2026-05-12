# PROVENANCE — cpu16

Upstream-URL: https://github.com/koyamanX/cpu16
Upstream-SHA: d02d567a6dd1c949d1f83d804b036e95d85dd9d8
License: Apache-2.0 WITH LLVM-exception (original-author grant)
Vendored-At: 2026-05-12
Vendored-By: ckoyama

## Notes

Original-author grant: the upstream repository at
`https://github.com/koyamanX/cpu16` has no LICENSE file at the
vendored SHA. The original author (Chihiro Koyama,
ckoyama.1996@gmail.com) — who is also the maintainer of nslc —
grants Apache-2.0 WITH LLVM-exception licensing for the vendored
copy under this directory, for inclusion in the nslc M7 audited
corpus. Upstream license posture remains the author's discretion;
this grant applies to the vendored copy only.

Files vendored (verbatim from upstream master at the recorded
SHA):
  - cpu.h         — register/opcode constants
  - cpu.nsl       — CPU16 core implementation
  - cpu_sim.nsl   — simulation top-level (test harness)
  - opcode.h      — opcode mnemonic table
  - Makefile      — upstream build orchestration (not consumed
                    by M7's audited-corpus regression; preserved
                    for upstream-flow reproducibility)
  - README.md     — upstream description

No vendor-time modifications. License-header SPDX insertion deferred
to a follow-on documentation pass.
