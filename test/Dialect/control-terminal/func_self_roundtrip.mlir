// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.func_self` (design §7 line 900). Form:
// `nsl.func_self "fire"(%w)`. Per FR-013 parent = `nsl.module`.

// CHECK-LABEL: nsl.module @FuncSelfHost
nsl.module @FuncSelfHost {
  // CHECK: nsl.wire "w" : !nsl.bits<1>
  %w = nsl.wire "w" : !nsl.bits<1>
  // Per Phase 4 SYN-4: standard `functional-type` form.
  // CHECK: nsl.func_self "fire"({{.*}}) : (!nsl.bits<1>) -> ()
  nsl.func_self "fire"(%w) : (!nsl.bits<1>) -> ()
}
