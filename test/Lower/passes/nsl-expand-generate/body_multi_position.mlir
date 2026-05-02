// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt --mlir-very-unsafe-disable-verifier-on-parsing -nsl-expand-generate %s | FileCheck %s
//
// M5 US2 / FR-014 — acceptance fixture-axis (e) (`spec.md:215`):
// "generate body referencing the loop variable in multiple
// positions (decl name `buf_%i%`, expression `%i% + 1`)."
//
// At M5's frozen dialect surface, expressions inside a generate
// body that reference `%i%` would appear as `StringAttr` payloads
// (the visitor preserves the textual splice for residue-detection
// downstream). This fixture exercises substitution into multiple
// `StringAttr` slots on multiple ops — both the `name` slot of
// nsl.reg and the same op's hypothetical second StringAttr (here,
// init's textual stand-in via a probe op) — and verifies all
// substitution sites get the per-iteration integer.
//
// Phase-4 pragmatic shape: only `nsl.reg`'s `name` is a
// StringAttr today, so this fixture exercises multiple `%i%`
// occurrences within one nsl.reg name (e.g. `pre_%i%_post_%i%`).

// CHECK-LABEL: nsl.module @GenMultiPos
// CHECK-NOT: nsl.structural_generate
nsl.module @GenMultiPos {
  // CHECK: nsl.reg "pre_0_post_0" : !nsl.bits<8>
  // CHECK: nsl.reg "pre_1_post_1" : !nsl.bits<8>
  // CHECK: nsl.reg "pre_2_post_2" : !nsl.bits<8>
  nsl.structural_generate attributes {lower = 0 : i64, upper = 3 : i64, step = 1 : i64, loop_var = "i"} {
    nsl.reg "pre_%i%_post_%i%" : !nsl.bits<8>
  }
}
