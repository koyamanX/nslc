// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-02 (#5) round-trip for `nsl.for`
// **enum-form 2-operand** (NSL `for (i = 0..N) { ... }` per
// `lang.ebnf §8`). Step is implicit in the enum form; the
// canonical 3-operand C-style form remains exercised by
// `for_roundtrip.mlir`.

// CHECK-LABEL: nsl.module @For2OpHost
nsl.module @For2OpHost {
  // CHECK: nsl.wire "init" : !nsl.bits<8>
  %init = nsl.wire "init" : !nsl.bits<8>
  // CHECK: nsl.wire "cond" : !nsl.bits<1>
  %cond = nsl.wire "cond" : !nsl.bits<1>
  // CHECK: nsl.func @loop
  nsl.func @loop {
    nsl.seq {
      // CHECK: nsl.for %{{.*}}, %{{.*}} : !nsl.bits<8>, !nsl.bits<1>
      nsl.for %init, %cond : !nsl.bits<8>, !nsl.bits<1> {
      }
    }
  }
}
