// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt -nsl-resolve-params %s | FileCheck %s
//
// M5 US2 / FR-013 — multi-param scenario. Asserts the pass is a
// no-op against pure-NSL inputs at M5 (per the rationale in
// `literal_param.mlir`'s docblock — no `nsl::*` op carries a
// param-ref `FlatSymbolRefAttr` slot at M5's frozen surface). When
// a future op grows such a slot, this fixture's CHECK lines tighten
// to assert the substitution.

// CHECK: nsl.param_int @N = 8
nsl.param_int @N = 8

// CHECK: nsl.param_int @WIDTH = 16
nsl.param_int @WIDTH = 16

// CHECK: nsl.param_int @DEPTH = 256
nsl.param_int @DEPTH = 256

// CHECK: nsl.param_str @MODE = "async"
nsl.param_str @MODE = "async"

// CHECK-LABEL: nsl.module @MultiParamHost
nsl.module @MultiParamHost {
}
