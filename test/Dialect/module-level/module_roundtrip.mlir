// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.module`. Two forms: empty body and
// populated body. Symbol/SymbolTable traits keep the `@M` form;
// `SingleBlockImplicitTerminator<"ModuleTerminatorOp">` makes the
// terminator implicit at print time.

// CHECK-LABEL: nsl.module @Empty
nsl.module @Empty {
}

// CHECK-LABEL: nsl.module @WithChildren
nsl.module @WithChildren {
  // CHECK: %{{.*}} = nsl.wire "w" : !nsl.bits<8>
  %w = nsl.wire "w" : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.reg "q" : !nsl.bits<8> = 0 : i64
  %q = nsl.reg "q" : !nsl.bits<8> = 0 : i64
}
