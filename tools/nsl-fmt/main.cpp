//===- main.cpp - nsl-fmt CLI driver (Phase 1 stub) -------------*- C++ -*-=//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// `nsl-fmt` — the NSL formatter command-line driver. T2 milestone,
// branch `010-t2-formatter-v0`.
//
// At Phase 1 (T002), this is a five-line stub that exists only to
// prove the build pipeline wires `tools/nsl-fmt/` into the executable
// graph. The real CLI (cl::opt flag matrix per
// `specs/010-t2-formatter-v0/contracts/cli-surface.contract.md`)
// lands in Phases 3–6:
//   - T056 (US1) — default mode (positional file → stdout)
//   - T057 (US1) — `-i` / `--in-place`
//   - T058 (US1) — `--stdin`
//   - T077 (US2) — `-c` / `--check`
//   - T078 (US2) — multi-file continue-on-error
//   - T079 (US2) — mutually-exclusive flag rejection
//   - T090 (US3) — `--range LINE:LINE`
//   - T107 (US4) — `--config PATH`
//
// Phase 1's stub deliberately produces NO output and returns 0.
// Verification: `nsl-fmt` builds and exits cleanly.
//
//===----------------------------------------------------------------------===//

#include "nsl/Fmt/Fmt.h"

int main(int /*argc*/, char ** /*argv*/) {
  return 0;
}
