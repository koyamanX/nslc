<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: `nsl-lower` public API surface (M6 amendment to M5)

**Branch**: `010-m6-circt-lowering` | **Date**: 2026-05-04
**Spec**: [../spec.md](../spec.md) | **Plan**: [../plan.md](../plan.md)
**Supersedes-by-extension**:
[`specs/008-m5-structural-passes/contracts/lower-api.contract.md`](../../008-m5-structural-passes/contracts/lower-api.contract.md)

This contract amends the M5 public-API surface of the `nsl-lower`
library by adding **two** symbols. The M5 freeze list of 8 symbols
remains intact; M6 grows the surface from 8 to 10. The single
public umbrella header `include/nsl/Lower/Lower.h` is the only
header exposed; M6 does NOT introduce a new public header
(Constitution Principle II).

---

## §1. Header location

`include/nsl/Lower/Lower.h` (UNCHANGED from M5).

The M5 contract's §1 stipulates this is the single public header
for `nsl-lower`. M6 honours it: every new symbol re-exports from
this header.

---

## §2. Frozen public-symbol list (post-M6)

The 10 symbols below constitute the entire public API surface of
`libnsl-lower.a` after M6 lands. Adding an 11th symbol is an
M6-contract-amendment-class change (separate spec).

| # | Symbol | Source | Purpose |
|---|---|---|---|
| 1 | `mlir::OwningOpRef<mlir::ModuleOp> nsl::lower::astToMLIR(mlir::MLIRContext&, const ast::CompilationUnit&, const sema::SemaResult&)` | M5 | Public entry to AST → nsl dialect lowering visitor |
| 2 | `std::unique_ptr<mlir::Pass> nsl::lower::createNSLResolveParamsPass()` | M5 | Pass-pipeline slot 1 |
| 3 | `std::unique_ptr<mlir::Pass> nsl::lower::createNSLExpandGeneratePass()` | M5 | Pass-pipeline slot 2 |
| 4 | `std::unique_ptr<mlir::Pass> nsl::lower::createNSLExpandVariablesPass()` | M5 | Pass-pipeline slot 3 |
| 5 | `std::unique_ptr<mlir::Pass> nsl::lower::createNSLExplodeSubmodArrayPass()` | M5 | Pass-pipeline slot 4 |
| 6 | `std::unique_ptr<mlir::Pass> nsl::lower::createNSLInlineInternalFuncPass()` | M5 | Pass-pipeline slot 5 (no-op) |
| 7 | `std::unique_ptr<mlir::Pass> nsl::lower::createNSLCheckSemanticsPass()` | M5 | Pass-pipeline slot 6 |
| 8 | `void nsl::lower::registerNSLPasses()` | M5 | One-call registration helper for nsl-opt |
| **9** | **`std::unique_ptr<mlir::Pass> nsl::lower::createNSLToCIRCTPass()`** | **M6 NEW** | nsl → CIRCT conversion pass |
| **10** | **`void nsl::lower::registerNSLToCIRCTPass()`** | **M6 NEW** | nsl-opt registration helper for the new pass |

The two M6 additions sit in the same `nsl::lower` namespace, in
the same `Lower.h` umbrella header.

---

## §3. ABI guarantees

**Stable across M6**: every M5 symbol retains its exact signature.
A consumer of `Lower.h` from M5 source code links and runs against
M6's `libnsl-lower.a` without recompilation iff the consumer never
referenced symbols #9 or #10 (which did not exist at M5).

**Behavioral note on `registerNSLPasses` (#8)**: M5 documented this
helper as registering the six M5 passes. M6 extends the body to
ALSO call `registerNSLToCIRCTPass()`. The signature is unchanged;
the registration set grows from 6 to 7 passes. Consumers that
called `registerNSLPasses()` at M5 will, at M6, find the M6 pass
also registered — this is a benign behavior change (more passes
available is strictly more functionality; no existing pass
disappears).

---

## §4. `createNSLToCIRCTPass` semantics (symbol #9)

**Signature**:

```cpp
namespace nsl::lower {
std::unique_ptr<mlir::Pass> createNSLToCIRCTPass();
}
```

**Constructed pass instance** (frozen):
- Type: `mlir::OperationPass<mlir::ModuleOp>` subclass.
- Registered name (returned by `getArgument()`):
  **`"nsl-to-circt"`**.
- Description (returned by `getDescription()`): `"Lower nsl::*
  dialect ops to CIRCT (hw/comb/seq/fsm/sv)"`.
- `getDependentDialects` declares: `circt::hw::HWDialect`,
  `circt::comb::CombDialect`, `circt::seq::SeqDialect`,
  `circt::fsm::FSMDialect`, `circt::sv::SVDialect`.

**Post-condition on success**: input `mlir::ModuleOp` contains
zero ops in the `nsl` dialect. Every reachable op belongs to one
of the five CIRCT dialects above.

**Post-condition on failure**: at least one
`basic::DiagnosticEngine` diagnostic emitted; pass return is
`mlir::failure()`; input IR is unchanged (atomic rollback per
MLIR `applyFullConversion` contract).

---

## §5. `registerNSLToCIRCTPass` semantics (symbol #10)

**Signature**:

```cpp
namespace nsl::lower {
void registerNSLToCIRCTPass();
}
```

**Behavior**: registers the M6 pass with MLIR's global pass
registry so that `nsl-opt -nsl-to-circt %s` works. Idempotent
(safe to call multiple times in one process). Mirrors M5's
six per-pass `register*Pass` helpers in shape and intent;
calling `registerNSLPasses()` (symbol #8) covers this implicitly.

---

## §6. Symbol count freeze

The total count of public symbols in `Lower.h` after M6 is **10**.
This count is the contract surface. Tooling (e.g.,
`scripts/audit-public-headers.sh` if such a script exists, or a
manual code-review checklist) MUST verify the count after every
PR touching `Lower.h`.

A reviewer rejecting a PR that grows this surface to 11 without a
corresponding spec amendment is enforcing the contract correctly.

---

## §7. Cross-references

- M5 contract source (frozen 8-symbol baseline):
  [`specs/008-m5-structural-passes/contracts/lower-api.contract.md`](../../008-m5-structural-passes/contracts/lower-api.contract.md)
- Constitution Principle II (single-public-header rule):
  [`.specify/memory/constitution.md`](../../../.specify/memory/constitution.md)
  §239 ff.
- Design §3 (nine-library layout):
  [`docs/design/nsl_compiler_design.md`](../../../docs/design/nsl_compiler_design.md)
  lines 132–148.
