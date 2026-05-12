// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: not nsl-opt %s 2>&1 | FileCheck %s
//
// Post-merge M4-amendment 2026-05-05 (#9): `nsl.declare` requires a
// non-empty `pair_name` attribute (the attribute is `pair_name`, NOT
// `sym_name` — the latter would collide with the paired
// `nsl.module @M`'s magic-`sym_name` Symbol-trait. This fixture
// exercises the defensive empty-name reject in `DeclareOp::verify()`.
// Diagnostic message updated per PR #14 review-#1 fix to reference the
// actual attribute name `pair_name` (was `sym_name` — misleading).

// CHECK: requires a non-empty 'pair_name' attribute
"nsl.declare"() ({
^bb0:
}) {pair_name = ""} : () -> ()
