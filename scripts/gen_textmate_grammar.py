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

**T005 skeleton state**: at this point in the milestone, the
script emits a TextMate grammar with an empty `patterns` array
— enough to be valid JSON consumed by `vscode-tmgrammar-test`
but assigning no scopes to anything. T016-T020 (Phase 3 of
tasks.md) progressively add pattern groups for keywords,
system names, numerics, comments/strings, and operators. T034
+ T035 (Phase 5) add directives and macros. The empty-grammar
skeleton is what makes the Phase 3 fixture tests observe
FAILING per Constitution Principle VIII.

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
# Grammar emission — T005 skeleton state (empty patterns)
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


def emit_grammar(spellings: list[str]) -> dict:
    """Build the TextMate grammar dict.

    **T005 skeleton state**: the `patterns` array is empty;
    pattern groups will be added by T016-T020 (Phase 3) and
    T034-T035 (Phase 5) per tasks.md. The skeleton is enough to
    validate as JSON, register the scope name `source.nsl`, and
    declare file extensions — but it assigns no scope to any
    token. Phase 3 fixture tests will observe FAILING against
    this skeleton per Constitution Principle VIII (TDD).
    """
    # `spellings` is parsed and coverage-checked here even
    # though the skeleton emits no patterns; the check_coverage
    # call earlier in main() establishes the spec-coupling
    # invariant from T005 onward.
    del spellings  # consumed for coverage check; not yet emitted

    grammar = {
        "_comment_top": SPDX_COMMENT_TOP,
        "name": "NSL",
        "scopeName": "source.nsl",
        "fileTypes": ["nsl", "nslh", "inc"],
        "patterns": [],
        "repository": {},
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
        committed = GRAMMAR_OUT.read_text(encoding="utf-8") if GRAMMAR_OUT.exists() else ""
        if rendered != committed:
            sys.stderr.write(
                f"[gen_textmate_grammar] {GRAMMAR_OUT} is stale.\n"
                f"  Run: python3 scripts/gen_textmate_grammar.py\n"
                f"  Then commit the regenerated file.\n"
            )
            return 1
        return 0

    GRAMMAR_OUT.parent.mkdir(parents=True, exist_ok=True)
    GRAMMAR_OUT.write_text(rendered, encoding="utf-8")
    sys.stdout.write(
        f"[gen_textmate_grammar] wrote {GRAMMAR_OUT} "
        f"(skeleton state: {len(spellings)} keywords coverage-"
        f"checked, 0 patterns emitted; populated in T016-T020, "
        f"T034-T035)\n"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
