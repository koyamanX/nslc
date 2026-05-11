// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// Post-merge M4-amendment 2026-05-05 (#10): round-trip for
// `nsl.declare`'s S20 `interface_clock` + `interface_reset`
// OptionalAttr<StrAttr> pair. Two-pass round-trip per FR-017 +
// stability §5.
//
// Three placement scenarios exercised:
//   (1) Both attrs present — the explicit S20 `interface(clock=...,
//       reset=...)` modifier path. M6 emits user-named clock + reset
//       input ports verbatim (`circt-lowering.contract.md` §3 rule 7).
//   (2) Both attrs absent — the implicit `clk`/`rst_n` path (status
//       quo, identical to amendment-#9 behaviour).
//   (3) Reset name preserves polarity hint (`_n` suffix kept verbatim
//       — polarity interpretation is M6's responsibility, not the
//       dialect's).

// CHECK-LABEL: nsl.declare @WithInterface
// CHECK-SAME: attributes {interface_clock = "my_clk", interface_reset = "my_rst_n"}
nsl.declare @WithInterface attributes {interface_clock = "my_clk", interface_reset = "my_rst_n"} {
  // CHECK: %{{.*}} = nsl.input_port "a" : !nsl.bits<8>
  %a = nsl.input_port "a" : !nsl.bits<8>
  // CHECK: %{{.*}} = nsl.output_port "q" : !nsl.bits<8>
  %q = nsl.output_port "q" : !nsl.bits<8>
}

// CHECK-LABEL: nsl.declare @NoInterface
// CHECK-NOT: interface_clock
// CHECK-NOT: interface_reset
nsl.declare @NoInterface {
  %a = nsl.input_port "a" : !nsl.bits<8>
}

// CHECK-LABEL: nsl.declare @WithActiveHighReset
// CHECK-SAME: interface_reset = "rst"
nsl.declare @WithActiveHighReset attributes {interface_clock = "clk", interface_reset = "rst"} {
  %a = nsl.input_port "a" : !nsl.bits<8>
}
