<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->
# Quickstart: clang-tidy Cleanup

**Audience**: a contributor working on this feature OR validating it
post-merge.

All commands assume the canonical build container per memory note
`project_build_environment.md`:

```bash
ghcr.io/koyamanx/nsl-nslc:dev
```

## 1. Reproduce the baseline (today, on master)

```bash
cd /path/to/nslc
sg docker -c 'docker run --rm -v $(pwd):/work -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev bash -c "
    /work/scripts/ci.sh static-checks
"' 2>&1 | tail -5
```

Expected on master HEAD `73e49ae` (pre-cleanup):

```text
927 warnings treated as errors
[ci.sh]   python3 scripts/check_spdx.py --all
spdx-check: 295 passed, 0 failed, 128 exempt (out of 423 files)
```

## 2. Check the per-category breakdown

```bash
sg docker -c 'docker run --rm -v $(pwd):/work -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev bash -c "
    /work/scripts/ci.sh static-checks 2>&1 \
      | grep -oE \"\\[(misc|readability|modernize|cppcoreguidelines|llvm)-[a-z-]+,-warnings-as-errors\\]\" \
      | sort | uniq -c | sort -rn | head -20
"'
```

Top categories on master (per `research.md` §1):

| Count | Category | Disposition |
|---|---|---|
| 434 | `misc-const-correctness` | FIX |
| 114 | `readability-identifier-naming` | FIX |
| 59  | `readability-uppercase-literal-suffix` | FIX |
| 47  | `modernize-use-nodiscard` | FIX |
| 35  | `misc-include-cleaner` | FIX |
| 27  | `readability-implicit-bool-conversion` | FIX |
| 24  | `modernize-return-braced-init-list` | FIX |
| 22  | `misc-non-private-member-variables-in-classes` | SUPPRESS |
| 17  | `readability-convert-member-functions-to-static` | MIXED (per-site) |
| 16  | `cppcoreguidelines-avoid-const-or-ref-data-members` | SUPPRESS |
| 14  | `misc-no-recursion` | SUPPRESS |
| 13  | `readability-function-cognitive-complexity` | SUPPRESS |
| 12  | `llvm-include-order` | FIX |

## 3. Run a single-category cleanup (developer workflow)

To work on a single category — say, `readability-uppercase-literal-suffix`:

```bash
sg docker -c 'docker run --rm -it \
  -v $(pwd):/work -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev bash -c "
    cd /work
    cmake -B build-Release-host -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build-Release-host

    # Apply --fix for ONLY this category
    run-clang-tidy -p build-Release-host \
      -checks='-*,readability-uppercase-literal-suffix' \
      -fix \
      -header-filter='^(include|lib|tools|test_unit)/.*' \
      include/ lib/ tools/ test_unit/

    # Re-format the touched files (clang-tidy doesn't reformat)
    git ls-files -m | xargs clang-format -i

    # Verify the gate is happier
    /work/scripts/ci.sh static-checks 2>&1 | tail -3
"'
```

Expected: warnings count drops by ~59 (the count for that category),
and the lit + ctest suites continue to pass (run them after to
confirm).

## 4. Verify lit + ctest after a category fix

```bash
sg docker -c 'docker run --rm -v $(pwd):/work -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev bash -c "
    cmake --build build-Release-host
    cmake --build build-Release-host --target check-nslc 2>&1 | tail -5
    ctest --test-dir build-Release-host 2>&1 | grep \"100%\"
"'
```

Expected at every intermediate commit: `118/118 lit pass`, `129/129
ctest pass`.

## 5. Verify the post-cleanup gate

After all category commits land, the gate should be green:

```bash
sg docker -c 'docker run --rm -v $(pwd):/work -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev bash -c "
    /work/scripts/ci.sh all
"' 2>&1 | tail -10
```

Expected on the post-cleanup HEAD:

- All 6 stages exit 0 (`build-matrix`, `static-checks`, `unit-tests`,
  `lowering-tests`, `e2e`, `formal`).
- `static-checks` reports zero warnings-treated-as-errors.

## 6. Verify the constitution close-out

```bash
grep -ic "transitional" .specify/memory/constitution.md
```

Expected: a count > 0 ONLY if the references that survive are NOT
inside the Principle IX section. The Principle IX section itself
contains zero "transitional" mentions.

For the precise check:

```bash
awk '/^### Principle IX/{p=1} p && /^### /{if (NR>1 && !/Principle IX/) p=0} p' \
  .specify/memory/constitution.md \
  | grep -c "transitional"
```

Expected: `0` (per SC-004).

## 7. Verify the regression-prevention mechanism (US3)

Open a sandbox PR with a deliberate single new warning — for instance,
add a `int foo;` (unconst, uninitialized) inside a touched cpp file
in the test branch. Push and observe CI:

```text
CI: static-checks FAILED
  warning: variable 'foo' is uninitialized [cppcoreguidelines-init-variables]
```

Discard the test PR. The mechanism works without manual list upkeep
(per `research.md` §2).

## Troubleshooting

- **"clang-tidy --fix broke my build"** — expected when fixing
  multiple categories at once. Run `--fix` for ONE category at a time
  per memory note `feedback_clang_tidy_batch_unsafe.md`.
- **"clang-format keeps reformatting my fixes"** — run the format
  sweep FIRST, then per-category fixes. Per `research.md` §3 step 1.
- **"a public header gained warnings I can't fix without API change"**
  — file the API-change as a separate feature; this cleanup is
  style/correctness only (FR-010).
- **"I see warnings on master that don't reproduce on my host"** —
  use the canonical container. Host clang-tidy versions diverge.
