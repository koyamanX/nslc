// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
//
// FR-010 round-trip for `nsl.seq` placed inside `nsl.proc` and
// `nsl.state` (post-merge M4 amendment 2026-04-30 #6). Per S7
// (`lang.ebnf §8 line 850`), `seq` may appear inside a func OR a
// proc body; amendment #6 relaxed `nsl.seq`'s parent trait from
// `HasParent<"FuncOp">` to
// `ParentOneOf<["FuncOp", "ProcOp", "StateOp"]>` so the M5
// AST→nsl seam can lower NSL's `proc p { seq { ... } }` shape
// structurally (Q1 Option A) without inventing a synthetic
// wrapping `nsl.func`. The state-scope variant covers procs whose
// state body wraps a `seq` (M3-corpus s19/s25 shape).

// CHECK-LABEL: nsl.module @SeqInProcHost
nsl.module @SeqInProcHost {
  // CHECK: nsl.proc @p_direct
  nsl.proc @p_direct {
    // CHECK: nsl.seq
    nsl.seq {
    }
  }
  // CHECK: nsl.proc @p_via_state
  nsl.proc @p_via_state {
    // CHECK: nsl.first_state @entry
    nsl.first_state @entry
    // CHECK: nsl.state @entry
    nsl.state @entry {
      // CHECK: nsl.seq
      nsl.seq {
      }
    }
  }
}
