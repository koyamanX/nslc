// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// Post-merge M4-amendment 2026-05-02 cluster 7b: `nsl.extract` rejects
// a slice that exceeds the operand width.

nsl.module @ExtractHost {
  %v = nsl.constant 255 : !nsl.bits<8>
  // expected-error@+1 {{extract slice [lowBit=6, width=4] exceeds operand width 8}}
  %r = nsl.extract %v, 6 : !nsl.bits<8> -> !nsl.bits<4>
}
