// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test/Lower/circt/round_trip/structural_generate_fail_fast.mlir —
// M6 Phase 8 polish task (T138). Asserts that a residual
// `nsl.structural_generate` op (an invariant violation that should
// never escape M5's NSLExpandGeneratePass) fail-fasts cleanly when
// it reaches M6's NSLToCIRCTPass. The op is illegal per the
// conversion target (`addIllegalDialect<nsl::dialect::NSLDialect>`)
// and `applyFullConversion` reports the standard "failed to
// legalize operation" diagnostic.
//
// Closes the explicit FR-022 fail-fast coverage for
// `nsl::StructuralGenerateOp` per spec.md and the C1 finding from
// /speckit-analyze.

// RUN: not nsl-opt -nsl-to-circt %s 2>&1 | FileCheck %s --check-prefix=ERR

module {
  nsl.module @M {
    nsl.structural_generate attributes {lower = 0 : i64, upper = 4 : i64, step = 1 : i64, loop_var = "i"} {
    }
  }
}

// ERR: error
// ERR-SAME: nsl.structural_generate
