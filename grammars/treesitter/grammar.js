// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// scripts/templates/grammar.js.template — hand-authored template for
// the T8 tree-sitter grammar. The Python generator
// (scripts/gen_treesitter_grammar.py) reads this file, splices the
// keyword block produced from include/nsl/Lex/KeywordSet.def into the
// `// %%KEYWORD_BLOCK%%` marker below, and writes the result to
// grammars/treesitter/grammar.js.
//
// Spec anchors (frozen by contracts/grammar-coverage.contract.md §1
// for nsl_lang.ebnf and §2 for nsl_pp.ebnf):
//   - nsl_lang.ebnf §1   — compilation_unit          → source_file
//   - nsl_lang.ebnf §3   — struct_declaration        → struct_declaration
//   - nsl_lang.ebnf §3.1 — top_level_parameter       → top_level_parameter
//   - nsl_lang.ebnf §4   — declare_block             → declare_block
//   - nsl_lang.ebnf §5   — module_block              → module_block
//   - nsl_lang.ebnf §6   — internal-structure        → register_declaration / wire_declaration / memory_declaration / proc_name_declaration / state_name_declaration
//   - nsl_lang.ebnf §7   — definitions               → func_definition / proc_definition / state_definition
//   - nsl_lang.ebnf §8   — action statements         → par_block / alt_block / any_block / seq_block / if_statement / for_statement / while_statement / generate_block
//   - nsl_lang.ebnf §9   — atomic actions            → transfer_action / control_call / finish_action / system_task_call
//   - nsl_lang.ebnf §10  — system tasks              → system_task_call
//   - nsl_lang.ebnf §11  — expressions               → _expression / sign_extend_expr / zero_extend_expr / bit_slice / concat_expression / dot_aggregate / reduction_op / bitwise_binary_op / proc_method_access / conditional_expression
//   - nsl_lang.ebnf §12  — width / constant exprs    → width_expr / constant_expr
//   - nsl_lang.ebnf §13  — lexical                   → identifier / number_literal / string_literal / macro_identifier
//   - nsl_lang.ebnf §14  — whitespace + comments     → line_comment / block_comment (extras)
//   - nsl_lang.ebnf §15  — reserved keywords         → _keyword (GENERATED from KeywordSet.def)
//   - nsl_pp.ebnf  §2    — preprocessor directives   → preprocessor_directive
//   - nsl_pp.ebnf  §4    — %IDENT% macro splice      → macro_identifier
//
// At Phase 2 (T009/T010/T011) only the lexical-level scaffolding +
// keyword block + a token-permissive `source_file` rule are present;
// the §3–§11 productions land in Phase 3 (US1: T023–T030) editing
// this same template. After each substantive edit, the regenerator
// pipeline is:
//
//   1. python3 scripts/gen_treesitter_grammar.py   (template + KeywordSet.def → grammar.js)
//   2. cd grammars/treesitter && npx tree-sitter generate   (grammar.js → parser.c, grammar.json, node-types.json)
//
// Per Constitution Principle V (deterministic): given the same input
// (this template + KeywordSet.def) the generator produces byte-
// identical grammar.js across runs. The `treesitter-grammar-regen-diff`
// CI sub-step (T036) gates this.
//
// THIS FILE IS THE TEMPLATE — do not hand-edit grammars/treesitter/
// grammar.js directly. Edit this template and re-run the generator.

module.exports = grammar({
  name: 'nsl',

  // Skip whitespace and comments anywhere a token boundary is allowed.
  // Per nsl_lang.ebnf §14 / contracts/grammar-coverage.contract.md §1
  // rows for `line_comment` and `block_comment`.
  extras: $ => [
    /\s+/,
    $.line_comment,
    $.block_comment,
  ],

  // External tokens (none yet). Reserved for a hand-rolled scanner if
  // needed later (e.g. for nsl_lang.ebnf N3 `.{` two-character
  // lookahead) — research.md §3 prefers tree-sitter's native conflicts
  // resolution where possible.
  externals: $ => [],

  // Tree-sitter conflicts (none yet). Will be populated in Phase 3
  // (US1) as productions are added; see contracts/grammar-coverage.contract.md
  // §3 for the parser-note disambiguation strategy (N1, N2, N3, N5, N6).
  conflicts: $ => [],

  // The hidden `_keyword` rule below is generated from
  // include/nsl/Lex/KeywordSet.def. Marking the splice point.
  word: $ => $.identifier,

  rules: {
    // ----------------------------------------------------------------
    // §1 — compilation_unit (root rule; tree-sitter convention names
    // the root `source_file`). Token-permissive at Phase 2; refined to
    // a strict choice over §3–§7 top-level items in Phase 3 (US1 T024).
    // ----------------------------------------------------------------
    source_file: $ => repeat($._top_level_token),

    _top_level_token: $ => choice(
      $.identifier,
      $.number_literal,
      $.string_literal,
      $.macro_identifier,
      $._keyword,
      // Operators and punctuation are surfaced as anonymous tokens
      // by the productions that consume them. Phase 2's permissive
      // root accepts any single character not handled above.
      /[^\s]/,
    ),

    // ----------------------------------------------------------------
    // §13 — lexical tokens. Frozen at Phase 2; future edits adjust the
    // regex shape (e.g. Verilog-sized number literal forms with
    // Z/X/U markers in Phase 3 T023).
    // ----------------------------------------------------------------
    identifier: $ => /[A-Za-z][A-Za-z0-9_]*/,

    // Phase-2 stub: accepts decimal, hex, binary, octal. The five
    // forms (incl. Verilog-sized + Z/X/U markers) land in T023.
    number_literal: $ => choice(
      /0[xX][0-9a-fA-F_]+/,
      /0[bB][01_]+/,
      /0[oO][0-7_]+/,
      /\d[\d_]*/,
    ),

    // §13 lines 761–770 — string literal with backslash escapes.
    string_literal: $ => seq(
      '"',
      repeat(choice(
        /[^"\\\n]/,
        /\\./,
      )),
      '"',
    ),

    // nsl_pp.ebnf §4 lines 312–343 — %IDENT% macro splice form.
    macro_identifier: $ => /%[A-Za-z_][A-Za-z0-9_]*%/,

    // ----------------------------------------------------------------
    // §14 — comments (consumed via `extras`). Block comment is
    // non-nestable per nsl_lang.ebnf §14.
    // ----------------------------------------------------------------
    line_comment: $ => token(seq('//', /[^\n]*/)),
    block_comment: $ => token(seq('/*', /[^*]*\*+([^/*][^*]*\*+)*/, '/')),

    // ----------------------------------------------------------------
    // §15 — reserved keywords. The `_keyword` rule below is GENERATED
    // by scripts/gen_treesitter_grammar.py from KeywordSet.def. The
    // marker BELOW is replaced verbatim by the generator; do not
    // hand-edit the resulting block in grammars/treesitter/grammar.js.
    // ----------------------------------------------------------------
    _keyword: $ => token(choice(
      'alt',
      'any',
      'declare',
      'finish',
      'for',
      'func',
      'func_in',
      'func_out',
      'func_self',
      'generate',
      'goto',
      'inout',
      'input',
      'integer',
      'interface',
      'label',
      'label_name',
      'm_clock',
      'mem',
      'module',
      'output',
      'p_reset',
      'proc',
      'proc_name',
      'reg',
      'seq',
      'state',
      'state_name',
      'variable',
      'while',
      'wire',
      'else',
      'first_state',
      'function',
      'if',
      'invoke',
      'param_int',
      'param_str',
      'parameter',
      'return',
      'simulation',
      'struct',
    ))
  },
});
