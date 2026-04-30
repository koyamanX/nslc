// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.mem` (design §7 line 895). Form:
// `nsl.mem "name" : !nsl.mem<[D x T]>`. Per FR-013, parent =
// `nsl.module`; result type is `!nsl.mem<[D x T]>`.

// CHECK-LABEL: nsl.module @MemHost
nsl.module @MemHost {
  // CHECK: %{{.*}} = nsl.mem "ram" : !nsl.mem<[256 x !nsl.bits<8>]>
  %ram = nsl.mem "ram" : !nsl.mem<[256 x !nsl.bits<8>]>
  // CHECK: %{{.*}} = nsl.mem "rom" : !nsl.mem<[1024 x !nsl.bits<32>]>
  %rom = nsl.mem "rom" : !nsl.mem<[1024 x !nsl.bits<32>]>
}
