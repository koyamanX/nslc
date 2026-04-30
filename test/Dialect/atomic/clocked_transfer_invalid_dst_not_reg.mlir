// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.clocked_transfer` first operand
// (`$dst`) must be reg-like (the result of an `nsl.reg` op or an
// `nsl.field` of a reg-typed struct, per data-model §4 helper
// `isRegLikeValue`). Passing a `nsl.wire` result violates this.
// Expects diagnostic substring "reg-like" once T109 lands.

nsl.module @ClockedHost {
  %w = nsl.wire "w" : !nsl.bits<8>
  %src = nsl.wire "src" : !nsl.bits<8>
  // expected-error@+1 {{reg-like}}
  nsl.clocked_transfer %w, %src : !nsl.bits<8>
}
