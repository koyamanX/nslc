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
      // Per Phase 4 SYN-4: variadic operands use `functional-type`.
      // CHECK: nsl.invoke_method @procInst(%{{.*}}) : (!nsl.bits<8>) -> ()
      nsl.invoke_method @procInst(%a) : (!nsl.bits<8>) -> ()
    }
  }
}
