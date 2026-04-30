// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.func_out` (design §7 line 899). Form:
// `nsl.func_out "done"(%r)`. Per FR-013 parent = `nsl.module`.

// CHECK-LABEL: nsl.module @FuncOutHost
nsl.module @FuncOutHost {
  // CHECK: nsl.wire "r" : !nsl.bits<8>
  %r = nsl.wire "r" : !nsl.bits<8>
  // Per Phase 4 SYN-4: standard `functional-type` form.
  // CHECK: nsl.func_out "done"({{.*}}) : (!nsl.bits<8>) -> ()
  nsl.func_out "done"(%r) : (!nsl.bits<8>) -> ()
  // CHECK: nsl.func_out "ack"() : () -> ()
  nsl.func_out "ack"() : () -> ()
}
