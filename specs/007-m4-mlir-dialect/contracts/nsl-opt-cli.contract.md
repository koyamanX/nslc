<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: `nsl-opt` CLI Surface

**Branch**: `007-m4-mlir-dialect` | **Date**: 2026-04-30
**Plan**: [../plan.md](../plan.md)

This contract documents the M4 `nsl-opt` developer/test binary's
behavior. Per Constitution Principle II §4, `nsl-opt` is a developer/
test tool — NOT a user-facing T-track deliverable; release tarballs
do not bundle it (FR-016). The contract scope is what tests +
contributors rely on, not what end-users see.

## 1. Invocation forms

```
nsl-opt [flags] <input.mlir>      # parse + verify + print to stdout
nsl-opt [flags] -                 # read from stdin
nsl-opt [flags]                   # equivalent to "-" (stdin)
nsl-opt -h                        # list all flags (upstream MlirOptMain help)
```

## 2. Flag set at M4

`nsl-opt` inherits **the upstream `MlirOptMain` flag set verbatim**
(no flag added or removed at M4). The most commonly used flags:

| Flag | Behavior |
|---|---|
| `-o <file>` | Write output to `<file>` instead of stdout |
| `--verify-diagnostics` | Match `// expected-error{{...}}` annotations; fail if mismatch |
| `--allow-unregistered-dialect` | Allow ops from unregistered dialects to parse (off by default) |
| `--mlir-print-debuginfo` | Print location attributes inline |
| `--mlir-pretty-debuginfo` | Render locations in human-readable form |
| `--show-dialects` | List all registered dialects and exit |

**Pass-flag set**: at M4, **zero passes are registered** (per FR-015).
The `--<pass-name>` flag space is empty beyond MLIR's built-in
canonicalize / cse / etc. passes (which are no-ops on the dialect
since no canonicalization patterns are registered for `nsl.*` at M4).

## 3. Registered dialects at M4

`nsl-opt` registers the following dialects in its
`mlir::DialectRegistry`:

- `nsl` (the M4 deliverable)
- `builtin` (always; MLIR's `builtin.module` framing)
- `circt::hw`, `circt::comb`, `circt::seq`, `circt::fsm`,
  `circt::sv` (per design §11 lines 1146–1150 — pre-loaded so
  mixed-dialect fixtures parse correctly; per spec Edge-Cases §
  "mixed-dialect fixtures")

`--show-dialects` output thus contains `nsl`, `builtin`, `hw`,
`comb`, `seq`, `fsm`, `sv`.

## 4. Exit codes

- `0` — successful round-trip (parse + verify + print, no
  diagnostics)
- `non-zero` — any of: parse failure, verify failure, attempting to
  open an unreadable input file, OOM. Specifically:
  - parse failure: input doesn't tokenize / doesn't conform to the
    dialect's syntax. `mlir::ParseResult::failure()` propagates.
  - verify failure: any op's `::verify()` returns failure(). The
    diagnostic stream contains the op's `emitOpError` output.

## 5. Stdin/stdout shape

- Input: UTF-8 `.mlir` text. Reading from `-` blocks until EOF.
- Output: UTF-8 `.mlir` text matching MLIR's standard pretty-print
  format (deterministic per stability contract §3).
- Stderr: zero or more diagnostic lines, one per emitted diagnostic,
  in MLIR's standard format
  `<path>:<line>:<col>: error: '<op-name>' op <message>`.
- The `--verify-diagnostics` flag swaps roles: diagnostics that
  match `// expected-error` annotations consume those annotations
  silently; remaining diagnostics OR unmatched annotations cause
  exit-code 1.

## 6. Extension surface (forward-looking)

When M5 lands `Compilation::lowerToNSL` and the structural-expansion
passes (per design §9: `NSLResolveParamsPass`, `NSLExpandGeneratePass`,
…), `nsl-opt` will gain `--nsl-resolve-params`,
`--nsl-expand-generate`, etc. flags through the standard
`mlir::registerPass<>()` machinery. M4 reserves no flag names; M5's
flag set will be the upstream-MLIR convention (kebab-case
pass-name).

When M6 lands `nsl::*` → CIRCT lowering, `--nsl-to-circt` will
appear similarly.

The contract guarantee at M4 is: **no flag added at M5/M6 will
silently re-shape a flag that already had M4 semantics**. M4's
flag set is empty beyond MLIR built-ins, so this is trivially
true.

## 7. Test-driver compatibility

- lit + FileCheck (Constitution Principle VI's "Dialect tests use
  `nsl-opt` for round-trip verification of `.mlir`") works against
  this CLI without further adapter.
- `--verify-diagnostics` + `// expected-error` is the M4 invalid-
  fixture mechanism.
- The two-pass round-trip pattern `nsl-opt %s | nsl-opt - |
  FileCheck %s` works because stdin mode is supported.

## 8. Stability surface

- The flag set at M4 is "whatever upstream `MlirOptMain` provides
  + dialect registrations". Upstream `MlirOptMain` evolution
  (within the CIRCT-pinned LLVM/MLIR commit) is a project-wide
  upgrade decision; M4 does not pin a sub-set of upstream flags.
- The exit-code contract above is stable across M5+ as new flags
  are added.
- The `--show-dialects` output gains entries as new dialects load
  but does not lose entries.
