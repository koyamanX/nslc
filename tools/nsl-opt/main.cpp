// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// tools/nsl-opt/main.cpp — `nsl-opt` developer/test binary (M4).
//
// **Specification anchors**:
//   - `specs/007-m4-mlir-dialect/spec.md` FR-014 — link `nsl-dialect`
//     + the 5 CIRCT dialects (`hw`, `comb`, `seq`, `fsm`, `sv`)
//     loaded by `Compilation` per design §11 lines 1146–1150.
//   - `specs/007-m4-mlir-dialect/spec.md` FR-015 — stdin/stdout
//     support, upstream `MlirOptMain` flag layout.
//   - `specs/007-m4-mlir-dialect/spec.md` FR-016 — classified as
//     developer/test tool (Principle II §4); not bundled in release
//     tarballs.
//   - `specs/007-m4-mlir-dialect/contracts/nsl-opt-cli.contract.md`
//     §1–§5 — invocation forms, flag set, registered dialects, exit
//     codes, stdin/stdout shape.
//   - `specs/007-m4-mlir-dialect/research.md` §6 — vanilla
//     `MlirOptMain`-style binary (~50 lines).
//
// At M4 zero passes are registered (per FR-015); the
// `--<pass-name>` flag space is empty beyond MLIR's built-in
// canonicalize / cse / etc. passes. M5+ adds the structural-
// expansion passes via `mlir::registerPass<>()`.

#include "nsl/Dialect/NSL/IR/NSLDialect.h"

#include "circt/Dialect/Comb/CombDialect.h"
#include "circt/Dialect/FSM/FSMDialect.h"
#include "circt/Dialect/HW/HWDialect.h"
#include "circt/Dialect/SV/SVDialect.h"
#include "circt/Dialect/Seq/SeqDialect.h"

#include "mlir/IR/DialectRegistry.h"
#include "mlir/Tools/mlir-opt/MlirOptMain.h"

int main(int argc, char **argv) {
  mlir::DialectRegistry registry;

  // The `nsl` dialect itself (M4 deliverable).
  nsl::dialect::registerNSLDialect(registry);

  // The five CIRCT dialects loaded by `Compilation` per design §11
  // lines 1146–1150. Pre-loading them in `nsl-opt` lets hand-
  // authored mixed-dialect fixtures parse correctly even though the
  // M4 dialect itself doesn't reference CIRCT (per spec Edge-Cases
  // § "mixed-dialect fixtures" + research §6).
  registry.insert<circt::hw::HWDialect, circt::comb::CombDialect,
                  circt::seq::SeqDialect, circt::fsm::FSMDialect,
                  circt::sv::SVDialect>();

  return mlir::asMainReturnCode(
      mlir::MlirOptMain(argc, argv, "NSL dialect tool\n", registry));
}
