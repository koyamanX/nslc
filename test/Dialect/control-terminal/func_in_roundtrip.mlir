// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.func_in` (design §7 line 898). Form:
// `nsl.func_in "do"(%a, %b) : !nsl.bits<N>`. Per FR-013 parent =
// `nsl.module`; carries arg + ret attributes.

// CHECK-LABEL: nsl.module @FuncInHost
nsl.module @FuncInHost {
  // CHECK: nsl.wire "a" : !nsl.bits<8>
  %a = nsl.wire "a" : !nsl.bits<8>
  // CHECK: nsl.wire "b" : !nsl.bits<8>
  %b = nsl.wire "b" : !nsl.bits<8>
  // Per Phase 4 SYN-4: variadic-operand ops use MLIR's standard
  // `functional-type($args, results)` spelling — `(args) -> result`.
  // CHECK: nsl.func_in "do"({{.*}}, {{.*}}) : (!nsl.bits<8>, !nsl.bits<8>) -> !nsl.bits<8>
  nsl.func_in "do"(%a, %b) : (!nsl.bits<8>, !nsl.bits<8>) -> !nsl.bits<8>
  // CHECK: nsl.func_in "noret"({{.*}}) : (!nsl.bits<8>) -> ()
  nsl.func_in "noret"(%a) : (!nsl.bits<8>) -> ()
}
