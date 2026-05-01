// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// Post-merge M4-amendment 2026-05-02 cluster 6: `nsl.sign_extend`'s
// hand-written verifier rejects narrowing (result-width < operand-
// width). 16 → 8 is a narrowing, NOT an extend.

nsl.module @SignExtHost {
  %a = nsl.constant 5 : !nsl.bits<16>
  // expected-error@+1 {{sign_extend result width 8 is smaller than operand width 16}}
  %r = nsl.sign_extend %a : !nsl.bits<16> to !nsl.bits<8>
}
