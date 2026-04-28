#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""scripts/gen_grammar_fixtures.py — generate per-grammar-production
fixture stubs for `lang.ebnf §§1–11`.

Per `specs/005-m2-parser/research.md` §9, US1's per-production
AST-snapshot fixtures live under `test/parse/grammar/<production>/`.
This script reads a curated list of EBNF productions (defined inline
below — same precedent as M1's `scripts/gen_keyword_fixtures.py`,
which embeds the keyword set as a Python data structure rather than
re-parsing the EBNF) and emits a `pass.test` skeleton for each. Each
generated fixture is a lit + FileCheck `.test` whose expected
`-emit=ast` output mirrors the contract example in
`specs/005-m2-parser/contracts/nslc-emit-ast.contract.md`.

Per Constitution Principle V (deterministic): given the same curated
list, this script produces byte-identical output across runs. No
hash-derived ordering, no timestamps, no environment-derived state in
the emitted text — entries iterate in source order.

Per Constitution Principle VIII (TDD): the fixtures land BEFORE
`nslc -emit=ast` is implemented. Running lit against the unchanged
tree observes each fixture FAILING (no `-emit=ast` flag yet → exit
code 2 / 3 from the M1 driver; see FR-029).

Usage:
  python3 scripts/gen_grammar_fixtures.py [--force]

Options:
  --force   Overwrite existing .test files. Without --force the
            script skips files that already exist (safer
            regeneration).
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

# ----------------------------------------------------------------------------
# Curated list of `lang.ebnf §§1–11` productions covered by US1's
# per-production AST snapshot fixtures. Each entry is:
#
#   (production_name,        # directory + file basename
#    section,                # EBNF section number (string)
#    description,            # one-line human description
#    nsl_input,              # the NSL source the fixture exercises
#    expected_root_kind,     # outermost AST node kind name
#    expected_inner_kinds)   # ordered substring list to FileCheck
#
# `expected_inner_kinds` is a list of `(NodeKindName, fields...)`
# tuples whose textual form FileCheck asserts as substring matches in
# nesting order. Source-range coordinates are not pinned in
# `expected_inner_kinds` — fixtures use `loc=` regex placeholders so
# minor whitespace shifts in the input don't break the match.
#
# Source order is the print order: keep this list grouped by EBNF
# section. Adding a new production: append one tuple in the matching
# section group; rerun `--force`.
# ----------------------------------------------------------------------------

# The structural tag conveys nesting / containment without pinning the
# exact whitespace or column of every byte. For the format-frozen
# golden, see `test/Driver/emit-ast.test`. Per-production fixtures
# below assert *structural* presence, not byte-exact bytes.

PRODUCTIONS: list[tuple[str, str, str, str, list[str]]] = [
    # ---- §1 Compilation unit ----
    (
        "compilation-unit",
        "1",
        "compilation_unit — empty top-level item list",
        "",
        ["(CompilationUnit"],
    ),
    # ---- §3 Struct type declaration ----
    (
        "struct-declaration",
        "3",
        "struct_declaration with one width-bearing member",
        "struct s_t { v[8]; };\n",
        ["(CompilationUnit", "(StructDecl", "name=s_t"],
    ),
    # ---- §3.1 Top-level parameters ----
    (
        "top-level-param-int",
        "3.1",
        "top_level_parameter — param_int form",
        "param_int N = 8;\n",
        ["(CompilationUnit", "(TopLevelParamDecl", "name=N"],
    ),
    (
        "top-level-param-str",
        "3.1",
        "top_level_parameter — param_str form",
        "param_str MODE = \"SIM\";\n",
        ["(CompilationUnit", "(TopLevelParamDecl", "name=MODE"],
    ),
    # ---- §4 declare block ----
    (
        "declare-block",
        "4",
        "declare_block — minimal form, no modifier, single port",
        "declare d {\n  input clk;\n}\n",
        ["(CompilationUnit", "(DeclareBlock", "name=d"],
    ),
    (
        "data-terminal-declaration",
        "4",
        "data_terminal_declaration — input/output/inout directions",
        "declare d {\n  input clk;\n  output q[8];\n  inout io;\n}\n",
        ["(DeclareBlock", "(PortDecl"],
    ),
    (
        "control-terminal-declaration",
        "4",
        "control_terminal_declaration — func_in / func_out forms",
        "declare d {\n  func_in start();\n  func_out done;\n}\n",
        ["(DeclareBlock"],
    ),
    # ---- §5 module block ----
    (
        "module-block",
        "5",
        "module_block — empty body",
        "module m {\n}\n",
        ["(CompilationUnit", "(ModuleBlock", "name=m"],
    ),
    # ---- §6 Internal declarations ----
    (
        "internal-wire",
        "6",
        "internal_terminal_declaration — wire form",
        "module m {\n  wire q[8];\n}\n",
        ["(ModuleBlock", "(WireDecl", "name=q"],
    ),
    (
        "internal-reg",
        "6",
        "register_declaration — width + initializer",
        "module m {\n  reg q[8] = 0;\n}\n",
        ["(ModuleBlock", "(RegDecl", "name=q"],
    ),
    (
        "internal-func-self",
        "6",
        "control_internal_declaration — func_self form",
        "module m {\n  func_self ready();\n}\n",
        ["(ModuleBlock", "(FuncSelfDecl", "name=ready"],
    ),
    (
        "internal-submodule",
        "6",
        "submodule_declaration — single instance",
        "module m {\n  cpu_t inst;\n}\n",
        ["(ModuleBlock", "(SubmoduleDecl"],
    ),
    (
        "internal-proc-name",
        "6",
        "procedure_name_declaration — bare form",
        "module m {\n  proc_name idle;\n}\n",
        ["(ModuleBlock", "(ProcNameDecl", "name=idle"],
    ),
    (
        "internal-state-name",
        "6",
        "state_name_declaration — multi-name list",
        "module m {\n  state_name s1, s2, s3;\n}\n",
        ["(ModuleBlock", "(StateNameDecl"],
    ),
    (
        "internal-first-state",
        "6",
        "first_state_declaration — explicit initial state",
        "module m {\n  state_name s1;\n  first_state s1;\n}\n",
        ["(ModuleBlock", "(FirstStateDecl", "target=s1"],
    ),
    (
        "internal-mem",
        "6",
        "memory_declaration — depth × width without init list",
        "module m {\n  mem buf[256][8];\n}\n",
        ["(ModuleBlock", "(MemDecl", "name=buf"],
    ),
    (
        "internal-struct-instance",
        "6",
        "struct_instance_declaration — reg form, scalar instance",
        "module m {\n  s_t reg slot;\n}\n",
        ["(ModuleBlock", "(StructInstDecl"],
    ),
    (
        "internal-integer",
        "6",
        "integer_declaration — single name",
        "module m {\n  integer i;\n}\n",
        ["(ModuleBlock", "(IntegerDecl", "name=i"],
    ),
    (
        "internal-variable",
        "6",
        "variable_declaration — width form",
        "module m {\n  variable v[16];\n}\n",
        ["(ModuleBlock", "(VariableDecl", "name=v"],
    ),
    # ---- §7 func / proc / state ----
    (
        "function-definition",
        "7",
        "function_definition — single-id name, empty body",
        "module m {\n  func ready { }\n}\n",
        ["(ModuleBlock", "(FuncDefn"],
    ),
    (
        "procedure-definition",
        "7",
        "procedure_definition — empty body",
        "module m {\n  proc idle { }\n}\n",
        ["(ModuleBlock", "(ProcDefn", "name=idle"],
    ),
    (
        "state-definition",
        "7",
        "state_definition — empty body",
        "module m {\n  proc p {\n    state_name s1;\n    state s1 { }\n  }\n}\n",
        ["(ModuleBlock", "(ProcDefn", "(StateDefn", "name=s1"],
    ),
    # ---- §8 action statements ----
    (
        "action-parallel",
        "8",
        "parallel_block — empty {} pair",
        "module m {\n  { }\n}\n",
        ["(ModuleBlock", "(ParallelBlock"],
    ),
    (
        "action-alt",
        "8",
        "alt_block — single case",
        "module m {\n  alt { c: q = 0; }\n}\n",
        ["(ModuleBlock", "(AltBlock"],
    ),
    (
        "action-any",
        "8",
        "any_block — single case",
        "module m {\n  any { c: q = 0; }\n}\n",
        ["(ModuleBlock", "(AnyBlock"],
    ),
    (
        "action-seq",
        "8",
        "seq_block — empty body",
        "module m {\n  proc p { seq { } }\n}\n",
        ["(ModuleBlock", "(ProcDefn", "(SeqBlock"],
    ),
    (
        "action-if",
        "8",
        "conditional_if_statement — statement form (per N1)",
        "module m {\n  if (c) q = 0;\n}\n",
        ["(ModuleBlock", "(IfStmt"],
    ),
    (
        "action-for",
        "8",
        "for_block — C-style three-clause form",
        "module m {\n  proc p { seq { for (i := 0; i < 8; i++) { } } }\n}\n",
        ["(ProcDefn", "(SeqBlock", "(ForBlock"],
    ),
    (
        "action-while",
        "8",
        "while_block — empty body inside seq",
        "module m {\n  proc p { seq { while (c) { } } }\n}\n",
        ["(ProcDefn", "(SeqBlock", "(WhileBlock"],
    ),
    (
        "action-generate",
        "8",
        "structural_generate — single iteration",
        "module m {\n  generate (i = 0; i < 1; i++) { }\n}\n",
        ["(ModuleBlock", "(StructuralGenerate"],
    ),
    # ---- §9 atomic actions ----
    (
        "atomic-transfer-wire",
        "9",
        "wire_or_variable_transfer — `=`",
        "module m {\n  wire q;\n  q = 0;\n}\n",
        ["(ModuleBlock", "(TransferStmt"],
    ),
    (
        "atomic-transfer-reg",
        "9",
        "register_transfer — `:=`",
        "module m {\n  reg q;\n  proc p { q := 0; }\n}\n",
        ["(ModuleBlock", "(ProcDefn", "(TransferStmt"],
    ),
    (
        "atomic-incdec",
        "9",
        "increment_decrement — postfix `++`",
        "module m {\n  reg i;\n  proc p { i++; }\n}\n",
        ["(ProcDefn", "(IncDecStmt"],
    ),
    (
        "atomic-control-call",
        "9",
        "control_terminal_call — bare invocation",
        "module m {\n  func_self ready();\n  ready();\n}\n",
        ["(ModuleBlock", "(ControlCallStmt"],
    ),
    (
        "atomic-bare-finish",
        "9",
        "bare_finish_statement — `finish`",
        "module m {\n  proc p { finish; }\n}\n",
        ["(ProcDefn", "(BareFinishStmt"],
    ),
    (
        "atomic-system-task",
        "9",
        "system_task — `_display(\"hi\")`",
        "module m {\n  _display(\"hi\");\n}\n",
        ["(ModuleBlock", "(SystemTaskStmt"],
    ),
    (
        "atomic-return",
        "9",
        "return_statement — bare form",
        "module m {\n  func ready { return; }\n}\n",
        ["(FuncDefn", "(ReturnStmt"],
    ),
    (
        "atomic-labeled",
        "9",
        "labeled_statement — `lab:` marker",
        "module m {\n  proc p { seq { lab: } }\n}\n",
        ["(SeqBlock", "(LabeledStmt"],
    ),
    (
        "atomic-goto",
        "9",
        "goto_statement — single target",
        "module m {\n  proc p { seq { lab: goto lab; } }\n}\n",
        ["(SeqBlock", "(GotoStmt"],
    ),
    (
        "atomic-init-block",
        "10",
        "init_block — empty `_init` body",
        "module m {\n  _init { }\n}\n",
        ["(ModuleBlock", "(InitBlockStmt"],
    ),
    (
        "atomic-delay",
        "10",
        "delay_task — `_delay(1)`",
        "module m {\n  _init { _delay(1); }\n}\n",
        ["(InitBlockStmt", "(DelayTaskStmt"],
    ),
    # ---- §11 expressions ----
    #
    # These fixtures use `reg q = <expr>;` (register_declarator,
    # `lang.ebnf §6` line 219 — `[ "=" constant_expression ]`) rather
    # than `wire q = <expr>;`, because `internal_terminal_declaration`
    # (line 211) does NOT permit an initializer on `wire` — only `reg`
    # does. The expression *value* being asserted is the same; the
    # carrier declaration is what differs. See `internal-reg` above
    # for the bare carrier form. The mass regen of the original
    # (illegal) `wire q = <expr>;` form was the "M2 Group α"
    # fixture-side fix flagged in `15b2bb7`'s findings note covering
    # the 24 outstanding lit fixtures grouped α–δ.
    (
        "expr-literal-decimal",
        "11",
        "literal — decimal",
        "module m {\n  reg q = 8;\n}\n",
        ["(RegDecl", "(LiteralExpr", "kind=Decimal"],
    ),
    (
        "expr-identifier",
        "11",
        "identifier expression",
        "module m {\n  wire a;\n  reg b = a;\n}\n",
        ["(ModuleBlock", "(IdentifierExpr"],
    ),
    (
        "expr-system-var",
        "11",
        "system_variable — `_random` (per N11(b))",
        "module m {\n  reg q = _random;\n}\n",
        ["(RegDecl", "(SystemVarExpr"],
    ),
    (
        "expr-unary",
        "11",
        "unary_expr — bitwise NOT",
        "module m {\n  wire a;\n  reg b = ~a;\n}\n",
        ["(RegDecl", "(UnaryExpr"],
    ),
    (
        "expr-binary",
        "11",
        "binary_expr — additive",
        "module m {\n  wire a, b;\n  reg c = a + b;\n}\n",
        ["(RegDecl", "(BinaryExpr"],
    ),
    (
        "expr-conditional",
        "11",
        "conditional_expression — `if (c) a else b` (per N1 expr form)",
        "module m {\n  wire c, a, b;\n  reg q = if (c) a else b;\n}\n",
        ["(RegDecl", "(ConditionalExpr"],
    ),
    (
        "expr-concat",
        "11",
        "concat_expression — `{a, b}`",
        "module m {\n  wire a, b;\n  reg q = {a, b};\n}\n",
        ["(RegDecl", "(ConcatExpr"],
    ),
    (
        "expr-repeat",
        "11",
        "repeat_expression — `4{1'b0}` form",
        "module m {\n  reg q = 4{0};\n}\n",
        ["(RegDecl", "(RepeatExpr"],
    ),
    (
        "expr-sign-extend",
        "11",
        "sign_extend_expression — `8 # sig` (per N5)",
        "module m {\n  wire sig;\n  reg q = 8 # sig;\n}\n",
        ["(RegDecl", "(SignExtendExpr"],
    ),
    (
        "expr-zero-extend",
        "11",
        "zero_extend_expression — `8'(sig)`",
        "module m {\n  wire sig;\n  reg q = 8'(sig);\n}\n",
        ["(RegDecl", "(ZeroExtendExpr"],
    ),
    (
        "expr-slice",
        "11",
        "bit_select_or_slice — `a[3:0]`",
        "module m {\n  wire a;\n  reg q = a[3:0];\n}\n",
        ["(RegDecl", "(SliceExpr"],
    ),
    (
        "expr-field-access",
        "11",
        "postfix_tail field access — `inst.port`",
        "module m {\n  cpu_t inst;\n  reg q = inst.port;\n}\n",
        ["(RegDecl", "(FieldAccessExpr"],
    ),
    (
        "expr-call",
        "11",
        "postfix_tail call — `f(a, b)`",
        "module m {\n  func_self f(x, y);\n  reg q = f(0, 0);\n}\n",
        ["(RegDecl", "(CallExpr"],
    ),
    (
        "expr-struct-cast",
        "11",
        "struct_cast_expr — `(T)(x).m`",
        "module m {\n  wire x;\n  reg q = (T)(x).m;\n}\n",
        ["(RegDecl", "(StructCastExpr"],
    ),
    (
        "expr-incdec",
        "11",
        "increment_decrement in expression position — `i++`",
        "module m {\n  reg i;\n  reg q = i++;\n}\n",
        ["(RegDecl", "(IncDecExpr"],
    ),
]


FIXTURE_TEMPLATE = """\
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Generated from `lang.ebnf` §§1–11 — DO NOT EDIT.
// Regenerate with: python3 scripts/gen_grammar_fixtures.py
//
// Per-production AST-snapshot fixture for `{production}` (`lang.ebnf
// §{section}`) per Constitution Principle VIII (TDD; FR-029) and
// `specs/005-m2-parser/research.md` §9 (test corpus organization).
//
// Subject: {description}
//
// The input is written to a temp file via lit's `%t` substitution
// rather than embedded in this `.test`; otherwise the SPDX header
// and FileCheck directive lines would enter the parsed buffer and
// confuse the AST-snapshot. A temp file gives a stable path the parser
// reports cleanly via the post-`#line` virtual coordinates per
// Principle IV / FR-018.
//
// Source-range coordinates in the printer's `loc=` field are NOT
// pinned to byte-exact values here — fixtures assert structural
// presence per `specs/005-m2-parser/research.md` §9. The byte-frozen
// format golden is `test/Driver/emit-ast.test`; if format drift
// occurs there, this fixture is unaffected.
//
// **Failing-state evidence (FR-029)**: against the unchanged tree at
// `7bc1aea`, the M1 driver does not implement `-emit=ast` — invoking
// it exits with the M1 driver's "unknown emit stage" path (exit 2),
// so the FileCheck pipeline observes a mismatch and the test fails.

// RUN: printf {input_repr} > %t.nsl
// RUN: %nslc -emit=ast %t.nsl | FileCheck %s

{check_lines}
"""


def encode_for_printf(text: str) -> str:
    """Return a single-line shell-safe single-quoted format string for
    `printf <fmt>`.

    Bash's builtin `printf` interprets backslash escapes inside its
    format string when invoked without `%s` / `%b` — e.g.
    `printf 'a\\nb'` writes `a`, newline, `b`. We encode real
    newlines as the two-character `\\n` sequence so the format
    string stays on a single line (lit's `RUN:` directive only
    consumes the line on which it appears). Backslashes already in
    the source must be doubled so printf's interpretation reproduces
    them. Single quotes inside the input use the standard
    close-quote / double-quoted-single-quote / open-quote sandwich
    (`'"'"'`) so the surrounding shell single-quoted literal stays
    intact.

    Same convention M1's `test/Driver/emit-tokens.test` uses (commit
    `1d10b5d` lines 29 / 33), reused for cross-fixture consistency.
    """
    out = []
    for ch in text:
        if ch == "\\":
            out.append(r"\\")
        elif ch == "\n":
            out.append(r"\n")
        elif ch == "'":
            out.append("'\"'\"'")
        else:
            out.append(ch)
    return "'" + "".join(out) + "'"


def emit_fixture(production: str, section: str, description: str,
                 nsl_input: str,
                 expected_inner: list[str]) -> str:
    """Render the lit + FileCheck fixture for one production."""
    input_repr = encode_for_printf(nsl_input)
    check_lines_list = []
    if expected_inner:
        check_lines_list.append(f"// CHECK: {expected_inner[0]}")
        for sub in expected_inner[1:]:
            check_lines_list.append(f"// CHECK: {sub}")
    else:
        check_lines_list.append("// CHECK: (CompilationUnit")
    check_lines = "\n".join(check_lines_list)
    return FIXTURE_TEMPLATE.format(
        production=production,
        section=section,
        description=description,
        input_repr=input_repr,
        check_lines=check_lines,
    )


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--force",
        action="store_true",
        help="Overwrite existing fixtures (default: skip them).",
    )
    parser.add_argument(
        "--out-dir",
        default=None,
        help="Output directory (default: test/parse/grammar/).",
    )
    args = parser.parse_args(argv)

    repo_root = Path(__file__).resolve().parent.parent
    out_dir = (
        Path(args.out_dir)
        if args.out_dir
        else repo_root / "test" / "parse" / "grammar"
    )

    out_dir.mkdir(parents=True, exist_ok=True)

    written = 0
    skipped = 0
    # Iterate in declaration order (Principle V — deterministic).
    for production, section, desc, nsl_in, expected in PRODUCTIONS:
        prod_dir = out_dir / production
        prod_dir.mkdir(parents=True, exist_ok=True)
        out_path = prod_dir / "pass.test"
        if out_path.exists() and not args.force:
            skipped += 1
            continue
        out_path.write_text(
            emit_fixture(production, section, desc, nsl_in, expected),
            encoding="utf-8",
        )
        written += 1

    if skipped:
        print(f"Generated {written} fixtures, skipped {skipped} existing")
    else:
        print(f"Generated {written} fixtures")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
