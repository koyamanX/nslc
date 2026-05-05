// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test/Lower/circt/fsm/seq_body_leaf_deferred.mlir — Round-1 review
// fix for PR #14 Finding #8 coverage. A non-goto leaf op (e.g., a
// constant computation) inside a `nsl.seq` body now produces an
// explicit deferral diagnostic instead of being silently dropped
// during the funcOp.erase() cascade. Full lowering of seq-body
// leaf ops + proper label-to-state resolution is M7-or-later
// territory.

// RUN: not nsl-opt -nsl-to-circt %s 2>&1 | FileCheck %s

nsl.module @M {
  nsl.func @F {
    nsl.seq {
      // Non-goto leaf op — should fail-fast with a deferral
      // diagnostic. We use nsl.constant here because most other
      // ops have `HasParent<"ModuleOp">` parent restrictions.
      %c = nsl.constant 42 : !nsl.bits<8>
    }
  }
}

// CHECK: M6 round-1 deferral: nsl.seq body op
// CHECK-SAME: nsl.constant
