// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.call` (design §7 line 915). Symbol ref
// to `func_in` / `func_out` / `func_self` / `proc_name`. Per Q5
// Option A', cross-op refs use `FlatSymbolRefAttr` literal-match
// against the target's `sym_name` StringAttr.

// CHECK-LABEL: nsl.module @CallHost
nsl.module @CallHost {
  // CHECK: nsl.wire "a" : !nsl.bits<8>
  %a = nsl.wire "a" : !nsl.bits<8>
  // CHECK: nsl.wire "b" : !nsl.bits<8>
  %b = nsl.wire "b" : !nsl.bits<8>
  // CHECK: nsl.func @body
  nsl.func @body {
    nsl.seq {
      // CHECK: nsl.call @target(%{{.*}}, %{{.*}})
      nsl.call @target(%a, %b)
      // Dotted-form call (per N7 / Q5).
      // CHECK: nsl.call @ic.ready(%{{.*}})
      nsl.call @ic.ready(%a)
    }
  }
}
