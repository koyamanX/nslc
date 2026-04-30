<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Quickstart: M4 — `nsl` MLIR Dialect

**Branch**: `007-m4-mlir-dialect` | **Date**: 2026-04-30
**Plan**: [plan.md](./plan.md)

A 5-minute walkthrough of building, exercising, and extending the
M4 dialect inside the project's dev container. Mirrors M3's
quickstart pattern.

---

## 1. Prerequisites

- Docker with the `ghcr.io/koyamanx/nsl-nslc:dev` image cached
  locally (per [`README.md`](../../README.md) §Building).
- A clone of the repo at `master` head with the M4 patch applied
  (this branch: `007-m4-mlir-dialect`).
- Basic familiarity with MLIR's `mlir-opt` flow.

---

## 2. Build (Release)

```bash
docker run --rm -v "$PWD:/work" -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  sh -c '
    cmake -S . -B build-Release-gcc -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=g++ && \
    cmake --build build-Release-gcc --target nsl-opt nslc
  '
```

Expected output: `nsl-opt` and `nslc` binaries land in
`build-Release-gcc/bin/`. The TableGen invocation produces the
`.h.inc` / `.cpp.inc` artifacts under `build-Release-gcc/lib/Dialect/NSL/IR/`
(private; not in source tree).

---

## 3. Exercise a round-trip pass fixture

Create a minimal hand-written `.mlir` exercising `nsl.module` +
`nsl.reg`:

```bash
cat > /tmp/hello.mlir <<'EOF'
// RUN: nsl-opt %s | FileCheck %s
// CHECK: nsl.module @hello
// CHECK:   nsl.reg "q" : !nsl.bits<8> = 0
nsl.module @hello {
  nsl.reg "q" : !nsl.bits<8> = 0
}
EOF

docker run --rm -v "$PWD:/work" -v "/tmp:/tmp" -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  ./build-Release-gcc/bin/nsl-opt /tmp/hello.mlir
```

Expected output:

```mlir
nsl.module @hello {
  nsl.reg "q" : !nsl.bits<8> = 0
}
```

Two-pass round-trip:

```bash
docker run --rm -v "$PWD:/work" -v "/tmp:/tmp" -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  sh -c './build-Release-gcc/bin/nsl-opt /tmp/hello.mlir | ./build-Release-gcc/bin/nsl-opt -'
```

Output: byte-identical to the first pass (the dialect's
parser/printer is a fixed point per FR-017 + stability contract §5).

---

## 4. Exercise a verifier-reject fixture

Construct a structurally-malformed input — an `nsl.seq` placed
directly under `nsl.module` (illegal: `nsl.seq`'s parent must be
`nsl.func`):

```bash
cat > /tmp/bad-seq.mlir <<'EOF'
nsl.module @bad {
  nsl.seq {
  }
}
EOF

docker run --rm -v "$PWD:/work" -v "/tmp:/tmp" -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  ./build-Release-gcc/bin/nsl-opt /tmp/bad-seq.mlir; echo "exit=$?"
```

Expected output (on stderr):

```
/tmp/bad-seq.mlir:2:3: error: 'nsl.seq' op expects parent op 'nsl.func'
exit=1
```

The diagnostic locates to the offending op's `.mlir` text
position (line 2, column 3). The exit code is non-zero. The
substring-match policy means a fixture asserting
`'nsl.seq' op expects parent op 'nsl.func'` is robust to MLIR
upstream wording drift (per `verifier-diagnostic.contract.md` §3).

---

## 5. Exercise a transitive-parent verifier (Q2 Option B)

Construct an `nsl.while` placed inside `nsl.parallel` inside `nsl.seq`
(legal — the transitive ancestor is `nsl.seq`):

```bash
cat > /tmp/legal-while.mlir <<'EOF'
nsl.module @M {
  nsl.func @f {
    nsl.seq {
      nsl.parallel {
        nsl.while %c {
        }
      }
    }
  }
}
EOF
```

`nsl-opt` accepts this — the hand-written ancestor-walk per Q2
Option B finds `nsl.seq` 2 levels up. Now construct an illegal
variant — `nsl.while` directly in `nsl.alt` with no `nsl.seq`
anywhere up-stack:

```bash
cat > /tmp/bad-while.mlir <<'EOF'
nsl.module @M {
  nsl.func @f {
    nsl.alt {
      nsl.while %c {
      }
    }
  }
}
EOF

./build-Release-gcc/bin/nsl-opt /tmp/bad-while.mlir; echo "exit=$?"
```

Expected output:

```
/tmp/bad-while.mlir:4:5: error: 'nsl.while' op must be enclosed by 'nsl.seq'
exit=1
```

---

## 6. Run the full M4 lit test corpus

```bash
docker run --rm -v "$PWD:/work" -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  sh -c 'cd build-Release-gcc && lit -v ../test/Dialect'
```

Expected: ~88 lit tests pass (35 round-trip + 3 type round-trip +
~50 invalid fixtures).

The full project pipeline:

```bash
docker run --rm -v "$PWD:/work" -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  ./scripts/ci.sh all
```

passes the six Constitution Principle IX stages including M4's
contributions to stages 3 (unit + layer) and 4 (lowering tests).

---

## 7. Confirm `nslc` driver is unchanged at M4

```bash
docker run --rm -v "$PWD:/work" -v "/tmp:/tmp" -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  sh -c '
    echo "module hello { reg q[8] = 0; }" > /tmp/hello.nsl && \
    ./build-Release-gcc/bin/nslc -emit=ast /tmp/hello.nsl
  '
```

Output: identical to M3's `-emit=ast` output (post-Sema AST with
resolved types and decl-loc enrichments per M3 FR-020). Confirm
that `-emit=mlir` is rejected:

```bash
docker run --rm -v "$PWD:/work" -v "/tmp:/tmp" -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  ./build-Release-gcc/bin/nslc -emit=mlir /tmp/hello.nsl; echo "exit=$?"
```

Expected: `error: invalid -emit= choice 'mlir' (M5 deliverable)`,
`exit=1` (per FR-023).

---

## 8. Adding a new op (forward-looking)

When NSL gains a new construct (a hypothetical future `nsl.assert`,
say) the routine PR shape is:

1. **Spec amendment**: edit `docs/spec/nsl_lang.ebnf` to add the
   construct (Principle I monotonic numbering); document Sema
   constraints if any.
2. **Design amendment**: edit `docs/design/nsl_compiler_design.md`
   §7 op summary; add a row to FR-010 in `specs/007-m4-mlir-dialect/spec.md`
   (the M4 amendment).
3. **TableGen edit**: add `def NSL_AssertOp : NSL_Op<"assert", [...]> { ... }`
   to `lib/Dialect/NSL/IR/NSLOps.td`.
4. **Fixture**: add `test/Dialect/<category>/assert_roundtrip.mlir`
   + `assert_invalid_<reason>.mlir` per any new structural invariant.
5. **Verifier glue**: hand-written `LogicalResult AssertOp::verify();`
   in `NSLOps.cpp` if the op needs more than TableGen-trait checks.
6. **Tests authored before implementation** (Principle VIII TDD).
7. **CI guard data**: regenerate `.specify/m4_invariant_table.json`
   from the FR-013 amendment.
8. **`scripts/check_dialect_coverage.py` runs green**: per-op
   fixture existence verified; layered-deps invariant verified.

The pattern is the same per op category. The existing 41 ops at
M4 already follow this pattern — the per-op subdirectories under
`test/Dialect/` plus the FR-010/FR-013 tables are the documented
extension points.

---

## 9. Stop conditions

- **Don't add a new op without amending FR-010**: the CI guard
  `check_dialect_coverage.py` will fail because the
  `m4_invariant_table.json` regeneration won't include the new op.
- **Don't add a new pass at M4**: passes are M5+ deliverables;
  registering a pass at M4 violates the spec scope (FR-015 zero-
  passes).
- **Don't introduce a `nsl-dialect → nsl-ast` link edge**: per
  FR-005 + stability contract §8; `scripts/check_layering.py`
  fails fast.
- **Don't add `-emit=mlir` to `nslc` at M4**: per FR-023; CI fails
  on the help-text fixture (per US3 acceptance scenario 4).

The architectural seam is the deliverable; respecting it through
M5/M6/M7 is the project's payoff.
