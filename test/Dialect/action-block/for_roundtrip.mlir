// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.for` (design §7 line 909). One region;
// transitive parent = `nsl.seq` per Q2 Option B. Form:
// `nsl.for %init, %cond, %step { ... }` (C-style three-operand form).

// CHECK-LABEL: nsl.module @ForHost
nsl.module @ForHost {
  // CHECK: nsl.wire "init" : !nsl.bits<8>
  %init = nsl.wire "init" : !nsl.bits<8>
  // CHECK: nsl.wire "cond" : !nsl.bits<1>
  %cond = nsl.wire "cond" : !nsl.bits<1>
  // CHECK: nsl.wire "step" : !nsl.bits<8>
  %step = nsl.wire "step" : !nsl.bits<8>
  // CHECK: nsl.func @loop
  nsl.func @loop {
    nsl.seq {
      // CHECK: nsl.for %{{.*}}, %{{.*}}, %{{.*}}
      nsl.for %init, %cond, %step {
      }
    }
  }
}
