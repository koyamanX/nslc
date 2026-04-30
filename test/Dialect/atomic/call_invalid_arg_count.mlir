// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// FR-013 / FR-019 invariant: `nsl.call` arg count must match the
// resolved control-terminal symbol's arity (per data-model §2.6 +
// T111). Calling a 2-arg `func_in` with only 1 argument violates
// this.
// Expects diagnostic substring "arg count" once T111 lands.

nsl.module @CallHost {
  %a = nsl.wire "a" : !nsl.bits<8>
  nsl.func_in "target"(%a, %a) : (!nsl.bits<8>, !nsl.bits<8>) -> ()
  nsl.func @body {
    nsl.seq {
      // expected-error@+1 {{arg count}}
      nsl.call @target(%a) : (!nsl.bits<8>) -> ()
    }
  }
}
