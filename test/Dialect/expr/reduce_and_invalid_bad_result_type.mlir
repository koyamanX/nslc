// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// Post-merge M4-amendment 2026-05-02 cluster 5: `nsl.reduce_and`
// rejects a result type other than `!nsl.bits<1>`.

nsl.module @ReduceAndHost {
  %a = nsl.constant 255 : !nsl.bits<8>
  // expected-error@+1 {{result type must be '!nsl.bits<1>'}}
  %r = "nsl.reduce_and"(%a) : (!nsl.bits<8>) -> !nsl.bits<8>
}
