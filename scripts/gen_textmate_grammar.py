#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""scripts/gen_textmate_grammar.py — generate the canonical T1
TextMate grammar JSON from `include/nsl/Lex/KeywordSet.def`
plus the inline category-mapping table below.

Per `specs/009-t1-textmate-grammar/data-model.md` §1.2 (token-
category mapping) and `contracts/grammar-coverage.contract.md`
§§1, 4-7 (frozen production-to-scope bindings).

This script is **the** source of truth for the keyword block in
`grammars/textmate/nsl.tmLanguage.json`. The keyword set itself
is mirrored mechanically from `KeywordSet.def` (which is also
consumed by `lib/Lex/KeywordSet.cpp`, the TokenKind enum, and
`scripts/gen_keyword_fixtures.py`); the category mapping below
encodes the design decisions of `nsl_tooling_design.md §4.1`.

Per Constitution Principle V (deterministic): given the same
input `KeywordSet.def`, this script produces byte-identical
output across runs. Iteration order is source order
(`KeywordSet.def` line order → JSON pattern order); no
hash-derived ordering, no timestamps.

Per Constitution Principle VII (spec ↔ design coupling): if a
new `KEYWORD(...)` row in `KeywordSet.def` has no entry in the
`KEYWORD_CATEGORY` table below, this script raises with a
localised message identifying the missing keyword. CI gates a
regenerate-and-diff check (per `data-model.md` §3 / §4) so spec
↔ grammar drift is mechanically impossible.

Usage:
  python3 scripts/gen_textmate_grammar.py [--check]

Options:
  --check   Generate to a temp file and diff against the
            committed grammar; exit non-zero if they differ.
            Used by CI stage 2 (static-checks).
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

# -----------------------------------------------------------------------------
# Repository layout
# -----------------------------------------------------------------------------

REPO_ROOT = Path(__file__).resolve().parent.parent
KEYWORD_SET_DEF = REPO_ROOT / "include" / "nsl" / "Lex" / "KeywordSet.def"
GRAMMAR_OUT = REPO_ROOT / "grammars" / "textmate" / "nsl.tmLanguage.json"
# Mirror copy under `editors/vscode/syntaxes/`. Per CodeRabbit
# review on PR #13: a symlink fails on Windows / zip-archive
# extraction (the path becomes a literal string instead of a
# resolved file). We materialise both files and let the stage-2
# `tooling-grammar-mirror` byte-equality check enforce sync.
GRAMMAR_MIRROR = REPO_ROOT / "editors" / "vscode" / "syntaxes" / "nsl.tmLanguage.json"

# -----------------------------------------------------------------------------
# Category mapping — data-model.md §1.2
# -----------------------------------------------------------------------------
# Maps the literal source-text spelling (the second arg of each
# KEYWORD(...) row in KeywordSet.def) to the TextMate scope
# sub-name. Values use the abbreviated category labels; the
# emitter expands them to the full `keyword.<...>.nsl` /
# `storage.<...>.nsl` form per data-model §1.2 + grammar-
# coverage.contract §1.

KEYWORD_CATEGORY: dict[str, str] = {
    # keyword.declaration.nsl
    "declare":     "declaration",
    "module":      "declaration",
    "struct":      "declaration",
    # keyword.control.block.nsl
    "alt":         "control_block",
    "any":         "control_block",
    "if":          "control_block",
    "else":        "control_block",
    "seq":         "control_block",
    "for":         "control_block",
    "while":       "control_block",
    "generate":    "control_block",
    # keyword.control.flow.nsl
    "goto":        "control_flow",
    "finish":      "control_flow",
    "return":      "control_flow",
    # keyword.modifier.nsl
    "interface":   "modifier",
    "simulation":  "modifier",
    # storage.type.register.nsl
    "reg":         "storage_register",
    # storage.type.wire.nsl
    "wire":        "storage_wire",
    # storage.type.memory.nsl
    "mem":         "storage_memory",
    # storage.type.integer.nsl
    "integer":     "storage_integer",
    "variable":    "storage_integer",
    # storage.type.param.nsl
    "param_int":   "storage_param",
    "param_str":   "storage_param",
    "parameter":   "storage_param",
    # storage.type.control.nsl
    "func":        "storage_control",
    "function":    "storage_control",
    "func_in":     "storage_control",
    "func_out":    "storage_control",
    "func_self":   "storage_control",
    "proc":        "storage_control",
    "proc_name":   "storage_control",
    "state":       "storage_control",
    "state_name":  "storage_control",
    "first_state": "storage_control",
    "label_name":  "storage_control",
    "label":       "storage_control",
    "invoke":      "storage_control",
    # storage.modifier.direction.nsl  (T1-introduced sub-scope; see data-model §1.2)
    "input":       "port_direction",
    "output":      "port_direction",
    "inout":       "port_direction",
    # support.type.clock.nsl
    "m_clock":     "support_clock",
    "p_reset":     "support_clock",
}

# Maps category label -> full TextMate scope name. Per
# grammar-coverage.contract §1.
SCOPE_FOR_CATEGORY: dict[str, str] = {
    "declaration":      "keyword.declaration.nsl",
    "control_block":    "keyword.control.block.nsl",
    "control_flow":     "keyword.control.flow.nsl",
    "modifier":         "keyword.modifier.nsl",
    "storage_register": "storage.type.register.nsl",
    "storage_wire":     "storage.type.wire.nsl",
    "storage_memory":   "storage.type.memory.nsl",
    "storage_integer":  "storage.type.integer.nsl",
    "storage_param":    "storage.type.param.nsl",
    "storage_control":  "storage.type.control.nsl",
    "port_direction":   "storage.modifier.direction.nsl",
    "support_clock":    "support.type.clock.nsl",
}

# Built-in `_`-prefix system-name closed set per data-model.md §1.3
# and grammar-coverage.contract.md §2. The list is FROZEN at T1;
# modifying it requires both contract + data-model amendments
# (Principle VII).
SYSTEM_FUNCTIONS: list[str] = [
    "_display", "_monitor", "_write", "_finish", "_stop",
    "_readmemh", "_readmemb", "_init", "_delay",
]
SYSTEM_VARIABLES: list[str] = [
    "_random", "_time",
]


# -----------------------------------------------------------------------------
# KeywordSet.def parser
# -----------------------------------------------------------------------------

_KEYWORD_RE = re.compile(r'^KEYWORD\(\s*\w+\s*,\s*"([^"]+)"\s*\)')


def parse_keyword_set(path: Path) -> list[str]:
    """Return the spellings (literal source text) of every
    KEYWORD(...) row in source order."""
    spellings: list[str] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        m = _KEYWORD_RE.match(line.strip())
        if m:
            spellings.append(m.group(1))
    return spellings


# -----------------------------------------------------------------------------
# Coverage check
# -----------------------------------------------------------------------------

def check_coverage(spellings: list[str]) -> None:
    """Raise if any KeywordSet.def spelling has no
    KEYWORD_CATEGORY entry (or vice versa)."""
    set_def = set(spellings)
    set_map = set(KEYWORD_CATEGORY.keys())

    missing_in_map = set_def - set_map
    if missing_in_map:
        raise RuntimeError(
            f"KeywordSet.def has spellings without category-mapping "
            f"entries: {sorted(missing_in_map)}. Add them to "
            f"KEYWORD_CATEGORY in scripts/gen_textmate_grammar.py "
            f"per data-model.md §1.2."
        )

    orphan_in_map = set_map - set_def
    if orphan_in_map:
        raise RuntimeError(
            f"KEYWORD_CATEGORY has spellings not present in "
            f"KeywordSet.def: {sorted(orphan_in_map)}. Either add "
            f"them to KeywordSet.def or remove them from the map."
        )


# -----------------------------------------------------------------------------
# Grammar emission
# -----------------------------------------------------------------------------

SPDX_COMMENT_TOP = (
    "SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception. "
    "JSON has no comment syntax; the SPDX header rides on this "
    "top-level key per specs/009-t1-textmate-grammar/research.md "
    "§2 (precedent: .github/branch-protection.json). This file "
    "is GENERATED by scripts/gen_textmate_grammar.py from "
    "include/nsl/Lex/KeywordSet.def — do not hand-edit; "
    "regenerate via `python3 scripts/gen_textmate_grammar.py`."
)


def _category_groups(spellings: list[str]) -> dict[str, list[str]]:
    """Group spellings by category label preserving source order
    within each category (Principle V determinism)."""
    groups: dict[str, list[str]] = {}
    for sp in spellings:
        cat = KEYWORD_CATEGORY[sp]
        groups.setdefault(cat, []).append(sp)
    return groups


def _build_keyword_repository(spellings: list[str]) -> dict[str, dict]:
    """Build a `repository` block with one entry per category.

    Each entry is a single-pattern object whose `match` is a
    `\\b(spelling1|spelling2|…)\\b` alternation over the spellings
    in that category. Per data-model §1.2 + grammar-coverage.contract §1.
    """
    repo: dict[str, dict] = {}
    groups = _category_groups(spellings)
    # Iterate over SCOPE_FOR_CATEGORY for deterministic order
    # (the dict keeps insertion order in Python 3.7+).
    for cat, scope in SCOPE_FOR_CATEGORY.items():
        if cat not in groups:
            # No spellings for this category yet (defensive; the
            # coverage check ensures every category in
            # KEYWORD_CATEGORY has at least one entry).
            continue
        # Sort within category by length DESC then alphabetic so
        # longer spellings (`func_in`) match before shorter
        # prefixes (`func`). This is required for Oniguruma
        # `\b(...|...)\b` matching since the leading `\b` would
        # otherwise allow `func` to win against `func_in`.
        ordered = sorted(groups[cat], key=lambda s: (-len(s), s))
        alt = "|".join(re.escape(sp) for sp in ordered)
        repo[f"keyword-{cat}"] = {
            "name": scope,
            "match": rf"\b({alt})\b",
        }
    return repo


def _build_system_name_repository() -> dict[str, dict]:
    """Per data-model §1.3 + grammar-coverage.contract §2:
    closed set of `_`-prefix names. Two repository entries:
    one for system functions, one for system variables.

    Pattern uses `\\b` boundaries — note Oniguruma `\\b` does
    not break on `_`, so `_display` matches as a whole token.
    """
    funcs = sorted(SYSTEM_FUNCTIONS, key=lambda s: (-len(s), s))
    vars_ = sorted(SYSTEM_VARIABLES, key=lambda s: (-len(s), s))
    return {
        "system-function": {
            "name": "support.function.system.nsl",
            "match": rf"\b({'|'.join(re.escape(f) for f in funcs)})\b",
        },
        "system-variable": {
            "name": "support.variable.system.nsl",
            "match": rf"\b({'|'.join(re.escape(v) for v in vars_)})\b",
        },
    }


def _build_numeric_repository() -> dict[str, dict]:
    """Per data-model §1.4 + grammar-coverage.contract §3.

    Pattern ordering matters: Verilog-sized must match before
    bare decimal so `8'hFF` does not fragment into `8` + `'hFF`.
    The order is enforced by listing the `include` references in
    the parent `numerics` group (see _build_root_patterns).
    """
    return {
        "number-verilog": {
            "name": "constant.numeric.verilog.nsl",
            # \d+ width, then ' base char, then digits with markers + separators
            "match": r"\b\d+'[bBoOdDhH][0-9a-fA-FxXzZuU_]+",
        },
        "number-hex": {
            "name": "constant.numeric.hex.nsl",
            "match": r"\b0[xX][0-9a-fA-F_]+",
        },
        "number-binary": {
            "name": "constant.numeric.binary.nsl",
            "match": r"\b0[bB][01_]+",
        },
        "number-octal": {
            "name": "constant.numeric.octal.nsl",
            "match": r"\b0[oO][0-7_]+",
        },
        "number-decimal": {
            "name": "constant.numeric.decimal.nsl",
            "match": r"\b\d[\d_]*",
        },
    }


def _build_comment_string_repository() -> dict[str, dict]:
    """Per data-model §1.8 (comments) + §1.9 (strings + escapes)
    + grammar-coverage.contract §4. Block comments are non-
    nestable per nsl_lang.ebnf §14; the `begin`/`end` rule
    handles non-nesting natively (TextMate does not support
    nested begin/end without explicit recursion).
    """
    return {
        "comment-line": {
            "name": "comment.line.double-slash.nsl",
            "match": r"//.*$",
        },
        "comment-block": {
            "name": "comment.block.nsl",
            "begin": r"/\*",
            "end": r"\*/",
        },
        "string-double": {
            "name": "string.quoted.double.nsl",
            "begin": r"\"",
            "end": r"\"",
            "patterns": [
                {
                    "name": "constant.character.escape.nsl",
                    "match": r"\\.",
                },
            ],
        },
    }


def _build_operator_repository() -> dict[str, dict]:
    """Per data-model §1.5 + grammar-coverage.contract §5.

    Multi-character operators are listed before single-character
    in the `match` alternation so `==` wins over `=` etc.
    Each category gets its own repository entry.
    """
    # Within each category, list multi-char before single-char.
    return {
        "operator-arithmetic": {
            "name": "keyword.operator.arithmetic.nsl",
            "match": r"\+\+|--|\+|-|\*",
        },
        "operator-bitwise": {
            "name": "keyword.operator.bitwise.nsl",
            "match": r"&|\||\^|~",
        },
        "operator-shift": {
            "name": "keyword.operator.shift.nsl",
            "match": r"<<|>>",
        },
        "operator-comparison": {
            "name": "keyword.operator.comparison.nsl",
            "match": r"==|!=|<=|>=|<|>",
        },
        "operator-logical": {
            "name": "keyword.operator.logical.nsl",
            "match": r"&&|\|\||!",
        },
        "operator-assignment": {
            "name": "keyword.operator.assignment.nsl",
            "match": r":=|=",
        },
        "operator-extension": {
            "name": "keyword.operator.extension.nsl",
            "match": r"#|'",
        },
    }


def _build_directive_repository() -> dict[str, dict]:
    """Per data-model §1.6 + grammar-coverage.contract §6.

    Line-start anchored (^\\s*#... \\b) so a sign-extend `#` in
    expression position does NOT match as a directive. Per
    FR-021 best-effort regex disambiguation; T8 (tree-sitter)
    refines.

    NOTE: TextMate's regex engine sees each line as a fresh
    input, so `^` is line-start. The literal `#` after optional
    whitespace pins the directive prefix.
    """
    return {
        "directive-preprocessor": {
            "name": "keyword.directive.preprocessor.nsl",
            "match": r"^\s*#(include|define|undef|ifdef|ifndef|if|else|endif|line)\b",
        },
    }


def _build_macro_repository() -> dict[str, dict]:
    """Per data-model §1.7 + grammar-coverage.contract §7.
    %IDENT% macro splice form. Distinct scope from identifier
    and keyword scopes so the preprocessor seam is visible.
    """
    return {
        "macro-reference": {
            "name": "variable.other.macro.nsl",
            "match": r"%[A-Za-z_][A-Za-z0-9_]*%",
        },
    }


def _build_root_patterns(repo: dict[str, dict]) -> list[dict]:
    """Top-level `patterns` array — references repository entries
    in the order required by first-match-wins semantics.

    Ordering rationale (data-model §§1.4, 1.5, 1.6):
      1. Comments + strings FIRST so embedded keyword-like tokens
         inside them don't match keyword/storage scopes.
      2. Directives (line-start anchored) before extension-`#`
         operator so `#line` wins over operator `#`.
      3. Macro reference (`%IDENT%`) — distinctive form, before
         operators which include `%` is not in (no overlap).
      4. Numerics in the order Verilog-sized → hex → binary →
         octal → decimal (so `8'hFF` doesn't fragment).
      5. Keywords (sub-categorised). Order within the keyword
         block doesn't matter much because each category's
         alternation is anchored by `\\b…\\b`.
      6. System names (`_display`, `_random`, …) — closed set,
         distinct from regular identifiers.
      7. Operators. Multi-char alternation handles ordering
         within each category.
    """
    refs: list[dict] = []

    # 1. Comments + strings first (begin/end rules so embedded
    # tokens don't match other scopes).
    if "comment-line" in repo:
        refs.append({"include": "#comment-line"})
    if "comment-block" in repo:
        refs.append({"include": "#comment-block"})
    if "string-double" in repo:
        refs.append({"include": "#string-double"})

    # 2. Preprocessor directives (line-start) before extension `#`.
    if "directive-preprocessor" in repo:
        refs.append({"include": "#directive-preprocessor"})

    # 3. Macro references.
    if "macro-reference" in repo:
        refs.append({"include": "#macro-reference"})

    # 4. Numerics — first-match-wins ordering.
    for key in (
        "number-verilog", "number-hex", "number-binary",
        "number-octal", "number-decimal",
    ):
        if key in repo:
            refs.append({"include": f"#{key}"})

    # 5. Keywords (sub-categorised). Iterate SCOPE_FOR_CATEGORY
    # for deterministic order.
    for cat in SCOPE_FOR_CATEGORY:
        key = f"keyword-{cat}"
        if key in repo:
            refs.append({"include": f"#{key}"})

    # 6. System names.
    if "system-function" in repo:
        refs.append({"include": "#system-function"})
    if "system-variable" in repo:
        refs.append({"include": "#system-variable"})

    # 7. Operators. Multi-character variants must win over
    # single-character — that means `operator-logical` (which
    # owns `&&`, `||`) must precede `operator-bitwise` (which
    # owns `&`, `|`); `operator-comparison` (which owns `<=`,
    # `>=`, `<<` would be ambiguous so shift goes first) must
    # precede the single-char `<`/`>` would be in the same
    # category. Order:
    #   shift (`<<`, `>>`) → comparison (`==`/`!=`/`<=`/`>=`/`<`/`>`)
    #     → logical (`&&`/`||`/`!`) → bitwise (`&`/`|`/`^`/`~`)
    #     → assignment (`:=`/`=`) → arithmetic (`++`/`--`/`+`/`-`/`*`)
    #     → extension (`#`/`'`).
    for key in (
        "operator-shift",
        "operator-comparison",
        "operator-logical",
        "operator-bitwise",
        "operator-assignment",
        "operator-arithmetic",
        "operator-extension",
    ):
        if key in repo:
            refs.append({"include": f"#{key}"})

    return refs


def emit_grammar(spellings: list[str]) -> dict:
    """Build the TextMate grammar dict.

    Populates the `repository` and `patterns` arrays from
    `KeywordSet.def` (keyword block) plus inline pattern
    templates for system names, numerics, comments, strings,
    operators, directives, and macro references.
    """
    repo: dict[str, dict] = {}
    repo.update(_build_comment_string_repository())
    repo.update(_build_directive_repository())
    repo.update(_build_macro_repository())
    repo.update(_build_numeric_repository())
    repo.update(_build_keyword_repository(spellings))
    repo.update(_build_system_name_repository())
    repo.update(_build_operator_repository())

    grammar = {
        "_comment_top": SPDX_COMMENT_TOP,
        "name": "NSL",
        "scopeName": "source.nsl",
        "fileTypes": ["nsl", "nslh", "inc"],
        "patterns": _build_root_patterns(repo),
        "repository": repo,
    }
    return grammar


def render_grammar(grammar: dict) -> str:
    """Render the grammar to a deterministic JSON string. Ends
    with a trailing newline for POSIX-text-file convention."""
    return json.dumps(grammar, indent=2, ensure_ascii=False) + "\n"


# -----------------------------------------------------------------------------
# Main
# -----------------------------------------------------------------------------

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check", action="store_true",
        help="Generate to a temp file and diff against the "
             "committed grammar; exit non-zero if they differ.",
    )
    args = parser.parse_args(argv)

    spellings = parse_keyword_set(KEYWORD_SET_DEF)
    check_coverage(spellings)
    grammar = emit_grammar(spellings)
    rendered = render_grammar(grammar)

    if args.check:
        rc = 0
        for path in (GRAMMAR_OUT, GRAMMAR_MIRROR):
            committed = path.read_text(encoding="utf-8") if path.exists() else ""
            if rendered != committed:
                sys.stderr.write(
                    f"[gen_textmate_grammar] {path} is stale.\n"
                    f"  Run: python3 scripts/gen_textmate_grammar.py\n"
                    f"  Then commit the regenerated file.\n"
                )
                rc = 1
        return rc

    for path in (GRAMMAR_OUT, GRAMMAR_MIRROR):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(rendered, encoding="utf-8")
    n_patterns = len(grammar["patterns"])
    n_repo = len(grammar["repository"])
    sys.stdout.write(
        f"[gen_textmate_grammar] wrote {GRAMMAR_OUT} "
        f"({len(spellings)} keywords, {n_patterns} root patterns, "
        f"{n_repo} repository entries)\n"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
