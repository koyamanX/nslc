// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// Post-merge M4-amendment 2026-05-02 cluster 6: `nsl.zero_extend`'s
// hand-written verifier rejects narrowing.

nsl.module @ZeroExtHost {
  %a = nsl.constant 5 : !nsl.bits<32>
  // expected-error@+1 {{zero_extend result width 8 is smaller than operand width 32}}
  %r = nsl.zero_extend %a : !nsl.bits<32> to !nsl.bits<8>
}
