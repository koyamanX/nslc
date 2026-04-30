<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Research: M4 — `nsl` MLIR Dialect (`nsl-dialect`)

**Branch**: `007-m4-mlir-dialect` | **Date**: 2026-04-30
**Plan**: [plan.md](./plan.md)

This file resolves every Technical Context decision with a
**Decision / Rationale / Alternatives considered** entry, mirroring
the pattern established in
[`specs/006-m3-sema/research.md`](../006-m3-sema/research.md).
Each decision is anchored in the Constitution, the design docs, the
spec FRs, the prior-milestone precedent, or upstream MLIR/CIRCT
conventions — no decision is made on "engineering taste" alone.

---

## 1. Verifier strictness: structural-only (Q1 Option A)

**Decision**: Each `nsl.*` op's `hasVerifier = 1` hook checks
**structural invariants only** — parent-op kind, region count + kind,
attribute presence/type, and operand-result type relations declared
via TableGen traits (`SameOperandsElementType`, `SameOperandsShape`,
`SingleBlockImplicitTerminator`, etc.). Re-checking of `S1`–`S29`
semantic constraints is OUT of scope at M4.

**Rationale**: Resolves Clarifications session 2026-04-30 Q1 →
Option A verbatim. Preserves the architectural seam between Sema
(the single semantic checker; M3) and the dialect (the IR; M4) —
Sema runs before any AST→MLIR lowering at M5, so by the time IR
reaches the dialect's verifier on the AST-built path, every `Sn`
violation has already been caught and reported. Hand-written `.mlir`
test fixtures bypassing Sema are an M4 developer/test artifact, not
a user surface; a fixture author writing semantically-malformed
`.mlir` is responsible for that. Aligns with Constitution Principle
III ("stock CIRCT below the dialect" → clean separation of
concerns) and matches MLIR upstream conventions: CIRCT's `hw`,
`comb`, `seq`, `fsm` dialects all verify structural invariants only,
not source-language semantics.

**Alternatives considered**:

- *Option B (structural + cheap post-Sema invariants)*: Re-check a
  curated subset of `S2`/`S23`/`S28` etc. in the dialect verifier.
  **Rejected** because every "cheap" re-check is a policy decision
  that drifts: Sema's wording can shift across patches while the
  dialect's lags, producing diagnostic-text divergence. Maintenance
  cost over the project lifetime exceeds the diagnostic value.
- *Option C (full Sn re-check)*: Duplicate Sema's per-`Sn` walks at
  the dialect layer. **Rejected** because Sema and the dialect
  become two competing semantic checkers, both authoritative, both
  maintained — an architectural anti-pattern explicitly forbidden
  by the Principle III seam. Verifier code roughly doubles for
  diagnostic value already provided by Sema.

---

## 2. Parent-op invariant style: TableGen `HasParent` for immediate, hand-walk for transitive (Q2 Option B)

**Decision**: Spec FR-013's parent-op invariants split by row into
two implementation styles:

- Rows specifying `parent = X` (immediate parent) MUST use the
  standard MLIR `HasParent<X>` TableGen trait — no hand-written
  body for that check, the trait handles diagnostic emission.
- Rows specifying `parent (transitively) = X` (any-ancestor) MUST
  use a hand-written `verify()` body that walks
  `op->getParentOp()` upward until it finds an ancestor of kind X
  or hits the top of the op tree. On hitting the top without
  finding X, emit `op->emitOpError("must be enclosed by 'nsl.X'")`.

**Rationale**: Resolves Clarifications session 2026-04-30 Q2 →
Option B verbatim. NSL grammar permits intervening control-flow ops
(`nsl.parallel`, `nsl.alt`, `nsl.any`, `nsl.if`) between an enclosing
`nsl.seq` and a contained `nsl.while` body; strict immediate-parent
(Option A) would force the M5 AST→MLIR lowering to rewrite block
shapes or insert wrapper ops, distorting the IR's structural
fidelity to the AST. Option C (widened immediate-parent set) would
silently accept `nsl.alt { nsl.while }` without any enclosing
`nsl.seq` — a `S8` violation that the dialect should still surface
even though Sema also catches it at M3 (the dialect-layer "must be
enclosed by 'nsl.seq'" diagnostic is still useful for hand-written
fixtures and refactoring regressions). The hand-walk is a ~5-line
helper (`findAncestorOfKind<T>(op)`) reused across the ~5 transitive-
parent ops; total verifier-code burden is small.

**Alternatives considered**:

- *Option A (immediate parent only)*: Rejected per Q2 — would force
  M5 lowering to insert wrapper ops or rewrite block shapes,
  distorting IR-AST structural fidelity.
- *Option C (widened immediate-parent set)*: Rejected per Q2 — laxer
  than NSL grammar; silently accepts `nsl.alt { nsl.while }`
  without `nsl.seq` enclosure.

---

## 3. TableGen file partition: three .td files (NSLDialect.td + NSLOps.td + NSLTypes.td)

**Decision**: Partition the dialect's TableGen sources into three
files:

- `NSLDialect.td` — dialect class declaration (`def NSL_Dialect :
  Dialect { ... }`); namespace, `useDefaultTypePrinterParser = 1`,
  the `NSL_Op` base class definition that ops inherit from.
- `NSLOps.td` — every op record. ~35 records + auto-generated
  terminators. Includes `NSLDialect.td` and `NSLTypes.td`.
- `NSLTypes.td` — the 3 type records (`!nsl.bits`, `!nsl.struct`,
  `!nsl.mem`). Includes `NSLDialect.td`.

CMake `add_mlir_dialect(NSL nsl)` invocation generates all three
in the standard pattern.

**Rationale**: Matches CIRCT's convention exactly. Inspecting
upstream CIRCT (`circt/include/circt/Dialect/HW/`,
`circt/include/circt/Dialect/Comb/`,
`circt/include/circt/Dialect/Seq/`) shows every dialect uses this
three-file partition — Dialect class, Ops, Types — even when one of
the latter is small. Following the upstream convention reduces
contributor onboarding cost (a CIRCT contributor opening this repo
immediately recognizes the layout) and makes the
TableGen-generated header set predictable
(`NSLDialect.h.inc`, `NSLOps.h.inc`, `NSLTypes.h.inc`). The
partition is small enough that ~35 ops in `NSLOps.td` stays
manageable (~1600 LOC TableGen at typical density); a future split
into per-category .td files (`NSLActionOps.td`,
`NSLProcedureOps.td`, etc.) is a routine refactor if the file
grows past ~3000 LOC.

**Alternatives considered**:

- *Single monolithic `NSL.td`*: Simpler but doesn't match upstream
  convention; loses the natural separation between dialect-level,
  op-level, and type-level records. **Rejected** for upstream-
  alignment reasons.
- *Per-category partition* (e.g., `NSLModuleOps.td`,
  `NSLActionOps.td`, `NSLAtomicOps.td`, …): More files, more
  CMake plumbing, more `include` directives between .td files,
  more cognitive load to find a given op. **Rejected** at M4 as
  premature subdivision; can be done later if NSLOps.td grows.

---

## 4. Verifier-implementation language: hand-written C++ in NSLOps.cpp

**Decision**: Verifier bodies live in
`lib/Dialect/NSL/IR/NSLOps.cpp`, hand-written, with
`#include "NSLOps.cpp.inc"` at the bottom for the TableGen-
generated dispatcher / accessor glue. Each op's `verify()` is a
short C++ function (typically 5–20 lines) that performs the
structural checks (Q1 Option A) including the hand-walk for
transitive-parent rows (Q2 Option B). Helper utilities — most
importantly `findAncestorOfKind<T>(op)` — live in an anonymous
namespace at the top of `NSLOps.cpp`.

**Rationale**: TableGen's declarative `let hasVerifier = 1` plus
an out-of-line `<Op>::verify()` C++ body is the canonical MLIR
pattern (CIRCT, MLIR-core, all major downstream dialects use this).
Inlining the verifier into the .td file is theoretically possible
via `let extraClassDeclaration` / `let extraClassDefinition` but
hurts readability and complicates debugging. C++17 + LLVM utilities
(`llvm::cast`, `llvm::dyn_cast`, `llvm::isa`) keep the verifiers
short and idiomatic.

**Alternatives considered**:

- *All checks in TableGen-declarative traits*: Some structural
  invariants (e.g., the transitive-parent walk) cannot be expressed
  declaratively; would require extending TableGen with a new trait
  pattern, which is upstream-MLIR territory. **Rejected** as
  out-of-scope.
- *Per-op verifier file* (e.g., `NSLOps_ModuleOp_Verify.cpp`,
  one file per op): Excessive file count; verifier bodies are
  short and benefit from co-location for shared helpers like
  `findAncestorOfKind<T>`. **Rejected**.

---

## 5. Dialect type interning: MLIR's standard storage

**Decision**: All three dialect types use MLIR's standard `TypeStorage`
interning (`mlir::Type` is itself a uniqued handle to a
`TypeStorage` allocated in the `MLIRContext`'s arena). The three
TableGen `Type_Class` records inherit from `NSL_Type` (a base in
`NSLDialect.td`) which sets `mnemonic` and the parameter list:

- `NSL_BitsType` — parameter `unsigned width` (the N in `!nsl.bits<N>`).
- `NSL_StructType` — parameter `mlir::SymbolRefAttr name` (refers to
  a sibling `nsl.struct` op).
- `NSL_MemType` — parameters `unsigned depth`, `mlir::Type elementType`.

`useDefaultTypePrinterParser = 1` on the dialect generates the
default `<>`-bracketed parser/printer; no hand-written
`Type::print` / `Type::parse` is needed at M4.

**Rationale**: Matches design §7 line 981 ("These lower bijectively
to CIRCT's `i<N>` and `hw.array<D x T>` and `hw.struct<...>`") —
the bijective property is preserved by MLIR's interning (pointer
equality implies type equality, parallel to M3's `TypeSystem`
interning contract from FR-007 of M3). The default printer/parser
generated by `useDefaultTypePrinterParser = 1` covers FR-008
(round-trip byte-identical) without hand-written code at M4.

**Alternatives considered**:

- *Hand-written type printer/parser for each type*: Necessary if the
  default form's syntax doesn't match the design's preferred form.
  Inspection of MLIR's default `<>`-bracketed printer shows it
  produces `!nsl.bits<8>` exactly as design §7 line 892 specifies;
  hand-written variant is unnecessary. **Rejected** on YAGNI
  grounds.
- *Build types as MLIR built-in `IntegerType` aliases*: e.g.,
  `!nsl.bits<8>` ≡ `i8`. Simpler but loses the dialect-namespacing
  the design doc requires; mixing-with-CIRCT fixtures couldn't
  distinguish "an `i8` originated from NSL's `bits<8>`" vs.
  "an `i8` originated from CIRCT's `hw.constant`". **Rejected** —
  the mapping to CIRCT's `i<N>` is the M6 lowering pass's job, not
  an M4 type-aliasing decision.

---

## 6. `nsl-opt` CLI: vanilla `MlirOptMain` + dialect registration

**Decision**: `tools/nsl-opt/main.cpp` is a vanilla MLIR-style
`opt`-tool main, ~50 lines:

```cpp
#include "mlir/Tools/mlir-opt/MlirOptMain.h"
#include "nsl/Dialect/NSL/IR/NSLDialect.h"
// CIRCT dialect headers per design §11 lines 1146–1150
#include "circt/Dialect/HW/HWDialect.h"
// ... etc

int main(int argc, char **argv) {
    mlir::DialectRegistry registry;
    nsl::dialect::registerNSLDialect(registry);
    registry.insert<circt::hw::HWDialect, circt::comb::CombDialect,
                    circt::seq::SeqDialect, circt::fsm::FSMDialect,
                    circt::sv::SVDialect>();
    return mlir::asMainReturnCode(
        mlir::MlirOptMain(argc, argv, "NSL dialect tool\n", registry));
}
```

Zero passes registered at M4 (the `--nsl-*` pass-flag set is empty
beyond MLIR's built-in `--canonicalize`, `--cse`, etc., which are
no-ops on the dialect at M4 since no canonicalize patterns are
registered).

**Rationale**: The MLIR `MlirOptMain` driver provides a complete
CLI surface (`--allow-unregistered-dialect`, `--verify-diagnostics`,
`-o <file>`, stdin input on bare `-`, `-h` listing all flags) that
matches FR-015. Re-using it costs ~5 lines per main; rewriting
costs hundreds and reproduces upstream behavior imperfectly. CIRCT's
`circt-opt` follows the same pattern (the `circt-opt/circt-opt.cpp`
on master is ~80 lines, mostly dialect-registration). The CIRCT
dialect-registration list is taken verbatim from
`Compilation`'s ctor-load list at design §11 lines 1146–1150 so
hand-written mixed-dialect fixtures parse correctly even though the
M4 dialect itself doesn't reference CIRCT.

**Alternatives considered**:

- *Custom CLI not based on `MlirOptMain`*: Reproduces dozens of
  upstream features (color output, source-loc decoration,
  diagnostic JSON) imperfectly. **Rejected** on
  reuse-the-upstream-tool grounds.
- *Don't register CIRCT dialects at M4*: Rejected because spec
  Edge-Cases § "mixed-dialect fixtures" explicitly requires it
  (`The dialect registration in nsl-opt MUST register the CIRCT
  dialects too`).

---

## 7. Driver dialect-load + `lowerToNSL` / `runNSLPasses` stub form

**Decision**: At M4, `lib/Driver/Compilation.cpp` gains exactly one
new line in the constructor — `mlirCtx_.loadDialect<nsl::dialect::NSLDialect>();`
inserted after the existing CIRCT-dialect-load lines (per design §11
line 1145). The `lowerToNSL` and `runNSLPasses` member functions
exist as **trivial stub bodies** (NOT forward declarations) in two
new files:

- `lib/Driver/LowerToNSL.cpp`:
  ```cpp
  mlir::OwningOpRef<mlir::ModuleOp>
  Compilation::lowerToNSL(ast::CompilationUnit&, sema::SemaResult&) {
      diag_.emit(Severity::Fatal, /*loc*/{},
                 "MLIR lowering not yet implemented; see M5");
      return {};
  }
  ```
- `lib/Driver/RunNSLPasses.cpp`: parallel stub.

**Rationale**: Forward-declaration would force the M4 driver to fail
to link against `nsl-driver` (linker reports unresolved symbol),
preventing the `nslc --version` / `nslc -emit=tokens` /
`nslc -emit=ast` paths from being exercised in CI. A trivial
diagnostic-emitting stub keeps the binary linkable and reachable on
the public `-emit=tokens` / `-emit=ast` paths (which never call
these stubs — FR-023 ensures the CLI rejects `-emit=mlir`), while
guaranteeing the stubs are unreachable from the public surface. M5
replaces both stub files with real implementations in the M5 patch;
no driver-level CMake or header changes are needed at the M4/M5
seam.

**Alternatives considered**:

- *Forward declarations only (no stub bodies)*: Driver fails to
  link at M4. **Rejected** for testability (CI would reject the M4
  build).
- *Inline trivial bodies in `Compilation.cpp`*: Single-file
  approach; rejected because M5 will replace each function with a
  ~200–500 LOC body, and a per-file split keeps the M4 → M5 diff
  surgical (one new file added per M4 stub becomes the entire body
  at M5; no `Compilation.cpp` churn).
- *Throw a runtime exception instead of emitting a diagnostic*:
  Rejected because Constitution Principle IV ("source-locating
  diagnostics") mandates that all error reporting flow through
  `DiagnosticEngine`; an exception bypasses this and would surprise
  downstream tooling.

---

## 8. Test-corpus organization: per-category subdirectories under `test/Dialect/`

**Decision**: Test fixtures live under `test/Dialect/<category>/`
where `<category>` matches the FR-010 op-table category column —
`module-level`, `storage`, `control-terminal`, `action-block`,
`action-helper`, `atomic`, `procedure`, `procedure-helper`,
`system-task`, `marker`, `expansion-only`, plus a top-level `Types/`
for the 3 type-round-trip fixtures. Each subdirectory holds:

- `<op>_roundtrip.mlir` for round-trip pass fixtures (one per op).
- `<op>_invalid_<reason>.mlir` for invalid fixtures (one per cell
  in FR-013 with ≥ 1 invariant; ~50 total at M4).

Standard lit run-line in each `<op>_roundtrip.mlir`:

```mlir
// RUN: nsl-opt %s | FileCheck %s
// RUN: nsl-opt %s | nsl-opt - | FileCheck %s
// CHECK: nsl.<op>
```

Standard lit run-line in each `<op>_invalid_<reason>.mlir`:

```mlir
// RUN: nsl-opt --verify-diagnostics %s
// expected-error@+1 {{<op-name> + invariant-shape substring}}
nsl.<op> ...  /* malformed shape */
```

**Rationale**: Matches MLIR/CIRCT upstream convention exactly —
`circt/test/Dialect/HW/`, `circt/test/Dialect/Comb/`, etc. all use
the per-category subdirectory layout. The two-pass round-trip
(`nsl-opt %s | nsl-opt - | FileCheck %s`) asserts the second pass
is a fixed point per FR-017's two-pass run-line clause. The
`--verify-diagnostics` flag plus `// expected-error@+N {{...}}`
syntax is MLIR's standard mechanism for fixture-based diagnostic
assertion (substring match by design — see §11 below for the
substring-match rationale).

**Alternatives considered**:

- *Flat `test/Dialect/*.mlir` with per-op file naming*: ~85 files
  in one directory; navigation cost. **Rejected**.
- *Per-op subdirectories* (e.g., `test/Dialect/module/`,
  `test/Dialect/struct/`, …): ~35 directories instead of ~12;
  excessive subdivision. **Rejected**.
- *gtest-style C++ unit tests instead of lit fixtures*: Possible
  but Constitution Principle VI mandates lit + FileCheck for
  dialect tests ("Dialect tests use `nsl-opt` for round-trip
  verification of `.mlir`"). **Rejected** on constitutional grounds.

---

## 9. CI guard for fixture-existence: `scripts/check_dialect_coverage.py`

**Decision**: A new Python script
`scripts/check_dialect_coverage.py` runs in CI's static-checks
stage (Principle IX stage 2) to enforce FR-021. The script:

1. Parses `lib/Dialect/NSL/IR/NSLOps.td` (or alternatively
   greps `def NSL_*Op : NSL_Op<"<name>", ...>` records) to build
   the registered op set.
2. For each op `nsl.<name>`, asserts a `<name>_roundtrip.mlir`
   file exists under `test/Dialect/<some-category>/`.
3. For each cell in the spec FR-013 invariant table (encoded as a
   small data file, `.specify/m4_invariant_table.json`, generated
   from the spec at PR-author time), asserts ≥ 1 matching
   `<op>_invalid_*.mlir` fixture exists.

**Rationale**: Mechanical, fast (sub-second), self-contained, and
parallel to M3's `scripts/check_layering.py` extension. Putting the
op-set ground-truth in the .td file (Principle I "spec is
authoritative" applied to the dialect's own surface) ensures the
guard updates automatically when an op is added, eliminating drift
between code and test corpus. The invariant-table JSON adapter
covers FR-013's per-row invariant set without forcing the script to
parse Markdown tables.

**Alternatives considered**:

- *Manual fixture-coverage review at PR time*: Drifts. **Rejected**.
- *Generate fixtures from a template script*: Couples test content
  to a generator; harder to audit per-fixture content.
  **Rejected** — fixtures are hand-authored per Principle VIII TDD
  (each fixture must have been observed-failing before the
  implementation lands).

---

## 10. Layered-deps guard extension at M4

**Decision**: Extend `scripts/check_layering.py` (introduced at M2,
extended at M3 to forbid `nsl-sema`→`nsl-parse` edges) with new
forbidden-edge entries:

- `nsl-dialect → {nsl-ast, nsl-sema, nsl-parse, nsl-lex, nsl-preprocess}`
- `nsl-driver` MAY depend on `nsl-dialect` (added; this is the
  driver's dialect-load call site per §7).

The script runs against the link-graph extracted from the build's
`compile_commands.json` + ar output, parallel to M3's mechanism.

**Rationale**: Constitution Principle II's downward-flow rule is
mechanically enforced at every layer addition. M4 introduces layer
7 and the driver edge; both rules go in the same patch.

**Alternatives considered**:

- *Static review only*: Drifts. **Rejected**.
- *CMake-level dependency assertion*: Not enforceable (CMake's
  `target_link_libraries` is permissive). **Rejected**.

---

## 11. Diagnostic-message text policy: substring match (carve-out documented in spec Assumptions)

**Decision**: Verifier diagnostic message text is **NOT frozen** by
fail-case fixture text-asserts. Instead, fixtures use FileCheck's
`// expected-error{{<substring>}}` syntax matching only the op-name
+ the invariant-shape (e.g.,
`expected-error{{'nsl.seq' op expects parent op 'nsl.func'}}`).
The substring ignores MLIR upstream wording drifts. This is a
**deliberate carve-out from Principle VIII's `Sn`/`Nn`/`Pn` clause**,
documented in spec Assumptions and FR-012.

**Rationale**: Dialect verifier wording is set by upstream MLIR
conventions (e.g., the `HasParent` trait's standard "expects parent
op '...'" format), not NSL conventions. Re-stating MLIR's wording
in our fixtures with literal-string asserts would defeat the
upstream convention and create a maintenance treadmill every time
upstream MLIR refines its diagnostics. Principle VIII's
diagnostic-string clause specifically scopes to NSL `Sn`/`Nn`/`Pn`
constraints (visible in `docs/spec/*.ebnf`); MLIR-layer structural
rules are out of that scope by construction. The carve-out is
documented in spec Assumptions and surfaces in
`contracts/verifier-diagnostic.contract.md` as the M4 contract.

**Alternatives considered**:

- *Literal-string match (parallel to M3's `Sn` model)*: Couples our
  test corpus to upstream MLIR's exact wording. **Rejected** on
  maintenance-cost grounds.
- *Constitution amendment to formally carve out MLIR-layer
  diagnostics*: Possible if a future audit flags this. The current
  reading is that Principle VIII's text already scopes to
  NSL-spec constraints; an amendment would only codify the implicit
  carve-out. **Deferred** unless `/nsl-constitution-review` flags
  the implicit reading as ambiguous.

---

## 12. SymbolTable trait integration with MLIR's standard machinery

**Decision**: Ops with `Symbol` and `SymbolTable` traits use MLIR's
standard machinery (`mlir::SymbolTable`, `mlir::SymbolRefAttr`)
rather than NSL's own M3 `nsl::sema::SymbolTable`. The two are
distinct entities: MLIR's `SymbolTable` is a per-region symbol-name
lookup (used at the dialect/IR layer); NSL's `nsl::sema::SymbolTable`
is the AST-level scope stack from M3. They share the name "symbol
table" but live at different layers.

**Rationale**: MLIR's `SymbolTable` trait + `SymbolRefAttr`
attribute are the canonical mechanism for cross-op symbolic
reference within a region (CIRCT, all major MLIR dialects use this).
At M4 the dialect needs ops like `nsl.first_state @s0` (referring
to a sibling `nsl.state @s0`), `nsl.goto @s1`, `nsl.call @func_in`,
`nsl.connect %sub.port` — all of these are dialect-layer
cross-references and use `SymbolRefAttr`. The M3 `nsl::sema::Symbol`
hierarchy is consumed only by the M5 AST→MLIR builder (which
translates `Symbol*` into `SymbolRefAttr`); it does not appear in
the M4 dialect surface. This matches Constitution Principle II's
layer separation: Sema symbols are AST-level (layer 6); dialect
symbols are IR-level (layer 7), and they communicate only via the
M5 lowering pass.

**Alternatives considered**:

- *Replace MLIR's `SymbolRefAttr` with an NSL-specific
  `SemaSymbolAttr` carrying a `nsl::sema::Symbol*`*: Would couple
  the dialect to `nsl-sema` (Principle II violation per FR-001).
  **Rejected** on layer-independence grounds.
- *Don't use `SymbolRefAttr` at all; use string-name attributes*:
  Loses MLIR's standard symbol-resolution machinery (`SymbolTable
  ::lookup`, etc.) and means the dialect's verifier can't validate
  that a `@s0` reference actually resolves. **Rejected**.

---

## 13. Auto-generated implicit-terminator ops

**Decision**: The TableGen `SingleBlockImplicitTerminator<...>`
trait on region-bearing ops (`nsl.module` →
`SingleBlockImplicitTerminator<"ModuleTerminatorOp">`; `nsl.proc`
→ `SingleBlockImplicitTerminator<"ProcTerminatorOp">`; etc.)
auto-generates the corresponding terminator op classes. These
terminator ops are in the FR-010 op table's "Auto-terminators" row;
they're real ops with class names (`ModuleTerminatorOp`,
`ProcTerminatorOp`, ...) but are not user-facing (the printer omits
them when printing a region whose only-or-last op is the implicit
terminator). They appear in the per-region-bearing-parent
TableGen `def`s but get **no explicit `def` of their own** —
TableGen's `SingleBlockImplicitTerminator` is a class-generator
trait.

**Rationale**: Following MLIR upstream's convention exactly. The
trait-generated terminator class is an implementation detail of the
parent's region; round-trip fixtures don't need to test the
terminator explicitly because round-tripping the parent op
implicitly round-trips the terminator (the printer/parser handle it
under the parent's framing).

**Alternatives considered**:

- *Hand-define each terminator as a separate `def`*: Excessive
  boilerplate, no diagnostic value. **Rejected**.
- *Use `Block` ops without an implicit terminator*: Semantics
  unclear for ops like `nsl.module` whose region must terminate
  somewhere. **Rejected**.

---

## 14. `mlir::Location` mapping at the AST→MLIR seam (M4 stub anticipation)

**Decision**: At M4, every op constructed by hand-written `.mlir`
fixtures carries the `.mlir` text-location via MLIR's standard
parser-attached `mlir::FileLineColLoc`. At M5, the AST→MLIR builder
will use design §12 line 1211's pattern:
`builder.create<nsl::ModuleOp>(builder.getLoc(astNode->sourceRange))`
where `builder.getLoc(SourceRange)` is a small helper introduced at
M5. The dialect itself, at M4, makes no special arrangement for
SourceRange-derived locations — it just receives whatever `Location`
the op's constructor was passed.

**Rationale**: MLIR's `Location` machinery is a uniform attribute
on every op; the dialect doesn't need to know whether the location
came from a file-parse or a SourceRange-encode. Designing the
dialect agnostic to its source-of-location keeps M4 clean and pushes
the SourceRange↔FileLineColLoc translation into M5's lowering
helper, which is the natural owner.

**Alternatives considered**:

- *NSL-specific `Location` subclass carrying a `SourceRange`*:
  Couples the dialect to nsl-basic in a way that's already
  required (FR-001 lists `nsl-basic` as a dep), but builds in a
  naming scheme MLIR doesn't recognize for diagnostics. **Rejected**
  in favor of MLIR's `FileLineColLoc` representation, which works
  out of the box.

---

## 15. CMake integration — `add_mlir_dialect` + `add_nsl_library`

**Decision**: The dialect's CMakeLists uses BOTH macros:

```cmake
add_mlir_dialect(NSL nsl)              # generates NSLOps.h.inc, NSLOps.cpp.inc, NSLDialect.h.inc, NSLDialect.cpp.inc, NSLTypes.h.inc, NSLTypes.cpp.inc, plus tablegen targets
add_mlir_doc(NSLOps NSL nsl/ -gen-op-doc)  # markdown doc generation (optional but conventional)

add_nsl_library(nsl-dialect           # M0's helper for SPDX + clang-tidy + linking
  NSLDialect.cpp
  NSLOps.cpp
  NSLTypes.cpp
  DEPENDS
    nsl-basic
    MLIRNSLIncGen          # tablegen-generated headers depend
    MLIRIR
    MLIRSupport
)
```

**Rationale**: `add_mlir_dialect` is the upstream-MLIR helper that
runs `mlir-tblgen` over the `.td` files and registers the generated
`.h.inc` / `.cpp.inc` as build artifacts; it is the standard idiom.
`add_nsl_library` (introduced at M0) layers project-specific
discipline on top — SPDX checking, clang-tidy profile, the
forbidden-edge layering check. The two are complementary, not
overlapping: `add_mlir_dialect` handles tablegen, `add_nsl_library`
handles the C++ library wrap. Both CIRCT and MLIR-core use this
pattern.

**Alternatives considered**:

- *Roll our own tablegen invocation manually*: Reproduces upstream
  helper imperfectly. **Rejected**.
- *Skip `add_nsl_library` for the dialect (use plain
  `add_library`)*: Loses M0's SPDX + clang-tidy hygiene. **Rejected**.

---

## Post-design Constitution re-check

After Phase 1 design (data-model.md, contracts/, quickstart.md
drafted), the Constitution Check above is re-evaluated. **Result:
PASSES on second evaluation, no new violations introduced by the
Phase 1 artifacts.** The carve-out documented in research §11
(substring-match diagnostic policy) remains a Principle IV/VIII
*application detail* per spec Assumptions, not a constitutional
amendment. The Phase 1 contracts (in particular
`verifier-diagnostic.contract.md`) make the substring-match policy
mechanically enforceable. No items move to Complexity Tracking.
