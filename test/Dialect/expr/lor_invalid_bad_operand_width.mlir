// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// Post-merge M4-amendment 2026-05-02 cluster 2: `nsl.lor` requires
// width-1 operands. Width-4 operand pair triggers the operand-width
// check.

nsl.module @LorHost {
  %a = nsl.constant 1 : !nsl.bits<4>
  %b = nsl.constant 0 : !nsl.bits<4>
  // expected-error@+1 {{logical-op operands must be '!nsl.bits<1>'}}
  %r = "nsl.lor"(%a, %b) : (!nsl.bits<4>, !nsl.bits<4>) -> !nsl.bits<1>
}
