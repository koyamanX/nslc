// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// Post-merge M4-amendment 2026-05-02 cluster 5: `nsl.reduce_xor` rejects
// a non-`!nsl.bits<1>` result type.

nsl.module @ReduceXorHost {
  %a = nsl.constant 7 : !nsl.bits<8>
  // expected-error@+1 {{result type must be '!nsl.bits<1>'}}
  %r = "nsl.reduce_xor"(%a) : (!nsl.bits<8>) -> !nsl.bits<2>
}
