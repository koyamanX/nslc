; SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
;
; grammars/treesitter/queries/locals.scm — T8 scope + binding queries.
;
; Tree-sitter-highlight (used by `tree-sitter highlight` AND
; `tree-sitter test --highlight`) consumes locals.scm to resolve
; reference identifiers to their nearest enclosing definition. When a
; `(identifier) @local.reference` resolves to a definition annotated
; with `@local.definition.<NAME>`, the highlight engine applies
; `@<NAME>` to the reference site automatically — without the
; reference position needing its own pattern in `highlights.scm`.
;
; This file therefore CLOSES the two assertions documented as
; "RHS-reference-resolution-dependent" in
; `grammars/treesitter/queries/highlights.scm`:
;   (a) reg_vs_wire.nsl col 4 of `w = q;` — `q` (RHS of `=`)
;       resolves to the `register_declarator q[8]` definition in the
;       enclosing module_block scope → `@variable.register` applied.
;   (b) control_terminal_s27.nsl col 11 of `busy = work;` — `work`
;       (RHS of `=`) resolves to the `proc_declarator work` definition
;       in the same scope → `@variable.builtin.terminal` applied.
;
; If the dev-container `tree-sitter test` run shows either assertion
; still fails after this file lands, the iteration step (T034) is
; the appropriate place to refine the scope set or the reference-
; pattern coverage. See specs/010-t8-tree-sitter-grammar/contracts/
; highlights-coverage.contract.md §1 for the locked capture name set
; this file transparently extends with reference-site resolution.
;
; ============================================================
; Scopes — every block that introduces a fresh binding namespace.
; ============================================================

(module_block) @local.scope
(declare_block) @local.scope
(proc_definition) @local.scope
(state_definition) @local.scope
(func_definition) @local.scope
(seq_block) @local.scope
(parallel_block) @local.scope
(for_block) @local.scope
(while_block) @local.scope
(alt_block) @local.scope
(any_block) @local.scope
(init_block) @local.scope
(structural_generate) @local.scope

; ============================================================
; Definitions — keyed by the HIGHLIGHT capture name suffix.
;
; When a reference resolves to one of these, the highlight engine
; applies the corresponding `@<suffix>` capture to the reference
; site automatically.
; ============================================================

; #13 — register
(register_declarator
  name: (identifier) @local.definition.variable.register)

; #14 — wire (anchored to wire_declaration so port-declaration's
; signal_declarator is treated separately).
(wire_declaration
  (signal_declarator
    name: (identifier) @local.definition.variable.wire))

; #15 — memory
(memory_declaration
  name: (identifier) @local.definition.variable.memory)

; FR-009 — control terminals (proc_name / func_in / func_out / func_self).
; All three declaration shapes resolve to the same plan-locked capture
; `@variable.builtin.terminal` per highlights-coverage.contract.md §2.
(proc_declarator
  name: (identifier) @local.definition.variable.builtin.terminal)

(control_terminal_declaration
  name: (identifier) @local.definition.variable.builtin.terminal)

(control_internal_declaration
  name: (identifier) @local.definition.variable.builtin.terminal)

; #16 — proc
(proc_definition
  name: (identifier) @local.definition.function.proc)

; #17 — func (capture the call-name tail; supports dotted
; `func ic.ready { ... }` form).
(func_definition
  name: (scoped_identifier
    (identifier) @local.definition.function.func .))

; #20 — state label (definition site + state_name decl list).
(state_name_declaration
  name: (identifier) @local.definition.label.state)

(state_definition
  name: (identifier) @local.definition.label.state)

; Struct members (no FR-007 sub-capture; left as definitions so
; future struct-field highlight rules can reference them).
(struct_instance_declaration
  (struct_inst_declarator
    name: (identifier) @local.definition.variable))

(integer_declaration
  name: (identifier) @local.definition.variable)

(variable_declaration
  (variable_declarator
    name: (identifier) @local.definition.variable))

(label_name_declaration
  name: (identifier) @local.definition.label)

; ============================================================
; References — every identifier that may resolve to a definition.
;
; The broad `(identifier) @local.reference` form is the convention
; used by tree-sitter-rust / tree-sitter-go / tree-sitter-c. Identifiers
; that ARE definition sites get the definition's capture; identifiers
; whose scope walk doesn't reach a definition keep whatever
; `highlights.scm` says (or no capture). This is why placing the
; broad rule LAST is safe under tree-sitter's last-match-wins policy.
; ============================================================

(identifier) @local.reference
