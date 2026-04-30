// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.if` (design §7 line 905). Two regions
// (then, else); else region MAY be empty per FR-013.

// CHECK-LABEL: nsl.module @IfHost
nsl.module @IfHost {
  // CHECK: nsl.wire "c" : !nsl.bits<1>
  %c = nsl.wire "c" : !nsl.bits<1>
  // CHECK: nsl.func @body
  nsl.func @body {
    nsl.seq {
      // CHECK: nsl.if %{{.*}}
      nsl.if %c : !nsl.bits<1> {
      } else {
      }
      // CHECK: nsl.if %{{.*}}
      nsl.if %c : !nsl.bits<1> {
      } else {
      }
    }
  }
}
