<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: `nsl-dialect` Stability Invariants

**Branch**: `007-m4-mlir-dialect` | **Date**: 2026-04-30
**Plan**: [../plan.md](../plan.md)

Determinism, identity, and ordering invariants that the dialect
upholds across patches. Parallel to M3's `sema-stability.contract.md`.

## 1. Determinism (Constitution Principle V)

- **Byte-stable IR output (FR-025)**. Two `nsl-opt` invocations on
  the same input + flag list MUST produce byte-identical stdout
  across (a) two consecutive runs, (b) Debug and Release build
  types, (c) gcc and clang compilers. Verified by `scripts/ci.sh`'s
  cross-compiler stage.
- **No env-var influence**. The dialect's printer/parser does not
  read any environment variable. The `mlir::MLIRContext` does not
  inject env-derived state into op attributes.
- **No embedded timestamps, hostnames, build paths**. The dialect's
  output is purely a function of the input MLIR + (when consumed
  via M5+) the input AST + the CLI flag list.

## 2. Symbol identity (FR-027)

- Every printed `@<name>` reference resolves by symbol name, not by
  `mlir::Operation*` pointer. MLIR's standard `SymbolRefAttr`
  machinery satisfies this — pointer-derived ordering is never
  introduced into serialization paths.
- Cross-references in the printed IR (e.g., `nsl.first_state @s0`,
  `nsl.goto @s1`, `nsl.connect %sub.port`) print exactly the
  symbol's name string, with MLIR's standard escape rules for
  identifiers containing unusual characters.

## 3. Ordering (FR-026)

- **Op-region iteration**: the printer iterates each op's regions
  in declaration order (MLIR's `Operation::getRegions()` is
  insertion-ordered).
- **Block-op iteration**: the printer iterates ops within a block
  in source-order (MLIR's `Block::getOps()` is a linked-list
  iterating in insertion order).
- **Attribute dictionary iteration**: MLIR's `DictionaryAttr` iterates
  in sorted-key order (alphabetical); printers use this convention.
- **No `unordered_map` / unordered `DenseMap` iteration in any
  printer or diagnostic-producing path**. The dialect's
  hand-written code MUST audit any auxiliary data structure used
  during verification or pretty-printing for this property; the
  CI's static-checks stage reviews diffs to flag introductions.

## 4. Type interning (FR-008)

- `BitsType::get(ctx, 8) == BitsType::get(ctx, 8)` — pointer
  equality (MLIR's standard interning).
- `BitsType::get(ctx, 8) != BitsType::get(ctx, 16)`.
- `StructType::get(ctx, SymbolRefAttr::get(ctx, "@MyStruct")) ==
  StructType::get(ctx, SymbolRefAttr::get(ctx, "@MyStruct"))`.
- Two `MLIRContext`s have **independent** type intern pools; equality
  across `MLIRContext`s is undefined (consumers SHOULD NOT compare
  types across contexts).

## 5. Round-trip invariants (FR-017)

- Single-pass round-trip: `nsl-opt fixture.mlir` produces output
  whose `// CHECK:` lines match.
- Two-pass round-trip: `nsl-opt fixture.mlir | nsl-opt -` produces
  output byte-identical to the first-pass output. (Asserted on every
  fixture by the second `// RUN:` line per research §8.)

## 6. Verifier idempotency

- Re-verifying an op (calling `op->verify()` twice) is harmless —
  produces the same diagnostic stream the second time.
- Verifier hooks do not mutate op state. `verify()` is `const` w.r.t.
  the op's data; only the diagnostic engine receives output.

## 7. Dialect registration idempotency

- `registerNSLDialect(registry)` called twice on the same registry
  is a no-op (the second call observes the dialect is already
  registered and returns silently). Verified by
  `test_unit/dialect_register_test/`.

## 8. Layered-deps invariant (FR-001 / FR-003 / FR-005)

- `nsl-dialect` link-graph dependencies: `nsl-basic`, MLIR `IR`,
  MLIR `Support`. Period.
- `nsl-dialect` MUST NOT link against `nsl-ast`, `nsl-sema`,
  `nsl-parse`, `nsl-lex`, or `nsl-preprocess`. The CI guard
  `scripts/check_layering.py` (extended at M4 per research §10)
  enforces this on every build.
- A new transitive dependency introduced by an MLIR upstream change
  (e.g., MLIR's `IR` library starts depending on a new MLIR
  sub-library) is acceptable; the rule applies to project-internal
  layers only.
