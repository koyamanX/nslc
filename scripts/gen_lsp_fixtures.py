#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""One-shot helper to lift test/sema/s<NN>/fail*.nsl bodies into
test/lsp/fixtures/ for T3 Phase-3-deferred T051. Strips RUN/CHECK
lines + the original SPDX header; emits a fresh SPDX header that
points at the LSP-fixture role.
"""
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SEMA = REPO / "test" / "sema"
DST = REPO / "test" / "lsp" / "fixtures"

ENTRIES = [
    ("03", "fail_eq_on_reg.nsl",          "s03_eq_on_reg.nsl",          "S3: '=' on a reg should be ':='"),
    ("04", "fail_funcin.nsl",             "s04_funcin_dummy_dir.nsl",   "S4: func_in dummy arg must be 'input'"),
    ("05", "fail_funcin.nsl",             "s05_funcin_return_dir.nsl",  "S5: func_in return-terminal direction"),
    ("06", "fail.nsl",                    "s06_proc_arg_reg_only.nsl",  "S6: proc_name args must be 'reg'"),
    ("07", "fail_seq.nsl",                "s07_seq_outside_funcproc.nsl", "S7: seq only inside func/proc"),
    ("08", "fail_while.nsl",              "s08_while_outside_seq.nsl",  "S8: while only inside seq"),
    ("09", "fail.nsl",                    "s09_for_var_reg.nsl",        "S9: for-loop var must be 'reg'"),
    ("10", "fail.nsl",                    "s10_generate_var_integer.nsl","S10: generate var must be 'integer'"),
    ("11", "fail.nsl",                    "s11_state_name_proc_scoped.nsl", "S11: state_name proc-scoped"),
    ("12", "fail.nsl",                    "s12_partial_lhs_variable.nsl",  "S12: partial LHS only on 'variable'"),
    ("14", "fail.nsl",                    "s14_conditional_else_required.nsl", "S14: conditional needs 'else'"),
    ("15", "fail.nsl",                    "s15_slice_indices_const.nsl","S15: bit-slice index compile-time"),
    ("16", "fail.nsl",                    "s16_param_int_submodules.nsl","S16: param_int only for HDL submods"),
    ("17", "fail.nsl",                    "s17_system_task_simulation.nsl","S17: system task needs 'simulation'"),
    ("20", "fail.nsl",                    "s20_interface_clk_rst.nsl",  "S20: interface modifier explicit clk/rst"),
    ("21", "fail_bare_outside_proc.nsl",  "s21_bare_finish_outside_proc.nsl","S21: bare finish only inside proc"),
    ("22", "fail_outside_func.nsl",       "s22_return_outside_func.nsl","S22: return only inside func"),
    ("25", "fail_label_in_state.nsl",     "s25_goto_target.nsl",        "S25: goto target scoping"),
    ("26", "fail.nsl",                    "s26_function_synonym.nsl",   "S26: 'function' canonical is 'func'"),
    ("28", "fail_unknown_target.nsl",     "s28_first_state.nsl",        "S28: first_state target"),
    ("29", "fail.nsl",                    "s29_init_block_placement.nsl","S29: _init block placement"),
]

HEADER = """// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test/lsp/fixtures/{name} — T051 (US1).
// {desc}.
// Frozen diagnostic per `specs/006-m3-sema/contracts/diagnostic-string.contract.md`.

"""


def extract_body(text):
    out = []
    in_check = False
    for line in text.splitlines():
        s = line.strip()
        if s.startswith("// CHECK") or s.startswith("// CHECK-NEXT"):
            in_check = True
            continue
        if s.startswith("// RUN"):
            continue
        if in_check and s.startswith("//"):
            continue
        if (s.startswith("// SPDX") or s.startswith("// test/sema") or
                (s.startswith("// S") and ":" in s) or
                s.startswith("// Phase") or s.startswith("// Frozen") or
                s.startswith("// FixIt") or s.startswith("// **") or
                s.startswith("//   ") or s == "//"):
            continue
        out.append(line)
    while out and not out[0].strip(): out.pop(0)
    while out and not out[-1].strip(): out.pop()
    return "\n".join(out) + "\n"


DST.mkdir(parents=True, exist_ok=True)
created = 0
for sn, src_name, dst_name, desc in ENTRIES:
    src_path = SEMA / f"s{sn}" / src_name
    if not src_path.exists():
        print(f"!! missing {src_path}", file=sys.stderr)
        continue
    body = extract_body(src_path.read_text())
    out = HEADER.format(name=dst_name, desc=desc) + body
    (DST / dst_name).write_text(out)
    created += 1

print(f"created {created} fixtures")
