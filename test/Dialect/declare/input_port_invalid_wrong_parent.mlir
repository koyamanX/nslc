// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: not nsl-opt %s 2>&1 | FileCheck %s
//
// Post-merge M4-amendment 2026-05-05 (#9): `nsl.input_port` carries
// `ParentOneOf<["DeclareOp", "ModuleOp"]>` (the dual-placement rule —
// see `lib/Dialect/NSL/IR/NSLOps.td` for the rationale). Placing it
// directly under the builtin `mlir::ModuleOp` (i.e., NOT inside
// `nsl.declare` or `nsl.module`) fires the standard parent-mismatch
// diagnostic.

// CHECK: 'nsl.input_port' op expects parent op to be one of 'nsl.declare, nsl.module'
%a = nsl.input_port "a" : !nsl.bits<8>
