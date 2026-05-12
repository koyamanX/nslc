#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""scripts/gen_treesitter_grammar.py — generate the canonical T8
tree-sitter grammar.js from `scripts/templates/grammar.js.template`
plus the keyword block from `include/nsl/Lex/KeywordSet.def`.

Per `specs/010-t8-tree-sitter-grammar/contracts/grammar-coverage.
contract.md` §1 row for nsl_lang.ebnf §15 — the `_keyword` token
rule is GENERATED from `KeywordSet.def`, mirroring the T1 precedent
where `gen_textmate_grammar.py` consumes the same source.

Per Constitution Principle V (deterministic): given the same input
(template + KeywordSet.def), this script produces byte-identical
grammar.js across runs. Iteration order is `KeywordSet.def` source
order; no hash-derived ordering, no timestamps.

Per Constitution Principle II (single-source-of-truth): the keyword
spelling list is owned by `KeywordSet.def`. This generator and
`gen_textmate_grammar.py` are mechanical mirrors of that source.

Usage:
  python3 scripts/gen_treesitter_grammar.py [--check]

Options:
  --check   Generate to memory and diff against the committed
            grammar.js; exit non-zero if they differ. Used by CI
            stage 2 sub-step `treesitter-grammar-regen-diff`.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# -----------------------------------------------------------------------------
# Repository layout
# -----------------------------------------------------------------------------

REPO_ROOT = Path(__file__).resolve().parent.parent
KEYWORD_SET_DEF = REPO_ROOT / "include" / "nsl" / "Lex" / "KeywordSet.def"
TEMPLATE = REPO_ROOT / "scripts" / "templates" / "grammar.js.template"
GRAMMAR_OUT = REPO_ROOT / "grammars" / "treesitter" / "grammar.js"

# The literal marker the template reserves for the keyword block.
# Located on a line of its own; the generator replaces the entire
# line (including its leading indent) with the rendered block.
KEYWORD_MARKER = "// %%KEYWORD_BLOCK%%"

# -----------------------------------------------------------------------------
# KeywordSet.def parser
# -----------------------------------------------------------------------------

_KEYWORD_RE = re.compile(r'^KEYWORD\(\s*\w+\s*,\s*"([^"]+)"\s*\)')


def parse_keyword_set(path: Path) -> list[str]:
    """Return the spellings (literal source text) of every
    KEYWORD(...) row in source order. Same regex as
    gen_textmate_grammar.py for parity."""
    spellings: list[str] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        m = _KEYWORD_RE.match(line.strip())
        if m:
            spellings.append(m.group(1))
    return spellings


# -----------------------------------------------------------------------------
# Keyword block emission
# -----------------------------------------------------------------------------

def render_keyword_block(spellings: list[str], indent: str) -> str:
    """Render the `_keyword` rule as a tree-sitter `choice(...)` over
    the keyword spellings.

    Spelling order matches `KeywordSet.def` source order (Principle V
    determinism). Each entry is emitted as a JavaScript single-quoted
    string literal; the closed keyword set has no embedded quotes or
    backslashes so escape handling is a hard no-op (assertion below).

    The leading `_` on `_keyword` makes it a tree-sitter HIDDEN rule:
    matches contribute tokens to the parse tree but the rule name
    itself does not appear as a node. This matches the contract row
    "reserved keywords | `_keyword` (token rule)". Wrapped with
    `token(...)` so consumers see a single anonymous keyword token
    rather than a sub-rule node.
    """
    for sp in spellings:
        if "'" in sp or "\\" in sp:
            raise RuntimeError(
                f"Unexpected character in KeywordSet.def spelling "
                f"{sp!r}: keyword block emitter does not handle "
                f"quote or backslash escapes (the spelling closed "
                f"set is ASCII-letter-and-underscore-only)."
            )

    inner_indent = indent + "  "
    quoted = [f"'{sp}'" for sp in spellings]
    body = f",\n{inner_indent}".join(quoted)
    return (
        f"{indent}_keyword: $ => token(choice(\n"
        f"{inner_indent}{body},\n"
        f"{indent}))"
    )


# -----------------------------------------------------------------------------
# Template splicing
# -----------------------------------------------------------------------------

def splice_template(template: str, keyword_block: str) -> str:
    """Replace the (single) marker line with the pre-indented keyword
    block.

    The marker line in the template is::

        <indent>// %%KEYWORD_BLOCK%%

    The keyword block already carries its own indentation (computed
    from the marker's indent in `render_grammar_js`). The marker line
    is replaced WITHOUT a trailing comma — the template places the
    marker as the last entry inside the `rules` object literal, and
    JavaScript object literals do not require a trailing comma.
    Phase-3 template edits MUST preserve this invariant.
    """
    lines = template.splitlines(keepends=False)
    out: list[str] = []
    found = 0
    for line in lines:
        if line.lstrip() == KEYWORD_MARKER:
            out.append(keyword_block)
            found += 1
        else:
            out.append(line)

    if found != 1:
        raise RuntimeError(
            f"Template {TEMPLATE} has {found} `{KEYWORD_MARKER}` "
            f"marker lines; expected exactly 1."
        )

    # POSIX-text-file convention: trailing newline.
    return "\n".join(out) + "\n"


def render_grammar_js(spellings: list[str]) -> str:
    """Return the rendered grammar.js content.

    Reads the template + KeywordSet.def, computes the marker indent,
    renders the keyword block at that indent, splices, returns.
    """
    template = TEMPLATE.read_text(encoding="utf-8")

    # Find the marker indent (need it before render_keyword_block).
    marker_indent = ""
    for line in template.splitlines():
        stripped = line.lstrip()
        if stripped == KEYWORD_MARKER:
            marker_indent = line[: len(line) - len(stripped)]
            break

    keyword_block = render_keyword_block(spellings, marker_indent)
    return splice_template(template, keyword_block)


# -----------------------------------------------------------------------------
# Main
# -----------------------------------------------------------------------------

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check", action="store_true",
        help="Render to memory and diff against the committed "
             "grammars/treesitter/grammar.js; exit non-zero if they "
             "differ. Used by CI stage-2 sub-step "
             "`treesitter-grammar-regen-diff`.",
    )
    args = parser.parse_args(argv)

    spellings = parse_keyword_set(KEYWORD_SET_DEF)
    rendered = render_grammar_js(spellings)

    if args.check:
        committed = GRAMMAR_OUT.read_text(encoding="utf-8") if GRAMMAR_OUT.exists() else ""
        if rendered != committed:
            sys.stderr.write(
                f"[gen_treesitter_grammar] {GRAMMAR_OUT} is stale.\n"
                f"  Run: python3 scripts/gen_treesitter_grammar.py\n"
                f"  Then commit the regenerated file.\n"
            )
            return 1
        return 0

    GRAMMAR_OUT.parent.mkdir(parents=True, exist_ok=True)
    GRAMMAR_OUT.write_text(rendered, encoding="utf-8")
    sys.stdout.write(
        f"[gen_treesitter_grammar] wrote {GRAMMAR_OUT} "
        f"({len(spellings)} keywords)\n"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
