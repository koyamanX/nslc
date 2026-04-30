// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.func` (design §7 line 924). Per Q5
// Option A', `sym_name` is `StringAttr` containing the literal
// dotted form: bare-form `nsl.func @reset` stores `"reset"`; dotted-
// form `nsl.func @ic.ready` stores `"ic.ready"` as a single
// StringAttr (MLIR symbol identifiers permit `.` per upstream
// lexical rules; matches `func.func @some.dotted.name` precedent).

// CHECK-LABEL: nsl.module @FuncHost
nsl.module @FuncHost {
  // Bare-form sym_name (per Q5).
  // CHECK: nsl.func @reset
  nsl.func @reset {
    nsl.seq {
    }
  }
  // Dotted-form sym_name per N7 / Q5 (submodule-out form).
  // CHECK: nsl.func @ic.ready
  nsl.func @ic.ready {
    nsl.seq {
    }
  }
}
