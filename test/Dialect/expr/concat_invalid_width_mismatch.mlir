// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// Post-merge M4-amendment 2026-05-02 cluster 7a: `nsl.concat` rejects
// a result width that does not equal the sum of operand widths.

nsl.module @ConcatHost {
  %a = nsl.constant 0 : !nsl.bits<3>
  %b = nsl.constant 5 : !nsl.bits<5>
  // expected-error@+1 {{result width 16 does not equal sum of operand widths 8}}
  %r = "nsl.concat"(%a, %b) : (!nsl.bits<3>, !nsl.bits<5>) -> !nsl.bits<16>
}
