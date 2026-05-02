// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt -nsl-expand-variables %s | FileCheck %s
//
// M5 US3 / FR-015 — `NSLExpandVariablesPass` (slot 3). Fixture
// axis (e) per `spec.md:272-273`:
//
//   "(e) variable declared in a func and consumed in an enclosing
//    proc (cross-scope visibility)."
//
// **Post-merge M4-amendment 2026-05-02 (#5) status.** `nsl.wire`'s
// parent trait was widened from `HasParent<"ModuleOp">` to
// `ParentOneOf<["ModuleOp", "FuncOp"]>` (NSLOps.td) so the pass
// can replace func-scope `nsl.variable` ops with sibling
// `nsl.wire` ops in-place (no hoisting). The expansion mechanics
// are unchanged — same wire-chain version remap as the module-
// scope case (`scalar_chain_of_3.mlir`); only the scope guard
// changed (`expandOne` admits `FuncOp` as a parent in addition
// to `ModuleOp`).
//
// Pre-amendment #5 this fixture was XFAIL on the parent-trait
// rejection; post-amendment it's a regular round-trip GREEN.

// CHECK-LABEL: nsl.module @CrossScope
nsl.module @CrossScope {
  // CHECK: nsl.func @f
  nsl.func @f {
    // The pass replaces this func-scope variable with a sibling
    // wire under the same `nsl.func` parent. The wire is inserted
    // immediately before the transfer (the variable's first
    // write-site) so the post-pass source order is:
    //   constant -> wire -> transfer (variable erased).
    %v = nsl.variable "v" : !nsl.bits<8>
    // CHECK: %[[K:.*]] = nsl.constant 0 : !nsl.bits<8>
    %k = nsl.constant 0 : !nsl.bits<8>
    // CHECK-NEXT: %[[V:.*]] = nsl.wire "v" : !nsl.bits<8>
    // CHECK-NEXT: nsl.transfer %[[V]], %[[K]]
    nsl.transfer %v, %k : !nsl.bits<8>
  }
  // CHECK-NOT: nsl.variable
}
