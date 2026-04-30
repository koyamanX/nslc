// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.default` (design §7 lines 903–904). Per
// FR-013 parent ∈ {`nsl.alt`, `nsl.any`} (variadic HasParent). Cover
// both placements.

// CHECK-LABEL: nsl.module @DefaultHost
nsl.module @DefaultHost {
  // CHECK: nsl.func @body
  nsl.func @body {
    nsl.seq {
      // CHECK: nsl.alt
      nsl.alt {
        // CHECK: nsl.default
        nsl.default {
        }
      }
      // CHECK: nsl.any
      nsl.any {
        // CHECK: nsl.default
        nsl.default {
        }
      }
    }
  }
}
