<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Data Model: M6 — `nsl-lower` part 2

**Branch**: `010-m6-circt-lowering` | **Date**: 2026-05-04
**Plan**: [plan.md](./plan.md)

This document catalogs the entities introduced or extended at M6:
their fields, relationships, lifecycle, and the validation rules
they impose. M6 is a compiler-engineering milestone — most
"entities" are C++ classes, MLIR ops (M4-frozen here, referenced),
or fixture-corpus shapes (FR-032 / FR-033 mandated). The CIRCT
op surface M6 produces is a *consumed* surface (CIRCT's own
TableGen ODS); M6 does not own those op definitions.

---

## 1. `nsl::lower::NSLToCIRCTPass` (pass class)

**Layer**: 8b (private, lib-internal — declared in
`lib/Lower/NSLToCIRCTPass.h`).

**Class shape** (frozen by FR-001 + FR-002):

```cpp
namespace nsl::lower {

class NSLToCIRCTPass
    : public mlir::PassWrapper<NSLToCIRCTPass,
                               mlir::OperationPass<mlir::ModuleOp>> {
public:
    NSLToCIRCTPass();

    mlir::StringRef getArgument() const final {
        return "nsl-to-circt";  // FR-002
    }

    mlir::StringRef getDescription() const final {
        return "Lower nsl::* dialect ops to CIRCT (hw/comb/seq/fsm/sv)";
    }

    void getDependentDialects(
        mlir::DialectRegistry& registry) const override;

    void runOnOperation() override;
};

// Public-surface factory + registration (frozen by
// lower-api.contract.md §6 amendment for M6).
std::unique_ptr<mlir::Pass> createNSLToCIRCTPass();
void registerNSLToCIRCTPass();

}  // namespace nsl::lower
```

**Lifecycle**:
- Constructed at `Compilation::lowerToCIRCT` time once per
  invocation, via `createNSLToCIRCTPass()`.
- Standalone-usable from `nsl-opt -nsl-to-circt %s` per FR-002.
- `getDependentDialects` registers `circt::hw::HWDialect`,
  `circt::comb::CombDialect`, `circt::seq::SeqDialect`,
  `circt::fsm::FSMDialect`, `circt::sv::SVDialect` so the
  PassManager loads them before the pass runs.
- `runOnOperation` constructs the `ConversionTarget` + `TypeConverter`
  + `RewritePatternSet`, invokes `applyFullConversion`, and on
  failure calls `signalPassFailure()` after the
  `DiagnosticBridge` has translated MLIR diagnostics through
  `basic::DiagnosticEngine`.

**Validation rules**:
- The pass MUST NOT mutate input IR on failure (MLIR's
  `applyFullConversion` already enforces atomicity — partial
  rewrites are rolled back automatically).
- The pass MUST produce zero ops in the `nsl` dialect on success
  (the conversion target's illegal-dialect rule enforces this).
- The pass MUST inherit `mlir::Location` onto every CIRCT op (per
  pattern; `OpConversionPattern::rewriter.create<>(...)` receives
  `loc` explicitly, sourced from `op->getLoc()` of the source
  `nsl::*` op).

---

## 2. `nsl::lower::CIRCTTypeConverter` (TypeConverter)

**Layer**: 8b (private — declared in
`lib/Lower/CIRCTTypeConverter.h`).

**Class shape**:

```cpp
namespace nsl::lower {

class CIRCTTypeConverter : public mlir::TypeConverter {
public:
    CIRCTTypeConverter(mlir::MLIRContext& ctx,
                       const sema::TypeSystem& typeSystem);

private:
    const sema::TypeSystem& typeSystem_;

    // Internal width arithmetic for !nsl.struct<@T>.
    unsigned computeStructPackedWidth(
        nsl::dialect::StructType t) const;
};

}  // namespace nsl::lower
```

**Conversion rules** (registered in constructor):

| Source type (M5 IR) | Target type (CIRCT IR) | Rule |
|---|---|---|
| `!nsl.bits<W>` | `iW` | Direct width-preserving conversion |
| `!nsl.struct<@T>` | `iN` (where N = `typeSystem_.sizeOf(@T)`) | S18 MSB-first packing — first field at MSB end |
| `!nsl.bits<1>` (control terminal) | `i1` | Same as general `!nsl.bits<W=1>` |
| Built-in MLIR types (`IntegerType`, etc.) | identity | passed through |
| Unhandled `!nsl.*` types | conversion failure | triggers ConversionTarget illegal verdict |

**Materialization rules** (for cross-pattern value handoff):
- Source materialization for `!nsl.bits<W>` ↔ `iW` produces a
  `comb::BitcastOp` (no-op width-preserving bitcast).
- Target materialization is the same.
- No materialization registered for `!nsl.struct<@T>` — struct-
  typed SSA values are fully eliminated by M5's
  `NSLExpandVariablesPass`; reaching M6 with one is an M5-pass
  bug (FR-022).

---

## 3. `mlir::OpConversionPattern<nsl::*>` subclasses

**Layer**: 8b (private — one `.cpp` per family under
`lib/Lower/CIRCTPatterns/`).

**Family taxonomy** (one file per family, totaling 9 files,
~40 patterns total — one per design-§10 mapping-table row):

| Family file | Pattern count | Source ops |
|---|---|---|
| `ModulePatterns.cpp` | 2 | `nsl::ModuleOp`, `nsl::DeclareOp` (port wiring on the module's signature; both ops added by post-merge M4-amendment #9, 2026-05-05). The ModuleOp pattern walks the paired `nsl::DeclareOp`'s body for the port-list shape and rewrites the in-module port-info ops as block-arg substitutions / output wiring. |
| `PortPatterns.cpp` | 3 | `nsl::InputPortOp`, `nsl::OutputPortOp`, `nsl::InoutPortOp` (post-merge M4-amendment #9). Per-port-direction signature legalisation. The dual-placement design (declare-body = metadata; module-body = SSA-Value-bearing) means the ModulePatterns/PortPatterns pair walks both surfaces — see `contracts/circt-lowering.contract.md` §3 for the rewrite rules. |
| `StatePatterns.cpp` | 5 | `nsl::RegOp`, `nsl::WireOp`, `nsl::MemOp`, `nsl::TransferOp`, `nsl::ClockedTransferOp` |
| `ControlPatterns.cpp` | 4 | `nsl::AltOp`, `nsl::AnyOp`, `nsl::IfOp`, `nsl::CallOp` (func_in variant) |
| `FSMPatterns.cpp` | 7 | `nsl::ProcOp`, `nsl::StateOp`, `nsl::SeqOp`, `nsl::FirstStateOp`, `nsl::GotoOp` (state form), `nsl::GotoOp` (label form), `nsl::FinishOp`, `nsl::CallOp` (proc_name variant) |
| `ArithPatterns.cpp` | 9 | `nsl::AddOp`, `SubOp`, `MulOp`, `EqOp`, `NeOp`, `LtOp`, `LeOp`, `GtOp`, `GeOp` |
| `BitOpPatterns.cpp` | 19 | `nsl::AndOp`, `OrOp`, `XorOp`, `ShlOp`, `ShrOp`, `LandOp`, `LorOp`, `NotOp`, `NegOp`, `LnotOp`, `ReduceAndOp`, `ReduceOrOp`, `ReduceXorOp`, `SignExtendOp`, `ZeroExtendOp`, `MuxOp`, `ConcatOp`, `ExtractOp`, `RepeatOp` |
| `SimPatterns.cpp` | 4 + S29 | `nsl::SimDisplayOp`, `SimFinishOp`, `SimInitOp`, `SimDelayOp`, plus the S29 `_init` block lowering |
| `ParamPatterns.cpp` | 3 | `nsl::ParamIntOp`, `ParamStrOp`, `SubmoduleOp` |

(Pattern counts above are the per-op count, totaling **42** patterns
across the 9 family files (post-merge M4-amendment #9 adds
`InputPortOp`/`OutputPortOp`/`InoutPortOp` to PortPatterns, growing
the row from 1 to 3). The design-§10 mapping-table rows are the
contract surface — see [`contracts/circt-lowering.contract.md`](../contracts/circt-lowering.contract.md)
§1 — and they bound the pattern count from below. Adding a new op
to the M4 dialect adds one new pattern AND one new fixture AND one
new contract row in lock-step.)

**Pattern interface** (every pattern extends `mlir::OpConversionPattern`):

```cpp
template <typename SourceOp>
class NSLToCIRCT_<SourceOp>_Pattern
    : public mlir::OpConversionPattern<SourceOp> {
public:
    using mlir::OpConversionPattern<SourceOp>::OpConversionPattern;

    mlir::LogicalResult matchAndRewrite(
        SourceOp op,
        typename mlir::OpConversionPattern<SourceOp>::OpAdaptor adaptor,
        mlir::ConversionPatternRewriter& rewriter) const override;
};
```

**Family populate-helpers** (FR-pattern-organization decision per
research §11):

```cpp
// Each family exposes one populate function called from
// NSLToCIRCTPass::runOnOperation.
void populateModulePatterns(mlir::RewritePatternSet&,
                            CIRCTTypeConverter&);
void populatePortPatterns(mlir::RewritePatternSet&,
                          CIRCTTypeConverter&);
void populateStatePatterns(mlir::RewritePatternSet&,
                           CIRCTTypeConverter&);
void populateControlPatterns(mlir::RewritePatternSet&,
                             CIRCTTypeConverter&);
void populateFSMPatterns(mlir::RewritePatternSet&,
                         CIRCTTypeConverter&);
void populateArithPatterns(mlir::RewritePatternSet&,
                           CIRCTTypeConverter&);
void populateBitOpPatterns(mlir::RewritePatternSet&,
                           CIRCTTypeConverter&);
void populateSimPatterns(mlir::RewritePatternSet&,
                         CIRCTTypeConverter&);
void populateParamPatterns(mlir::RewritePatternSet&,
                           CIRCTTypeConverter&);
```

**Validation rules**:
- Each pattern MUST replace its source op via
  `rewriter.replaceOp(op, newValues)` or
  `rewriter.replaceOpWithNewOp<>(...)` — never plain
  `op->erase()`.
- Each pattern MUST source `mlir::Location` from `op->getLoc()`
  for every CIRCT op it creates.
- Patterns MUST NOT call `getRewriter()` outside `matchAndRewrite`
  (the rewriter is request-scoped).

---

## 4. `nsl::driver::Compilation::lowerToCIRCT` (member)

**Layer**: 9 (driver, public — declared in
`include/nsl/Driver/Compilation.h`).

**Signature** (frozen by FR-024 + FR-027):

```cpp
class Compilation {
public:
    // ... existing members from M3/M4/M5 ...

    /// Runs NSLToCIRCTPass over `module` in-place.
    /// Returns mlir::success() if all nsl::* ops converted.
    /// Failures are reported through DiagnosticEngine.
    mlir::LogicalResult lowerToCIRCT(mlir::ModuleOp module);

private:
    // ... existing private members ...
};
```

**Lifecycle**:
- Called from `Compilation::emit` only when
  `opts_.emit == EmitKind::HW`.
- Internally: constructs a `mlir::PassManager`, adds the
  `NSLToCIRCTPass` (via `createNSLToCIRCTPass()`), instantiates a
  `DiagnosticBridge`, and calls `pm.run(module)`.
- Returns the PassManager's result directly.

**Validation rules**:
- MUST NOT be called before `lowerToNSL` + `runNSLPasses` have
  succeeded on the same `module`.
- MUST NOT print or otherwise observe the IR — printing happens
  in `Compilation::emit` after `lowerToCIRCT` returns.

---

## 5. `nsl::driver::CompileOptions::EmitKind::HW`

**Layer**: 9 (driver, public — declared in
`include/nsl/Driver/CompileOptions.h`).

**Status at M6**: enum value reserved at M3 (per design §11 line
1281); M6 transitions it from "reserved/stubbed" to "operational".
No enum-value addition. The frozen enum text is:

```cpp
enum class EmitKind {
    Tokens,    // M1
    AST,       // M2
    NSLMLIR,   // M5  (-emit=mlir)
    CIRCT,     // M6  RESERVED at M3; alias for HW at M6 — see below
    HW,        // M6  (-emit=hw)
    Verilog,   // M7  reserved
};
```

**Note on `CIRCT` vs `HW` enum values**: M3 reserved both
`EmitKind::CIRCT` and `EmitKind::HW` per design §11 line 1281,
treating them as distinct potential emission stages. M6 elects to
**alias `CIRCT` to `HW`** in `Compilation::emit` (the same
`lowerToCIRCT` + print path serves both flag spellings). This
avoids producing two distinct emission stages with identical IR
shape; if a future divergence emerges (e.g., `-emit=circt` adds
`circt-opt -canonicalize` post-conversion), the alias breaks
trivially. Both `nslc -emit=hw` and `nslc -emit=circt` produce
byte-identical output at M6.

---

## 6. `nsl-lower` library link surface

**Layer**: 8 (build-system declaration — `lib/Lower/CMakeLists.txt`).

**Status at M6**:

```cmake
# M5 baseline (unchanged):
add_nsl_library(
  nsl-lower
  ASTToMLIR.cpp
  DiagnosticBridge.cpp
  NSLResolveParamsPass.cpp
  NSLExpandGeneratePass.cpp
  NSLExpandVariablesPass.cpp
  NSLExplodeSubmodArrayPass.cpp
  NSLInlineInternalFuncPass.cpp
  NSLCheckSemanticsPass.cpp
  # M6 additions:
  NSLToCIRCTPass.cpp
  CIRCTTypeConverter.cpp
  CIRCTPatterns/ModulePatterns.cpp
  CIRCTPatterns/PortPatterns.cpp
  CIRCTPatterns/StatePatterns.cpp
  CIRCTPatterns/ControlPatterns.cpp
  CIRCTPatterns/FSMPatterns.cpp
  CIRCTPatterns/ArithPatterns.cpp
  CIRCTPatterns/BitOpPatterns.cpp
  CIRCTPatterns/SimPatterns.cpp
  CIRCTPatterns/ParamPatterns.cpp
  DEPENDS
    nsl-sema
    nsl-dialect
  LINK_LIBS
    MLIRTransforms      # M5 (DialectConversion)
    CIRCTHW             # M5 declared inert; M6 active
    CIRCTComb           # ditto
    CIRCTSeq            # ditto
    CIRCTSV             # ditto
    CIRCTFSM            # M6 NEW — FSM dialect for proc/state/seq lowering
)
```

**Validation rules** (Q1 → A enforces):
- `LINK_LIBS` MUST NOT add `CIRCTHwArith` (Q1 → A: `comb`-only
  arithmetic).
- `LINK_LIBS` MAY drop `CIRCTSV` if and only if M5 already
  declared it; M6 does not re-declare (the M5 baseline is
  authoritative).

---

## 7. Public-header umbrella surface (`Lower.h`)

**Layer**: 8 (public header — `include/nsl/Lower/Lower.h`).

**M5 contract**: 8 symbols (per
[`specs/008-m5-structural-passes/contracts/lower-api.contract.md`](../008-m5-structural-passes/contracts/lower-api.contract.md)
§2 freeze list).

**M6 amendment**: +2 symbols, total **10**.

| Symbol | Source | Status |
|---|---|---|
| `mlir::OwningOpRef<mlir::ModuleOp> astToMLIR(...)` | M5 | Unchanged |
| `std::unique_ptr<mlir::Pass> createNSLResolveParamsPass()` | M5 | Unchanged |
| `std::unique_ptr<mlir::Pass> createNSLExpandGeneratePass()` | M5 | Unchanged |
| `std::unique_ptr<mlir::Pass> createNSLExpandVariablesPass()` | M5 | Unchanged |
| `std::unique_ptr<mlir::Pass> createNSLExplodeSubmodArrayPass()` | M5 | Unchanged |
| `std::unique_ptr<mlir::Pass> createNSLInlineInternalFuncPass()` | M5 | Unchanged |
| `std::unique_ptr<mlir::Pass> createNSLCheckSemanticsPass()` | M5 | Unchanged |
| `void registerNSLPasses()` | M5 | Unchanged (M6 extends body to also register the new pass — but the ABI is preserved; calling sites do not change) |
| `std::unique_ptr<mlir::Pass> createNSLToCIRCTPass()` | M6 NEW | Frozen at M6 |
| `void registerNSLToCIRCTPass()` | M6 NEW | Frozen at M6 |

**Validation rules**:
- The frozen list above is the M6 contract; adding an 11th symbol
  is an M6 contract amendment (separate spec change), not a
  silent surface growth.
- `registerNSLPasses()` body MAY be extended at M6 to also call
  `registerNSLToCIRCTPass()` (one-liner internal-only change;
  ABI is preserved).

---

## 8. Test-corpus shape (`test/Lower/circt/`)

**Layer**: 8b test surface (FR-032 + FR-033 mandated).

**Directory tree**:

```text
test/Lower/circt/
├── coverage_guard.cmake     # CI mechanism (FR-033)
├── module/                  # US2 axis (~10 .nsl + .mlir.expected pairs)
│   ├── port_input_only.nsl
│   ├── port_output_only.nsl
│   ├── port_mixed.nsl
│   ├── interface_modifier.nsl
│   ├── implicit_clk_rstn.nsl
│   ├── submodule_singleton.nsl
│   ├── submodule_with_param.nsl
│   ├── empty_module.nsl
│   └── …
├── fsm/                     # US3 axis (~10 pairs)
│   ├── single_state.nsl
│   ├── two_state_goto.nsl
│   ├── first_state_not_first.nsl
│   ├── finish_to_sink.nsl
│   ├── seq_inside_func.nsl
│   ├── proc_call_to_proc.nsl
│   └── …
├── arith/                   # US4 axis (~9 pairs — one per ArithPatterns row)
│   ├── add.nsl
│   ├── sub.nsl
│   ├── mul.nsl
│   ├── eq.nsl
│   ├── ne.nsl
│   ├── lt.nsl
│   ├── le.nsl
│   ├── gt.nsl
│   └── ge.nsl
├── state/                   # US4 axis (~8 pairs)
│   ├── reg_basic.nsl
│   ├── reg_with_init.nsl
│   ├── reg_with_interface.nsl    # Q2: explicit-interface path → seq.compreg
│   ├── reg_with_if.nsl           # Q3: mux-on-data conditional
│   ├── reg_with_chained_if.nsl   # Q3: nested mux
│   ├── wire_basic.nsl
│   ├── mem_basic.nsl
│   └── transfer_combinational.nsl
├── control/                 # US4 axis (~10 pairs)
│   ├── alt_priority.nsl          # S13 priority semantics
│   ├── any_parallel.nsl          # S13 parallel semantics
│   ├── if_wire_lhs.nsl           # comb.mux for wire LHS
│   ├── if_reg_lhs.nsl            # mux-on-data per Q3 → A
│   ├── call_func_in.nsl          # combinational + valid signal
│   └── …
├── sim/                     # US4 axis (~6 pairs)
│   ├── sim_display.nsl
│   ├── sim_finish.nsl
│   ├── sim_init.nsl
│   ├── sim_delay.nsl
│   ├── s29_init_block.nsl        # Q1-specify-time → B: sv.initial under ifdef
│   └── multi_sim_per_module.nsl  # research §9: shared sv.ifdef
└── round_trip/              # US5 axis (~8 .nsl files; assertion is
                             #   byte-stable double-emit + circt-opt-clean)
    ├── small_cpu_subset.nsl
    ├── handshake_pattern.nsl
    ├── memory_array.nsl
    ├── …
```

**Fixture-format conventions** (FR-032 + research §14):

- Each `<op>.nsl` is the smallest legal NSL source exercising one
  design-§10 row (or one US axis); typically 5–20 lines.
- Each `<op>.mlir.expected` is the FileCheck-asserted post-pass IR
  shape. FileCheck directives (`CHECK:`, `CHECK-NEXT:`,
  `CHECK-NOT:`) are inline in the source file or in a sibling
  `<op>.test` lit recipe.
- Lit recipe form: `RUN: nslc -emit=hw %s | FileCheck %s`. Each
  fixture exercises one path; multi-path fixtures are split.

**CI coverage guard** (`coverage_guard.cmake`):
- At configure time, walks `lib/Lower/CIRCTPatterns/*.cpp` looking
  for `populate*Patterns` body lines registering pattern types.
- Walks `test/Lower/circt/<family>/*.nsl` collecting filenames.
- Asserts: every registered pattern name has a matching fixture
  filename (modulo the snake-case normalization).
- Failure mode: configure fails with a list of pattern↔fixture
  mismatches.

---

## 9. Diagnostic plumbing extension (M5's `DiagnosticBridge` reused)

**Layer**: 8b (private — uses M5's `lib/Lower/DiagnosticBridge.h`).

**Status at M6**: zero changes. The class shape M5 froze:

```cpp
class DiagnosticBridge {
public:
    DiagnosticBridge(mlir::MLIRContext& ctx,
                     basic::DiagnosticEngine& diag);
    ~DiagnosticBridge();
    // RAII: registers ScopedDiagnosticHandler on construction;
    // unregisters on destruction.
private:
    mlir::ScopedDiagnosticHandler handler_;
};
```

is reused as-is. M6's `Compilation::lowerToCIRCT` instantiates one
`DiagnosticBridge` per invocation (mirroring M5's
`lowerToNSL` / `runNSLPasses` pattern).

**No new fields, no new translations** — the M5 mapping table from
`mlir::DiagnosticSeverity` to `basic::DiagnosticEngine::Severity`
is sufficient for M6's diagnostic needs.

---

## 10. Forward-non-deliverables (M6 contract surface only)

The following M5/M4 surfaces are **frozen at M6** — touching them
constitutes an M5/M4 contract amendment, not an M6 stealth change:

- **M4 dialect API** (`specs/007-m4-mlir-dialect/contracts/dialect-api.contract.md`):
  every `nsl::*` op definition, type, attribute, and verifier.
- **M5 `Lower.h` 8-symbol public surface** (per
  `specs/008-m5-structural-passes/contracts/lower-api.contract.md`):
  the 8 symbols listed in §7 above.
- **M5 pass-pipeline contract** (`specs/008-m5-structural-passes/contracts/pass-pipeline.contract.md`):
  six-pass ordering and per-pass post-conditions.
- **M5 driver-emit-mlir contract** (`specs/008-m5-structural-passes/contracts/driver-emit-mlir.contract.md`):
  `-emit=mlir` flag behaviour.
- **M5 residue-detection contract** (`specs/008-m5-structural-passes/contracts/residue-detection.contract.md`):
  regex + scanned-attr-table.
- **M3 driver `EmitKind` enum** (per design §11 line 1281): the
  six enum values and their reserved/operational status. M6
  transitions `HW` from reserved → operational; the other values
  are unchanged.

A change to any of the above outside M6's spec surface is a
Principle VII coupling violation.
