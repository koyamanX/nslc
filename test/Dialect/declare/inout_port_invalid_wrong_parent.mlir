// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: not nsl-opt %s 2>&1 | FileCheck %s
//
// Post-merge M4-amendment 2026-05-05 (#9): `nsl.inout_port` parent
// constraint test (mirrors `input_port_invalid_wrong_parent.mlir`).

// CHECK: 'nsl.inout_port' op expects parent op to be one of 'nsl.declare, nsl.module'
%io = nsl.inout_port "io" : !nsl.bits<4>
