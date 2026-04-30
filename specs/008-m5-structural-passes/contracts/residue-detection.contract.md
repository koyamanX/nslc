<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: `%IDENT%` Residue Detection at `NSLCheckSemanticsPass`

**Branch**: `008-m5-structural-passes` | **Date**: 2026-04-30
**Spec**: [spec.md](../spec.md) FR-018, US4, SC-006, Clarifications Q1

This contract freezes the regex-based residue-detection mechanism
introduced at M5. It is the operational realization of Constitution
Principle IV (every diagnostic at its earliest layer) for the
preprocessor's last-mile residue case.

---

## 1. Detection mechanism

The pass walks every `mlir::Operation*` reachable from the input
`nsl::dialect::ModuleOp` via `op->walk(...)`. For each op, it
iterates over `op->getAttrs()` and tests every attribute whose
value is a `mlir::StringAttr` against the C++ regex frozen below.

---

## 2. Frozen regex

```cpp
static constexpr const char* kResidueRegex = R"((%[A-Za-z_][A-Za-z0-9_]*%))";
```

The implementation MUST use `std::regex` (or `llvm::Regex` — both
are deterministic and use ECMAScript-compatible syntax for the
character classes used here). The capturing group exists so the
diagnostic message can quote the exact splice text.

**Why this regex**:

- `%` literal start
- `[A-Za-z_]` first identifier char per pp.ebnf §5 lexical rules
- `[A-Za-z0-9_]*` body of identifier
- `%` literal end

This matches the `%IDENT%` lexical form exactly per
`docs/spec/nsl_pp.ebnf` §4 lines 312–343.

**Multiple matches per attribute**: a single string attribute can
contain multiple residue substrings (e.g., `"buf_%X%_%Y%_inner"`).
The regex is applied with `std::regex_iterator` to find ALL
non-overlapping matches; each emits its own diagnostic.

---

## 3. Scanned attribute table (M4-frozen op set)

The pass MUST scan the following `mlir::StringAttr`-typed fields
on the following M4-frozen ops. Adding an op or a string-attr
field to this table is a contract amendment.

| `nsl::*` op | StringAttr fields |
|---|---|
| `nsl.module` | `sym_name` |
| `nsl.reg` | `sym_name`, `n` |
| `nsl.wire` | `sym_name`, `n` |
| `nsl.mem` | `sym_name`, `n` |
| `nsl.func` | `sym_name` |
| `nsl.proc` | `sym_name` |
| `nsl.state` | `sym_name` |
| `nsl.first_state` | (no string-attr; carries `FlatSymbolRefAttr`) |
| `nsl.field_decl` | `sym_name` |
| `nsl.struct` | `sym_name` |
| `nsl.submodule` | `sym_name`, `n` |
| `nsl.fire_probe` | (carries `FlatSymbolRefAttr` only) |

**Symbol references** (`FlatSymbolRefAttr`, `SymbolRefAttr`) are
ALSO scanned via their `getValue()` string view (since
`FlatSymbolRefAttr::getValue()` returns a `StringRef`, the same
regex applies). This catches residue in `nsl.call @<name>` /
`nsl.goto @<name>` references.

**NOT scanned**:

- `IntegerAttr`, `FloatAttr`, `TypeAttr` — non-string-typed; no
  residue can hide there.
- Op operand `mlir::Value` types — values carry `Type` not strings.
- Nested `DictionaryAttr` / `ArrayAttr` — explicit non-recursion
  per FR-018 last sentence (a future op needing nested-string
  scan is a contract amendment).

---

## 4. Diagnostic format (frozen by FR-018)

```text
<file>:<line>:<col>: error: unresolved macro splice '%<IDENT>%' after structural expansion
```

Where:

- `<file>:<line>:<col>` derives from the carrying op's
  `mlir::Location` via the `DiagnosticBridge` translation
  (`FileLineColLoc` preferred, `FusedLoc` deepest-first fallback).
- `<IDENT>` is the captured group from the regex match (the inner
  identifier without the surrounding `%` characters).

**Multi-error recovery**: if the pass finds N residue matches AND
M sensitive-`Sn` violations (per `pass-pipeline.contract.md` §3),
N+M diagnostics are emitted in source-order before the pass
signals failure. The driver's diagnostic-engine output is sorted
by source location.

---

## 5. False-positive contract

The regex matches MUST NOT fire on the following legitimate
patterns:

- `mlir::StringAttr` values whose substring `%X%` is intentional
  literal content (e.g., a hand-authored test fixture testing the
  detector itself). **Resolution**: a fixture testing the detector
  exercises the failure path; it is expected that the detector
  fires. This is a tautology, not a false-positive.

- AST source comments. **Resolution**: comments are stripped by
  the M1 lexer; they do not survive to AST nodes; they cannot
  appear in `mlir::StringAttr` values.

- The literal six-character sequence `%foo %` (with embedded
  whitespace). **Resolution**: the regex requires `[A-Za-z_]`
  immediately after `%` and `[A-Za-z0-9_]*` until the closing
  `%`; whitespace breaks the pattern. Not a false-positive.

If a true false-positive is discovered post-M5, the resolution is
to amend the regex (this contract) or add an exclusion list — NOT
to silently relax the detection.

---

## 6. False-negative contract

The regex MUST fire on the following patterns:

| Pattern | Expected match | Diagnostic count |
|---|---|---|
| `"buf_%TYPO%"` | `%TYPO%` | 1 |
| `"%X%_%Y%"` | `%X%`, `%Y%` | 2 |
| `"%i%"` (inside post-unroll body if `%i%` was the loop var and substitution failed) | `%i%` | 1 |
| `""` (empty string-attr) | none | 0 |
| `"reg_q"` (no residue) | none | 0 |

If a real residue case is missed (silent miscompilation through
to M6 CIRCT lowering), it is a CI-blocking bug class and the
regex / scanned-attr table is amended in the same change.

---

## 7. Performance contract (informational)

The residue scan is bounded by:

```text
T_residue ≤ N_ops × N_string_attrs_per_op × L_attr_string × C_regex_match
```

For the largest M3-corpus fixture (≤200 ops, ≤3 string-attrs per
op, ≤64 chars per attr value), this is ≤ 38400 char-tests per
fixture. `std::regex_iterator` is linear in input size. Expected
runtime: ≤1 ms per fixture; under 10 ms in aggregate across the
M5 lit corpus. NOT a contract surface — informational budget only.

---

## 8. Test-coverage contract (frozen by FR-028)

`test/Lower/passes/nsl-check-semantics/<scenario>.mlir` MUST
include AT MINIMUM:

| Scenario | Input shape | Expected diagnostic count |
|---|---|---|
| `residue_typo.mlir` | `nsl.reg "buf_%TYPO%"` | 1 |
| `residue_undefined.mlir` | `nsl.reg "%UNDEFINED%"` | 1 |
| `residue_multi.mlir` | `nsl.reg "%X%_%Y%"` | 2 |
| `s15_post_param.mlir` | bit-slice with surviving `param_int` ref | 1 |
| `s16_pure_nsl.mlir` | `nsl.module` with surviving `param_int` op | 1 |
| `s25_replicated_collision.mlir` | replicated body with name collision | ≥1 |
| `clean_baseline.mlir` | well-formed module with zero residue / violations | 0 (round-trip success) |

Each fixture's `.mlir.expected_diagnostic.txt` contains the exact
diagnostic text per §4 above. The lit `RUN:` line:

```text
// RUN: not nsl-opt -nsl-check-semantics %s 2>&1 | FileCheck %s
// CHECK: error: unresolved macro splice '%TYPO%' after structural expansion
```

For the `clean_baseline.mlir` fixture, the `RUN:` line is
positive (no `not`) and asserts a clean round-trip.

---

## 9. Cross-references

- Spec: [`../spec.md`](../spec.md) FR-018, US4, SC-006, Clarifications Q1
- Pass-pipeline contract: [`pass-pipeline.contract.md`](./pass-pipeline.contract.md) §3
- Lower API contract: [`lower-api.contract.md`](./lower-api.contract.md) §2.2 (createNSLCheckSemanticsPass)
- Driver contract: [`driver-emit-mlir.contract.md`](./driver-emit-mlir.contract.md) §4 (exit-code on residue)
- M1 preprocessor contract: pp.ebnf §4 + P3 (residue forwarding rationale)
- Constitution: Principle IV (diagnostic at earliest layer); Principle VIII (diagnostic-string frozen)
