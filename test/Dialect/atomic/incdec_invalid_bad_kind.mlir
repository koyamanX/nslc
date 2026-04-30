// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.incdec` `kind` enum-attr must be
// one of {pre_inc, post_inc, pre_dec, post_dec} (per data-model §2.6
// + the `IncDecKindAttr` enum). An out-of-range integer-encoded value
// violates the verifier's enum-validity check.
// Expects diagnostic substring "kind" once T110 lands.

nsl.module @IncDecHost {
  %q = nsl.reg "q" : !nsl.bits<8> = 0
  // expected-error@+1 {{kind}}
  "nsl.incdec"(%q) {kind = 99 : i32} : (!nsl.bits<8>) -> ()
}
