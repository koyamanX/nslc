<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Feature Specification: M5 — `nsl-lower` part 1: AST → `nsl` dialect + structural-expansion passes (`nslc -emit=mlir` operational)

**Feature Branch**: `008-m5-structural-passes`
**Created**: 2026-04-30
**Status**: Draft
**Input**: User description: "M5"

> **Scope interpretation.** "M5" maps to the **M5** row of
> [`README.md`](../../README.md) §Roadmap, which delivers the next
> compiler-track library: `nsl-lower` part 1 (layer 8a per
> [`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md)
> §3 lines 132–148) — the **AST → `nsl` dialect** lowering plus the
> **structural-expansion pass pipeline** that rewrites `nsl.*` IR
> in-place before any CIRCT lowering. The same row defines the
> milestone's test gate ("**FileCheck on `nslc -emit=mlir` for
> representative samples per AST node kind; determinism gate
> (byte-stable across two builds)**") and its constitutional anchors
> (VI lowering tests; V determinism; III `nsl` dialect is the seam).
> The compiler-track table in [`CLAUDE.md`](../../CLAUDE.md) §1
> confirms which language-spec rows land their "Lower to dialect"
> column entry at M5: every grammar row whose dialect target is an
> `nsl::*` op (catalogued at M4) gains a working AST→nsl visitor
> case here, plus the row "Action statements: `seq` / `if` / `for` /
> `while` / `generate`" extends with M5's `generate` unroll pass.
> M3 → M4 → M5 forms the single critical spine: M3 produced the typed
> AST + Sema observables, M4 produced the dialect IR shape + verifiers
> + `nsl-opt` round-trip, and M5 turns the AST into that IR and
> finishes the structural-expansion phase before handing off to M6's
> CIRCT lowering.
>
> **What lands as a deliverable.** Three observable artefacts:
>
> 1. The `nsl-lower` static library (layer 8) gains its first body of
>    code under a single public umbrella header
>    `include/nsl/Lower/Lower.h` (per Constitution Principle II's
>    single-public-header rule — `nsl-lower` is NOT one of the named
>    exceptions for `nsl-ast` / `nsl-sema`). The library's M0
>    `add_nsl_library` declaration in `lib/Lower/CMakeLists.txt`
>    (`DEPENDS nsl-sema nsl-dialect` + `LINK_LIBS CIRCTHW CIRCTComb
>    CIRCTSeq CIRCTSV`) is unchanged at M5 — the CIRCT link
>    dependencies are forward-prepared for M6 and stay quiet at this
>    milestone (no `nsl::*` → CIRCT lowering ships at M5; the umbrella
>    header re-exports only the AST→nsl entry point + the six pass
>    constructors).
> 2. An `ASTToMLIR` visitor (`nsl::lower::ASTToMLIR` per design §8
>    lines 1003–1051) that walks an `ast::CompilationUnit` plus its
>    paired `sema::SemaResult` and produces a `mlir::OwningOpRef<
>    mlir::ModuleOp>` containing one `nsl.module` per `ast::ModuleBlock`.
>    Every MLIR op the visitor creates carries the originating AST
>    node's `SourceRange` as an `mlir::Location` (Constitution
>    Principle IV — diagnostic plumbing crosses the AST → MLIR seam
>    intact). Every AST node kind reachable from M2/M3's grammar
>    (every concrete `ast::*` `visit(...)` override in the visitor
>    declaration) has a corresponding lowering rule per the table at
>    design §8 lines 1057–1078, plus the additional rules surfaced by
>    Sema's `S27` (control-name-as-1-bit-value → `nsl.fire_probe`).
> 3. A six-pass `nsl::*` → `nsl::*` rewrite pipeline (`mlir::PassManager`
>    instance assembled inside `Compilation::runNSLPasses`) per design
>    §9 lines 1086–1093, in the listed order:
>    `NSLResolveParamsPass` → `NSLExpandGeneratePass` →
>    `NSLExpandVariablesPass` → `NSLExplodeSubmodArrayPass` →
>    `NSLInlineInternalFuncPass` → `NSLCheckSemanticsPass`. Each pass
>    is a `mlir::OperationPass<nsl::ModuleOp>` that registers under a
>    stable pass-name string usable from the command line (`nsl-opt
>    -nsl-resolve-params`, `-nsl-expand-generate`, …) so individual
>    passes can be unit-tested standalone via the M4 `nsl-opt` binary.
>    The final `NSLCheckSemanticsPass` enforces residue-freedom for
>    `%IDENT%` macro splices (per design §9 last row + the cross-
>    reference table in [`docs/CLAUDE.md`](../../docs/CLAUDE.md) §8
>    line 311 "`%IDENT%` macros … `NSLCheckSemanticsPass` checks
>    residue-free") and re-checks the post-expansion-sensitive subset
>    of `S1`–`S29` (the constraints whose verdict can change once
>    `generate` is unrolled, parameters resolved, or submod arrays
>    exploded).
>
> Plus two driver-surface artefacts wiring the library into `nslc`:
>
> 4. The `Compilation::lowerToNSL` and `Compilation::runNSLPasses`
>    member functions (signatures **frozen at M4**, bodies
>    **stubbed at M4**) gain real bodies. `lowerToNSL` constructs an
>    `ASTToMLIR` visitor and returns its result; `runNSLPasses`
>    assembles the six-pass pipeline and runs it. Both report
>    failures through the project's single `basic::DiagnosticEngine`
>    (Constitution Principle IV) — no MLIR-built-in diagnostic stream
>    leaks past the seam.
> 5. A new driver flag `nslc -emit=mlir` (per design §11
>    `CompileOptions::EmitKind::NSLMLIR`, the M3-frozen enum value)
>    halts the pipeline after `runNSLPasses` and prints the final
>    `mlir::ModuleOp` to stdout (or to `-o <file>`) using MLIR's
>    default printer. Pipeline stages strictly before this gate
>    (`-emit=tokens`, `-emit=ast`) are unchanged from M3; pipeline
>    stages strictly after (`-emit=hw`, `-emit=verilog`) are still
>    forward-looking (M6, M7).
>
> **What does NOT land at M5.** No `nsl::*` → CIRCT lowering and no
> `circt::*` op generation — that is M6's `NSLToCIRCTPass` per design
> §10 (lines 1099–1132). No `-emit=hw`, no `-emit=verilog` — those
> flag wirings stay stubbed at M5. No new `nsl::*` ops in the dialect
> (the M4-frozen op set + the M4-frozen public surface from
> [`specs/007-m4-mlir-dialect/`](../007-m4-mlir-dialect/) `dialect-api.contract.md`
> §2 freeze list MUST stay byte-stable through M5 — adding a new op
> means amending the M4 contract, not stealth-introducing it under the
> M5 banner). End-to-end Verilog (M7), formal (M8), tagged release
> (M9) are all forward-looking. The audited-corpus test fixtures
> under `test/audited/` (P-VEN, P-VCD) remain a M7 deliverable — M5's
> test corpus is hand-authored representative samples per AST node
> kind, NOT the seven full audited projects.

## Clarifications

### Session 2026-04-30

- Q: How does `ASTToMLIR` handle forward references across regions of the same compilation unit? (single-pass + SymbolTable lazy / two-pass decls-then-bodies / single-pass + fixup queue) → A: **Option A — single-pass + MLIR `SymbolTable` lazy resolution.** `ASTToMLIR::lower(...)` walks the AST exactly once. When the visitor encounters a cross-region symbol reference (e.g., `nsl.call @Q` inside `proc P` where `func Q` is declared later in the source), it constructs the `mlir::FlatSymbolRefAttr` referring to `@Q` immediately — regardless of whether `@Q`'s defining op has been emitted yet. MLIR's stock `SymbolTable` machinery resolves the reference at op-tree finalization (verification time): every `nsl::*` op carrying a `Symbol`-trait declaration is registered in the enclosing `nsl.module`'s symbol table, and every `FlatSymbolRefAttr` use is resolved post-walk by `mlir::SymbolTable::lookup(...)` during the M4 verifier pass. M3 Sema has already validated that all references resolve, so no validation work falls on the visitor. Rationale: matches MLIR upstream convention (`func.func` and `mlir::SymbolTable` work this way); simplest implementation (one walk, no fixup queue, no two-phase synchronization, no per-pass valueMap_ reset complexity); M4's verifiers already enforce symbol-reference validity post-walk via the standard `mlir::SymbolTable` machinery the dialect inherits. The visitor's debug-diagnostic output for an internal ICE (FR-010) does not need a fully-built symbol table; the failing op's `mlir::Location` plus the AST node-kind name is sufficient. Option B (two-pass) was rejected because it doubles AST walk cost for no observable user value (M3 Sema already provides the symbol-resolution guarantee that two-pass would re-derive); Option C (single-pass + fixup queue) was rejected because it adds a second post-walk traversal but with strictly more complexity than the lazy-resolution path the MLIR upstream already provides.
- Q: What does `NSLInlineInternalFuncPass` deliver at M5? (functional inlining / no-op slot / functional with XFAIL) → A: **Option B — registered no-op slot.** The pass exists in the pipeline at slot 5, registers under the `-nsl-inline-internal-func` flag for `nsl-opt`, walks the input `nsl::ModuleOp` once, and returns `mlir::success()` without modifying the IR. FR-017's "MAY inline if conditions hold" is satisfied trivially (the condition "is implementer-determined whether to attempt inlining" is read as "no" at M5). FR-028's "one well-formed scenario, one edge-case scenario" for this pass collapses to a single trivial fixture: an `nsl-opt -nsl-inline-internal-func` round-trip on a `func_self` containing `.mlir` snippet that proves the pass produces byte-identical output. Rationale: design §9 explicitly tags this pass "(optional perf pass)"; M5's deliverable contract is the SHAPE of the pipeline (six slots, six flags) not optimization quality; the three named M5 sub-deliverables (generate-unroll, struct-SSA-split, residue-check) saturate the milestone budget. A future PR can fill in functional inlining without amending this spec; the slot's pass-name + signature + position in `Compilation::runNSLPasses` is the M5 ABI commitment. Option A (functional inlining) was rejected as scope creep at M5; Option C (functional with XFAIL) was rejected because XFAIL'd fixtures rot and CodeRabbit/CI cannot tell "intentionally deferred" from "broken" without out-of-band annotation.
- Q: Which MLIR text format does `nslc -emit=mlir` produce? (default printer / generic form / configurable per-flag) → A: **Option A — default printer (assembly form, op-builder pretty-print).** `Compilation::emit` for `EmitKind::NSLMLIR` calls `mlir::ModuleOp::print(os)` with default `mlir::OpPrintingFlags()` — no `printGenericOpForm()`, no `useLocalScope()`, no `enableDebugInfo()`. The same flags are used by every `test/Lower/**.mlir.expected` golden, by the `nsl-opt` M4 round-trip baseline, and by the determinism-gate diff pair under US5. Rationale: matches the M4 round-trip property (M4 spec US1 acceptance scenario 1: "`nsl-opt %s | nsl-opt -` is a fixed point" — both ends use default printer); matches MLIR upstream convention (`mlir-opt` defaults the same way); readable for fixture authoring and CodeRabbit review; deterministic-by-construction (default printer's whitespace-normalization is part of the round-trip property M4 already proved). Option B (generic form) was rejected because M4's existing `<op>_roundtrip.mlir` fixtures assert default-printer output — switching M5 to generic-form output would force re-authoring every M4 fixture (M4-contract drift). Option C (configurable per-flag) was rejected because it doubles the test-fixture authoring surface (default-form goldens vs generic-form goldens) for no observable user value at M5. If a future debug need surfaces, a `nslc -emit=mlir-generic` follow-up flag can be added without amending this spec.
- Q: How does `NSLCheckSemanticsPass` detect a surviving `%IDENT%` residue in the IR? (M1-error-out / regex-scan-string-attrs / dedicated marker op) → A: **Option B — regex scan over string-typed attributes on every `nsl::*` op for `%[A-Za-z_]\w*%`.** Detection is self-contained at M5: the pass walks every `mlir::Operation*` reachable from the input `nsl::ModuleOp`, and for every `mlir::StringAttr`-typed attribute on that op (`sym_name`, `n` (the textual identifier carried on `nsl.reg` / `nsl.wire` / `nsl.mem`), and any other string-typed dialect attribute), runs the C++ regex `%[A-Za-z_][A-Za-z0-9_]*%` over the string contents and emits one diagnostic per match. The M1 preprocessor contract stays unchanged (it forwards unresolved `%IDENT%` as a literal substring on the assumption a downstream phase will detect; the post-`NSLExpandGeneratePass` shape determines what is residue). The M4 dialect surface stays unchanged — no new op, no new attribute type, no `dialect-api.contract.md` amendment. Rationale: matches `%IDENT%`'s textual nature per pp.ebnf P3 line 419 ("A `%IDENT%` reference inside an identifier produces a single longer identifier"); cheap O(N_ops × N_string_attrs_per_op); diagnostic emits at the carrying op's `mlir::Location` for source-locating granularity (Principle IV). Option A (M1 errors out) was rejected because it would force a M1 contract amendment (currently locked at the M1 freeze line per `specs/002-m1-lex-preprocess/contracts/`); Option C (dedicated `nsl.macro_residue` marker op) was rejected because it grows the M4-frozen `dialect-api.contract.md` §2 freeze surface from 48 to 49 public types/functions, which is exactly what FR-031 + the "What does NOT land at M5" guard explicitly forbid.

## User Scenarios & Testing *(mandatory)*

### User Story 1 — `nslc -emit=mlir` produces verified `nsl.*` IR for every AST shape (Priority: P1)

A contributor authors a representative `.nsl` source file (or selects
one from the M3 sema-corpus fixtures), runs
`nslc -emit=mlir input.nsl -o output.mlir`, and observes that:
(1) the command exits zero with no diagnostics, (2) the output file
is valid `nsl::*` MLIR text that `nsl-opt output.mlir` re-verifies and
re-prints byte-identically (the M4 round-trip property, transitively
inherited), (3) the printed IR's `loc(...)` attributes resolve to the
original `.nsl` source positions for every op (Principle IV), and
(4) every AST node kind exercised by the input has produced the
`nsl::*` op named in design §8's lowering table. Equivalently, on
the canonical M3 corpus the visitor never asserts, never produces an
`mlir::ModuleOp` containing `unrealized_conversion_cast`, and never
emits a `op-not-yet-supported` diagnostic.

**Why this priority**: This **is** the M5 acceptance gate per the
README's M5 row literal text — "`-emit=mlir`" is the user-visible
deliverable; the AST→MLIR visitor and the six-pass pipeline are the
two engines that make it work. Without `-emit=mlir`, M6 (CIRCT
conversion) has no source IR to consume, M7 (end-to-end Verilog) has
no plumbing for its first stage past `-emit=ast`, and the dialect
delivered at M4 has no producer feeding it. Constitution Principle VI
names "**Lowering tests** use `nslc -emit=mlir` (or `-emit=hw`) for
FileCheck-style verification" as the layer's canonical test driver
(README §Roadmap M5 anchor). P1 because every downstream milestone is
gated on this stage being operational and stable.

**Independent Test**: Build the project (the `nsl-lower` library +
`nslc` driver). For every distinct AST node kind in the M2 grammar
(`ast::ModuleBlock`, `ast::DeclareBlock`, `ast::RegDecl`,
`ast::WireDecl`, `ast::MemDecl`, `ast::ProcDefn`, `ast::StateDefn`,
`ast::FirstStateDecl`, `ast::FuncDefn`, `ast::ParallelBlock`,
`ast::AltBlock`, `ast::AnyBlock`, `ast::SeqBlock`, `ast::WhileBlock`,
`ast::ForBlock` (enum form), `ast::ForBlock` (C-style form),
`ast::IfStmt`, `ast::TransferStmt` (`=`), `ast::TransferStmt` (`:=`),
`ast::ControlCallStmt`, `ast::BareFinishStmt`, `ast::SystemTaskStmt`
× 4 sim tasks, `ast::StructCastExpr`, `ast::FieldAccessExpr`,
plus expression node kinds: `ast::BinaryExpr` × operator kinds,
`ast::UnaryExpr`, `ast::LiteralExpr`, `ast::IdentifierExpr`,
`ast::ConditionalExpr`, `ast::SliceExpr`, `ast::ConcatExpr`),
ship `test/Lower/<category>/<node>_emit_mlir.nsl` plus its
`<node>_emit_mlir.mlir.expected` golden — assert via lit + FileCheck
that `nslc -emit=mlir %s` produces output containing the dialect ops
named in the lowering table. The CI guard mechanically enumerates
every concrete `visit()` override in `ASTToMLIR` and asserts a
matching fixture exists. Does not depend on US2/US3/US4/US5 — those
exercise specific pass behaviour; US1 exercises the headline pipeline.

**Acceptance Scenarios**:

1. **Given** an NSL source `module M { declare M { input a[8]; output q[8]; } reg r[8] = 0; r := a; q = r; }`, **When** `nslc -emit=mlir M.nsl` runs, **Then** stdout contains `nsl.module @M`, an `nsl.reg "r" : !nsl.bits<8> = 0`, an `nsl.clocked_transfer` (lowered from `:=`), an `nsl.transfer` (lowered from `=`), AND every op carries an `mlir::Location` resolvable to `M.nsl:<line>:<col>`.
2. **Given** an NSL source containing a `proc P { state s0 { goto s1; } state s1 { ... } }` with a paired `first_state s0;` declaration, **When** `nslc -emit=mlir P.nsl` runs, **Then** the output contains `nsl.proc @P` with an `nsl.first_state @s0` attribute-marker child, two `nsl.state` siblings, and an `nsl.goto @s1` op inside `@s0`'s region.
3. **Given** any input that compiles cleanly through M3 Sema (zero `S1`–`S29` diagnostics), **When** `nslc -emit=mlir` runs, **Then** the run is also diagnostic-free at M5 — Sema's verdict is the gate; M5 does not introduce new diagnostics on Sema-clean input (the residue-check is a fail-fast for invariant violations the M3 Sema cannot see, NOT a re-derivation of `S1`–`S29`).
4. **Given** an NSL source exercising every action-block kind nested inside a `func` / `proc` per the grammar's parental-context rules, **When** `nslc -emit=mlir` runs, **Then** every nested action lowers to its dialect-table counterpart with parental-region invariants (per M4 verifiers) satisfied — the `nsl-opt` round-trip on the output IR is a fixed point.
5. **Given** any AST source-locating attribute (`SourceRange`) on any AST node, **When** that node is lowered, **Then** the resulting MLIR op's `loc(...)` is an `mlir::FileLineColLoc` (or `FusedLoc` aggregating multiple ranges) whose path/line/col match the AST's `SourceRange`.
6. **Given** an output `.mlir` file produced by `nslc -emit=mlir`, **When** that file is fed to `nsl-opt -`, **Then** the round-trip is a fixed point (byte-identical second-pass output); when fed to `nsl-opt -verify-each`, **Then** every op-verifier from M4 returns success (no structural-invariant violations introduced by lowering).
7. **Given** a piped invocation `cat input.nsl | nslc -emit=mlir -`, **When** the driver runs, **Then** the output goes to stdout and the exit code is zero on success / non-zero on diagnostic failure (matches `-emit=tokens` / `-emit=ast` from M3).

---

### User Story 2 — `generate` loops are unrolled into N replicated bodies (Priority: P1)

A contributor authors an NSL source with a `generate(i = 0; i < N; i++)
{ body referencing %i% }` block (`structural_generate` per
`docs/spec/nsl_lang.ebnf` §6 lines 458–467). After `nslc -emit=mlir`,
the output IR contains zero `nsl.structural_generate` ops — every
such op has been **unrolled** into N copies of its body, each with
the loop-variable `%IDENT%` references replaced by the per-iteration
constant value. The unrolled bodies preserve the source location of
the original `generate` block (each replicated op's `loc(...)`
carries a `FusedLoc` of the body source-range plus the unroll-index
metadata). When `N` derives from a `param_int` / `param_str` /
`#define` symbol, parameter resolution (`NSLResolveParamsPass`,
upstream of generate-expansion) MUST have substituted the literal
value before unroll runs.

**Why this priority**: `generate`-loop unrolling is the headline
structural-expansion pass — it is the most user-visible,
most-aggressive IR rewrite at M5, and several of the seven audited
NSL projects (notably `cpu16` and `mips32_single_cycle` per the M4
spec's audited-corpus references) rely on it. Without it, those
projects cannot reach M6's CIRCT lowering. Constitutionally, this
pass is named in the README's M5 row parenthetical
"(generate-loop unroll, struct-SSA-split, `%IDENT%` residue check)"
— it is one of the three M5 sub-deliverables the README calls out by
name. P1 because the M7 audited-corpus regression cannot pass
without it.

**Independent Test**: Ship `test/Lower/generate/<scenario>.nsl` +
`<scenario>.mlir.expected` per axis: (a) literal-bound generate
(`generate(i=0; i<4; i++)`), (b) `param_int`-bound generate, (c)
`#define`-bound generate, (d) nested generate (inner generate uses
outer's `%i%`), (e) generate body referencing the loop variable in
multiple positions (decl name `buf_%i%`, expression `%i% + 1`).
Assert via lit + FileCheck that the post-pipeline IR contains zero
`nsl.structural_generate` ops AND the expected number of replicated
bodies. The pass-standalone test path uses `nsl-opt
-nsl-expand-generate` on a hand-authored `.mlir` fixture (independent
of the AST→nsl visitor — proves the pass works on its own).
Independent of US1 (full pipeline), US3 (variable expansion), US4
(residue check), US5 (determinism).

**Acceptance Scenarios**:

1. **Given** an `nsl.module @M` containing a single `nsl.structural_generate` with literal bounds `0..4` and a body declaring `nsl.reg "buf_%i%" : !nsl.bits<8>`, **When** `NSLExpandGeneratePass` runs, **Then** the post-pass IR contains four `nsl.reg` ops named `"buf_0"`, `"buf_1"`, `"buf_2"`, `"buf_3"` AND zero `nsl.structural_generate`.
2. **Given** an `nsl.module @M` with `param_int @N = 8` and an `nsl.structural_generate` whose bound expression references `%N%`, **When** the pipeline runs through `NSLResolveParamsPass` then `NSLExpandGeneratePass`, **Then** the post-pipeline IR contains eight replicated bodies AND zero `nsl.structural_generate` AND zero unresolved `%N%` references.
3. **Given** a nested-generate scenario (outer `i = 0..4`, inner `j = 0..i`), **When** the pass runs, **Then** the post-pass IR contains the triangular number (0+1+2+3 = 6) of inner-body replications AND zero `nsl.structural_generate`.
4. **Given** any pre-pass IR with N `nsl.structural_generate` ops, **When** the pass runs, **Then** the post-pass IR's op count for the body is `sum(unroll_count_i for i in 1..N)` AND every replicated op carries the per-iteration `unroll_index` attribute resolvable through `loc(...)`.
5. **Given** a `generate` body whose loop variable does NOT appear inside any `%IDENT%` splice (legal per S10 but rare in practice), **When** the pass runs, **Then** the post-pass IR contains the expected N copies of the body AND each copy is structurally identical (no spurious distinguishers).
6. **Given** any malformed pre-pass IR that violates `S10` (`generate` loop variable is not an `integer` per Sema's verdict), **When** Sema has rejected such input upstream at M3, **Then** M5 never sees that input and the unroll pass is not exercised on it (Sema is the gate; M5 trusts its verdict).
7. **Given** a `generate` body identifier referenced via `%IDENT%` that is not the loop variable but a separately-defined macro (per pp.ebnf P3), **When** the pass runs, **Then** that macro reference is preserved literally for the residue-check pass to handle (US4) — the unroll pass is concerned only with the loop variable, not with all `%IDENT%` references.

---

### User Story 3 — `nsl.variable` lowers to a wire+transfer SSA chain (struct-SSA-split) (Priority: P2)

A contributor authors an NSL source containing one or more
`variable` declarations (lang.ebnf §6: `variable v[W];`) with
multiple write-then-read sites. After `nslc -emit=mlir`, the output
contains zero `nsl.variable` ops — every variable has been
**expanded** into a chain of `nsl.wire` declarations and
`nsl.transfer` ops such that each "version" of the variable is a
distinct SSA value (matching the design §9 row "`NSLExpandVariablesPass`
— Convert `nsl.variable` to an SSA chain of `nsl.wire`+`nsl.transfer`
operations"). Struct-typed variables (variables whose type is a
`!nsl.struct<@T>`) decompose per-field: the SSA chain operates on
each field independently, preserving the user's per-field
write/read pattern. The `%LHS partial assignment` permitted by `S12`
on variables (but NOT on wires/regs) is preserved through the
expansion — every partial-write becomes a wire-level `nsl.transfer`
of the modified slice followed by a re-merge.

**Why this priority**: `struct-SSA-split` is the README's named
second M5 sub-deliverable and is the single most-impactful
correctness-and-readability pass — it converts NSL's mutable
variable semantics into MLIR's SSA discipline, which is what makes
M6's CIRCT lowering tractable (CIRCT's `comb` / `seq` dialects assume
SSA inputs). Without it, M6 cannot lower a non-trivial NSL source.
P2 (rather than P1) because the M5 acceptance gate is the
`-emit=mlir` round-trip per US1; this pass is necessary FOR US1 on
inputs containing variables but is itself testable in isolation.
Some audited projects compile cleanly without ever using `variable`
(simpler designs use `reg` + `wire` only) — those projects can
reach M6 without this pass exercising its full behaviour.

**Independent Test**: Ship `test/Lower/variables/<scenario>.nsl` /
`.mlir.expected` per axis: (a) scalar variable single-write
single-read, (b) scalar variable multiple writes (chain of three or
more transfers), (c) variable with partial assignment `v[3:0] = …`
(S12), (d) struct-typed variable with per-field writes, (e) variable
declared in a `func` and consumed in an enclosing `proc` (cross-
scope visibility). Assert post-pipeline IR contains zero
`nsl.variable` AND the expected wire-chain shape via FileCheck. The
pass-standalone path uses `nsl-opt -nsl-expand-variables` on a
hand-authored `.mlir` fixture. Independent of US1/US2/US4/US5.

**Acceptance Scenarios**:

1. **Given** an `nsl.module` containing `nsl.variable "v" : !nsl.bits<8>` with a single transfer-write `v = a` and a single read `b = v`, **When** `NSLExpandVariablesPass` runs, **Then** the post-pass IR contains zero `nsl.variable` AND a single `nsl.wire "v" : !nsl.bits<8>` AND the read's source becomes the wire's defining op.
2. **Given** the same module with a chain of three writes (`v = a; v = v + 1; v = v * 2;`) followed by one read, **When** the pass runs, **Then** the post-pass IR contains three distinct wire-version SSA values AND the read consumes the third version.
3. **Given** a variable with partial-assignment `v[3:0] = x; v[7:4] = y;` (S12 permits this on variables), **When** the pass runs, **Then** the post-pass IR contains a wire-chain whose final SSA value is the slice-merge of `x` and `y` per the bit-positions assigned.
4. **Given** a struct-typed variable `variable s : @T;` with `s.a = x; s.b = y;`, **When** the pass runs, **Then** the post-pass IR has the chain operating per-field (the `.a` field sees one transfer, the `.b` field sees one transfer; reads of `s` materialize via field-level wires).
5. **Given** any `nsl.wire` or `nsl.reg` in the input (not a variable), **When** the pass runs, **Then** these ops pass through unchanged — the pass is selective: only `nsl.variable` is its target.

---

### User Story 4 — Post-expansion `%IDENT%` residue triggers a sourced fail diagnostic (Priority: P2)

A contributor authors an NSL source whose preprocessor `%IDENT%`
splices CANNOT be fully resolved at expansion time (because they
reference an identifier that exists neither as a `#define` macro,
nor as a `param_*`, nor as a `generate` loop variable, nor as any
other lexical-scope symbol the preprocessor + AST + Sema chain
recognises). M3 Sema does not catch this case in general because
the residue is a property of the *post-unroll* IR — the
preprocessor's expansion phase substitutes what it can, leaves the
rest as a special "unresolved residue" marker, and trusts a later
phase to reject it if it survives. M5 is that later phase. After
`NSLCheckSemanticsPass` runs (last in the M5 pipeline), any
surviving `%IDENT%` residue produces a **single** diagnostic of the
form `<path>:<line>:<col>: error: unresolved macro splice
'%<IDENT>%' after structural expansion` and the driver exits non-
zero. The location resolves to the original NSL source range of the
splice (Principle IV — diagnostic plumbing crosses the seam).

**Why this priority**: `%IDENT%` residue check is the README's
named third M5 sub-deliverable. The check is a fail-fast for an
ambiguity the M1 preprocessor + M3 Sema chain cannot resolve in
isolation — without it, residue identifiers leak into M6 CIRCT
lowering and produce mysterious "unknown symbol" errors deep in
CIRCT, which violates Principle IV's "every diagnostic at its
earliest layer" rule. P2 (rather than P1) because the typical
well-formed NSL source produces zero residue and never exercises
this path; the check exists specifically for the malformed-preprocessor
case.

**Independent Test**: Ship `test/Lower/residue/<scenario>.nsl` /
`<scenario>.expected_diagnostic.txt` pairs per axis: (a) `%IDENT%`
referencing a symbol that exists nowhere (typo case), (b) `%IDENT%`
referencing a symbol that exists in the source but in a different
scope (visibility case), (c) `%IDENT%` inside a `generate` body
referencing neither the loop variable nor any `#define` (legitimate
residue case). Assert via lit + FileCheck-on-stderr that the
expected diagnostic line matches AND the driver exit code is non-
zero. The pass-standalone path uses `nsl-opt -nsl-check-semantics`
on a hand-authored `.mlir` fixture containing a deliberately-
unresolved `%IDENT%` marker. Independent of US1/US2/US3/US5.

**Acceptance Scenarios**:

1. **Given** an NSL source `reg buf_%TYPO% [8];` where `TYPO` is undefined everywhere, **When** `nslc -emit=mlir` runs, **Then** stderr contains `error: unresolved macro splice '%TYPO%' after structural expansion` AND exit code is non-zero AND no `.mlir` output is produced.
2. **Given** an NSL source whose `%IDENT%` is a valid `generate`-loop-variable reference, **When** `nslc -emit=mlir` runs, **Then** the post-`NSLExpandGeneratePass` IR has the splice resolved to the per-iteration constant AND `NSLCheckSemanticsPass` finds zero residue AND the driver exits zero.
3. **Given** an NSL source with TWO `%IDENT%` residue cases on different lines, **When** the residue check runs, **Then** TWO diagnostics are emitted (one per residue, each with its own source location) AND the driver exits non-zero.
4. **Given** any IR that has cleanly survived all five upstream M5 passes, **When** `NSLCheckSemanticsPass` runs, **Then** the pass succeeds quickly (no walks beyond a single op-tree traversal looking for the residue marker) — the check is a final correctness gate, not a heavy re-analysis.

---

### User Story 5 — Two-build determinism: byte-stable `-emit=mlir` output (Priority: P3)

A contributor invokes `nslc -emit=mlir input.nsl -o build_a.mlir`,
discards the build directory, rebuilds the project from a clean
state, and re-invokes the same command producing `build_b.mlir`. The
two output files are byte-identical. The same property holds across
two independent CI runners (host paths differ; output paths must NOT
leak into the IR). This is the README's M5 row determinism gate
("byte-stable across two builds") — Constitution Principle V is the
binding authority.

**Why this priority**: Determinism is a Principle V mandate that
applies project-wide; it is not a M5-specific deliverable. P3 here
because it falls out of correct construction of all other passes
(no time-of-day sources, no pointer-address-derived names, no host-
path embedding, no unordered set/map iteration in code paths
producing names) — there is no separate "determinism feature" to
build. The user story exists to make the test gate **explicit and
mechanically auditable**, parallel to how the M3 and M4 specs called
out their constitutional anchors as standalone US.

**Independent Test**: A CI matrix job builds the project in
`$WORKSPACE_A` and `$WORKSPACE_B` (two distinct host paths), runs
the same `nslc -emit=mlir` invocation against the same M3-corpus
fixtures in both, and `diff -q`s every output. The test passes if
and only if every diff is empty. The same job re-runs after a
`make clean && make` cycle in a single workspace (intra-workspace
determinism). Independent of US1/US2/US3/US4 — those test correctness
of individual passes; this tests the whole-pipeline output's bit-
level reproducibility.

**Acceptance Scenarios**:

1. **Given** the canonical M3 corpus, **When** the project is built in two distinct host paths (e.g., `/build-a/` and `/build-b/`) and `nslc -emit=mlir` is invoked over each input, **Then** every output file is byte-identical across the two host paths.
2. **Given** any single input file processed twice in the same workspace (back-to-back), **When** `nslc -emit=mlir` runs, **Then** the second invocation's output is byte-identical to the first.
3. **Given** any output IR file, **When** the file is grepped for build-tree paths (e.g., `/home/`, `/build-`, `$TMPDIR`), **Then** zero matches are found — host paths must not leak into the IR (this is the same anti-leakage class that surfaced as M4 commit `3326eb6`'s GCC-ASan globals-instrumentation fix).
4. **Given** a CI matrix run with N parallel runners, **When** the M5 determinism gate executes, **Then** N byte-identical artefacts result and the gate passes if and only if the cardinality of unique output checksums equals 1.

---

### Edge Cases

- **Empty compilation unit**. An NSL source containing zero `module` declarations (e.g., a header consisting only of `#define`s) lowers to an empty `mlir::ModuleOp` with no `nsl.module` children. `nslc -emit=mlir` exits zero and prints the empty top-level module; `NSLCheckSemanticsPass` on an empty module is a no-op.
- **`generate` with bound resolving to zero or one**. `generate(i = 0; i < 0; i++)` unrolls to zero replicated bodies (entire generate is removed); `generate(i = 0; i < 1; i++)` unrolls to a single replicated body. Both cases must NOT produce verifier failures from M4's parent-region invariants.
- **`generate` loop variable aliasing a `#define` macro**. The grammar permits a name collision; the unroll pass binds the loop variable in a scope above any conflicting `#define` for the duration of the body's expansion. After the body is replicated, the loop-variable scope is discarded — any subsequent `%IDENT%` resolution sees only the original `#define`.
- **`variable` declared but never written before read**. M3 Sema is the gate (under `S6` or related); M5 trusts Sema's verdict. If such input reaches M5 (because Sema produced no diagnostic for whatever reason), `NSLExpandVariablesPass` produces a wire chain whose initial value is undefined per MLIR semantics — this is acceptable because the upstream Sema-verdict invariant has been violated and the residue check will not catch it (residue check is for `%IDENT%` only, not for variable-init).
- **Submodule array of size zero or one**. `NSLExplodeSubmodArrayPass` produces zero or one `nsl.submodule` ops respectively. Both cases must verify clean against M4's instance-list invariants.
- **`func_self` inlining failure**. `NSLInlineInternalFuncPass` is "optional perf pass" per design §9 — if a particular `func_self` cannot be inlined (e.g., the call site is inside a region where inlining would violate M4's structural invariants), the pass leaves it un-inlined and the pipeline continues; this is a no-op, not a failure.
- **Diagnostic flood from cascading post-expansion errors**. If `NSLCheckSemanticsPass` finds N residue cases AND M re-checked-`Sn` violations, the driver emits N+M diagnostics, sorted by source location, and exits non-zero. The diagnostic engine MUST NOT abort after the first error (consistency with M2/M3 multi-error recovery behaviour).
- **Pass-pipeline failure mid-flight**. If pass `i` fails (e.g., `NSLExpandGeneratePass` crashes on malformed pre-IR), the pipeline halts at pass `i`, emits the failing pass's diagnostic, and exits non-zero — passes `i+1..6` are NOT executed (matches `mlir::PassManager::run`'s default failure semantics).
- **Output to a non-writable path**. `nslc -emit=mlir input.nsl -o /readonly/out.mlir` fails with a single I/O diagnostic from the driver layer (NOT from a pass) and exits non-zero — the IR is fully produced before the write attempt; the failure is a write-stage failure.
- **Stdin-piped input via `nslc -emit=mlir -`**. The driver reads from stdin; the source-locating diagnostic uses the synthetic path `<stdin>` (matches `-emit=ast -` behaviour from M3).

## Requirements *(mandatory)*

### Functional Requirements

#### Library scaffolding

- **FR-001**: The `nsl-lower` library MUST expose **exactly one public umbrella header** `include/nsl/Lower/Lower.h` per Constitution Principle II's single-public-header rule. The umbrella re-exports (a) the `ASTToMLIR` visitor entry point, (b) the six pass-construction free functions (`createNSLResolveParamsPass`, `createNSLExpandGeneratePass`, `createNSLExpandVariablesPass`, `createNSLExplodeSubmodArrayPass`, `createNSLInlineInternalFuncPass`, `createNSLCheckSemanticsPass`), and (c) the public registration helper that adds all six passes to a given `mlir::PassManager`.
- **FR-002**: The library MUST link only against `nsl-basic`, `nsl-ast`, `nsl-sema`, `nsl-dialect`, the upstream MLIR `IR` / `Pass` / `Support` libraries, and the four CIRCT link-libs already declared in `lib/Lower/CMakeLists.txt` (kept inert at M5 — no `circt::*` symbol is referenced from any compiled translation unit at this milestone). It MUST NOT depend on `nsl-parse` or `nsl-preprocess` (those produce the AST consumed at the seam, not used at the lowering layer).
- **FR-003**: The library MUST be built via the `add_nsl_library` macro from M0 (the existing `lib/Lower/CMakeLists.txt` declaration is preserved verbatim — only the source-file list grows).

#### AST → `nsl` dialect lowering (the visitor)

- **FR-004**: The `ASTToMLIR` visitor MUST be a class deriving from `ast::ConstASTVisitor` (per design §8 line 1008). Its constructor takes an `mlir::MLIRContext&` and a `sema::SemaResult&` (Sema's verdict is consumed; M5 does not re-run Sema).
- **FR-005**: The visitor's public entry point `lower(const ast::CompilationUnit&)` MUST return an `mlir::OwningOpRef<mlir::ModuleOp>` whose body contains one `nsl.module` per `ast::ModuleBlock` in the input compilation unit. The internal walk strategy MUST be **single-pass with MLIR `SymbolTable` lazy resolution** (per Clarifications Q4 → Option A): the visitor walks the AST exactly once, and cross-region symbol references (`nsl.call @Q`, `nsl.goto @s1`, etc.) are emitted as `mlir::FlatSymbolRefAttr` regardless of whether the referent has been visited yet. MLIR's stock `SymbolTable` machinery resolves all references at op-tree finalization (M4 verifier time). M3 Sema has already validated that all references resolve, so no symbol-validation work falls on the visitor. Forward references across `func`/`proc` boundaries within the compilation unit are supported by construction.
- **FR-006**: Every concrete `visit(...)` override on the visitor MUST produce the `nsl::*` op named in design §8's lowering table (lines 1057–1078). Each row of that table maps to a frozen "MUST" requirement here:
  - `ModuleBlock` → `nsl.module` (port list built from associated `DeclareBlock`)
  - `RegDecl` → `nsl.reg "n" : !nsl.bits<W> = <init>` (init as attribute, not SSA)
  - `WireDecl` → `nsl.wire "n" : !nsl.bits<W>`
  - `MemDecl` → `nsl.mem "n" [D x T] = <init>`
  - `ProcDefn` → `nsl.proc @p { ... }` (body is an isolated region)
  - `StateDefn` → `nsl.state @s { ... nsl.goto @target ... }`
  - `FirstStateDecl` → `nsl.first_state @s` (attribute-marker child)
  - `AltBlock` → `nsl.alt { nsl.case ... nsl.default ... }`
  - `AnyBlock` → `nsl.any { nsl.case ... }`
  - `SeqBlock` → `nsl.seq { ... }`
  - `WhileBlock` → `nsl.while %c { ... }`
  - `ForBlock` (enum form) → `nsl.for %init, %end { ... }`
  - `ForBlock` (C-style form) → `nsl.for %init, %cond, %step { ... }`
  - `GenerateBlock` (per `lang.ebnf §8`, the `structural_generate` clause referenced under S10) → `nsl.structural_generate` (the marker op consumed by `NSLExpandGeneratePass` at slot 2; survives AST → nsl lowering and is fully eliminated post-pipeline per FR-014)
  - `VariableDecl` (per `lang.ebnf §6`) → `nsl.variable` (the marker op consumed by `NSLExpandVariablesPass` at slot 3; fully eliminated post-pipeline per FR-015)
  - `TransferStmt` (`=`) → `nsl.transfer`
  - `TransferStmt` (`:=`) → `nsl.clocked_transfer`
  - `ControlCallStmt` → `nsl.call @target(...)`
  - `BareFinishStmt` → `nsl.finish`
  - `SystemTaskStmt` × {`_display`, `_finish`, `_init`, `_delay`} → `nsl.sim_display` / `nsl.sim_finish` / `nsl.sim_init` / `nsl.sim_delay`
  - `StructCastExpr` → `nsl.struct_cast %v : @T` + chain of `nsl.field`
  - Control-name used as 1-bit value (per S27) → `nsl.fire_probe @name`
- **FR-007**: Expression-position AST nodes (`BinaryExpr`, `UnaryExpr`, `LiteralExpr`, `IdentifierExpr`, `ConditionalExpr`, `SliceExpr`, `ConcatExpr`) MUST lower into `mlir::Value` SSA results consumed by the enclosing statement-level op. Width/sign attributes from M3's `TypeSystem` MUST flow into the result type without reinterpretation. The visitor MUST NOT introduce implicit narrowing/widening — type mismatches are Sema's domain at M3 and would have been rejected upstream.
- **FR-008**: Every MLIR op the visitor creates MUST carry an `mlir::Location` derived from the originating AST node's `SourceRange`. Composite ops (e.g., `nsl.struct_cast` plus a chain of `nsl.field`s lowered from a single `StructCastExpr`) MUST use a `mlir::FusedLoc` aggregating the relevant sub-ranges. No op MAY carry `mlir::UnknownLoc` after lowering completes.
- **FR-009**: The visitor MUST NOT mutate the input AST or the input `SemaResult`. Both are `const`-borrowed for the lifetime of the `lower(...)` call.
- **FR-010**: The visitor MUST NOT lower input that contains M3-level Sema diagnostics — the driver gate ensures Sema-clean input is the only input the visitor sees. If Sema-clean input nonetheless triggers an internal invariant violation in the visitor (e.g., a missing case in the `visit()` switch), the visitor MUST emit a sourced ICE-style diagnostic (`"internal: AST→MLIR lowering: unhandled <node>"`) and the driver MUST exit non-zero — silent miscompilation is forbidden.

#### Pass pipeline

- **FR-011**: Six passes MUST be implemented under the names `NSLResolveParamsPass`, `NSLExpandGeneratePass`, `NSLExpandVariablesPass`, `NSLExplodeSubmodArrayPass`, `NSLInlineInternalFuncPass`, and `NSLCheckSemanticsPass`. Each MUST be an `mlir::OperationPass<nsl::ModuleOp>` registered with a stable command-line flag name (`-nsl-resolve-params`, `-nsl-expand-generate`, `-nsl-expand-variables`, `-nsl-explode-submod-array`, `-nsl-inline-internal-func`, `-nsl-check-semantics`).
- **FR-012**: `Compilation::runNSLPasses` MUST construct the pipeline in this exact order: (1) resolve-params → (2) expand-generate → (3) expand-variables → (4) explode-submod-array → (5) inline-internal-func → (6) check-semantics. Reordering would change semantics (resolve-params produces the constants that expand-generate consumes; expand-generate must run before expand-variables so that variables inside replicated bodies receive consistent SSA names; check-semantics must run last so it sees the post-expansion shape).
- **FR-013**: `NSLResolveParamsPass` MUST replace every `nsl.param_int` / `nsl.param_str` operand reference inside any op (including inside `nsl.structural_generate` bound expressions) with the constant value from the M3 Sema parameter map. Post-pass IR MUST contain zero unresolved param references.
- **FR-014**: `NSLExpandGeneratePass` MUST replace each `nsl.structural_generate` op with N inline copies of its body, where N is the integer-typed loop bound expression (`integer` is required by `S10`, enforced upstream by M3 Sema). Each copy's `%IDENT%` references to the loop variable MUST be replaced by the per-iteration constant value (matching the preprocessor's textual splice semantics). Post-pass IR MUST contain zero `nsl.structural_generate` ops.
- **FR-015**: `NSLExpandVariablesPass` MUST replace each `nsl.variable` op with a chain of `nsl.wire` declarations and `nsl.transfer` ops that preserve the user's write-then-read semantics in SSA form. Struct-typed variables MUST decompose per-field. Partial-assignment behaviour preserved by `S12` MUST be preserved through expansion. Post-pass IR MUST contain zero `nsl.variable` ops.
- **FR-016**: `NSLExplodeSubmodArrayPass` MUST replace each `nsl.submodule` op carrying an array-form name (`SUB[3]`) with N independent `nsl.submodule` ops named `SUB[0]`, `SUB[1]`, ..., `SUB[N-1]`. References to array-element ports across the broader IR MUST be rewritten to address the corresponding numbered instance. Post-pass IR MUST contain zero array-form `nsl.submodule` ops.
- **FR-017**: `NSLInlineInternalFuncPass` MUST exist as a registered pass at pipeline slot 5 with the `-nsl-inline-internal-func` flag, but it ships at M5 as a **registered no-op slot** (per Clarifications Q3 → Option B): the pass walks the input `nsl::ModuleOp` once, performs no inlining, and returns `mlir::success()` with the IR byte-identical to its input. The slot exists to preserve the pipeline ABI (pass-name + flag + position) so a future PR can fill in functional inlining without amending this spec. FR-028's per-pass fixture requirement for this pass is a single round-trip fixture asserting the no-op invariant.
- **FR-018**: `NSLCheckSemanticsPass` MUST detect surviving `%IDENT%` residue **via regex scan** over every `mlir::StringAttr`-typed attribute on every `mlir::Operation*` reachable from the input `nsl::ModuleOp` (per Clarifications Q1 → Option B). The regex MUST be `%[A-Za-z_][A-Za-z0-9_]*%`. Each match MUST produce one diagnostic of the form `error: unresolved macro splice '%<IDENT>%' after structural expansion` with an `mlir::Location` set to the carrying op's location (which traces back to the original splice's NSL source range per Principle IV). The pass MUST also re-check the post-expansion-sensitive subset of `S1`–`S29` (specifically those whose verdict can change after generate-unroll, param-resolution, or submod-array-explosion) and emit sourced diagnostics for any violation. The pass MUST process all such violations in a single pass invocation (multi-error recovery — no abort-after-first). The pass MUST NOT walk into nested `mlir::DictionaryAttr` / `ArrayAttr` recursively beyond top-level string-attrs; if a future op needs nested-string residue detection, that is a follow-up amendment to this FR.
- **FR-019**: Every pass MUST funnel its diagnostics through the project's single `basic::DiagnosticEngine` (Constitution Principle IV). MLIR's built-in `mlir::DiagnosticEngine` MAY be used internally as a transit mechanism (it is the only way to associate `mlir::Location` with a `mlir::Operation*` failure) but its output MUST be intercepted via a `mlir::ScopedDiagnosticHandler` and reposted to the project diagnostic engine before any user-visible printing.

#### Driver wiring

- **FR-020**: `Compilation::lowerToNSL` (signature frozen at M4) MUST construct an `ASTToMLIR` visitor over its input `ast::CompilationUnit&` + `sema::SemaResult&` parameters and return its result as `mlir::OwningOpRef<mlir::ModuleOp>`. Any visitor-internal failure MUST be translated to a diagnostic-engine error and a returned `nullptr` (or equivalent failed-`OwningOpRef` representation that the caller can inspect via the diagnostic engine's `hasErrors()`).
- **FR-021**: `Compilation::runNSLPasses` (signature frozen at M4) MUST construct an `mlir::PassManager` rooted at the supplied `mlir::ModuleOp`, register the six passes in the FR-012 order, run them, and return `mlir::success()` / `mlir::failure()` matching the pipeline outcome. Any pass-internal failure MUST be translated to diagnostic-engine errors per FR-019.
- **FR-022**: A new driver flag `nslc -emit=mlir` MUST be wired into `CompileOptions::EmitKind::NSLMLIR` (the M3-frozen enum value) such that the driver halts the pipeline after `runNSLPasses` returns success and prints the final `mlir::ModuleOp` to stdout (or `-o <file>`) using MLIR's **default printer** (`mlir::OpPrintingFlags()` with no flags set — no `printGenericOpForm`, no `useLocalScope`, no `enableDebugInfo`). The output MUST be byte-identical to what `nsl-opt %.mlir` would print on the same IR (per Clarifications Q2 → Option A — printer-format symmetry with the M4 round-trip baseline).
- **FR-023**: The `-emit=tokens` and `-emit=ast` driver flags from M3 MUST behave identically at M5 — no behavioural drift on existing flags. The `-emit=hw` and `-emit=verilog` flags MAY remain stubbed (return non-zero with a "not yet implemented" diagnostic) at M5; M6/M7 will fill them in.
- **FR-024**: `nslc -emit=mlir -` (stdin-piped input) MUST work, with the synthetic path `<stdin>` appearing in any source-locating diagnostics.

#### Determinism (Principle V)

- **FR-025**: `nslc -emit=mlir <input>` MUST produce byte-identical output across (a) two invocations in the same workspace, (b) two builds in different host paths, (c) two CI runners. The output MUST NOT contain any host-path string, time-of-day string, pointer-address-derived name, or unordered-iteration artifact.
- **FR-026**: Every code path that produces a name (region argument names, op symbol names, pass-internal temporary names) MUST be deterministic — derived from a stable counter rooted in lexical-source order or from the AST symbol table's deterministic iteration, never from `std::unordered_map`, `llvm::DenseMap` over pointer keys, address-derived hashes, or timestamps.

#### Testing (Principle VI — NON-NEGOTIABLE)

- **FR-027**: For every concrete `visit(...)` override on `ASTToMLIR`, `test/Lower/<category>/<node>_emit_mlir.nsl` + `<node>_emit_mlir.mlir.expected` MUST exist. A CI guard MUST mechanically enumerate the visitor's `visit()` overrides (via the AST visitor registry from M2) and assert a matching fixture exists; missing fixtures MUST fail CI per Principle IX.
- **FR-028**: For each of the six passes, `test/Lower/passes/<pass-flag>/<scenario>.mlir` + `<scenario>.expected.mlir` pass-standalone fixtures (consumed via `nsl-opt -<pass-flag>`) MUST exist covering at minimum: one well-formed scenario, one edge-case scenario (empty input, single-element bound, max-bound, etc.), and (for `NSLCheckSemanticsPass`) one rejection scenario per residue-class.
- **FR-029**: A determinism test MUST exist that builds the project twice in distinct host paths and `diff -q`s every `nslc -emit=mlir` output across the two builds. The test passes if and only if every diff is empty.
- **FR-030**: The M3-corpus end-to-end smoke test MUST be extended: every `.nsl` source under `test/sema/` that produces zero diagnostics MUST also be exercisable with `nslc -emit=mlir` and produce verifier-clean IR (M4 `nsl-opt` round-trip on each output is a fixed point). Failures here are CI-blocking per Principle IX.

#### Spec coupling (Principle VII)

- **FR-031**: This spec ships changes to (a) [`CLAUDE.md`](../../CLAUDE.md) §1 — the rows whose "Lower to dialect" column entry is now delivered (every M5-marked row) gain confirmation; the row "Action statements: `seq` / `if` / `for` / `while` / `generate`" gains its M5 generate-unroll entry; (b) [`docs/CLAUDE.md`](../../docs/CLAUDE.md) §3 (the "Writing a structural-expansion pass" entry) — line-range pointers updated if any design §9 line-range shifted; (c) [`docs/design/nsl_compiler_design.md`](../../docs/design/nsl_compiler_design.md) §8 + §9 — only if implementation reveals a design-doc inaccuracy, in which case both spec and design are amended in lockstep per Principle VII.

### Key Entities *(include if feature involves data)*

- **`ASTToMLIR` visitor** — A `nsl::lower` class deriving from `ast::ConstASTVisitor` that walks an `ast::CompilationUnit` **once** (per Clarifications Q4 → Option A: single-pass with MLIR `SymbolTable` lazy resolution) and produces an `mlir::OwningOpRef<mlir::ModuleOp>`. Owns an `mlir::OpBuilder`, a `valueMap_` (`Symbol*` → `mlir::Value`) for SSA name resolution per current region, and a `symbolRefs_` map (`Identifier*` → `FlatSymbolRefAttr`) for cross-region symbol references emitted during the walk; cross-region resolution itself is delegated to MLIR's `SymbolTable` at verification time. Borrows the `MLIRContext` and the `SemaResult` for its lifetime.
- **Pass pipeline (six passes)** — A linear sequence of `mlir::OperationPass<nsl::ModuleOp>` instances assembled in `Compilation::runNSLPasses`. Each pass exposes a stable `getArgument()` flag name for `nsl-opt` standalone testing. The order is fixed (FR-012).
- **`nsl.structural_generate` op** — The M4 marker op that survives AST→nsl lowering and becomes the input to `NSLExpandGeneratePass`. Carries the loop-init / loop-cond / loop-step expressions as operands or attributes (per the M4 dialect contract — to be confirmed at implementation time against `dialect-api.contract.md`).
- **`nsl.variable` op** — The M4 op representing NSL's mutable-variable semantics. Becomes the input to `NSLExpandVariablesPass` and is fully eliminated post-expansion; has no CIRCT counterpart.
- **`%IDENT%` residue** — A literal `%[A-Za-z_]\w*%` substring carried inside a `mlir::StringAttr` value on some `nsl::*` op (typically the `n` (textual identifier) or `sym_name` attribute), surviving from the M1 preprocessor's expansion phase as a forwarded-but-unresolved splice. The M5 AST→nsl visitor preserves these substrings byte-for-byte from the M3 AST. Detection is by regex scan at `NSLCheckSemanticsPass` per FR-018 (Clarifications Q1 → Option B). No dedicated marker op or attribute exists; the M4-frozen dialect surface is unchanged.
- **`Compilation` driver class** — The M4-instantiated singleton driver-pipeline class whose `lowerToNSL` and `runNSLPasses` member-function bodies M5 fills in. Signatures are M4-frozen.
- **`-emit=mlir` driver flag** — A new value of `CompileOptions::EmitKind` (the enum was frozen at M3 with all six values populated; M5 wires the `NSLMLIR` arm). Halts the pipeline after `runNSLPasses` and prints the final `nsl::*` MLIR.
- **Lowering test fixtures** — A `.nsl` source paired with a `.mlir.expected` golden, consumed by lit + FileCheck. One fixture per AST node kind (FR-027) plus one per pass-axis (FR-028) plus determinism cross-build artefacts (FR-029).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Every concrete `visit(...)` override on `ASTToMLIR` has a paired `test/Lower/<category>/<node>_emit_mlir.nsl` fixture in `test/Lower/`. Cardinality matches mechanically (CI guard counts both sides; counts must be equal). The cardinality target is the count of `visit()` overrides in `ASTToMLIR.h` — typically 30–40 per the AST node hierarchy at design §3 lines 299–682.
- **SC-002**: The six passes (`NSLResolveParamsPass` … `NSLCheckSemanticsPass`) collectively expose six distinct `nsl-opt -<flag>` command-line flags; running `nsl-opt --help` lists all six with stable single-line descriptions.
- **SC-003**: Every M3-corpus fixture (every `.nsl` source under `test/sema/` that compiles cleanly) produces a verifier-clean `mlir::ModuleOp` when fed to `nslc -emit=mlir`. Pass rate: 100% (zero exceptions). The M4 `nsl-opt` round-trip on every such output is a fixed point.
- **SC-004**: A `NSLExpandGeneratePass` post-pass IR contains zero `nsl.structural_generate` ops on every fixture under `test/Lower/generate/`. The replicated-body count for each fixture matches the integer bound × nesting product within ±0 (exact match).
- **SC-005**: A `NSLExpandVariablesPass` post-pass IR contains zero `nsl.variable` ops on every fixture under `test/Lower/variables/`. The wire-chain SSA value count per scenario matches the test fixture's golden expectation.
- **SC-006**: A `NSLCheckSemanticsPass` rejects every fixture under `test/Lower/residue/` with the expected diagnostic line; FileCheck-on-stderr asserts the line exactly. Pass rate: 100% (zero false negatives, zero false positives on the residue-clean baseline corpus).
- **SC-007**: Two independent CI builds in distinct host paths produce byte-identical `nslc -emit=mlir` output for every M3-corpus fixture. `diff -q` on every output pair returns zero in 100% of cases.
- **SC-008**: Zero output `.mlir` file contains a host-path string. A `grep -E '/build|/home|$TMPDIR'` over every output produced in CI returns no matches.
- **SC-009**: Every output op carries a non-`UnknownLoc` `mlir::Location` resolvable to the original NSL source. A CI assertion walks every op in every test output and asserts `getLoc()` is not `UnknownLoc`. Failure cardinality: zero.
- **SC-010**: M3-frozen behaviour is preserved — `nslc -emit=tokens` and `nslc -emit=ast` outputs on the M3-corpus are byte-identical between the M5 build and the M4 build (regression guard). Cardinality of changes: zero across the M3-corpus.
- **SC-011**: The merged feature passes all nine constitutional principles end-to-end (CI stages I–IX green per Principle IX). No transitional clauses are invoked.
- **SC-012**: The next AST node kind added to the M2 grammar (post-M5) lands a new visitor case + a new fixture pair in a single PR; the M5 spec is not amended (the visitor's "every override has a fixture" rule is the load-bearing invariant). Time-to-add baseline: under one engineering day for a routine node.

## Assumptions

- The AST→nsl visitor consumes the **M3-frozen** `SemaResult` API surface (typed-AST nodes plus the symbol table + type system). No new public API on M3-track libraries is required at M5; if implementation surfaces a need, the M3 contract amendment is its own PR per Principle II/VII.
- The dialect op set is **M4-frozen** per [`specs/007-m4-mlir-dialect/contracts/dialect-api.contract.md`](../007-m4-mlir-dialect/contracts/dialect-api.contract.md) §2 (79 public types/functions — Q6 in the M4 Clarifications grew it to 48; the post-merge amendment 2026-05-01 added `nsl.constant` to bring it to 49 per the four-way-decision option (a) recorded in [`specs/007-m4-mlir-dialect/spec.md`](../007-m4-mlir-dialect/spec.md) Clarifications session 2026-05-01 and in `specs/008-m5-structural-passes/research.md` §15; the post-merge amendment 2026-05-02 added the 28-op expression surface to bring it to 77 per the four-way-decision option (B) recorded in `specs/007-m4-mlir-dialect/spec.md` Clarifications session 2026-05-02 and in `specs/008-m5-structural-passes/research.md` §16; the post-merge amendment 2026-05-02 #3 relaxed the `nsl.struct` parent trait without changing the count per `specs/008-m5-structural-passes/research.md` §17; the post-merge amendment 2026-05-02 #4 added `nsl.param_int` + `nsl.param_str` (top-level S16 parameter ops) plus field additions on existing ops (`structural_generate.loop_var`, `submodule.array_size`) to bring the count to 79 per the four-way-decision option (B) recorded in `specs/007-m4-mlir-dialect/spec.md` Clarifications session 2026-05-02 and in `specs/008-m5-structural-passes/research.md` §18). No further `nsl::*` op is added at M5; if a structural-expansion pass surfaces a missing op, that is a M4-contract amendment, not a stealth M5 addition.
- The pass pipeline order is fixed (FR-012). Future per-pass ordering experiments are out of scope; if any future milestone needs a different order, that is a Principle II/III architectural change, not a routine PR.
- "Representative samples per AST node kind" in the README's M5 test gate is interpreted as **one fixture per concrete `visit()` override**, not "a small sampling of node kinds with broad coverage gaps". The CI guard's enumeration test (FR-027) is the binding interpretation.
- `NSLCheckSemanticsPass`'s "re-check post-expansion-sensitive `Sn`" subset is implementer-determined at planning time but MUST be documented in `plan.md`. Reasonable default: any `Sn` whose Sema-time verdict references a value that could be a `param_int` / generate-loop-variable / submod-array-index — these are the subset whose post-expansion verdict can differ. The exact list is a `/speckit-plan` deliverable.
- `NSLInlineInternalFuncPass` is "optional perf pass" per design §9; a safe-by-default no-op implementation that never inlines is a legitimate M5 outcome. The pass exists as a registered pipeline slot; whether it does any work is a downstream optimisation question.
- The CIRCT link-libs in `lib/Lower/CMakeLists.txt` (`CIRCTHW CIRCTComb CIRCTSeq CIRCTSV`) are **already declared** at M0. They stay inert at M5 — no `circt::*` symbol is referenced from any compiled translation unit; CMake/ninja keeps the linker happy via dead-strip. M6 activates them.
- The M5 acceptance gate has no audited-corpus dependency. The seven audited projects under `test/audited/` (P-VEN, P-VCD) remain a M7 deliverable; M5's test corpus is hand-authored representative fixtures plus the M3-corpus extension (FR-030).
- M5 does not introduce a new `Sn`, `Nn`, or `Pn`. The `%IDENT%` residue check is the implementation of an existing constitutional invariant (P3 + Principle IV) at the appropriate layer; it is not a new spec entity.
- Determinism rules out `llvm::DenseMap` over pointer keys for any name-producing path — this is a routine engineering constraint, not a research question. Same applies to unordered iteration over Sema's symbol-table results; the symbol-table API delivered at M3 already provides an iteration order primitive.
- The umbrella header `Lower.h` is a single file (per Principle II's single-public-header rule). The internal-header expansion (per-pass headers under `include/nsl/Lower/Pass/`, the visitor header, the helpers) is private to the library and is a planning-time decision, not a spec decision.
