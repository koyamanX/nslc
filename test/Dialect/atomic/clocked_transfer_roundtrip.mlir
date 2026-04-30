// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.clocked_transfer` (design §7 line 913;
// reg-style `:=`). First operand kind = reg-like per FR-013.

// CHECK-LABEL: nsl.module @ClockedHost
nsl.module @ClockedHost {
  // CHECK: %{{.*}} = nsl.reg "q" : !nsl.bits<8> = 0 : i64
  %q = nsl.reg "q" : !nsl.bits<8> = 0 : i64
  // CHECK: %{{.*}} = nsl.wire "src" : !nsl.bits<8>
  %src = nsl.wire "src" : !nsl.bits<8>
  // CHECK: nsl.clocked_transfer %{{.*}}, %{{.*}} : !nsl.bits<8>
  nsl.clocked_transfer %q, %src : !nsl.bits<8>
}
