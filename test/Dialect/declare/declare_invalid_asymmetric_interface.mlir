// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: not nsl-opt %s 2>&1 | FileCheck %s
//
// Post-merge M4-amendment 2026-05-05 (#10): the verifier rejects
// asymmetric presence of `interface_clock` / `interface_reset` —
// S20 mandates BOTH names when an `interface(...)` clause appears.

// CHECK: error: 'nsl.declare' op S20 'interface' modifier requires both 'interface_clock' and 'interface_reset' attributes to be set together
// CHECK-SAME: got only 'interface_clock'
nsl.declare @AsymmetricClkOnly attributes {interface_clock = "my_clk"} {
  %a = nsl.input_port "a" : !nsl.bits<8>
}
