// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-05 (#9): round-trip for `nsl.declare`
// + child port-info ops. Two-pass round-trip per FR-017 + stability §5.
//
// Three placement scenarios exercised:
//   (1) `nsl.declare @M { ... }` — port-list metadata for M6's
//       hw::HWModuleOp port-list derivation. Sibling of `nsl.module`.
//   (2) `nsl.declare @Empty {}` — anonymous-port-set declare (legal).
//   (3) Port-info ops inside `nsl.module @M` body — SSA-Value-bearing
//       port references (the dual-placement rule from amendment-#9
//       follow-on; see lib/Dialect/NSL/IR/NSLOps.td).

// CHECK-LABEL: nsl.declare @M
nsl.declare @M {
  // CHECK: %{{.*}} = nsl.input_port "a" : !nsl.bits<8>
  %a = nsl.input_port "a" : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.output_port "q" : !nsl.bits<8>
  %q = nsl.output_port "q" : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.inout_port "io" : !nsl.bits<4>
  %io = nsl.inout_port "io" : !nsl.bits<4>
}

// CHECK-LABEL: nsl.declare @Empty
nsl.declare @Empty {
}

// In-module placement (the SSA-Value-bearing form used by transfers).
// CHECK-LABEL: nsl.module @M
nsl.module @M {
  // CHECK: %{{.*}} = nsl.input_port "a" : !nsl.bits<8>
  %a = nsl.input_port "a" : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.output_port "q" : !nsl.bits<8>
  %q = nsl.output_port "q" : !nsl.bits<8>
}
