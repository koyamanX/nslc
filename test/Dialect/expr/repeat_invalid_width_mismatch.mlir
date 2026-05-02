// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// Post-merge M4-amendment 2026-05-02 cluster 7b: `nsl.repeat` rejects
// a result width that does not equal `count * operand-width`.

nsl.module @RepeatHost {
  %a = nsl.constant 5 : !nsl.bits<3>
  // expected-error@+1 {{result width 8 does not equal count×operand-width 12}}
  %r = nsl.repeat %a, 4 : !nsl.bits<3> -> !nsl.bits<8>
}
