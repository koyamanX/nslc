// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --verify-diagnostics %s
//
// Post-merge M4-amendment 2026-05-01: `nsl.constant`'s hand-written
// verifier rejects `value` literals that exceed the result-type width.
// Here `256` does not fit in `!nsl.bits<8>` (max admissible pattern is
// `255`).

nsl.module @ConstHost {
  // expected-error@+1 {{does not fit in '!nsl.bits<8>'}}
  %bad = nsl.constant 256 : !nsl.bits<8>
}
