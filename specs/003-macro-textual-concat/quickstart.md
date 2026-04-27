<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Quickstart: Bare-Macro Textual Concatenation

**Branch**: `003-macro-textual-concat` | **Date**: 2026-04-27
**Plan**: [plan.md](./plan.md)

This file is the **clone → build → exercise → verify** path for a
contributor coming fresh to this feature. Prerequisites are
identical to M1: Linux x86_64 host, Docker installed,
`ghcr.io/koyamanx/nsl-nslc:dev` cached locally.

## 1. Clone and check out the branch

```bash
git clone https://github.com/koyamanX/nslc.git
cd nslc
git checkout 003-macro-textual-concat
```

## 2. Configure and build (inside the dev container)

```bash
docker run --rm -v "$PWD:/work" -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  sh -c '
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build
  '
```

> Sections 3–6 below assume each invocation runs **inside the same
> dev container** (use `docker exec -it nslc-dev <cmd>` if you have
> a long-lived container — see M1 quickstart).

## 3. Verify the canonical pp.ebnf P5 example works (US1)

```bash
cat > /tmp/p5.nsl <<'EOF'
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#define DEPTH 8
#define MEMDEPTH _int(_pow(2.0, DEPTH.0))
reg ram[%MEMDEPTH%];
EOF

./build/bin/nslc -emit=tokens /tmp/p5.nsl
```

Expected — the `%MEMDEPTH%` use site emits the integer literal
`256`:

```
tk_reg          reg     /tmp/p5.nsl:4:1:N    /tmp/p5.nsl:4:1   []
tk_identifier   ram     /tmp/p5.nsl:4:5:N    /tmp/p5.nsl:4:5   []
tk_lbracket     [       /tmp/p5.nsl:4:8:N    /tmp/p5.nsl:4:8   []
tk_decimal_lit  256     /tmp/p5.nsl:4:9:N    /tmp/p5.nsl:4:9   []
tk_rbracket     ]       /tmp/p5.nsl:4:N:N    /tmp/p5.nsl:4:N   []
tk_semicolon    ;       /tmp/p5.nsl:4:N:N    /tmp/p5.nsl:4:N   []
tk_eof                                                     []
```

The bare identifier `DEPTH` was textually substituted with `8`
inside the `_pow` argument list, producing `_pow(2.0, 8.0) = 256.0`,
then `_int(...)` truncated to `256`. Without this feature (M1
baseline), the same input produces an error like
`error: missing ')' in helper call '_pow'`.

## 4. Verify recursive expansion (US3)

```bash
cat > /tmp/recur.nsl <<'EOF'
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#define A B
#define B C
#define C 8
#define X _int(A.0)
reg buf[%X%];
EOF

./build/bin/nslc -emit=tokens /tmp/recur.nsl
```

Expected: the `%X%` use site emits `tk_decimal_lit 8` (the chain
`A → B → C → 8` resolves transitively, then `8.0` is the float
literal handed to `_int`).

## 5. Verify cycle detection (US3, FR-007 locked diagnostic)

```bash
cat > /tmp/cycle.nsl <<'EOF'
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#define A A
#define X _int(A)
reg buf[%X%];
EOF

./build/bin/nslc -emit=tokens /tmp/cycle.nsl
echo "exit code: $?"
```

Expected:

- exit code `1`
- stdout EMPTY (no tokens; partial output forbidden on error)
- stderr contains exactly the locked diagnostic:
  ```
  /tmp/cycle.nsl:3:N:N: error: recursive macro expansion: A
  ```

## 6. Run the new test suite

GoogleTest unit suite for `MacroExpander`:

```bash
ctest --test-dir build --output-on-failure -R macro_expander
```

Expected: ~5 test cases pass.

lit + FileCheck regression for the 3 new fixtures:

```bash
cd build && lit -v ../test/preprocess/p05/textual-concat.pass.test \
                  ../test/preprocess/p10/recursive-expansion.pass.test \
                  ../test/preprocess/p10/cycle.fail.test
```

Expected: 3/3 PASS.

## 7. Verify no M1 regressions (SC-005)

```bash
cd build && lit -v ../test
ctest --output-on-failure
```

Expected:

- lit: 113 (M1) + 3 (this feature) = **116/116 PASS**
- ctest: all green (the M1 + this feature's gtest suites)

## 8. Determinism check (FR-016, SC-003)

```bash
./build/bin/nslc -emit=tokens /tmp/p5.nsl > /tmp/r1.tokens
./build/bin/nslc -emit=tokens /tmp/p5.nsl > /tmp/r2.tokens
diff /tmp/r1.tokens /tmp/r2.tokens && echo "DETERMINISTIC"
```

Expected: empty diff, "DETERMINISTIC" printed.

## What changed under the hood

| Layer | M1 state | This feature's state |
|-------|----------|----------------------|
| `nsl-preprocess/PPExpression` | Bare identifiers resolved via sub-expression re-parse | Bare identifiers resolved via textual substitution + re-tokenization (delegated to new `MacroExpander`) |
| `nsl-preprocess/MacroExpander` | (didn't exist) | NEW — character-walking pre-pass, depth-bounded recursion |
| `nsl-preprocess/IdentSplicer` | Handled `%IDENT%` only | Handled `%IDENT%` AND bare-identifier (calls `MacroExpander`) |
| `docs/spec/nsl_pp.ebnf` P10 | Step 1 only mentions `%IDENT%` | Step 1 covers `%IDENT%` AND bare-identifier (amended; same line count) |

Every other layer (lexer, driver, diagnostics, source manager,
include-stack notes) is unchanged. **No M1 regressions.**

## Troubleshooting

- **`error: missing ')' in helper call '_pow'`** when running the
  P5 example — you're on the M1-baseline code (no
  `MacroExpander`). Verify branch: `git branch --show-current`
  should print `003-macro-textual-concat`. Re-run `cmake --build`.
- **Cycle test passes silently (no diagnostic)** — the cycle
  detection in `MacroExpander::expandImpl()` is misfiring. Check
  that `kMaxExpansionDepth` is `256` and that the depth counter
  is incremented at every recursive call.
- **A 256-level non-cyclic chain triggers cycle detection** —
  this is by design (per research §4): pathologically-deep
  non-cyclic chains and true cycles are treated identically. The
  user-facing fix is to shorten the chain.
