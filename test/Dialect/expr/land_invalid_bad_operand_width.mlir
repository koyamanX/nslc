// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// Post-merge M4-amendment 2026-05-02 cluster 2: `nsl.land` requires
// width-1 operands AND a width-1 result. Width-8 operand pair triggers
// the operand-width check.

nsl.module @LandHost {
  %a = nsl.constant 1 : !nsl.bits<8>
  %b = nsl.constant 0 : !nsl.bits<8>
  // expected-error@+1 {{logical-op operands must be '!nsl.bits<1>'}}
  %r = "nsl.land"(%a, %b) : (!nsl.bits<8>, !nsl.bits<8>) -> !nsl.bits<1>
}
