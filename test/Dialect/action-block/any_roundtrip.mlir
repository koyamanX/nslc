// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.any` (design §7 line 904). Parallel
// (vs alt's priority); semantically distinct from alt per S13 but
// the dialect's verifier doesn't distinguish — same structural shape.

// CHECK-LABEL: nsl.module @AnyHost
nsl.module @AnyHost {
  // CHECK: nsl.wire "c1" : !nsl.bits<1>
  %c1 = nsl.wire "c1" : !nsl.bits<1>
  // CHECK: nsl.wire "c2" : !nsl.bits<1>
  %c2 = nsl.wire "c2" : !nsl.bits<1>
  // CHECK: nsl.func @body
  nsl.func @body {
    nsl.seq {
      // CHECK: nsl.any
      nsl.any {
        // CHECK: nsl.case %{{.*}}
        nsl.case %c1 {
        }
        // CHECK: nsl.case %{{.*}}
        nsl.case %c2 {
        }
        // CHECK: nsl.default
        nsl.default {
        }
      }
    }
  }
}
