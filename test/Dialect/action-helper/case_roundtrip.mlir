// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.case` (design §7 lines 903–904). Per
// FR-013 parent ∈ {`nsl.alt`, `nsl.any`} (variadic HasParent). Cover
// both placements.

// CHECK-LABEL: nsl.module @CaseHost
nsl.module @CaseHost {
  // CHECK: nsl.wire "c1" : !nsl.bits<1>
  %c1 = nsl.wire "c1" : !nsl.bits<1>
  // CHECK: nsl.wire "c2" : !nsl.bits<1>
  %c2 = nsl.wire "c2" : !nsl.bits<1>
  // CHECK: nsl.func @body
  nsl.func @body {
    nsl.seq {
      // CHECK: nsl.alt
      nsl.alt {
        // CHECK: nsl.case %{{.*}}
        nsl.case %c1 : !nsl.bits<1> {
        }
      }
      // CHECK: nsl.any
      nsl.any {
        // CHECK: nsl.case %{{.*}}
        nsl.case %c2 : !nsl.bits<1> {
        }
      }
    }
  }
}
