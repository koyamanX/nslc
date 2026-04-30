<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: M4 Dialect-Verifier Diagnostic Format

**Branch**: `007-m4-mlir-dialect` | **Date**: 2026-04-30
**Plan**: [../plan.md](../plan.md)

This contract codifies the **substring-match policy** for M4
verifier diagnostics — a deliberate carve-out from Principle VIII's
diagnostic-string clause, applied to MLIR-layer structural rules.
Parallel to M3's `diagnostic-string.contract.md` but with the
opposite rule (M3 freezes literal text; M4 substring-matches).

## 1. Why M4 differs from M3

Principle VIII's `Sn`/`Nn`/`Pn` clause requires fail-case fixtures
to assert the **literal** diagnostic message string for every
NSL-spec semantic constraint, so that renaming or weakening a
diagnostic is automatically caught. M3 implements this for `S1`–
`S29` per `diagnostic-string.contract.md`.

The M4 dialect verifier produces diagnostics for **MLIR-layer
structural rules** — parent-op kind, region count, attribute
presence, etc. The wording of these diagnostics follows upstream
MLIR conventions (e.g., `HasParent` trait's
`expects parent op '...'` format). Re-stating MLIR's wording with
literal-string asserts in our fixtures would:

1. Couple the test corpus to upstream MLIR's exact wording, which
   evolves outside our control.
2. Create a maintenance treadmill on every LLVM/MLIR version bump.
3. Defeat the upstream convention (we're not testing NSL-spec
   semantics — we're testing IR shape, which MLIR's verifiers
   already encode canonically).

Per spec Assumptions and FR-012, M4 deliberately uses **substring
match** instead. This is **NOT** a constitutional carve-out (no
amendment is needed) because Principle VIII's diagnostic-string
clause specifically scopes to NSL `Sn`/`Nn`/`Pn`, which M4 verifier
diagnostics ARE NOT.

## 2. Standard diagnostic shape

Every dialect-verifier diagnostic SHALL match the regex:

```
^[^:]+:\d+:\d+: error: 'nsl\.[a-z_]+' op .+$
```

(per spec SC-005). This is MLIR's standard `op->emitOpError(...)`
shape, with:

- `<path>:<line>:<col>` — the offending op's `mlir::Location`,
  rendered as `FileLineColLoc`.
- `error:` — severity tag (always `error` for M4 verifier output;
  `warning` is reserved for future use).
- `'nsl.<op>'` — single-quoted op-name in the standard MLIR form.
- `op` — fixed token following the op-name (MLIR convention).
- `<message>` — the structural-invariant message text. Wording
  follows MLIR upstream patterns; specifics are NOT frozen.

## 3. Substring-match policy in fixtures

Each invalid fixture under `test/Dialect/<category>/<op>_invalid_<reason>.mlir`
uses lit's `// expected-error{{<substring>}}` syntax (MLIR's
standard mechanism). The substring SHALL include:

- The op-name (`nsl.<op>`) — locks down WHICH op the diagnostic is
  about.
- The invariant-shape — locks down WHICH invariant violation the
  diagnostic addresses (e.g., "expects parent op", "must be
  enclosed by", "missing 'sym_name'", "operand types mismatch").

Example:

```mlir
// RUN: nsl-opt --verify-diagnostics %s
nsl.module @M {
  // expected-error@+1 {{'nsl.seq' op expects parent op 'nsl.func'}}
  nsl.seq { ... }
}
```

The substring `'nsl.seq' op expects parent op 'nsl.func'` is
specific enough to:

- Catch a regression where the parent rule changes silently.
- Tolerate an upstream MLIR rewording from
  `expects parent op` to `requires parent op`.
- Tolerate adjacent text changes (the substring is a fragment of a
  longer message).

## 4. What substring-match does NOT cover

- **MLIR's exact wording**: if upstream changes "expects" to
  "requires", the substring `'nsl.seq' op` still matches but the
  wording-specific assertion does not. This is intentional — the
  fixture protects against NSL-side regressions, not upstream
  rewording.
- **Multi-line diagnostic forms**: lit's `expected-error` syntax
  matches a single line. M4 verifier diagnostics are always single-
  line per `op->emitOpError(...)` convention.
- **Note diagnostics** attached to a primary error: M4 verifier
  doesn't currently emit `note:` follow-ups; if it did, fixtures
  would use `// expected-note{{...}}` per MLIR convention.

## 5. Per-op invariant → fixture map

For every cell in spec FR-013 with ≥ 1 invariant, at least one
`<op>_invalid_<reason>.mlir` fixture exists under
`test/Dialect/<category>/`. The CI guard
`scripts/check_dialect_coverage.py` (per research §9) enforces
fixture existence using `.specify/m4_invariant_table.json` as the
ground-truth.

Examples (illustrative — full list lives in the JSON):

| Op | `<reason>` | Substring matched |
|---|---|---|
| `nsl.module` | `nested` | `'nsl.module' op expects parent op` |
| `nsl.seq` | `wrong_parent` | `'nsl.seq' op expects parent op 'nsl.func'` |
| `nsl.first_state` | `outside_proc` | `'nsl.first_state' op expects parent op 'nsl.proc'` |
| `nsl.transfer` | `width_mismatch` | `'nsl.transfer' op` (matches MLIR's `SameOperandsElementType` diagnostic) |
| `nsl.alt` | `empty` | `'nsl.alt' op` (matches the empty-alt verifier diagnostic) |
| `nsl.proc` | `two_first_states` | `'nsl.proc' op at most one 'nsl.first_state' child` |
| `nsl.while` | `not_inside_seq` | `'nsl.while' op must be enclosed by 'nsl.seq'` |
| `nsl.goto` | `bad_target` | `'nsl.goto' op` (sym-resolve failure) |

## 6. Stability surface

- The substring-match policy is stable across M5+ — when AST→MLIR
  lowering at M5 starts producing IR that the M4 verifier checks,
  the same diagnostics flow through; AST-built MLIR's locations
  resolve to NSL `SourceRange` (per design §12 line 1211 and
  Principle IV) but the diagnostic format is identical.
- Adding a new structural invariant at M5+ (rare; ideally an M4
  amendment) requires a new substring-asserting fixture.
- Adding a new lowering pass at M5+ does NOT introduce new dialect-
  verifier diagnostics; passes use their own diagnostic-emission
  paths.
