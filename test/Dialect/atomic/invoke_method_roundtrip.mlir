// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.invoke_method` (design §7 line 918).
// Form: `nsl.invoke_method @procInst(%a)`. Symbol ref to `nsl.proc`
// per FR-013.

// CHECK-LABEL: nsl.module @InvokeMethodHost
nsl.module @InvokeMethodHost {
  // CHECK: nsl.wire "a" : !nsl.bits<8>
  %a = nsl.wire "a" : !nsl.bits<8>
  // CHECK: nsl.func @driver
  nsl.func @driver {
    nsl.seq {
      // CHECK: nsl.invoke_method @procInst(%{{.*}})
      nsl.invoke_method @procInst(%a)
    }
  }
}
