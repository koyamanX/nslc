// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.variable` (design §7 line 894). Per
// FR-013 result type is `!nsl.bits<N>`. Parent ∈ {`nsl.module`,
// `nsl.func`} (variadic HasParent).

// CHECK-LABEL: nsl.module @VarHost
nsl.module @VarHost {
  // CHECK: %{{.*}} = nsl.variable "v" : !nsl.bits<8>
  %v = nsl.variable "v" : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.variable "tmp" : !nsl.bits<16>
  %tmp = nsl.variable "tmp" : !nsl.bits<16>
}
