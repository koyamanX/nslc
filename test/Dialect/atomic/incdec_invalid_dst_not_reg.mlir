// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.incdec` first operand (`$dst`)
// must be reg-like (per data-model §2.6 + §4 helper `isRegLikeValue`).
// Passing a `nsl.wire` result violates this.
// Expects diagnostic substring "reg-like" once T110 lands.

nsl.module @IncDecHost {
  %w = nsl.wire "w" : !nsl.bits<8>
  // expected-error@+1 {{reg-like}}
  nsl.incdec %w : !nsl.bits<8> {kind = #nsl<incdec_kind pre_inc>}
}
