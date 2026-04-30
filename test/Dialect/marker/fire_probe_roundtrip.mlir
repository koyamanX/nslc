// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.fire_probe` (design §8 line 1062). Per
// S27 (constructive), control-terminal name is a 1-bit value. Symbol
// ref to a sibling `nsl.func_in` / `nsl.func_out` / `nsl.func_self`.

// CHECK-LABEL: nsl.module @FireProbeHost
nsl.module @FireProbeHost {
  // CHECK: nsl.wire "a" : !nsl.bits<8>
  %a = nsl.wire "a" : !nsl.bits<8>
  // CHECK: nsl.func_in "do"({{.*}}) : !nsl.bits<8>
  nsl.func_in "do"(%a) : !nsl.bits<8>
  // CHECK: nsl.fire_probe @do
  nsl.fire_probe @do
}
