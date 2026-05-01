// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// Post-merge M4-amendment 2026-05-02 cluster 2: `nsl.eq`'s hand-written
// verifier rejects a result type other than `!nsl.bits<1>`.

nsl.module @EqHost {
  %a = nsl.constant 1 : !nsl.bits<8>
  %b = nsl.constant 2 : !nsl.bits<8>
  // expected-error@+1 {{result type must be '!nsl.bits<1>'}}
  %r = "nsl.eq"(%a, %b) : (!nsl.bits<8>, !nsl.bits<8>) -> !nsl.bits<2>
}
