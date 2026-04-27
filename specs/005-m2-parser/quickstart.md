<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Quickstart: M2 — Parser + AST

**Branch**: `005-m2-parser` | **Date**: 2026-04-27
**Plan**: [plan.md](./plan.md)

This file is the **clone → build → exercise → verify** path for a
contributor coming fresh to M2. It assumes M0 + M1 have already
landed; prerequisites are identical to M1's quickstart (Linux
x86_64, the dev-container image, the LLVM/MLIR/CIRCT prestaged
toolchain).

## 1. Clone and check out the M2 branch

```bash
git clone https://github.com/koyamanX/nslc.git
cd nslc
git checkout 005-m2-parser
```

## 2. Configure and build

All build/test commands run inside the project's docker dev
container — **`ghcr.io/koyamanx/nsl-nslc:dev`** — which ships
`/opt/llvm` + `/opt/circt` pre-staged with `MLIR_DIR` / `CIRCT_DIR`
already exported. The host is not a supported build environment.
See `CONTRIBUTING.md` §3.11 for the full local-CI playbook.

```bash
docker run --rm -v "$PWD:/work" -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  sh -c '
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build
  '
```

> **Sections 3–8 below** assume each `./build/bin/nslc …` /
> `ctest …` / `lit …` / `./scripts/ci.sh …` invocation runs
> *inside the same dev container*. For sustained interactive
> work, launch a long-lived container and `exec` into it.

After M2 the build produces (additions over M1):

- `build/lib/AST/libnsl-ast.a` — every per-node-kind class plus
  the umbrella `ASTNode` / `ASTVisitor` / `Printer` / `NodeKind`.
- `build/lib/Parse/libnsl-parse.a` — `Parser` driver,
  `ParseDecl` / `ParseStmt` / `ParseExpr`, recovery primitive.
- `build/lib/Driver/libnsl-driver.a` — augmented with
  `EmitAST.cpp`.
- `build/bin/nslc` — the driver, now with `-emit=ast` support
  in addition to M1's `-emit=tokens`.

## 3. Smoke test: `nslc -emit=ast` on a hello-world module

```bash
cat > /tmp/hello.nsl <<'EOF'
module hello {
  reg q[8] = 0;
}
EOF

docker run --rm -v "$PWD:/work" -v /tmp:/tmp -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  ./build/bin/nslc -emit=ast /tmp/hello.nsl
```

Expected output (one of the goldens under
`test/Driver/emit-ast.test`):

```
(CompilationUnit  loc=/tmp/hello.nsl:1:1-3:2
  (ModuleBlock  loc=/tmp/hello.nsl:1:1-3:2  name=hello
    (RegDecl  loc=/tmp/hello.nsl:2:3-2:16  name=q
      (LiteralExpr  loc=/tmp/hello.nsl:2:9-2:10  kind=Decimal  value=8)
      (LiteralExpr  loc=/tmp/hello.nsl:2:14-2:15  kind=Decimal  value=0))))
```

(The exact `<path>` reflects what was passed on the command line.)

## 4. Determinism gate (Principle V)

Run twice and `diff` the outputs:

```bash
docker run --rm -v "$PWD:/work" -v /tmp:/tmp -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev sh -c '
    ./build/bin/nslc -emit=ast /tmp/hello.nsl > /tmp/run1.ast &&
    ./build/bin/nslc -emit=ast /tmp/hello.nsl > /tmp/run2.ast &&
    diff /tmp/run1.ast /tmp/run2.ast && echo "OK: byte-identical"
  '
```

A nonempty diff is a Principle V regression — the change that
introduced it must be reverted (`ast-stability.contract.md`
Invariant 2).

## 5. Multi-error recovery exercise

Provoke two independent errors in one input:

```bash
cat > /tmp/two-errors.nsl <<'EOF'
module first {
  reg a   // <-- missing ;
}

module second {
  wire b   // <-- also missing ;
}
EOF

docker run --rm -v "$PWD:/work" -v /tmp:/tmp -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  ./build/bin/nslc -emit=ast /tmp/two-errors.nsl
```

Expected (on stderr, in source order):

```
/tmp/two-errors.nsl:2:9: error: expected ';' after register declaration
/tmp/two-errors.nsl:6:10: error: expected ';' after wire declaration
```

Exit code: 1. **No AST on stdout** (per
[`nslc-emit-ast.contract.md`](./contracts/nslc-emit-ast.contract.md)
Behavior step 5).

## 6. Run the M2 test suite

```bash
docker run --rm -v "$PWD:/work" -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev sh -c '
    ctest --test-dir build --output-on-failure -R "parse|ast|driver-emit-ast"
  '
```

Expected: every per-grammar-production fixture under
`test/parse/grammar/` passes; every per-N-note fixture pair under
`test/parse/notes/n*/` passes; every multi-error fixture under
`test/parse/recovery/` passes; the format golden under
`test/Driver/emit-ast.test` passes; the gtest unit suites
(`ast_visitor_test`, `ast_printer_test`, `recovery_set_test`)
pass.

## 7. Run the full local CI pipeline

```bash
docker run --rm -v "$PWD:/work" -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  ./scripts/ci.sh all
```

Expected: all six stages green (`build-matrix`, `static-checks`,
`unit-tests`, `lowering-tests`, end-to-end remains
wired-but-empty pre-M7, formal remains wired-but-empty pre-M8).
The `static-checks` stage now exercises the layering guard
extension (research §11) — any cross-layer link edge violating
Principle II fails the stage.

## 8. (Optional) Inspect a parser-note disambiguation

The N1 disambiguation (`if` statement-vs-expression) is the most
visible parser-note. Try both forms:

Statement form (`/tmp/n1-stmt.nsl`):

```nsl
module m {
  reg flag = 0;
  reg out  = 0;
  if (flag) out = 1; else out = 0;   // statement form
}
```

Expression form (`/tmp/n1-expr.nsl`):

```nsl
module m {
  reg flag = 0;
  wire out = if (flag) 1 else 0;     // expression form
}
```

Run `nslc -emit=ast` on each and observe:
- The first produces an `IfStmt` node under the `ModuleBlock`'s
  `actions` vector.
- The second produces a `ConditionalExpr` node under a
  `TransferStmt::rhs` (the wire's RHS expression).

This is the user-visible signal that N1 is correctly implemented.

## 9. Where the code lives

| Surface | Where |
|---|---|
| Public AST headers | `include/nsl/AST/*.h` |
| AST printer | `lib/AST/Printer.cpp` |
| Public parser header | `include/nsl/Parse/Parser.h` |
| Top-level parse driver | `lib/Parse/Parser.cpp` |
| Per-decl parse rules | `lib/Parse/ParseDecl.cpp` |
| Per-stmt parse rules | `lib/Parse/ParseStmt.cpp` |
| Per-expr parse rules (Pratt) | `lib/Parse/ParseExpr.cpp` |
| Recovery primitive + sets | `lib/Parse/Recovery.cpp` |
| Driver wiring | `lib/Driver/EmitAST.cpp` (+ `tools/nslc/main.cpp` arg case) |
| Tests | `test/parse/` (lit), `test/Driver/emit-ast.test`, `test_unit/` (gtest) |

## 10. What this milestone unlocks

- **M3 (Sema)** — the next milestone — consumes the
  `CompilationUnit` AST that M2 produces. Every `S1`–`S29`
  semantic constraint test runs against an M2-built AST.
- **T-track LSP infrastructure** (T2 onward) gates on M3, but
  starts to exercise M2's recovery surface for
  `publishDiagnostics` UX once the LSP server is wired.
- **`nslc -emit=ast`** is now a first-class debug surface for
  contributors — running it on any audited NSL project in
  `test/audited/` (once P-VEN lands at M7) is a quick parse
  smoke check.
