// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// Post-merge M4-amendment 2026-05-02 cluster 7a: `nsl.mux` rejects
// then/else operand type mismatch.

nsl.module @MuxHost {
  %c = nsl.constant 1 : !nsl.bits<1>
  %a = nsl.constant 5 : !nsl.bits<8>
  %b = nsl.constant 9 : !nsl.bits<16>
  // expected-error@+1 {{then/else operand type mismatch}}
  %r = "nsl.mux"(%c, %a, %b) : (!nsl.bits<1>, !nsl.bits<8>, !nsl.bits<16>) -> !nsl.bits<8>
}
