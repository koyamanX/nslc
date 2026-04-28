<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Quickstart: M3 — Sema (`nsl-sema`)

**Branch**: `006-m3-sema` | **Date**: 2026-04-28
**Plan**: [plan.md](./plan.md)

This file is the **clone → build → exercise → verify** path for a
contributor coming fresh to M3. It assumes M0 + M1 + M2 have already
landed; prerequisites are identical to M2's quickstart (Linux
x86_64, the dev-container image, the LLVM/MLIR/CIRCT prestaged
toolchain).

## 1. Clone and check out the M3 branch

```bash
git clone https://github.com/koyamanx/nslc.git
cd nslc
git checkout 006-m3-sema
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

After M3 the build produces (additions over M2):

- `build/lib/Sema/libnsl-sema.a` — `Sema` engine, `SymbolTable`,
  `TypeSystem`, the `ResolutionPass`, and one TU per per-`Sn`
  constraint check (`S01_NoDoubleUnderscore.cpp` …
  `S29_InitBlockPlacement.cpp`).
- `build/lib/Driver/libnsl-driver.a` — augmented with `Sema.cpp`
  + modifications to `EmitAST.cpp` to call Sema after parse.
- `build/bin/nslc` — the driver, now running Sema between parse
  and the `-emit=ast` printer (the printer emits the post-Sema
  enriched format per Q2 Option A).

## 3. Smoke test: post-Sema `nslc -emit=ast`

```bash
cat > /tmp/hello.nsl <<'EOF'
module hello {
  reg q[8] = 0;
  func clk { q := q + 1; }
}
EOF

./build/bin/nslc -emit=ast /tmp/hello.nsl
```

You should see (post-Sema enrichments highlighted with `<<<`):

```
CompilationUnit <hello.nsl:1:1-4:1>
  ModuleBlock <hello.nsl:1:1-4:1> hello
    RegDecl <hello.nsl:2:3-2:16> q : BitVector(8)             <<< Q2 type suffix
      width: LiteralExpr <hello.nsl:2:9-2:10> 8 : BitVector(64) <<< inferred BitVector(64) for integer constants
      init:  LiteralExpr <hello.nsl:2:14-2:15> 0 : BitVector(8) <<< inferred to match LHS width
    FuncDefn <hello.nsl:3:3-3:30> clk
      body: TransferStmt <hello.nsl:3:13-3:28>
        lhs: IdentifierExpr <hello.nsl:3:13-3:14> q : BitVector(8) → decl@hello.nsl:2:7  <<< Q2 decl-loc suffix
        op:  ":="
        rhs: BinaryExpr <hello.nsl:3:18-3:25> + : BitVector(8)
          IdentifierExpr <hello.nsl:3:18-3:19> q : BitVector(8) → decl@hello.nsl:2:7
          LiteralExpr <hello.nsl:3:22-3:23> 1 : BitVector(8)
```

The `: <Type>` and `→ decl@<file>:<line>:<col>` suffixes are the
Q2 Option A enrichments per `contracts/emit-ast-format.contract.md`.
A pre-Sema invocation (hypothetical future `--no-sema`) would emit
the M2 format (no suffixes).

Verify byte-stability:

```bash
./build/bin/nslc -emit=ast /tmp/hello.nsl > /tmp/run1.ast
./build/bin/nslc -emit=ast /tmp/hello.nsl > /tmp/run2.ast
diff /tmp/run1.ast /tmp/run2.ast && echo OK   # → OK (Principle V; FR-029)
```

## 4. Walk one error/warning per-`Sn` fixture (TDD red→green flow)

Each of the 23 error/warning `Sn` ships under `test/sema/s<NN>/`
with `pass.nsl` and `fail.nsl`. Walk through `S2` (wire-with-init):

```bash
ls test/sema/s02/
# pass.nsl  fail.nsl

cat test/sema/s02/pass.nsl
# (a wire WITHOUT an initializer — should pass Sema clean)

cat test/sema/s02/fail.nsl
# (a wire WITH an initializer — should fail with the frozen S2 message)
```

Run only the per-`S2` fixtures via lit:

```bash
./build/bin/llvm-lit -v test/sema/s02/
# pass.nsl: PASS
# fail.nsl: PASS  (FileCheck saw the expected `(S2)` diagnostic)
```

The `fail.nsl` asserts the literal diagnostic text via:

```nsl
// expected-error@+1 {{'wire' may not have an initializer; use 'reg' instead (S2)}}
wire bad = 0;
```

The literal-string assertion is what freezes the message per
Constitution Principle VIII's `Sn` clause; see
`contracts/diagnostic-string.contract.md` for the full table of
23 frozen messages.

## 5. Walk one constructive-`Sn` fixture (paired-introspection per Q1 Option B)

The 6 constructive `Sn` (`S13`, `S18`, `S19`, `S23`, `S24`, `S27`)
ship a `pass.nsl` plus a unit-test introspection assertion under
`test_unit/constructive_sn_test/`. Walk through `S18`
(struct MSB-first packing):

```bash
cat test/sema/s18/pass.nsl
# struct S { a[4]; b[12]; }
# module M { S inst; }
```

The unit test:

```cpp
// test_unit/constructive_sn_test/s18_test.cc
TEST(S18, struct_msb_first_packing) {
    auto result = run_sema_on("test/sema/s18/pass.nsl");
    auto* s = result.symbols->lookup("S")->as<StructTypeSymbol>();
    EXPECT_EQ(s->fields().size(), 2u);
    EXPECT_EQ(s->fields()[0].name, "a");
    EXPECT_EQ(s->fields()[0].width, 4u);
    EXPECT_EQ(s->fields()[0].offset, 12u);  // MSB → first declared at offset = totalWidth - width
    EXPECT_EQ(s->fields()[1].name, "b");
    EXPECT_EQ(s->fields()[1].width, 12u);
    EXPECT_EQ(s->fields()[1].offset, 0u);   // LSB
    EXPECT_EQ(s->totalWidth(), 16u);
}
```

The "fail" fixture is the same `pass.nsl` paired with the
introspection-expected-value flipped (e.g., asserting LSB-first
order); the test fails iff Sema diverges from the spec's S18 rule
of MSB-first packing. Run:

```bash
ctest -R s18_test
# 100% tests passed
```

This is the Q1 Option B shape. See
`contracts/sema-api.contract.md` Invariant 4 for the full
introspection API (six methods, one per constructive `Sn`).

## 6. Verify the multi-error recovery (Q3 Option C hybrid)

```bash
cat test/sema/recovery/multi_K3.nsl
```

Three independent violations: an `S2` in module `A`, an `S7` in
module `B`, and an `S14` in module `C`.

```bash
./build/bin/nslc -emit=ast test/sema/recovery/multi_K3.nsl 2>&1 | grep -c 'error:'
# 3
```

Exactly three diagnostics, in source order. None cascade (each
constraint check runs over the full AST as an independent walker
per FR-016). Run via lit:

```bash
./build/bin/llvm-lit -v test/sema/recovery/multi_K3.nsl
# multi_K3.nsl: PASS
```

The fixture's FileCheck `// expected-error:` directives assert the
exact three messages.

## 7. Verify the no-cascade guarantee (FR-017)

```bash
cat test/sema/recovery/unresolved_cascade.nsl
```

One typo (`fooo` instead of `foo`) used at five different sites.
Naive Sema would emit five "unresolved name 'fooo'" + dozens of
"width mismatch" cascading errors driven by `fooo`'s `Unresolved`
type. M3's hybrid strategy (Q3 Option C) emits exactly one:

```bash
./build/bin/nslc -emit=ast test/sema/recovery/unresolved_cascade.nsl 2>&1 | grep -c "unresolved name 'fooo'"
# 1
./build/bin/nslc -emit=ast test/sema/recovery/unresolved_cascade.nsl 2>&1 | grep -c 'error:'
# 1
```

This is the FR-017 contract enforced.

## 8. Run the full M3 test suite

```bash
ctest --test-dir build               # gtest unit tests + lit integration tests
./build/bin/llvm-lit -v test/sema/   # only the Sema lit fixtures
```

Expected:

- 58 baseline per-`Sn` lit fixtures (29 pass + 29 fail) plus
  ~14 multi-variant `fail_<variant>.nsl` per the diagnostic-string
  contract — total ~72 lit fixtures pass.
- ~6 multi-error recovery fixtures pass.
- ~10 per-scope resolution fixtures pass (`test/sema/resolution/`).
- ~12 per-`Expr`-form width fixtures pass (`test/sema/width/`).
- 1 re-cut `-emit=ast` golden corpus passes byte-exactly.
- ~15 gtest unit cases pass (`test_unit/symbol_table_test`,
  `test_unit/type_system_test`, `test_unit/resolution_pass_test`,
  `test_unit/constructive_sn_test`).

Approximate total: **~100–120 fixtures + ~15 unit cases**.

## 9. Local CI green-path verification

```bash
./scripts/ci.sh all
```

This runs the same six-stage pipeline CI runs on every PR
(Constitution Principle IX; M3 lights up stages 3 + 4 with new
content; stages 5 + 6 remain wired-but-empty until M7 / M8). All
six stages MUST exit zero before opening a PR.

## 10. Authoring a new fixture (TDD red→green per Principle VIII)

The Sema test corpus is the executable spec for `S1`–`S29`.
Adding a new `Sn` (a hypothetical `S30` in a future spec patch) is:

1. Add the EBNF row for `S30` in `docs/spec/nsl_lang.ebnf` lines
   826–1009 (Principle I monotonic numbering — never reuse a
   retired number).
2. Add the file:line entry to `docs/CLAUDE.md` §5 quick-map table
   (Principle VII spec/design coupling).
3. Add the row to the project-root `CLAUDE.md` §1 NSL-feature
   roll-up table (Principle VII).
4. Author `test/sema/s30/pass.nsl` and `test/sema/s30/fail.nsl`,
   the latter with `// expected-error:` (or `// expected-warning:`)
   citing the literal frozen message text. Commit AS-IS — they MUST
   fail because the implementation doesn't exist yet (Principle
   VIII red phase).
5. Add `lib/Sema/Constraints/S30_<short_name>.cpp` registering the
   per-`Sn` walker. Commit. Tests now pass — Principle VIII green
   phase.
6. (If applicable) Add a row to
   `contracts/diagnostic-string.contract.md` for the new frozen
   message string.

This is the workflow every M3 PR uses for the 29 existing `Sn` —
one test pair authored first, observed failing on the
implementation-free tree, then the implementation lands.
