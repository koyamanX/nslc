// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.alt` (design §7 line 903). Priority-
// encoded; children = `nsl.case` followed optionally by `nsl.default`.
// Per FR-013, ≥ 1 child total. Per S13, alt is constructive (priority
// vs parallel) — the dialect verifier itself does NOT distinguish
// alt-vs-any semantics; that's an introspection observable.

// CHECK-LABEL: nsl.module @AltHost
nsl.module @AltHost {
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
        // CHECK: nsl.case %{{.*}}
        nsl.case %c2 : !nsl.bits<1> {
        }
        // CHECK: nsl.default
        nsl.default {
        }
      }
    }
  }
}
