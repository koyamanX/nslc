// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-02 (#5): `nsl.wire`'s parent
// constraint is widened from `HasParent<"ModuleOp">` to
// `ParentOneOf<["ModuleOp", "FuncOp"]>` so that
// `NSLExpandVariablesPass` can replace func-scope `nsl.variable`
// ops with sibling `nsl.wire` ops. The module-scope case stays
// covered by `wire_roundtrip.mlir`.

// CHECK-LABEL: nsl.module @WireFuncHost
nsl.module @WireFuncHost {
  // CHECK: nsl.func @scope
  nsl.func @scope {
    // CHECK: %{{.*}} = nsl.wire "tmp" : !nsl.bits<8>
    %tmp = nsl.wire "tmp" : !nsl.bits<8>
  }
}
