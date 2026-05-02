<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: `nslc -emit=mlir` Driver-Flag Behaviour

**Branch**: `008-m5-structural-passes` | **Date**: 2026-04-30
**Spec**: [spec.md](../spec.md) FR-022, FR-023, FR-024, US1, US5

This contract freezes the driver-flag surface introduced at M5 —
specifically the new `-emit=mlir` flag and the byte-stable output
format that follows from Q2 → Option A (default printer).

---

## 1. CLI flag surface

### 1.1 `-emit=mlir` (new at M5)

```text
nslc -emit=mlir [-o <file>] [-I <dir>] [-D<macro>=<val>] <input>...
```

- Halts the pipeline after `Compilation::runNSLPasses` returns
  success (i.e., after slot 6 `NSLCheckSemanticsPass`).
- Prints the post-pipeline `mlir::ModuleOp` using MLIR's default
  printer (`mlir::OpPrintingFlags()` with no flags set).
- `-o <file>`: writes to `<file>`. If absent, writes to stdout.
- `-I <dir>`: forwarded to M1 preprocessor for `#include` search
  (M3 contract; not M5-novel).
- `-D<macro>=<val>`: forwarded to M1 preprocessor.
- Stdin-piped input: `nslc -emit=mlir -` reads from stdin; the
  synthetic path `<stdin>` appears in any source-locating
  diagnostics (FR-024).

### 1.2 Pre-existing flags (M3-frozen, NOT modified at M5)

```text
nslc -emit=tokens <input>      # M1 lex output
nslc -emit=ast <input>         # M2/M3 AST snapshot
```

These flags MUST behave byte-identically between an M4 build and
an M5 build (FR-023, SC-010).

### 1.3 Forward-looking flags (stubbed at M5)

```text
nslc -emit=hw <input>          # M6 — not delivered
nslc -emit=verilog <input>     # M7 — not delivered
```

At M5, invocation produces:

```text
error: '-emit=hw' is not yet implemented (planned for M6)
```

The driver exits non-zero. The error string is byte-stable per
`Compilation::run()`'s default-arm output.

---

## 2. Output format (frozen by Q2 → Option A)

`Compilation::emit` for `EmitKind::NSLMLIR` MUST call the
following:

```cpp
llvm::raw_ostream& os = (opts_.outputFile.empty())
    ? llvm::outs()
    : openOutputFile(opts_.outputFile);
module->print(os);  // default OpPrintingFlags()
return 0;
```

The print invocation MUST NOT use:

- `printGenericOpForm()` — would force generic form, breaking M4 round-trip
- `useLocalScope()` — would break SSA-name globality
- `enableDebugInfo()` — would inject debug-info attributes that
  introduce determinism risk (timestamps, build paths)
- `assumeVerified()` — would skip MLIR's verifier; the M4 verifiers
  MUST run as a final correctness gate before printing

---

## 3. Determinism contract (frozen by Principle V + FR-025/FR-026)

For any single input file `input.nsl` and a fixed CLI flag set,
the output of `nslc -emit=mlir input.nsl` MUST be byte-identical
across:

| Axis | Variation | Output diff |
|---|---|---|
| Same workspace, two consecutive invocations | None | `diff -q a b` returns 0 |
| Same source code, two distinct host paths | `/build-a/` vs `/build-b/` | `diff -q a b` returns 0 |
| Same source code, two distinct CI runners | runner-1 vs runner-2 | `diff -q a b` returns 0 |
| Same source code, Debug vs Release | (build modes) | `diff -q a b` returns 0 |

**Forbidden in the output** (CI grep enforced):

- Any host-path string (`/build`, `/home`, `$TMPDIR`, etc.)
- Any time-of-day string (epoch-derived integer, ISO-8601 stamp)
- Any pointer-address-derived suffix (`%0x7f...`, etc.)
- Any non-deterministic SSA value naming (handled by MLIR default
  printer's emission-order counter — already deterministic by
  construction per research §8)

CI grep regex:

```text
/build|/home|/tmp/|\$TMPDIR|0x[0-9a-fA-F]{8,}|[0-9]{10,}T[0-9]{6,}
```

(applied to every `.mlir` file in `test/Lower/**/*.expected.mlir`
and to every CI-emitted output of `nslc -emit=mlir`)

---

## 4. Exit-code contract

| Scenario | Exit code | stderr | stdout (or `-o` file) |
|---|---|---|---|
| Successful lowering, residue-free | 0 | empty | well-formed `.mlir` text |
| M1 / M2 / M3 diagnostic | non-zero | one diagnostic per error | empty |
| M5 `Compilation::lowerToNSL` internal-error | non-zero | ICE diagnostic | empty (FR-010) |
| M5 pass-pipeline failure (residue / sensitive-`Sn`) | non-zero | one diagnostic per violation | empty (FR-018, US4 acceptance scenario 1) |
| `-o /readonly/path` write failure | non-zero | one I/O diagnostic | empty (IR fully produced before write attempt) |
| Forward-looking flag (e.g., `-emit=hw`) | non-zero | "not yet implemented" diagnostic | empty |

---

## 5. `Compilation::run()` dispatch arm (M5 wiring)

The arm wired at M5 in `lib/Driver/Compilation.cpp`:

```cpp
case EmitKind::NSLMLIR: {
    auto module = lowerToNSL(*cu, sr);
    if (!module) return printDiagsAndExitNonZero();
    if (mlir::failed(runNSLPasses(*module))) return printDiagsAndExitNonZero();
    return emitNSLMLIR(*module);
}
```

`emitNSLMLIR` is a private method on `Compilation` added in M5
(NOT part of the public `Compilation` API surface — the M4
contract froze the public API; private members are free to grow).

---

## 6. Stage outputs as next-stage inputs (Principle V invariant)

Per Constitution Principle V:

> Each stage's output MUST be loadable as the next stage's input
> where the abstraction permits (e.g., `nsl-opt` round-tripping
> `.mlir`).

For M5, this means:

```text
nslc -emit=mlir foo.nsl > foo.mlir
nsl-opt foo.mlir                     # MUST verify clean
nsl-opt foo.mlir | nsl-opt -          # MUST be a fixed point
```

The chained invocation `nslc -emit=mlir foo.nsl | nsl-opt -` MUST
produce byte-identical output to `nslc -emit=mlir foo.nsl` (the
trailing `nsl-opt -` is a no-op idempotent re-print). This is a
second-order determinism guarantee that falls out of the M4
round-trip property + the M5 default-printer choice.

---

## 7. Help-message contract (`nslc --help`)

`nslc --help` output MUST include `-emit=mlir` in the emit-flag
list. The exact line format:

```text
  -emit=<kind>     Stop after stage <kind>: tokens, ast, mlir, hw (M6+), verilog (M7+)
```

The "(M6+)" / "(M7+)" annotations are part of the help-text
contract — they signal forward-looking flags vs. operational
flags. M6 / M7 PRs MUST update this text to remove the annotation
when their respective flags become operational.

---

## 8. Test-fixture exposure

Per FR-027, every concrete `visit(...)` override on `ASTToMLIR`
gets a paired fixture exercising `nslc -emit=mlir` end-to-end.

The fixture format (lit + FileCheck convention):

```text
// RUN: nslc -emit=mlir %s | FileCheck %s

module M {
  declare M { input a[8]; output q[8]; }
  reg r[8] = 0;
  r := a;
  q = r;
}

// CHECK: nsl.module @M
// CHECK: nsl.reg "r" : !nsl.bits<8> = 0
// CHECK: nsl.clocked_transfer
// CHECK: nsl.transfer
```

Failures: `FileCheck` emits a `--dump-input-on-failure` block;
the lit harness exits non-zero; CI marks the test FAIL.
