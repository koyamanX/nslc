// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: not nsl-opt %s 2>&1 | FileCheck %s
//
// Post-merge M4-amendment 2026-05-05 (#9): `nsl.declare` requires a
// non-empty `sym_name` attribute (Symbol-trait machinery enforces
// presence; this fixture exercises the defensive empty-name reject in
// `DeclareOp::verify()`).

// CHECK: requires a non-empty 'sym_name' attribute
"nsl.declare"() ({
^bb0:
}) {pair_name = ""} : () -> ()
