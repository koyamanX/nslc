<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Quickstart: M1 — Lex + Preprocess

**Branch**: `002-m1-lex-preprocess` | **Date**: 2026-04-27
**Plan**: [plan.md](./plan.md)

This file is the **clone → build → exercise → verify** path for a
contributor coming fresh to M1. It assumes M0 has already landed;
prerequisites are identical to M0's quickstart (Linux x86_64, CMake
≥ 3.22, Ninja, GCC ≥ 9 or Clang ≥ 10, Python ≥ 3.8, vendored
prebuilt LLVM + MLIR + CIRCT).

## 1. Clone and check out the M1 branch

```bash
git clone https://github.com/koyamanX/nslc.git
cd nslc
git checkout 002-m1-lex-preprocess
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

> **Sections 3–9 below** assume each `./build/bin/nslc …` /
> `ctest …` / `lit …` / `./scripts/ci.sh …` invocation runs
> *inside the same dev container*. For sustained interactive work
> launch a long-lived container and `exec` into it, e.g.:
> `docker run --name nslc-dev -d -v "$PWD:/work" -w /work
> ghcr.io/koyamanx/nsl-nslc:dev sleep infinity` then
> `docker exec -it nslc-dev <cmd>` for each step.

After M1 the build produces:
- `build/lib/Basic/libnsl-basic.a` — `SourceManager`, `Diagnostic`,
  `SourceLocation`, `HelperSet.def` install.
- `build/lib/Lex/libnsl-lex.a` — `Lexer`, `Token`, `KeywordSet`,
  `NumberLiteral`.
- `build/lib/Preprocess/libnsl-preprocess.a` — `Preprocessor`,
  `DirectiveParser`, `MacroTable`, `HelperEvaluator`,
  `PPExpression`, `IdentSplicer`.
- `build/lib/Driver/libnsl-driver.a` — augmented with `EmitTokens.cpp`.
- `build/bin/nslc` — the driver, now with `-emit=tokens` support.

## 3. Smoke test: `nslc --version` (M0 carryover)

```bash
./build/bin/nslc --version
# expected: nslc <git-describe>
```

## 4. New: lex a simple NSL file

Create a fixture:

```bash
cat > /tmp/hello.nsl <<'EOF'
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#define WIDTH 8

module hello {
    declare {
        input clk;
        output q[WIDTH];
    }
    reg counter[WIDTH] = 0;
}
EOF
```

Run the lexer + preprocessor end-to-end:

```bash
./build/bin/nslc -emit=tokens /tmp/hello.nsl
```

Expected stdout (abridged — see
[`contracts/nslc-emit-tokens.contract.md`](./contracts/nslc-emit-tokens.contract.md)
for the full schema):

```
tk_module       module          /tmp/hello.nsl:4:1:71   /tmp/hello.nsl:4:1   []
tk_identifier   hello           /tmp/hello.nsl:4:8:78   /tmp/hello.nsl:4:8   []
tk_lbrace       {               /tmp/hello.nsl:4:14:84  /tmp/hello.nsl:4:14  []
tk_declare      declare         /tmp/hello.nsl:5:5:90   /tmp/hello.nsl:5:5   []
tk_lbrace       {               /tmp/hello.nsl:5:13:98  /tmp/hello.nsl:5:13  []
tk_input        input           /tmp/hello.nsl:6:9:108  /tmp/hello.nsl:6:9   []
tk_identifier   clk             /tmp/hello.nsl:6:15:114 /tmp/hello.nsl:6:15  []
tk_semicolon    ;               /tmp/hello.nsl:6:18:117 /tmp/hello.nsl:6:18  []
tk_output       output          /tmp/hello.nsl:7:9:127  /tmp/hello.nsl:7:9   []
tk_identifier   q               /tmp/hello.nsl:7:16:134 /tmp/hello.nsl:7:16  []
tk_lbracket     [               /tmp/hello.nsl:7:17:135 /tmp/hello.nsl:7:17  []
tk_decimal_lit  8               /tmp/hello.nsl:7:18:136 /tmp/hello.nsl:7:18  []
tk_rbracket     ]               /tmp/hello.nsl:7:25:143 /tmp/hello.nsl:7:25  []
tk_semicolon    ;               /tmp/hello.nsl:7:26:144 /tmp/hello.nsl:7:26  []
...
tk_eof                                                                  []
```

Notice the `8` token at `7:18` — that's the post-preprocess
expansion of `WIDTH`. The `#define WIDTH 8` directive itself was
consumed by the preprocessor and does NOT appear in the token
stream (P12 boundary).

## 5. New: lex a file with `#include` and verify the include search

Create two files:

```bash
mkdir -p /tmp/m1demo/include
cat > /tmp/m1demo/include/widths.nsl <<'EOF'
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#define DATA_WIDTH 32
#define ADDR_WIDTH 16
EOF

cat > /tmp/m1demo/top.nsl <<'EOF'
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "widths.nsl"

module top {
    declare {
        input  data[DATA_WIDTH];
        output addr[ADDR_WIDTH];
    }
}
EOF
```

Quote-form `#include` (resolved via `-I`):

```bash
./build/bin/nslc -emit=tokens -I /tmp/m1demo/include /tmp/m1demo/top.nsl
```

Expected: tokens for `top` with `data[32]` and `addr[16]`. The
output stream will also contain canonical `#line 1 "/tmp/m1demo/include/widths.nsl"`
markers bracketing the included content (P13 round-trip).

Angle-form `#include` would use the `NSL_INCLUDE` env var:

```bash
NSL_INCLUDE=/tmp/m1demo/include ./build/bin/nslc -emit=tokens \
  /tmp/m1demo/top_angle.nsl  # where top_angle.nsl has #include <widths.nsl>
```

## 6. New: provoke a diagnostic to verify source-locating output

Bad input:

```bash
cat > /tmp/bad.nsl <<'EOF'
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
module bad {
    declare {
        input  q = "unterminated
    }
}
EOF
```

```bash
./build/bin/nslc -emit=tokens /tmp/bad.nsl
echo "exit code: $?"
```

Expected:
- exit code `1`.
- stdout EMPTY (no tokens printed on error).
- stderr:
  ```
  /tmp/bad.nsl:4:20: error: unterminated string literal
  ```

JSON form of the same:

```bash
./build/bin/nslc --diagnostic-format=json -emit=tokens /tmp/bad.nsl
```

```
{"path":"/tmp/bad.nsl","line":4,"col":20,"severity":"error","message":"unterminated string literal"}
```

## 7. Run the M1 test suite

GoogleTest unit suite:

```bash
ctest --test-dir build --output-on-failure --label-regex unit
```

Should run (at minimum) the M1 unit suites:
- `source_manager_test`
- `diagnostic_engine_test`
- `macro_table_test`
- `helper_evaluator_test`

lit + FileCheck regression tree:

```bash
cd build && lit -v ../test
```

Should run (at minimum):
- `test/Driver/emit-tokens.test`
- `test/lex/keywords/*.test` (one per `lang.ebnf` §15 keyword)
- `test/lex/numbers/*.test`
- `test/lex/n5/*.test`, `test/lex/n11/*.test`,
  `test/lex/strings/*.test`, `test/lex/comments/*.test`
- `test/preprocess/p01/*.test` … `test/preprocess/p13/*.test`
- `test/preprocess/line/*.test` (the `#line` round-trip golden)
- `test/preprocess/include-stack/*.test` (multi-file diagnostic)

## 8. Run the local-CI reproduction

```bash
./scripts/ci.sh
```

This runs the same six stages GitHub Actions runs (M0 P-CI). At M1
the previously-empty stages 3 (Unit & layer tests) and 4 (Lowering
tests via lit + FileCheck) gain real content; stages 5/6 remain
wired-but-empty until M7/M8.

For a faster inner loop on a single stage:

```bash
./scripts/ci.sh static    # clang-format + clang-tidy + SPDX
./scripts/ci.sh unit      # the M1 GoogleTest suites
./scripts/ci.sh lit       # the M1 lit corpus
```

## 9. Verify determinism (Principle V)

```bash
./build/bin/nslc -emit=tokens /tmp/hello.nsl > /tmp/run1.tokens
./build/bin/nslc -emit=tokens /tmp/hello.nsl > /tmp/run2.tokens
diff /tmp/run1.tokens /tmp/run2.tokens && echo "deterministic"
```

Expected: empty diff, "deterministic" printed. SC-005 enforces this
across CI runs (cache hit vs cache miss).

## 10. What changed under the hood

The M1 milestone fills three previously-empty layer skeletons that
M0 stood up:

| Layer | M0 state | M1 state |
|-------|----------|----------|
| `nsl-basic` | empty `.cpp` smoke + `.keep` | `SourceLocation`, `SourceRange`, `FileID`, `SourceManager`, `Diagnostic`, `HelperSet.def` |
| `nsl-preprocess` | empty | full pp.ebnf preprocessor (directives + helpers + %IDENT% + #line) |
| `nsl-lex` | empty | full `lang.ebnf` §13–15 lexer (keywords + numbers + strings + comments + N5/N11) |
| `nsl-driver` | placeholder `--version` | + `EmitTokens.cpp` |

Layers 4–9 (`nsl-ast`, `nsl-parse`, `nsl-sema`, `nsl-dialect`,
`nsl-lower`, `nsl-driver` minus the EmitTokens addition) remain
empty skeletons; M2 fills the next two.

## Troubleshooting

- **Build fails with "unknown CMake variable `MLIR_DIR`"** — the
  vendored prebuilt LLVM + MLIR install path needs to be supplied;
  see M0 quickstart §1 for the canonical setup.
- **`nslc -emit=tokens` reports `unknown emit stage`** — the build
  did not pick up the M1 `EmitTokens.cpp`. Re-run `cmake --build`
  fully; an incremental rebuild after switching from `master` to
  `002-m1-lex-preprocess` may miss the new source file.
- **lit fixture fails with "expected token kind tk_xxx"** — the
  test golden was authored against a frozen `TokenKind` enum
  layout; if you've added or removed a kind locally, regenerate
  the relevant fixture's CHECK lines.
- **A `#include`'d file is not found** — verify your `-I` argument
  for quote-form, or `NSL_INCLUDE` env var for angle-form. The
  search order is documented in
  [`contracts/preprocessor-seam.contract.md`](./contracts/preprocessor-seam.contract.md).
