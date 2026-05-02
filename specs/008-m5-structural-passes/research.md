<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Research: M5 — `nsl-lower` part 1 (AST → `nsl` dialect + structural-expansion passes)

**Branch**: `008-m5-structural-passes` | **Date**: 2026-04-30
**Plan**: [plan.md](./plan.md)

This file resolves every Technical Context decision with a
**Decision / Rationale / Alternatives considered** entry, mirroring
the pattern established in
[`specs/007-m4-mlir-dialect/research.md`](../007-m4-mlir-dialect/research.md).
Each decision is anchored in the Constitution, the design docs, the
spec FRs, the four Clarifications-resolved questions (Q1–Q4 in
[`spec.md`](./spec.md) §Clarifications), prior-milestone precedent
(M0–M4), or upstream MLIR conventions — no decision is made on
"engineering taste" alone.

---

## 1. `%IDENT%` residue detection: regex over string-attrs (Q1 Option B)

**Decision**: `NSLCheckSemanticsPass` walks every `mlir::Operation*`
reachable from the input `nsl::ModuleOp` via `op->walk(...)`. For
each op, it iterates over `op->getAttrs()` and tests every
`mlir::StringAttr`-typed value against the C++ regex
`R"((%[A-Za-z_][A-Za-z0-9_]*%))"`. Each match emits one diagnostic
of the form `error: unresolved macro splice '%<IDENT>%' after
structural expansion` with `mlir::Location` set to the carrying op's
location. The pass does NOT recurse into nested
`mlir::DictionaryAttr` / `mlir::ArrayAttr` values; if a future op
needs nested-string residue detection, that is a follow-up
amendment to FR-018 (recorded in spec.md as a post-M5 carve-out).

**Rationale**: Resolves spec Q1 → Option B verbatim. Matches
`%IDENT%`'s textual nature per pp.ebnf §4 (lines 312–343) and P3
(lines 419–426) — the splice IS a substring inside a longer
identifier. Cheap O(N_ops × N_string_attrs_per_op); on the typical
M5 corpus (≤200 ops per fixture), each fixture's residue check runs
under one millisecond. Preserves the M4-frozen dialect surface
(no new op, no new attribute type, no
`dialect-api.contract.md` amendment). Diagnostic granularity
matches Principle IV: every match's `mlir::Location` resolves to
the carrying op's NSL `SourceRange`, which the AST → nsl visitor
preserved per FR-008.

**Alternatives considered**:

- *Option A (M1 errors out on unresolved `%IDENT%`)*: Rejected —
  forces an amendment to the M1-frozen contract documented in
  `specs/002-m1-lex-preprocess/contracts/preprocessor.contract.md`,
  and would void US4 + FR-018's residue clause + SC-006 (zero
  fail-test cases would exist).
- *Option C (dedicated `nsl.macro_residue` marker op)*: Rejected —
  grows the M4-frozen `dialect-api.contract.md` §2 freeze surface
  from 48 to 49 public types/functions; spec.md "What does NOT land
  at M5" forbids this.

---

## 2. MLIR text format for `-emit=mlir`: default printer (Q2 Option A)

**Decision**: `Compilation::emit` for `EmitKind::NSLMLIR` calls
`module.print(os)` with default-constructed `mlir::OpPrintingFlags()`
— no `printGenericOpForm()`, no `useLocalScope()`, no
`enableDebugInfo()`. The same flags are used by every
`test/Lower/**/*.mlir.expected` golden, by the `nsl-opt` M4
round-trip baseline, and by the determinism-gate diff pair under US5.

**Rationale**: Resolves spec Q2 → Option A verbatim. Matches the M4
round-trip property (M4 spec US1 acceptance scenario 1: "`nsl-opt
%s | nsl-opt -` is a fixed point" — both ends use default printer);
matches MLIR upstream convention (`mlir-opt` defaults the same
way); readable for fixture authoring and CodeRabbit review;
deterministic-by-construction (default printer's whitespace-
normalization is part of the round-trip property M4 already proved).
Output of `nslc -emit=mlir foo.nsl` is byte-identical to `nslc -emit=mlir
foo.nsl | nsl-opt -` (the trailing `nsl-opt -` is a no-op idempotent
re-print).

**Alternatives considered**:

- *Option B (generic form)*: Rejected — would force re-authoring
  every M4 `<op>_roundtrip.mlir` fixture (M4-contract drift) and
  reduce fixture readability.
- *Option C (configurable `--print-op-generic` flag)*: Rejected —
  doubles the test-fixture authoring surface for no observable user
  value at M5; if a future debug need surfaces, a `nslc
  -emit=mlir-generic` follow-up flag can be added without amending
  this spec.

---

## 3. `NSLInlineInternalFuncPass` deliverable: registered no-op slot (Q3 Option B)

**Decision**: The pass exists in the pipeline at slot 5 with the
flag `-nsl-inline-internal-func`, registers under that name in
`nsl-opt`'s pass-registry, walks the input `nsl::ModuleOp` once
(via `op->walk(...)`), and returns `mlir::success()` without
modifying the IR. FR-028's per-pass fixture for this pass is a
single round-trip golden under
`test/Lower/passes/nsl-inline-internal-func/noop_roundtrip.mlir`
asserting `input == output`.

**Rationale**: Resolves spec Q3 → Option B verbatim. Design §9
(`docs/design/nsl_compiler_design.md` lines 1086–1093) explicitly
tags this pass "(optional perf pass)"; M5's deliverable contract is
the SHAPE of the pipeline (six slots, six flags) not optimization
quality at this stage. The three named M5 sub-deliverables (per
README M5 row parenthetical: generate-loop unroll, struct-SSA-split,
`%IDENT%` residue check) saturate the milestone budget. The slot's
pass-name + signature + position in `Compilation::runNSLPasses` is
the M5 ABI commitment; a future PR can fill in functional inlining
without amending this spec.

**Alternatives considered**:

- *Option A (functional inlining)*: Rejected as scope creep — adds a
  full call-graph + inlining-cost analysis surface that pulls effort
  away from the three named M5 sub-deliverables.
- *Option C (functional with XFAIL fixtures)*: Rejected — XFAIL
  fixtures rot; CodeRabbit/CI cannot tell "intentionally deferred"
  from "broken" without out-of-band annotation; pre-M7 we have no
  forcing function to remove the XFAIL marker.

---

## 4. AST visitor strategy: single-pass + lazy `SymbolTable` resolution (Q4 Option A)

**Decision**: `ASTToMLIR::lower(...)` walks the AST exactly once.
Cross-region symbol references (e.g., `nsl.call @Q` inside `nsl.proc
@P` where `nsl.func @Q` is declared later in the source) are emitted
as `mlir::FlatSymbolRefAttr` regardless of whether the referent has
been visited yet. MLIR's stock `SymbolTable` machinery resolves all
references at op-tree finalization (M4 verifier time): every
`nsl::*` op carrying a `Symbol`-trait declaration is registered in
the enclosing `nsl.module`'s symbol table, and every
`FlatSymbolRefAttr` use is resolved post-walk by
`mlir::SymbolTable::lookup(...)` during the M4 verifier pass.

**Rationale**: Resolves spec Q4 → Option A verbatim. Matches MLIR
upstream convention (`func.func` and `mlir::SymbolTable` work this
way); simplest implementation (one walk, no fixup queue, no
two-phase synchronization); M3 Sema has already validated that all
references resolve, so no symbol-validation work falls on the
visitor; M4's verifiers already enforce symbol-reference validity
post-walk via the standard `mlir::SymbolTable` machinery the
dialect inherits.

**Alternatives considered**:

- *Option B (two-pass)*: Rejected — doubles AST walk cost for no
  observable user value.
- *Option C (single-pass + fixup queue)*: Rejected — adds a second
  post-walk traversal but with strictly more complexity than the
  lazy-resolution path the MLIR upstream already provides.

---

## 5. Post-expansion-sensitive `Sn` re-check subset (FR-018 deferred clause)

**Decision**: `NSLCheckSemanticsPass` re-checks the following
**six** `Sn` constraints (out of 23 error-emitting + 6 constructive
= 29 total per Constitution v1.7.0 Principle VIII):

| `Sn` | Constraint summary (per `lang.ebnf` §S) | Why post-expansion-sensitive |
|---|---|---|
| `S15` | Bit-slice indices `[hi:lo]` must be compile-time constants | Sema-time index can be a `param_int` reference; final-numeric verdict only after `NSLResolveParamsPass` |
| `S16` | `param_int` / `param_str` meaningful only for V/V/SC submodules | Pure-NSL modules' params must vanish post-resolve-params; surviving param-ref op is a violation |
| `S10` | `generate` loop variable must be `integer` | Sema validated the type at M3, but post-`NSLExpandGeneratePass` the loop variable is gone — re-check verifies cleanup |
| `S20` | Interface modifier (clk/reset binding) consistency | Submod-array elements per `NSLExplodeSubmodArrayPass` MUST each carry the parent's interface binding |
| `S6` | `reg` / `wire` declared before first use | Post-`NSLExpandVariablesPass` the wire-chain creates new `nsl.wire` ops; re-check confirms the chain is well-ordered |
| `S25` | No two declarations of the same name in the same scope | Post-`NSLExpandGeneratePass` the unrolled body's `%IDENT%`-spliced names MUST remain unique within the new (replicated) scope |

The remaining **23** `Sn` are NOT re-checked at M5 because their
verdict cannot change post-expansion (the structural rewrite
preserves the verdict by construction, or the relevant Sema
artifact has been consumed and is gone). The list above is frozen
by the FR-018 contract amendment in `contracts/residue-detection.contract.md`
§3.

**Rationale**: Spec.md FR-018 last clause + Assumptions section
("the exact list is a `/speckit-plan` deliverable") explicitly
deferred this. The selection criterion is mechanical: a `Sn` is
post-expansion-sensitive iff its Sema-time verdict references at
least one of {`param_int` / `param_str` value, `generate` loop
variable, submod-array index, post-expansion wire-chain ordering,
post-expansion replicated-scope uniqueness}. Walking the 29
constraints with this criterion produces exactly the six rows
above. The remaining 23 are constraints whose verdict reduces to
either pure-AST-shape (which structural expansion preserves) or
pure-Sema-derived-type (which structural expansion cannot mutate).

**Alternatives considered**:

- *Re-check ALL 29*: Rejected — most are vacuous (would always
  pass) and inflate the pass implementation 5×.
- *Re-check NONE (only `%IDENT%` residue)*: Rejected — `S15` /
  `S16` violations literally require post-resolve-params evaluation;
  not re-checking them would let them leak into M6 CIRCT lowering.
- *Implementer-determined at PR time*: Rejected — the contract
  freeze is the auditing surface for Principle VII (spec ↔ design
  coupling); leaving the list unfrozen creates a moving target for
  CodeRabbit and post-merge audits.

---

## 6. Internal header layout under `lib/Lower/` and `include/nsl/Lower/`

**Decision**: One PUBLIC umbrella header
`include/nsl/Lower/Lower.h` re-exports (a) the visitor entry-point
free function, (b) the six pass-construction free functions, (c) a
single registration helper. The umbrella re-exports
ONE-LINE forward declarations or `#include`s of internal headers
that live under `lib/Lower/` (private):

```text
include/nsl/Lower/
├── Lower.h                 (PUBLIC umbrella — Principle II § single-public-header)

lib/Lower/
├── ASTToMLIR.h             (private — class declaration)
├── ASTToMLIR.cpp           (private — visitor implementation)
├── Pass/
│   ├── NSLResolveParamsPass.cpp
│   ├── NSLExpandGeneratePass.cpp
│   ├── NSLExpandVariablesPass.cpp
│   ├── NSLExplodeSubmodArrayPass.cpp
│   ├── NSLInlineInternalFuncPass.cpp
│   └── NSLCheckSemanticsPass.cpp
├── Pass/Common/
│   ├── DiagnosticBridge.h  (ScopedDiagnosticHandler → basic::DiagnosticEngine)
│   └── DiagnosticBridge.cpp
└── CMakeLists.txt          (M0-declared; source list grows here)
```

**Rationale**: Constitution Principle II § "single public header"
rule — `nsl-lower` is NOT one of the named exceptions for
`nsl-ast` / `nsl-sema`, so it gets exactly one public header.
Internal-header expansion is private to the library and fully
visible only inside `lib/Lower/`; consumers (the `nsl-driver`
library and `nsl-opt` binary) include only `Lower.h`. The
`Pass/Common/DiagnosticBridge.{h,cpp}` files factor out the
`mlir::ScopedDiagnosticHandler` plumbing that every pass needs per
FR-019 — this is shared internal infrastructure, not a public
export.

**Alternatives considered**:

- *One file per pass at the public surface*
  (`include/nsl/Lower/Pass/NSLResolveParamsPass.h` etc.): Rejected —
  violates Principle II's single-public-header rule; the
  per-node-kind exception applies only to `nsl-ast` and `nsl-sema`
  per the constitution's explicit named-list.
- *Visitor declared in `Lower.h` directly* (no internal
  `ASTToMLIR.h`): Rejected — drags `mlir::OpBuilder` and Sema/AST
  internal types into every public-include consumer's compile cost;
  consumer `nsl-driver` and `nsl-opt` need only the entry-point
  function signature, not the visitor class.

---

## 7. `nsl-opt` integration: pass-registration via single helper

**Decision**: `Lower.h` exports a single C-linkage-free function
`void registerNSLLowerPasses();` that internally calls
`mlir::registerPass(create<X>Pass)` for each of the six passes.
The M4 `tools/nsl-opt/main.cpp` is amended with one new line in
`main()`:

```cpp
nsl::lower::registerNSLLowerPasses();
```

placed after the existing `nsl::dialect::registerNSLDialect()` call
from M4. The `nsl-opt` CMakeLists gains `nsl-lower` as a link
dependency.

**Rationale**: One amendment, one symbol exported, fits inside the
M4 `nsl-opt` shell unchanged otherwise. The function is declared in
the public umbrella header so the consumer (`nsl-opt`) does not
need to know the per-pass class names. Matches MLIR upstream
convention (`mlir::registerAllPasses()` is the same shape).

**Alternatives considered**:

- *Per-pass `register<X>Pass()` helpers*: Rejected — six public
  symbols where one suffices; clutters the umbrella header.
- *PassPlugin DSO loaded at runtime*: Rejected — `nsl-opt` is a
  developer/test tool per Constitution Principle II; adding a
  runtime-plugin loader is overkill for an in-tree build.

---

## 8. Visitor's intermediate-SSA naming: MLIR-default unnamed (deterministic counter)

**Decision**: The visitor leaves all intermediate-expression SSA
values UNNAMED, letting MLIR's default printer produce
`%0`/`%1`/`%2`/... in lexical-emission order. Top-level symbol
names (`@M`, `@P`, `@s0`, `@reg_q`, etc.) are derived from the
Sema symbol-table's stable string attribute (the same string the
M2 parser produced). The visitor MUST NOT use `mlir::Value::dump()`,
`std::ostream::operator<<` on a pointer, or any
`llvm::DenseMap<const ast::Node*, ...>` iteration in any code path
that produces a name (FR-026).

**Rationale**: MLIR's default printer's `%N` counter is purely
emission-order-derived (the printer maintains its own counter in a
local-scope walk, NOT pointer-derived). Emission order is itself
deterministic because the visitor walks the AST in source-order
(which is `ast::CompilationUnit`'s vector iteration order, NOT a
hash map). Therefore `%0`/`%1`/... is byte-stable across builds.
Top-level symbol names come from the Sema symbol-table which the
M3 contract says preserves source-order (per
`specs/006-m3-sema/contracts/sema-api.contract.md` §2 Invariant 7
"symbol-table iteration order matches lexical declaration order").

**Alternatives considered**:

- *Stable user-facing names for intermediate values* (e.g.,
  `%t_a_plus_b`): Rejected — increases visitor code complexity
  (name-construction must be deterministic AND collision-free);
  test-fixture readability gain is small (`%0` is well-understood
  by every MLIR contributor).
- *MLIR's `ValueBoundsConstraintSet`-style explicit naming hooks*:
  Rejected — doesn't apply at this layer; that infrastructure is
  for analysis passes, not for IR construction.

---

## 9. Sema warnings handling: warnings allowed, lowering proceeds

**Decision**: `Compilation::lowerToNSL` checks
`SemaResult::hasErrors()` before invoking the visitor. If true,
the function returns `nullptr` (or the failed-`OwningOpRef` form)
and the driver halts with diagnostic printing. If false (zero
errors but warnings MAY be present), the visitor proceeds normally.
Warnings are forwarded through the diagnostic engine to user-visible
output but do NOT block lowering.

**Rationale**: The spec is silent on warnings; this is the
reasonable default per the `Compilation::run()` contract from
design §11 lines 1191 ("If `diag_.hasErrors()` becomes true at any
point, the pipeline halts"). `hasErrors()` returns true only for
`Severity::Error` / `Severity::Fatal` (per
`include/nsl/Basic/DiagnosticEngine.h` from M1) — warnings are
informational and pass through. This matches every other compiler's
convention (clang, gcc, rustc all proceed past warnings).

**Alternatives considered**:

- *Warnings block lowering*: Rejected — would surprise users
  (warnings have no semantic-correctness implication) and breaks the
  `-W` flag's traditional advisory role.
- *Warnings logged in MLIR `loc(...)` metadata*: Rejected — adds
  determinism risk (loc-fused-attribute timing is implementation
  defined); the diagnostic engine is the right surface.

---

## 10. M3-corpus subset for FR-030: full corpus extension

**Decision**: FR-030's "every M3-corpus fixture under `test/sema/`
that produces zero diagnostics MUST also be exercisable with `nslc
-emit=mlir`" is taken at face value: the FULL set of pass-case
fixtures from M3 (all 23 error-`Sn` pass cases + all 6 constructive-`Sn`
pass cases + a sampling of N-disambiguation pass cases that
compile cleanly = approximately **34 pass-case fixtures** at M3
freeze) gain a paired `*.expected.mlir` golden at M5. The fail-case
M3 fixtures (one per `Sn`) are NOT extended — those produce
diagnostics at M3 and never reach M5's lowering layer.

**Rationale**: FR-030 is unambiguous; subsetting would weaken the
SC-003 cardinality target ("Every M3-corpus fixture … produces a
verifier-clean `mlir::ModuleOp` … Pass rate: 100%"). Authoring 34
goldens is a one-shot cost amortized over the project lifetime; the
goldens are mechanically regenerable via `nslc -emit=mlir
test/sema/<sn>/<case>.nsl > test/Lower/m3_corpus/<sn>/<case>.expected.mlir`
during test-author bootstrap. CI runs every fixture in the lit
matrix; none is conditional.

**Alternatives considered**:

- *Curated subset (one fixture per AST node kind only)*: Rejected —
  conflates US1's "per AST node kind" coverage with US-implicit
  "every M3-corpus input lowers cleanly". US1 is satisfied by the
  per-node fixtures under `test/Lower/<category>/`; FR-030 is a
  separate (additive) gate.
- *Full audited-corpus extension*: Rejected — audited corpus is M7
  (P-VEN); pulling it forward into M5 would force vendoring +
  `PROVENANCE.md` + `golden/REGEN.md` work that belongs at M7.

---

## 11. Pass implementation pattern: hand-written `OperationPass<nsl::ModuleOp>` derivative

**Decision**: Each of the six passes is a hand-written class
deriving from
`mlir::OperationPass<nsl::dialect::ModuleOp>`. The class
overrides `runOnOperation()` and `getArgument()` /
`getDescription()` to expose its CLI flag name + one-line
description. Implementation sites:

```cpp
namespace nsl::lower {

class NSLExpandGeneratePass
    : public mlir::OperationPass<nsl::dialect::ModuleOp> {
public:
  StringRef getArgument() const override { return "nsl-expand-generate"; }
  StringRef getDescription() const override {
    return "Unroll nsl.structural_generate into N copies of its body.";
  }
  void runOnOperation() override;
};

std::unique_ptr<mlir::Pass> createNSLExpandGeneratePass() {
  return std::make_unique<NSLExpandGeneratePass>();
}

} // namespace nsl::lower
```

The umbrella header `Lower.h` re-exports
`createNSL<X>Pass()` free functions for each of the six passes.

**Rationale**: Hand-written matches MLIR upstream convention for
project-specific passes (CIRCT's `circt::seq::LowerSeqToSV` etc.
follow the same pattern). The TableGen-driven pass-template
(`MLIR_PASS_BASE`) saves boilerplate but introduces a new
TableGen-record surface that complicates incremental rebuild and
conflicts with M5's "tight scope" philosophy. The per-pass class
size is ~20 lines of declaration + `runOnOperation()` body; a
TableGen-driven approach would save ~5 lines but cost 30 lines of
`.td` file per pass.

**Alternatives considered**:

- *TableGen-driven pass declarations* (`Pass.td` + `gen-pass-decls`):
  Rejected — see rationale above; for six passes the TableGen
  overhead exceeds the savings.
- *Pure free-function passes via
  `mlir::createOperationPass(callable)`*: Rejected — loses the
  `getArgument()` / `getDescription()` CLI metadata that FR-011 +
  SC-002 require.

---

## 12. Diagnostic bridging: `mlir::ScopedDiagnosticHandler`

**Decision**: A shared internal class
`nsl::lower::DiagnosticBridge` (declared in
`lib/Lower/Pass/Common/DiagnosticBridge.h`) owns an
`mlir::ScopedDiagnosticHandler` configured to intercept every
`mlir::Diagnostic` produced inside the visitor or any of the six
passes, translate it to a `basic::Diagnostic` (per the M1
diagnostic-engine API), and post it to the project-global
`basic::DiagnosticEngine` (the same engine used for M2 parser /
M3 Sema diagnostics). The bridge is constructed at the top of
`Compilation::lowerToNSL` and `Compilation::runNSLPasses` and
released when those functions return.

**Rationale**: FR-019 requires this bridging pattern. MLIR's
`ScopedDiagnosticHandler` is the upstream-blessed mechanism; using
it directly preserves Principle IV's "every diagnostic at its
earliest layer" rule because MLIR's internal diagnostics pass
through the bridge before any user-visible printing. The bridge is
private (one shared header for the M5 library), not part of any
public surface; future tooling-track work (`nsl-lsp` consuming
`-emit=mlir` output) routes through `Compilation` and gets the
same bridging for free.

**Alternatives considered**:

- *Inline diagnostic-bridging in each pass body*: Rejected — six
  copies of the same handler-installation code violates DRY and
  invites drift.
- *Translate only after pass-pipeline completion (post-walk
  collection)*: Rejected — would lose source-locating fidelity
  because some diagnostics carry MLIR-private state (e.g., op
  context) that is destroyed when the pass completes.

---

## 13. Determinism source-of-truth: AST-source-order iteration

**Decision**: Every name-producing or name-deriving code path in
the visitor and the six passes obeys the following rule, audited
by a CI grep + clang-tidy check:

| Construct | Permitted | Forbidden |
|---|---|---|
| `mlir::Value` → `mlir::Value` map | `llvm::DenseMap<mlir::Value, …>` (printer-side keys) | `std::unordered_map<…, mlir::Value*>` (pointer key) |
| AST symbol → MLIR value | `llvm::DenseMap<const sema::Symbol*, mlir::Value>` (used internally; iteration-order does NOT matter because lookups are key-based) | Iterating this map (would give pointer order — non-deterministic) |
| Symbol-table walk for emission | Iterate the M3 `sema::SymbolTable`'s ordered iterator (see M3 invariant 7) | Iterating `llvm::DenseMap` keys |
| AST node visitation | `ast::CompilationUnit`'s vector-iteration order (source order) | Hash-map iteration |
| Pass-internal temporaries | Counter rooted in lexical-emission-order (`nextTempId_++`) | `std::ostringstream{} << reinterpret_cast<uintptr_t>(this)` |

The CI grep is a `scripts/audit_determinism.sh` script run in CI
stage 2 (static checks). It greps for forbidden patterns (`std::unordered_`
/ `reinterpret_cast<uintptr_t>` / `std::time` / `std::chrono` / etc.)
inside `lib/Lower/`. Any match is a CI-blocking violation per
Principle IX.

**Rationale**: FR-025 + FR-026 establish the requirement; this
section makes the rule mechanically auditable. M3's symbol-table
iteration-order invariant (per
`specs/006-m3-sema/contracts/sema-api.contract.md` §2 Invariant 7)
gives M5 a guaranteed-deterministic input. The CI grep prevents
silent re-introduction of pointer-derived ordering during
implementation.

**Alternatives considered**:

- *Trust unit-test goldens to catch non-determinism*: Rejected —
  goldens catch reproduced violations but not first-time leaks
  (e.g., a new pass added in a follow-up PR that uses
  `std::unordered_map` would only fail when its golden was first
  authored, then the fix would be silently un-done by a subsequent
  refactor).
- *clang-tidy custom check*: Rejected for M5 — authoring a custom
  clang-tidy check is its own engineering project; the simpler
  `audit_determinism.sh` grep covers 95% of the failure modes for
  ~30 minutes of authoring time.

---

## 14. CMake integration: existing M0 `add_nsl_library` declaration extended

**Decision**: `lib/Lower/CMakeLists.txt` is amended to add the
visitor + six pass `.cpp` files to the `add_nsl_library(nsl-lower
…)` source list. The `DEPENDS` and `LINK_LIBS` lists are NOT
amended at M5 (the M0 declaration's `DEPENDS nsl-sema nsl-dialect`
+ `LINK_LIBS CIRCTHW CIRCTComb CIRCTSeq CIRCTSV` is preserved
verbatim; CIRCT link-libs stay inert at M5 — see
`spec.md` §Assumptions). `tools/nsl-opt/CMakeLists.txt` gains
`nsl-lower` as a link dependency. `lib/Driver/CMakeLists.txt` —
which already lists `Compilation.cpp`, `LowerToNSL.cpp`, and
`RunNSLPasses.cpp` from M4 — gains `nsl-lower` as a link
dependency to enable the M5 body fills.

**Rationale**: FR-002 + FR-003 lock this in. The M0 scaffold
already provided the library declaration; M5 only extends the
source list. CIRCT-link-libs preservation honors the spec.md
Assumption "stay inert at M5"; activation lands at M6 where actual
CIRCT lowering ops appear. The M4 driver-source files
(`LowerToNSL.cpp` / `RunNSLPasses.cpp`) link `nsl-lower` to access
the visitor entry-point + pass-construction free functions through
`Lower.h`.

**Alternatives considered**:

- *Move M4 driver-source body fills into `lib/Lower/`*: Rejected —
  the M4 contract froze `Compilation::lowerToNSL` /
  `Compilation::runNSLPasses` AS member functions of a
  `nsl::driver::Compilation` class that lives in `nsl-driver`;
  moving them violates the M4 architectural seam.
- *Add a new `nsl-lower-driver` library between `nsl-driver` and
  `nsl-lower`*: Rejected — adds a layer with no semantic meaning;
  M5's deliverable is a single library extension, not a new layer.

---

## 15. M4-amendment 2026-05-01: `nsl.constant` unblocks expression-lowering

**Decision**: Amend M4 to add `nsl.constant` (Pure + ConstantLike,
`I64Attr:$value`, `NSL_AnyBits:$result`) so M5 expression-lowering
has a value-producer of `!nsl.bits<N>` for every `LiteralExpr`.

**Why this is in the M5 research file** (rather than only M4): the
gap was surfaced *during* M5 implementation when the
`visit(LiteralExpr)` / `visit(TransferStmt)` pivot stalled with no
producer for the `mlir::Value` operand of `nsl.transfer`. The user
authorised the amendment via a four-way decision; recording the
options + rationale here keeps M5's implementation history honest
about why M4's freeze surface grew between merge and M5 completion.

**Options considered**:

- *(a) Amend M4 to add `nsl.constant`* — **CHOSEN.** One new op record
  + one verifier body + two fixtures. Closes the gap structurally;
  preserves `nsl.transfer`'s `SameTypeOperands` strict-type invariant
  (which Q3 Option A established as the architectural seam between
  the dialect and CIRCT). Freeze surface 48 → 49.
- *(b) Inline `hw.constant` + `unrealized_conversion_cast` helper at
  every literal site* — Rejected. Permanent dialect-bridge wart; every
  literal becomes two ops + a cross-dialect cast; printed IR loses
  one-to-one correspondence with NSL source; the M5 visitor's "every
  AST node maps to one `nsl::*` op" rule (FR-027 enumeration) breaks.
- *(c) Relax `nsl.transfer`'s `SameTypeOperands` constraint to admit
  cross-dialect operands* — Rejected. Erodes Q3 Option A's strict-type
  invariant and forces every downstream consumer (M5 lint, M6 lowering,
  M7 driver) to defensively check operand-type-pair shapes the dialect
  used to guarantee. Net cost is much larger than (a).
- *(d) Defer M5 expression-lowering until M6* — Rejected. Blocks the
  entire `LiteralExpr` / `BinaryExpr` / `UnaryExpr` lowering path
  until M6 lands `hw` activation; defeats the purpose of having an
  `nsl` IR layer that's structurally faithful to NSL source.

**Cost paid by this amendment**:

- `lib/Dialect/NSL/IR/NSLOps.td` — one new `def NSL_ConstantOp` (~15
  lines TableGen + the `mlir/Interfaces/SideEffectInterfaces.td`
  include for `Pure`).
- `lib/Dialect/NSL/IR/NSLOps.cpp` — one new `ConstantOp::verify()`
  body (~30 lines, width-fits check on the `I64Attr` value).
- `test/Dialect/storage/constant_roundtrip.mlir` + `test/Dialect/
  storage/constant_invalid_overflow.mlir` — two new fixtures.
- `specs/007-m4-mlir-dialect/contracts/dialect-api.contract.md` §2 +
  §6 — count bump 48 → 49 + post-merge note.
- `specs/007-m4-mlir-dialect/spec.md` Clarifications — new session
  2026-05-01 entry recording the four-way decision.
- `docs/design/nsl_compiler_design.md` §7 — op-summary entry for
  `nsl.constant`.

**Constraints honoured**:

- No RTTI: verifier uses `mlir::cast<BitsType>` (CRTP-based MLIR
  type-id machinery, NOT `dynamic_cast`).
- Determinism: no pointer-keyed iteration; the verifier reads
  `getValue()` (an `int64_t`) and a fixed type-width.
- Round-trip: `nsl-opt foo.mlir | nsl-opt -` is a fixed point on the
  new round-trip fixture (verified locally).
- Widths > 64 are deferred — the `value` attr is `I64Attr`. A future
  amendment (when M5 surfaces wide-literal needs) will widen this to
  `APIntAttr` or similar; that is a separate decision.

**Cross-references**:

- M4 spec: `specs/007-m4-mlir-dialect/spec.md` Clarifications
  session 2026-05-01.
- M4 contract: `specs/007-m4-mlir-dialect/contracts/
  dialect-api.contract.md` §2 (post-merge note) + §6 (op-class
  count).
- Design: `docs/design/nsl_compiler_design.md` §7 op summary.

---

## 16. M4-amendment 2026-05-02 (#2): expression-op surface unblocks FR-007

**Decision**: Amend M4 (a second time) to add the 28-op expression
surface — `nsl.add`, `nsl.sub`, `nsl.mul`, `nsl.and`, `nsl.or`,
`nsl.xor`, `nsl.shl`, `nsl.shr`, `nsl.eq`, `nsl.ne`, `nsl.lt`,
`nsl.le`, `nsl.gt`, `nsl.ge`, `nsl.land`, `nsl.lor`, `nsl.not`,
`nsl.neg`, `nsl.lnot`, `nsl.reduce_and`, `nsl.reduce_or`,
`nsl.reduce_xor`, `nsl.sign_extend`, `nsl.zero_extend`, `nsl.mux`,
`nsl.concat`, `nsl.extract`, `nsl.repeat` — so M5's `lowerExpr` /
`visit(BinaryExpr)` etc. honor FR-007 verbatim ("BinaryExpr / UnaryExpr
/ ConditionalExpr / SliceExpr / ConcatExpr lower to `mlir::Value`")
without violating Principle III's architectural seam.

**Why this is in the M5 research file** (rather than only M4): the gap
was surfaced *during* M5 implementation when work began on
`visit(BinaryExpr)` and there was no `nsl.*` op for the visitor to
construct. Recording the four-way decision history here keeps M5's
implementation history honest about the *second* freeze-surface growth.

**Options considered**:

- *(A) Skip the dialect, lower `BinaryExpr` directly to CIRCT `comb.*`*
  — Rejected. Violates Principle III: M5 lowers AST → `nsl` dialect;
  M6 handles the `nsl` → CIRCT conversion. Skipping the seam erodes
  the architectural separation that the entire compiler is built on.
- *(B) Amend M4 to add the full expression-op surface* — **CHOSEN.**
  Six commits, one per cluster (clusters 1+3+4 fused, 2, 5, 6, 7a, 7b).
  Each commit lands TableGen records + (where invariants aren't
  trait-covered) hand-written verifier bodies + round-trip fixtures
  + (where applicable) verifier-reject fixtures. The final commit
  bundles the contract / spec / design doc amendments.
- *(C) Defer FR-007 to M6 with cross-dialect placeholder ops at M5* —
  Rejected. Cross-dialect IR shape is opaque; every consumer (M5 lint,
  M6 lowering, M7 driver) defensively type-checks; the structural
  guarantee that "the IR speaks one dialect at a time per pass" is
  lost.
- *(D) Downgrade FR-007 to "stub-only"* — Rejected. Postpones the
  M5→M6 cut and breaks the Principle VIII test sequencing (FileCheck
  fixtures need the real IR shape, not stubs).

**Cost paid by this amendment**:

- `lib/Dialect/NSL/IR/NSLOps.td` — 28 new `def NSL_*Op` records
  (~250 LOC of TableGen, factored via three helper classes:
  `NSL_BinaryArithOp`, `NSL_BinaryCmpOp`, `NSL_UnaryArithOp`,
  `NSL_UnaryReduceOp`, `NSL_ExtendOp`).
- `lib/Dialect/NSL/IR/NSLOps.cpp` — ~150 LOC of hand-written verifier
  bodies for the 16 ops with non-trait-covered invariants. Three
  helper functions factored at namespace scope: `verifyResultIsBits1`,
  `verifyLogicalOpOperandsWidth1`, `verifyExtendWidthsMonotonic`.
- 28 round-trip fixtures + 16 invalid fixtures under
  `test/Dialect/expr/` (new directory mirroring `test/Dialect/storage/`).
- `.specify/m4_invariant_table.json` — 20 new entries for the ops
  with declared invariants (the 8 trait-covered ops in clusters 1+3+4
  are excluded; their trait-machinery rejection is observed via the
  upstream `SameOperandsAndResultType` machinery).
- `specs/007-m4-mlir-dialect/contracts/dialect-api.contract.md` §2 +
  §6 — count bump 49 → 77 + post-merge note #2.
- `specs/007-m4-mlir-dialect/spec.md` — Clarifications session
  2026-05-02 entry recording the four-way decision.
- `specs/008-m5-structural-passes/spec.md` Assumptions — count bump
  49 → 77 with cross-reference to this section.
- `docs/design/nsl_compiler_design.md` §7 — per-op rows for the new
  M4 ops (with EBNF §11 cross-references).
- `docs/design/nsl_compiler_design.md` §10 — per-op CIRCT mapping
  rows (`comb.add`, `comb.icmp ult`, `comb.mux`, etc.).

**Constraints honoured**:

- No RTTI: every verifier uses `mlir::dyn_cast<BitsType>` (CRTP type-id).
- Determinism: no pointer-keyed iteration; verifiers read only interned
  widths and I64Attr values.
- Round-trip: every new fixture is a fixed point under
  `nsl-opt %s | nsl-opt -`.
- Principle III firewall: design §10 documents the M6 mapping but
  M4 ships ZERO CIRCT-conversion code. The M4 dialect is the seam.
- Phase A scope (this set of commits) lands op surface only. Phase B
  (consume the new ops in `lowerExpr` / `visit(BinaryExpr)` / etc.)
  is a separate offload.

**Cross-references**:

- M4 spec: `specs/007-m4-mlir-dialect/spec.md` Clarifications
  session 2026-05-02.
- M4 contract: `specs/007-m4-mlir-dialect/contracts/
  dialect-api.contract.md` §2 post-merge note #2 + §6 (count).
- Design: `docs/design/nsl_compiler_design.md` §7 op summary +
  §10 mapping table.

---

## 17. M4-amendment 2026-05-02 (#3): top-level `nsl.struct` unblocks T043 + T044

**Decision**: Amend M4 (a third time) to **relax** the `nsl.struct`
parent trait from `HasParent<"ModuleOp">` to
`ParentOneOf<["::mlir::ModuleOp", "ModuleOp"]>` so a top-level
`nsl.struct` (sibling of `nsl.module` under the builtin
`mlir::ModuleOp`) is legal. This closes the structural-issue trilogy
that began with `nsl.constant` (#1) and continued with the 28-op
expression surface (#2): all three were gaps surfaced by M5 lowering
work, and all three are minimal additive amendments to the M4 op
surface.

**Why this is in the M5 research file** (rather than only M4): the
gap was surfaced during M5 implementation when T043 / T044
(`StructCastExpr` / `FieldAccessExpr` lowering) reached the AST→nsl
seam and discovered NSL grammar's top-level struct shape had no
legal MLIR placement. Recording the three-way decision here keeps
the implementation history honest about the *third* M4 amendment.

**Three-way decision** (user-authorised option (ii)):

- *(i) Keep the strict parent + synthesise a wrapping `nsl.module` for
  every top-level struct in the AST→nsl seam* — Rejected. Clutters
  every printed IR with synthetic modules that don't correspond to
  any source-level construct; forces M6 lowering to detect-and-discard
  them; violates the "structurally faithful" lowering principle
  established by Q1 Option A (M4 verifier scope is structural-only;
  the AST→nsl seam should mirror NSL grammar shape).
- *(ii) Relax the parent trait* — **CHOSEN.** Single TableGen-trait
  edit. Existing module-scoped struct fixtures continue to verify
  and round-trip (the amendment is purely additive). No new ops
  introduced; freeze surface stays at **77**. NSL grammar's
  `CompilationUnit::items()` places `struct S { ... }` at top level
  (sibling of `module B { ... }`) — the relaxed trait mirrors this
  exactly.
- *(iii) Move structs into a per-module child slot* — Rejected.
  Contradicts NSL grammar: `lang.ebnf §1` puts `struct` at the same
  level as `module`. Forcing structs into a per-module child would
  require every cross-module struct reference to use a multi-segment
  `SymbolRef` (e.g., `@HostMod::@MyRec`), which the M4 dialect's
  current type system (`!nsl.struct<@T>` is a `FlatSymbolRefAttr`)
  cannot express without a separate amendment to `NSLTypes.td`.

**Verifier behaviour confirmation**: `StructOp::verify()`'s field-
cycle check uses `mlir::SymbolTable::getNearestSymbolTable(*this)`,
which walks to the nearest enclosing op implementing the
`SymbolTable` trait. For the top-level placement, that op is the
builtin `mlir::ModuleOp` (which implements `SymbolTable`). The
existing `lookupSymbolIn(nearestSymTable, ...)` resolution logic
finds the struct sibling there without modification. Sibling
consumers (`StructCastOp::verify`, `FieldOp::verify`,
`computeStructTotalWidth`, `lookupFieldDeclByIndex`) likewise pivot
through `getNearestSymbolTable` and require no amendment. Confirmed
end-to-end via `test/Dialect/Types/struct_toplevel_roundtrip.mlir`
which round-trips a top-level struct that is referenced by a sibling
`nsl.module` — the dialect verifier accepts the layout and the
`!nsl.struct<@MyRec>` reference resolves.

**Cost paid by this amendment**:

- `lib/Dialect/NSL/IR/NSLOps.td` — single trait edit (one line
  changed in `NSL_StructOp`'s trait list; the description block
  grows by ~15 lines documenting the amendment).
- `test/Dialect/Types/struct_toplevel_roundtrip.mlir` — new round-
  trip fixture (~30 lines) demonstrating the previously-illegal
  layout.
- `specs/007-m4-mlir-dialect/contracts/dialect-api.contract.md` §2
  — post-merge amendment #3 note appended; count stays at 77.
- `specs/007-m4-mlir-dialect/spec.md` — Clarifications session
  2026-05-02 amendment #3 entry recording the three-way decision.
- `docs/design/nsl_compiler_design.md` §7 — `nsl.struct` op-summary
  row updated to reflect the relaxed parent.
- (No verifier-body edit; no new op records; no fixture retirements;
  no smoke-script changes for Phase A. Phase B will add T043 / T044
  fixtures + `structTable_` consumption — separate commit.)

**Constraints honoured**:

- No RTTI: trait machinery is TableGen-emitted; no `dyn_cast`-on-
  Operation-pointer code introduced.
- Determinism: trait check is a single immediate-parent test; no
  ordering surface added.
- Round-trip: the new top-level fixture is a fixed point under
  `nsl-opt %s | nsl-opt -` (verified locally).
- Principle III firewall: trait-only edit; M4 dialect remains the
  seam.
- Principle VII coupling: the spec / contract / design-doc updates
  land in the same commit as the code edit.

**Cross-references**:

- M4 spec: `specs/007-m4-mlir-dialect/spec.md` Clarifications
  session 2026-05-02 amendment #3.
- M4 contract: `specs/007-m4-mlir-dialect/contracts/
  dialect-api.contract.md` §2 post-merge note #3 (count unchanged).
- Design: `docs/design/nsl_compiler_design.md` §7 op summary
  (`nsl.struct` row).

---

## 18. M4-amendment 2026-05-02 (#4): bundled US2 + US3 unblock

**Decision**: Amend M4 (a fourth time) — bundled — to:

1. **Add two new top-level parameter ops**: `nsl.param_int`
   (`I64Attr:$value`) and `nsl.param_str` (`StrAttr:$value`), both
   Symbol-bearing with parent = `mlir::ModuleOp` (top-level
   placement, sibling of `nsl.module` per S16 + grammar §3.1).
2. **Add `OptionalAttr<StrAttr>:$loop_var` to
   `nsl.structural_generate`**: carries the loop variable name
   (e.g., `"i"` from `generate(i = 0..N)`).
3. **Add `OptionalAttr<I64Attr>:$array_size` to `nsl.submodule`**:
   carries the array size when the source spells `SUB[3] inst;`
   (NSL submodule-array form per FR-016).

**Why this is in the M5 research file** (rather than only M4): the
gaps were surfaced during M5 implementation. (1) was surfaced when
US2's `NSLResolveParamsPass` slot 1 implementation reached the
contract author's pen and needed an IR target — six M5 contracts
referenced `nsl.param_int` / `nsl.param_str` ops that didn't exist
in the M4 freeze surface. (2) was surfaced when M5's
`NSLExpandGeneratePass` realised the bare `lower`/`upper`/`step`
attrs (per amendment-cluster heritage) carried no loop-variable name
to substitute for `%IDENT%` residue in the per-iteration body
copies. (3) was surfaced by US3 design — `NSLExplodeSubmodArrayPass`
slot 4 needs an array-form input shape per FR-016, but the M4
`nsl.submodule` only spelled the singleton form. Recording the
four-way decision here keeps the implementation history honest about
the *fourth* M4 amendment.

**Four-way decision** (user-authorised option (B)):

- *(A) Emit CIRCT-side helpers directly from M5* — Rejected.
  Violates Principle III's architectural seam: M5 lowers AST →
  `nsl` dialect; the dialect must contain everything M5 produces.
  CIRCT-side mapping lives at M6 per design §10.
- *(B) Amend M4 with the bundled three changes* — **CHOSEN.**
  Two new ops + two OptionalAttr additions. Existing fixtures
  (`test/Dialect/module-level/submodule_roundtrip.mlir`,
  `test/Dialect/module-level/submodule_invalid_wrong_parent.mlir`,
  `test/Dialect/expansion-only/structural_generate_roundtrip.mlir`,
  `test/Dialect/expansion-only/structural_generate_invalid_bad_loop_attrs.mlir`)
  continue to verify and round-trip — the optional-attr defaults
  (empty StrAttr / absent I64Attr) preserve their printed form
  exactly. Freeze surface grows 77 → 79 (the two new ops only —
  field additions on existing ops do NOT add new op classes).
- *(C) Defer the structural-expansion passes to M6* — Rejected.
  Breaks the M5→M6 cut: `-emit=mlir` would emit semantically
  opaque IR with `%IDENT%` residue, undermining the inspectability
  guarantee in Principle V.
- *(D) Downgrade to stub-only ops* — Rejected. Postpones the
  M5→M6 cut and breaks Principle VIII test sequencing (FileCheck
  fixtures need the real IR shape, not stubs).

**Backward-compatibility guarantee**: The OptionalAttr additions
preserve existing fixtures byte-for-byte. `OptionalAttr<StrAttr>`
prints nothing (and parses absent) by default; an existing
`nsl.structural_generate attributes {lower = 0, upper = 8, step = 1}`
form prints exactly as it always did. `OptionalAttr<I64Attr>` with
the assemblyFormat `(`[` $array_size^ `]`)?` prints nothing when the
attr is absent; an existing `nsl.submodule @inst : @SUB` continues
to print as `nsl.submodule @inst : @SUB`. Confirmed end-to-end:
prior to this amendment 471/471 lit PASS; after this amendment
475/475 lit PASS (471 baseline preserved + 4 new round-trip
fixtures); 32/32 m5_smoke.sh ROUND-TRIP PASS unchanged.

**Why bundle?**: the three changes are individually small but
share a single cross-cutting motivation (M5 US2 + US3 implementation
contract author needs the M4 IR surface to refer to). Splitting
into three amendments would triple the documentation churn for
zero technical benefit; bundling matches the precedent set by
amendment cluster commits during the original M4 phase.

**Cost paid by this amendment**:

- `lib/Dialect/NSL/IR/NSLOps.td` — two new op records (`nsl.param_int`,
  `nsl.param_str`) + two OptionalAttr additions
  (`structural_generate.loop_var`, `submodule.array_size`) + a custom
  assemblyFormat clause on `nsl.submodule` for the bracketed array-
  size form. ~60 LOC including documentation comments.
- No verifier-body amendment. The two new ops are Symbol-bearing
  with `HasParent<"::mlir::ModuleOp">` — the trait machinery handles
  the structural invariants. The OptionalAttr additions are
  consumed at M5 pass time, not at M4 verifier time.
- Four new round-trip fixtures (~80 LOC).
- `specs/007-m4-mlir-dialect/contracts/dialect-api.contract.md` §2
  — post-merge amendment #4 note appended; freeze surface 77 → 79;
  ParamIntOp + ParamStrOp added to the symbol-export table.
- `specs/007-m4-mlir-dialect/spec.md` — Clarifications session
  2026-05-02 amendment #4 entry recording the four-way decision.
- `specs/008-m5-structural-passes/spec.md` Assumptions bullet —
  M4-frozen-count update 77 → 79.
- `docs/design/nsl_compiler_design.md` §7 op summary — two new ops
  added; field annotations on existing op rows.
- `docs/design/nsl_compiler_design.md` §10 mapping table —
  `nsl.param_int` → submodule parameter on `hw.instance`; the
  array-form `nsl.submodule` maps to per-element `hw.instance`;
  `structural_generate.loop_var` has no CIRCT counterpart since
  `nsl.structural_generate` is M5-eliminated.

**Constraints honoured**:

- No RTTI: trait machinery is TableGen-emitted; no `dyn_cast`-on-
  Operation-pointer code introduced.
- Determinism: OptionalAttr printer/parser is deterministic; new op
  printer/parser is deterministic (verified via two-pass
  `nsl-opt %.mlir | nsl-opt - | diff`).
- Round-trip: every new fixture is a fixed point under
  `nsl-opt %s | nsl-opt -` (verified locally).
- Principle III firewall: pure dialect-internal additions; no
  CIRCT-mapping code added at M4 (design §10 documents the M6
  mapping but no compile-time codegen).
- Principle VII coupling: spec / contract / data-model / research /
  design updates land in the same commit as the code edit.

**Cross-references**:

- M4 spec: `specs/007-m4-mlir-dialect/spec.md` Clarifications
  session 2026-05-02 amendment #4.
- M4 contract: `specs/007-m4-mlir-dialect/contracts/
  dialect-api.contract.md` §2 post-merge note #4 (count 77 → 79).
- Design: `docs/design/nsl_compiler_design.md` §7 op summary +
  §10 mapping table.
- M5 spec: `specs/008-m5-structural-passes/spec.md` Assumptions
  bullet for the M4-frozen op count.

---

## 19. M5 T109 — SC-010 M4-baseline regression check (2026-04-30)

**Trigger**: `tasks.md` T109 + `spec.md` SC-010 — `nslc -emit=tokens`
and `nslc -emit=ast` outputs over the M3 Sema pass-case corpus
(`test/sema/<sn>/pass.nsl`, 29 fixtures) MUST be byte-identical
between the M4 baseline (commit `b0d21ad`, the M4 merge head) and
the current M5 HEAD. SC-010 frames this as the cardinal regression-
guard against accidental M4-frozen-stage churn during the M5
amendment cycles.

**Method**:

1. Worktree the M4 merge head at `b0d21ad`:
   ```
   git worktree add $TMPDIR/m4-worktrees/baseline b0d21ad
   ```

2. Build M4 `nslc` with the same toolchain + flags
   (`-DCMAKE_BUILD_TYPE=Release -DNSL_ENABLE_ASAN=OFF`) inside the
   `ghcr.io/koyamanx/nsl-nslc:dev` container.

3. For each `test/sema/<sn>/pass.nsl` (29 fixtures), capture
   `nslc -emit=tokens` and `nslc -emit=ast` from BOTH the M4-baseline
   `nslc` and the M5-HEAD `nslc`. `diff -q` each pair.

**Result**: 0 token diffs + 0 AST diffs across 29 × 2 = 58 invocations.

```
M3 corpus: 29 fixtures
  -emit=tokens diffs: 0
  -emit=ast diffs: 0
```

**Interpretation**: SC-010 holds at HEAD. M5 introduced no
behavioural change in `-emit=tokens` or `-emit=ast` over the M3
corpus (consistent with the M4-FROZEN-AT-79 op-surface and the
no-change-to-M3-Sema-output invariant). The four M4 amendments
shipped on this branch (#1 `nsl.constant`, #2 expression op surface,
#3 top-level `nsl.struct`, #4 bundled US2+US3) all preserve the
upstream lex/parse/Sema printers byte-for-byte.

**Re-run**: `bash scripts/_regression_check_m4.sh` (not committed —
ad-hoc script captured in this offload). Future regressions can
recreate the worktree + diff loop in CI as a manual check; SC-010
does not require this to be on the per-PR critical path (the M3
corpus extension landed in T106 covers the per-PR golden-diff
guard).

**Cross-references**:

- Spec: `spec.md` SC-010.
- Tasks: `tasks.md` T109.
- M4 merge head: commit `b0d21ad`
  ("Merge pull request #9 from koyamanX/007-m4-mlir-dialect").
- Companion: §10 (M3 corpus extension) — T106 captures per-fixture
  `-emit=mlir` goldens; T109 here covers `-emit=tokens` /
  `-emit=ast` separately.

---

## 20. M4-amendment 2026-05-02 (#5): bundled five trait/type-constraint relaxations

**Trigger**: M5 close-out of the offload-2026-04-30 deferred bundle
(T035 ForBlock enum-form, T045 fire_probe proc/state targets, T066
+ T067 generate-with-reg body, T076 + T079 struct-typed variable,
T077 cross-scope variable in func) re-surfaced five M4-side
constraint mismatches, all left as deferred amendment-class blockers
in the previous offloads. None of the five requires a new op record
— each is a minimal "accept more" trait or type-constraint relax
on an existing op record. Aggregated as **amendment #5** because
the M5 consumption path is the same (the visitor + pass fan out
needed all five widenings to land before any of the deferred
fixtures could clear).

**Decision**: Option **(B)** — bundle the five relaxations into a
single M4 amendment (no new op records; freeze surface stays at
79). Same precedent as amendments #1–#4: the principle (Principle
III) requires that M5 lower AST → `nsl` dialect — the dialect
must contain everything M5 produces, including struct-typed
variables, func-scope wires, generate-body regs, 2-operand for
ops, and proc/state fire_probe targets. Options (A) emit CIRCT-
side helpers from M5, (C) defer the deferred items to M6 (opaque
IR), (D) downgrade to stub-only ops were rejected for the same
reasons as the prior amendments.

**Five relaxations**:

| # | Op | Change | M5 unblock |
|---|---|---|---|
| 1 | `nsl.for` | `step` operand: `NSL_BitsOrStruct` → `Variadic<NSL_BitsOrStruct>` (0 or 1) | T035 — ForBlock enum form |
| 2 | `nsl.fire_probe` | Verifier accepts sibling `nsl.proc` (module scope) + sibling `nsl.state` (per-proc scope) | T045 — proc/state probe targets |
| 3 | `nsl.reg` | Parent: `ParentOneOf<["ModuleOp", "ProcOp"]>` → adds `"StructuralGenerateOp"` | T066 + T067 — generate body regs |
| 4 | `nsl.variable` | Result type: `NSL_AnyBits` → `NSL_BitsOrStruct` | T076 + T079 — struct-typed variable |
| 5 | `nsl.wire` | Parent: `HasParent<"ModuleOp">` → `ParentOneOf<["ModuleOp", "FuncOp"]>` | T077 — cross-scope variable in func |

**Implementation footprint**:

- `lib/Dialect/NSL/IR/NSLOps.td`: 4 ops edited (RegOp, WireOp,
  VariableOp, ForOp) — trait list / arg constraint changes only.
- `lib/Dialect/NSL/IR/NSLOps.cpp`: 2 verifier bodies edited
  (`VariableOp::verify` to accept struct types; `ForOp::verify`
  to reject ≥ 2 step operands; `FireProbeOp::verify` extended
  with sibling-`nsl.proc` and sibling-`nsl.state` lookups via
  `mlir::SymbolTable::lookupSymbolIn`).
- `test/Dialect/`: 6 new round-trip fixtures (one per relaxation
  + the bonus state-target case under marker/).
- `test/Lower/passes/nsl-expand-variables/struct_typed.mlir`:
  XFAIL marker removed; banner updated to document the
  parse-clean post-amendment baseline (per-field decomposition
  is still a follow-up commit's deliverable).

**Backward compatibility**: All pre-existing fixtures continue
to PASS unchanged. Each relaxation is **strictly "accept more"**:

- 3-operand `nsl.for` still parses (variadic step accepts
  exactly-1).
- `nsl.fire_probe @func_in_target` still resolves (walk #1 is
  unchanged; the new walks are tried only after walk #1 misses).
- `nsl.reg` inside `nsl.module` or `nsl.proc` still parses.
- `nsl.variable : !nsl.bits<8>` still parses (the bits-side of
  the union constraint is unchanged).
- `nsl.wire` inside `nsl.module` still parses.

**Verifier-reject preservation**: All pre-existing invalid
fixtures continue to fail-as-expected. The relaxations don't
change what's REJECTED:

- `nsl.wire : !nsl.struct<@T>` is still rejected (wire's
  result-type constraint unchanged at `NSL_AnyBits`).
- `nsl.fire_probe @undefined_target` is still rejected (all
  three walks miss).
- `nsl.for` outside `nsl.seq` is still rejected (transitive-
  parent walk unchanged).

**Determinism (Principle V)**: Print/parse round-trip is
unchanged because no new attribute / operand encoding was added.
Verifier order in `FireProbeOp::verify` is fixed (walk #1 →
SymbolTable lookup at module scope → SymbolTable lookup at
enclosing proc scope), so the diagnostic ordering is
deterministic.

**Coverage gain**: lit suite grew 535 → 541 (+6 round-trip
fixtures); 521 → 527 PASS + 14 XFAIL (the
`nsl-expand-variables/struct_typed.mlir` XFAIL was retired in
the same commit because the parse-verify gate cleared).

**Cross-references**:

- Contract: `specs/007-m4-mlir-dialect/contracts/dialect-api.contract.md`
  §2 post-merge amendment 2026-05-02 (#5).
- Spec: `specs/007-m4-mlir-dialect/spec.md` Clarifications session
  2026-05-02 (post-merge amendment #5).
- Data model: `specs/007-m4-mlir-dialect/data-model.md` rows for
  `nsl.reg` / `nsl.wire` / `nsl.variable` / `nsl.for` /
  `nsl.fire_probe`.

---

## 21. M4-amendment 2026-04-30 (#6): single `nsl.seq` parent-trait relaxation

**Trigger**: M5 close-out triage of 5 XFAIL'd M3-corpus fixtures
(`test/Lower/m3_corpus/s{07,08,09,19,25}/pass.test`). Each fixture
emits a Sema-clean AST in which a `SeqBlock` lives directly inside
a `proc` body (S7 grammar shape) or inside a `state` body (S19/S25
labelled-`goto` shape). The visitor `visit(SeqBlock)` at
`lib/Lower/ASTToMLIR.cpp:565` is fully implemented and lowers each
to `nsl.seq` correctly — but the M4 op carried
`HasParent<"FuncOp">` (NSLOps.td:884), so the verifier rejected
the resulting IR before the golden could be captured.

**Decision**: Option **(B)** — single trait relaxation
`HasParent<"FuncOp"> → ParentOneOf<["FuncOp", "ProcOp", "StateOp"]>`.
Same precedent as amendments #3 and #5: a strictly "accept more"
trait relaxation that aligns the dialect's parent constraint with
NSL grammar (S7: `seq` permitted inside function or procedure
body). Options (A) emit CIRCT-side helpers from M5, (C) defer the
5 fixtures to M6, (D) downgrade `nsl.seq` to a stub-only op were
rejected for the same reasons as the prior amendments.

**Implementation footprint**:

- `lib/Dialect/NSL/IR/NSLOps.td`: 1 op edited (SeqOp) — single
  trait swap on the trait list. **Zero verifier code edits.**
- `test/Dialect/action-block/`: 1 new round-trip fixture
  (`seq_in_proc_roundtrip.mlir`) covering both the proc-direct
  and via-state placements.
- `test/Lower/m3_corpus/s{07,08,09,19,25}/pass.test`: XFAIL
  markers removed; goldens regenerated; standard `nslc -emit=mlir`
  + `diff` form restored.

**Backward compatibility**: All pre-existing fixtures continue
to PASS unchanged.

- `test/Dialect/action-block/seq_roundtrip.mlir` (immediate-parent
  `nsl.func` form) — still parses; `FuncOp` is still in the
  parent set.
- `test/Dialect/action-block/seq_invalid_wrong_parent.mlir` (`nsl.seq`
  directly under `nsl.module`) — still rejected; `nsl.module` is
  not in the parent set, so the diagnostic substring "expects
  parent op" continues to fire.

**Verifier-reject preservation**: The relaxation does NOT change
what's REJECTED. `nsl.seq` directly under a `nsl.module`,
`nsl.alt`, `nsl.parallel`, etc. is still trait-rejected.

**Determinism (Principle V)**: Print/parse round-trip is unchanged
because no new attribute / operand encoding was added; the trait
list is consulted only by the verifier, not the printer/parser.

**Coverage gain**: 5 M3-corpus XFAIL fixtures (s07/s08/s09/s19/s25)
flip to GREEN; lit XFAIL count drops by 5 from 13 to 8.

**Cross-references**:

- Contract: `specs/007-m4-mlir-dialect/contracts/dialect-api.contract.md`
  §2 post-merge amendment 2026-04-30 (#6).
- Spec: `specs/007-m4-mlir-dialect/spec.md` Clarifications session
  2026-04-30 (post-merge amendment #6).
- Data model: `specs/007-m4-mlir-dialect/data-model.md` row for
  `nsl.seq`.
- Grammar anchor: `docs/spec/nsl_lang.ebnf` §8 line 850 (S7).

---

## 22. M5 fix-bug offload 2026-04-30: `lowerExpr` width-inference plumbing

**Decision** — Add an optional `mlir::Type typeHint` parameter to
`ASTToMLIR::lowerExpr` (header default `nullptr`); thread it through
the recursive expression walk so unsized literals widen to the
context-supplied type at lowering time. Pair this with a sized-
literal spelling parser that consumes the `<W>'<base><digits>` form
(`2'd0`, `4'b0000`, `8'h2A`) verbatim, ignoring the hint when an
explicit width is present.

**Why** — Pre-fix, `lowerExpr(LiteralExpr)` defaulted every numeric
literal to `!nsl.bits<1>` and ignored the sized-literal prefix
embedded in the spelling. Two failure modes ensued at the trait-
verifier layer:

  1. **`SameOperandsAndResultType` (arith / bitwise / shift)** —
     `q := a + 1` where `q` is `reg[8]` lowered as `nsl.add %a,
     %const1 : !nsl.bits<8>` with `%const1` as `!nsl.bits<1>` — the
     trait rejects the type mismatch.
  2. **`SameTypeOperands` (eq/ne/lt/le/gt/ge/land/lor)** — `i < 8`
     where `i` is `reg[4]` lowered with `%const8 : !nsl.bits<1>` —
     and additionally the M4 `nsl.constant` verifier rejects
     "value 8 doesn't fit in `!nsl.bits<1>`".
  3. **Sized literals at the spelling layer** — `acc = 2'd0` failed
     because the parser didn't consume the `2'd...` prefix; the
     literal lowered as `!nsl.bits<1>` value 0 even though the
     spelling clearly specified width 2.

The fix is purely visitor-side; no M4 amendment was necessary. M4's
49+28 op surface stays frozen at 77 (sticking to the post-merge
amendment cluster #1+#2+#3+#4+#5+#6 ceiling).

**Lowering rule** (per `BinaryExpr` arith/bitwise/shift): lower LHS
first (with the outer `typeHint`), capture its `mlir::Type`, then
lower RHS with that type as RHS hint. The result type is inferred
from the operands. For comparisons / logical ops (result `bits<1>`):
same LHS-first / RHS-hinted-by-LHS pattern. For `ConditionalExpr`:
lower then-branch first (with outer hint), use its type as else-
branch hint. For `SignExtendExpr` / `ZeroExtendExpr`: hint the sub
with the result type (the explicit prefix dictates result width;
sized literals inside still self-resolve, unsized ones widen to fit
— a no-op extend `bits<N>` → `bits<N>` is structurally legal since
the verifier asserts `N ≥ M`). For `TransferStmt` / `IfStmt` /
`WhileBlock` / `ForBlock` / `AltBlock` / `AnyBlock`: hint cond with
`bits<1>`; for transfer specifically, lower LHS first and use its
type as RHS hint.

**Soft-fail bail-on-null-LHS** — When LHS lowering returns null
(e.g., `op == 0` where `op` is an unresolved Input port at Phase 3),
the recursive lowering bails BEFORE lowering RHS. This avoids
leaking RHS-side `nsl.constant` ops into the IR that would later
trip the module-level verifier (e.g., `nsl.constant 2 :
!nsl.bits<1>` at module level after the comparison's case-arm was
otherwise discarded).

**Erase-on-empty-body for AltOp / AnyOp** — Both ops require ≥ 1
case-or-default child. When all case-conds soft-fail and there's no
else clause, the visitor now records whether any child was emitted;
if none, it erases the parent op rather than leaving an invalid
body for the verifier to flag. AltBlock with an `else` clause still
emits the DefaultOp regardless of cond-resolution.

**Outcomes** —

  - 7 of 9 failing `examples/*.nsl` close (Category A: width
    inference). Only Category B examples (`01_hello.nsl`,
    `20_simulation_tb.nsl`) remain — both fail on the
    `nsl.sim_finish` parent constraint inside `nsl.sim_init`, which
    is **amendment territory** (`nsl.sim_finish`'s
    `HasParent<"ModuleOp">` would need relaxation to
    `ParentOneOf<["ModuleOp", "SimInitOp"]>`). Per offload guidance,
    that's deferred to M4 amendment #7 + escalation.
  - M3-corpus s09 fixture flips XPASS → real PASS. Golden recorded;
    XFAIL marker dropped from `pass.test`.
  - s12 fixture (partial-assignment `v[3:0] = 0`) remains XFAIL on a
    different blocker: the visitor emits `transfer(extract(v, 3:0),
    0)` but `NSLExpandVariablesPass` only walks the variable's parent
    block and crashes on the nested-region use. Fix is independent
    of the width-inference change; deferred.
  - lit count: 535 PASS + 9 XFAIL → 536 PASS + 8 XFAIL.

**Determinism** — All passing examples and the s09 golden round-trip
byte-identically across two consecutive `nslc -emit=mlir` runs
(Constitution Principle V holds).

## Cross-references

- Spec: [`spec.md`](./spec.md) (FR-001 … FR-031, SC-001 … SC-012)
- Constitution: [`.specify/memory/constitution.md`](../../.specify/memory/constitution.md)
  (Principles II, III, IV, V, VI, VII, VIII, IX)
- Compiler design: [`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md)
  §§3 (layered architecture), 8 (AST → nsl), 9 (passes), 10 (nsl → CIRCT, NOT delivered at M5),
  11 (Compilation class)
- M3 Sema contract: [`specs/006-m3-sema/contracts/sema-api.contract.md`](../006-m3-sema/contracts/sema-api.contract.md)
  (Invariant 7 — symbol-table iteration order)
- M4 dialect contract: [`specs/007-m4-mlir-dialect/contracts/dialect-api.contract.md`](../007-m4-mlir-dialect/contracts/dialect-api.contract.md)
  (77 frozen public types; the post-merge amendment 2026-05-01 grew it
  from 48 to 49 by adding `nsl.constant` — see §15 below; the post-
  merge amendment 2026-05-02 grew it from 49 to 77 by adding the 28-op
  expression surface — see §16)
- M1 preprocessor contract: M1 frozen surface for `%IDENT%` forwarding behaviour (Q1 alternative A reasoning)
- Quickstart: [`quickstart.md`](./quickstart.md) (developer onboarding)
- Data model: [`data-model.md`](./data-model.md) (entity catalog)
- Contracts: [`contracts/`](./contracts/) (four `.contract.md` files)
