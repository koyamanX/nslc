<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: `nslc -emit=hw` CLI flag (M6)

**Branch**: `010-m6-circt-lowering` | **Date**: 2026-05-04
**Spec**: [../spec.md](../spec.md) | **Plan**: [../plan.md](../plan.md)
**Pattern reference**:
[`specs/008-m5-structural-passes/contracts/driver-emit-mlir.contract.md`](../../008-m5-structural-passes/contracts/driver-emit-mlir.contract.md)

This contract pins the M6 `nslc -emit=hw` CLI surface. The
companion M5 contract for `-emit=mlir` is the precedent — M6's
`-emit=hw` is its sibling at one stage further down the pipeline.

---

## §1. Frozen flag spelling

The flag name is **`-emit=hw`**. The driver's CLI parser accepts
this as the value of the existing `-emit=` option, mapping to
`CompileOptions::EmitKind::HW` (an M3-reserved enum value
transitioned to operational at M6).

`nslc --emit=hw` (long form) is also accepted (matches M5's
`-emit=mlir` accepting both forms).

`nslc -emit=circt` is accepted as an **alias** for `-emit=hw`
(per data-model §5; both produce byte-identical output at M6).

---

## §2. Pipeline behaviour

When `EmitKind::HW` is selected, `Compilation::emit` runs the
following stages in order:

1. M1 preprocessor (`Compilation::preprocess`)
2. M1 lexer (consumed inside the parser)
3. M2 parser (`Compilation::parse`)
4. M3 sema (`Compilation::analyze`)
5. M5 AST → nsl-dialect lowering (`Compilation::lowerToNSL`)
6. M5 structural-expansion pipeline (`Compilation::runNSLPasses`)
7. **M6 nsl → CIRCT conversion** (`Compilation::lowerToCIRCT`) —
   the M6-specific stage.
8. **Print** the final `mlir::ModuleOp` to the output stream
   (stdout if no `-o`, the named file otherwise) using
   `mlir::ModuleOp::print(os, mlir::OpPrintingFlags())` — the
   default printer (matching M5's `-emit=mlir`).

Steps 1–6 are inherited from M5's `-emit=mlir` pipeline verbatim.
Steps 7–8 are M6-specific.

**Halting rule (Q2-specify-time → A)**: `-emit=hw` halts strictly
at step 8. The four stock CIRCT passes (`circt::fsm::convertFSMToSeq`,
`circt::seq::lowerSeqToSV`, `circt::sv::prepareForEmission`,
`circt::exportVerilog`) are NOT invoked. Those belong to M7's
`-emit=verilog`.

---

## §3. Diagnostic and exit-code behaviour

- On success: exit code 0; stdout (or `-o` file) contains the
  printed `mlir::ModuleOp`; stderr is empty (or contains only
  warnings).
- On any stage's diagnostic-failure: exit code non-zero; stderr
  contains the rendered diagnostics; stdout (or `-o` file) is
  empty or absent. The `-o` file is NOT created on failure (no
  partial-output leakage).
- On M6-specific conversion failure (FR-028): one diagnostic of
  the form `error: nsl→CIRCT conversion failed for op
  '<dialect.opname>'` at the offending op's `mlir::Location`;
  exit code non-zero.

---

## §4. Output format freeze

- MLIR text format: assembly form (default printer with default
  `OpPrintingFlags`).
- No `printGenericOpForm()`.
- No `useLocalScope()`.
- No `enableDebugInfo()`.

This matches M5's `-emit=mlir` printer convention exactly. M6
fixtures inherit M5's golden-comparison form.

---

## §5. Stdin / stdout / pipe support

- `nslc -emit=hw -` reads NSL source from stdin (matches
  `-emit=tokens` / `-emit=ast` / `-emit=mlir` from M3/M5).
- Output goes to stdout when `-o` is not specified.
- `cat input.nsl | nslc -emit=hw - | circt-opt …` is a supported
  invocation (the US5 round-trip gate uses it).

---

## §6. Determinism contract

Two consecutive invocations of `nslc -emit=hw input.nsl` MUST
produce byte-identical output (Constitution Principle V).
Determinism guarantees inherited from M5:

- File-list permutation invariance: `nslc -emit=hw a.nsl b.nsl`
  and `nslc -emit=hw b.nsl a.nsl` produce per-module outputs
  that, when sorted by module name, are byte-identical.
- Environment independence: ASLR, process-id, hostname, build-
  path, mtime, locale, CWD do NOT affect output.

The CI two-host-path determinism gate (M5-introduced) is extended
at M6 to compare `-emit=hw` outputs across two builds. Failure
flags as a Principle V violation.

---

## §7. Forward-compatibility

A future post-M6 change that adds stock-CIRCT-pass invocation
into `-emit=hw` (e.g., adding `-canonicalize` post-conversion)
would break byte-stable output and require a contract amendment
plus a fixture-baseline rewrite. This is intentional friction —
test goldens should not silently move under contributors' feet.

A future addition of `-emit=core-mlir`, `-emit=hw-canonical`, or
similar post-conversion flag is permitted without amending this
contract; this contract describes only `-emit=hw` (and its
`-emit=circt` alias).

---

## §8. Cross-references

- M5 sibling contract:
  [`specs/008-m5-structural-passes/contracts/driver-emit-mlir.contract.md`](../../008-m5-structural-passes/contracts/driver-emit-mlir.contract.md)
- Design §11 driver:
  [`docs/design/nsl_compiler_design.md`](../../../docs/design/nsl_compiler_design.md)
  lines 1136–1191 (Compilation class, `-emit=` flags)
- Constitution Principle V (determinism + inspectability):
  [`.specify/memory/constitution.md`](../../../.specify/memory/constitution.md)
  §317 ff.
