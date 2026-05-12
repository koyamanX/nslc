; SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
;
; grammars/treesitter/queries/highlights.scm — T8 highlight queries.
;
; Frozen by specs/010-t8-tree-sitter-grammar/contracts/highlights-coverage.contract.md §1.
; FR-007 required-minimum 20-capture set + the FR-009 dedicated
; control-terminal capture `@variable.builtin.terminal` per
; contracts/highlights-coverage.contract.md §2.
;
; Capture-ordering convention: GENERIC first, SPECIFIC last. Tree-sitter
; highlight (and `tree-sitter test`) uses the last-matching capture for
; any given node, so specific sub-captures placed later override the
; generic fallback above. This file is structured top-to-bottom from
; lexical tokens up through declaration / definition / reference
; sub-captures.
;
; Reference-site resolution: pure-syntactic patterns in this file cover
; declaration sites, operator-side LHS positions, and call-site
; positions:
;
;   - DECLARATION-SITE captures via field bindings (always precise).
;   - OPERATOR-SIDE LHS captures (`:=` → register-or-variable; `=` →
;     wire-or-output-or-variable) via the `register_transfer` /
;     `wire_or_variable_transfer` rules (precise by operator).
;   - CALL-SITE position captures (`control_call` statement-position →
;     proc; `call_expression` postfix in expression-position → func)
;     — precise by context.
;
; RHS reference resolution to a specific declaration kind (and S27
; control-terminal identifiers in expression position) is handled by
; `grammars/treesitter/queries/locals.scm` — the tree-sitter-highlight
; library walks scopes, matches `(identifier) @local.reference` against
; the nearest enclosing `@local.definition.<NAME>`, and applies
; `@<NAME>` to the reference automatically. The VS Code semantic-tokens
; provider (`editors/vscode/treesitter/highlight-provider.ts`)
; consumes the same query set at runtime for theme-side overrides.

; ============================================================
; Lexical-token captures (#10–#12)
; ============================================================

(number_literal) @number
(string_literal) @string
(line_comment) @comment
(block_comment) @comment

; The preprocessor-directive token is a whole-line directive (see
; grammar.js preprocessor_directive rule); scope it as a
; meta-preprocessor token, theme falls back via the tail.
(preprocessor_directive) @keyword.directive

; ============================================================
; #9 — @constant.macro (%IDENT% splice positions)
; ============================================================

(macro_identifier) @constant.macro

; ============================================================
; #6 — @type.builtin (storage-class keywords used as types)
; ============================================================

[
  "reg"
  "wire"
  "mem"
  "integer"
  "variable"
] @type.builtin

; ============================================================
; #5 — @keyword.storage (parameter-storage keywords)
; ============================================================

[
  "param_int"
  "param_str"
  "parameter"
] @keyword.storage

; ============================================================
; #4 — @keyword.modifier (declare-block modifiers)
;       Captured both via field binding on declare_block AND as bare
;       keyword tokens so theme fallback is consistent.
; ============================================================

[
  "interface"
  "simulation"
] @keyword.modifier

; ============================================================
; #3 — @keyword.control.flow (goto, return, finish)
; ============================================================

[
  "goto"
  "return"
  "finish"
] @keyword.control.flow

; ============================================================
; #2 — @keyword.control (alt, any, if, else, seq, for, while, generate)
; ============================================================

[
  "alt"
  "any"
  "if"
  "else"
  "seq"
  "for"
  "while"
  "generate"
] @keyword.control

; ============================================================
; #1 — @keyword (generic fallback for the remaining keyword set)
; ============================================================

[
  "declare"
  "module"
  "struct"
  "func"
  "function"
  "func_in"
  "func_out"
  "func_self"
  "proc"
  "proc_name"
  "state"
  "state_name"
  "label"
  "label_name"
  "first_state"
  "input"
  "output"
  "inout"
  "invoke"
  "m_clock"
  "p_reset"
] @keyword

; ============================================================
; #7 — @type at module / declare / struct name sites
; ============================================================

(module_block
  name: (identifier) @type)

(declare_block
  name: (identifier) @type)

(struct_declaration
  name: (identifier) @type)

; ============================================================
; FR-009 — dedicated control-terminal capture at DECLARATION sites
;   Plan-locked name: variable.builtin.terminal
;   (contracts/highlights-coverage.contract.md §2; locked by the
;   control_terminal_s27.nsl golden fixture)
;
; Control terminals include: proc_name (proc_declarator),
; func_in / func_out (control_terminal_declaration), and func_self
; (control_internal_declaration). Reference-site captures in
; expression position (S27) require binding lookup and are deferred
; to the VS Code semantic-tokens provider; see commentary at the
; top of this file.
; ============================================================

(control_terminal_declaration
  name: (identifier) @variable.builtin.terminal)

(control_internal_declaration
  name: (identifier) @variable.builtin.terminal)

; ============================================================
; #20 — @label.state at definition + reference (goto target) sites
;       (state_definition is the body declaration; state_name_declaration
;       names the state up front per spec §6; goto_statement.target +
;       first_state_declaration.target are reference sites resolved
;       syntactically without binding lookup.)
; ============================================================

(state_name_declaration
  name: (identifier) @label.state)

(state_definition
  name: (identifier) @label.state)

(first_state_declaration
  target: (identifier) @label.state)

(goto_statement
  target: (identifier) @label.state)

; ============================================================
; #16 — @function.proc at proc-definition + proc_name declaration sites
; ============================================================

(proc_declarator
  name: (identifier) @function.proc)

(proc_definition
  name: (identifier) @function.proc)

; ============================================================
; #17 — @function.func at func-definition site
;       (func_definition.name is a scoped_identifier supporting dotted
;       forms like `func ic.ready { ... }`; we capture the tail
;       identifier — the function-name proper — via the anchor `.`.)
; ============================================================

(func_definition
  name: (scoped_identifier
    (identifier) @function.func .))

; ============================================================
; #15 — @variable.memory at declaration site
; ============================================================

(memory_declaration
  name: (identifier) @variable.memory)

; ============================================================
; #14 — @variable.wire at declaration site
;       Anchored to wire_declaration so signal_declarator inside a
;       port_declaration is NOT captured as wire.
; ============================================================

(wire_declaration
  (signal_declarator
    name: (identifier) @variable.wire))

; ============================================================
; #13 — @variable.register at declaration site
; ============================================================

(register_declarator
  name: (identifier) @variable.register)

; ============================================================
; Operator-side LHS captures (precise by `:=` vs `=` discrimination)
;   - register_transfer LHS → @variable.register (the `:=` operator
;     marks the LHS as a sequential storage element; register or
;     variable — disambiguation deferred to runtime resolution).
;   - wire_or_variable_transfer LHS → @variable.wire (`=` operator
;     marks the LHS as combinational; wire, output port, or variable).
; ============================================================

(register_transfer
  lvalue: (simple_lvalue
    (scoped_identifier
      (identifier) @variable.register .)))

(wire_or_variable_transfer
  lvalue: (simple_lvalue
    (scoped_identifier
      (identifier) @variable.wire .)))

; Concat-lvalue LHS: each named target is a wire-side identifier
; (`.{tag, index, offset} = addr;` per N3); capture as @variable.wire
; for theme purposes.
(wire_or_variable_transfer
  lvalue: (concat_lvalue
    (simple_lvalue
      (scoped_identifier
        (identifier) @variable.wire .))))

; ============================================================
; Bit-select base on a bare identifier (heuristic for memory access).
; Over-fires on register bit-selects (q[5] would match) — refined at
; runtime by the VS Code semantic-tokens provider via binding lookup.
; ============================================================

(bit_select
  base: (identifier) @variable.memory)

; ============================================================
; #18 — @function.call.proc at control_call sites (statement-position
;       call). Captures the call-name tail (last identifier in the
;       callee chain).
; ============================================================

(control_call
  callee: (scoped_identifier
    (identifier) @function.call.proc .))

; ============================================================
; #19 — @function.call.func at call_expression sites (postfix call in
;       expression position).
; ============================================================

(call_expression
  function: (identifier) @function.call.func)

; ============================================================
; #11 — @keyword.system for system-task names (per N11; leading-
; underscore convention). Captured via the `name:` field on
; system_task_call which uses a closed-set choice of recognised names.
; ============================================================

(system_task_call
  name: _ @function.builtin)

(system_variable) @variable.builtin

; Anonymous tokens for _init / _delay (block + statement forms) so
; theme can route them as built-ins consistently with system_task_call.
[
  "_init"
  "_delay"
] @function.builtin
