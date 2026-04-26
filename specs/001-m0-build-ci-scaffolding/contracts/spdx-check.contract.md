<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: `scripts/check_spdx.py` SPDX-header presence checker

**File**: `scripts/check_spdx.py` (Python 3.8+, per research §6)
**Plan**: [../plan.md](../plan.md) §research §6
**Spec FRs covered**: FR-010 (recipe per extension; full-repo scan), FR-011 (CLI + CI step), FR-012 (exception list); spec Q4 (full-repo scope)

## Invocation

```
python3 scripts/check_spdx.py [--exceptions <path>] <file>...
python3 scripts/check_spdx.py [--exceptions <path>] --all      # equivalent to passing `git ls-files`
```

| Flag | Meaning | Default |
|---|---|---|
| `--exceptions <path>` | Path to the version-controlled exception list (one path per line, `#` for comments). | `scripts/spdx_exceptions.txt` |
| `--all` | Convenience: run on the output of `git ls-files`. | (off; explicit file list required otherwise) |
| `<file>...` | Files to check. Relative paths resolved against repo root. | (required unless `--all`) |

CI uses: `python3 scripts/check_spdx.py --all` (spec Q4: full-repo
scan on every PR + push).

## Per-file algorithm

For each input file:

1. If the path matches the exception list → record as `EXEMPT`,
   skip to next.
2. Determine the file's extension (or basename for files like
   `CMakeLists.txt`).
3. Look up the recipe in the per-extension table (data-model.md
   §entity 4). If no recipe → record as `FAIL: no recipe registered
   for extension '<ext>' — add to scripts/check_spdx.py recipe table
   or to scripts/spdx_exceptions.txt`.
4. Read the file's first non-shebang, non-empty line. (`#!`-led
   shebang lines on `.sh` and `.py` are skipped before applying the
   recipe so `#!/usr/bin/env bash` doesn't trigger a false positive.)
5. Construct the expected line per the recipe:
   - Line-comment style: `<opener> SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception`
   - Paired-comment style: `<opener> SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception <closer>`
6. If the read line equals the expected line → `PASS`. Otherwise
   → `FAIL: expected '<expected>'; observed '<observed-truncated>'`.

## Output schema

Human-readable, one line per failure:

```
<path>:1: SPDX header missing or malformed
  expected: <expected-line>
  observed: <observed-line>
```

Followed by a one-line summary at the end:

```
spdx-check: <pass> passed, <fail> failed, <exempt> exempt (out of <total> files)
```

When `fail == 0`, the script prints only the summary. When
`fail > 0`, the script prints the per-failure block(s) then the
summary.

## Exit codes

| Exit | Meaning |
|---|---|
| `0` | Every input file passed (or was exempt). |
| `1` | One or more files failed. |
| `2` | Script-internal error (bad arguments, unreadable exception list, etc.). |

Exit-code stability is part of the contract — CI parses these.

## Stale-exception detection

After processing all input files, the script compares the
exception list against the actual filesystem:

- Each path in the exception list MUST exist on disk. Stale
  entries (paths that no longer exist) → `FAIL: stale exception
  list entry: '<path>'` and exit 1. (Prevents the exception list
  from silently rotting.)

## Test contract

Tests live in `test_unit/spdx_check_test/` as pytest fixtures
(driven from CMake via `add_test(NAME spdx-check-tests COMMAND
${Python3_EXECUTABLE} -m pytest scripts/check_spdx_test.py)`).
Per Principle VIII, all tests written and observed failing before
`scripts/check_spdx.py` lands.

| Test | Asserts |
|---|---|
| `valid_md_passes` | `<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->` on a `.md` file → PASS, exit 0. |
| `valid_cpp_passes` | `// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception` on a `.cpp` file → PASS. |
| `valid_py_with_shebang_passes` | `#!/usr/bin/env python3` line 1, SPDX line 2 → PASS. |
| `missing_header_fails` | `.cpp` file with no SPDX line → FAIL exit 1, diagnostic names file:1. |
| `wrong_identifier_fails` | `// SPDX-License-Identifier: Apache-2.0` (missing LLVM-exception) → FAIL, diagnostic names both expected and observed. |
| `unknown_extension_fails_loudly` | `foo.xyz` with no recipe → FAIL exit 1 with "no recipe" diagnostic. |
| `exempt_path_skipped` | A file listed in `spdx_exceptions.txt` → recorded EXEMPT, exit 0 even if header is missing. |
| `stale_exception_fails` | Exception list names a path that doesn't exist on disk → FAIL exit 1 with "stale exception list entry" message. |
| `mixed_results_summary_correct` | 3 pass + 2 fail + 1 exempt → summary line correct. |
| `git_ls_files_mode_works` | `--all` flag invokes `git ls-files` and produces the same result as passing the equivalent file list. |

## Performance contract

| Metric | Target |
|---|---|
| Wall-clock on the M0 repo (~50 tracked files) | < 1 s on the reference host |
| Wall-clock on a 10× repo (~500 files) | < 2 s (linear scaling acceptable) |

(No SC-level success criterion; this is a hygiene-tier perf goal.)
