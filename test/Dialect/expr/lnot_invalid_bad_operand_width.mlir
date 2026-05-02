// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// Post-merge M4-amendment 2026-05-02 cluster 5: `nsl.lnot` requires
// width-1 operand AND width-1 result.

nsl.module @LnotHost {
  %a = nsl.constant 1 : !nsl.bits<8>
  // expected-error@+1 {{logical-op operands must be '!nsl.bits<1>'}}
  %r = "nsl.lnot"(%a) : (!nsl.bits<8>) -> !nsl.bits<1>
}
