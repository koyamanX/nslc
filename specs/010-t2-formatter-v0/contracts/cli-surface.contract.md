<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: `nsl-fmt` CLI surface (T2 freeze)

**Branch**: `010-t2-formatter-v0` | **Date**: 2026-05-04
**Plan**: [../plan.md](../plan.md) | **Spec**: [../spec.md](../spec.md)

This contract freezes the CLI surface of `tools/nsl-fmt/main.cpp`
at T2 acceptance. Any change to the flag list, exit-code
matrix, or multi-file behaviour after T2 lands is a
contract-breaking change and requires a `/speckit-clarify`
round.

---

## §1. Flag matrix

| Flag | Long form | Type | Default | Spec |
|---|---|---|---|---|
| `-i` | `--in-place` | bool | false | FR-002 |
| `-c` | `--check` | bool | false | FR-003 |
| (none) | `--stdin` | bool | false | FR-004 |
| (none) | `--config PATH` | string | (auto-discover) | FR-005 / FR-013 |
| (none) | `--range LINE:LINE` | string | (none) | FR-007 |
| (positional) | `<file>...` | list&lt;string&gt; | (none) | FR-001 |
| (none) | `--help` | bool | (LLVM-default) | (LLVM convention) |
| (none) | `--version` | bool | (LLVM-default) | (LLVM convention) |

**No additional flags exist at T2.** Adding a new flag is a
contract change.

---

## §2. Mutual exclusion (FR-006)

The following flag combinations MUST be rejected at argv-parse
time with a non-zero exit and a clear `error:` diagnostic on
stderr:

| Combination | Rejection message (frozen string) |
|---|---|
| `-i` + `-c` | `error: --check and --in-place are mutually exclusive` |
| `-i` + `--stdin` | `error: --in-place cannot be combined with --stdin` |
| `--stdin` + positional file argument | `error: --stdin cannot be combined with positional file arguments` |
| `--range` + multiple positional file arguments | `error: --range requires exactly one input file` |
| `--check` without input source (no `--stdin`, no positional) | `error: --check requires at least one input file or --stdin` |

These exact strings are frozen for the Principle VIII string-
stability rule (renaming any of them later requires updating the
matching lit fixture in `test/Fmt/cli/mutually-exclusive/`).

---

## §3. Exit-code matrix

| Scenario | Exit code | stdout | stderr | Spec |
|---|---|---|---|---|
| Default mode, all inputs format successfully | `0` | formatted output (one input after another, in input order) | empty | FR-001 |
| `-i` mode, all inputs rewrite successfully | `0` | empty | empty | FR-002 |
| `--check` mode, all inputs already canonical | `0` | empty | empty | FR-003 |
| `--check` mode, one or more inputs would change | `1` | unified diff per offending file | empty | FR-003 |
| `--check` mode, one or more inputs fail to parse | `1` | unified diff per offending check-mismatch (if any) | parse-error diagnostics for failing files | FR-003a / FR-012 |
| Default or `-i` mode, one or more inputs fail to parse | `1` | (default mode) successful files' output as usual | parse-error diagnostics for failing files | FR-003a / FR-012 |
| Mutually-exclusive flag combo (§2) | `2` | empty | one of the §2 frozen strings | FR-006 |
| Malformed config file (TOML syntax error or out-of-range value) | `2` | empty | TOML parse-error diagnostic | FR-016 |
| Unknown TOML key | (continues; exit code reflects subsequent file processing) | (formatted output if successful) | warning diagnostic naming the unknown key | FR-015 |
| Range out of bounds for `--range` | `2` | empty | `error: --range LINE:LINE falls outside file (file has N lines)` | FR-007 |
| `--help` / `--version` | `0` | help / version banner | empty | (LLVM convention) |

**Distinct exit codes**: `0` = success; `1` = formatting
mismatch / parse error in some inputs (continue-on-error,
FR-003a); `2` = CLI / config error (no formatting attempted).

---

## §4. Multi-file behaviour (FR-003a)

When invoked on multiple positional file arguments, `nsl-fmt`
MUST:

1. Process every file in input order, regardless of per-file
   errors.
2. For each file, emit the appropriate output (formatted text
   to stdout, diff to stdout in `--check`, in-place rewrite,
   diagnostic to stderr).
3. Track an aggregate "any failure?" flag.
4. After all files are processed, exit `0` if every file
   succeeded; exit `1` if any file failed (parse error, check
   mismatch, IO error).
5. NEVER abort the loop on a per-file error.

The aggregate flag MUST be reset only at process start; there is
no `--first-error` flag at T2 (would be a contract change).

---

## §5. Standard streams discipline

- **stdout**: formatted output (default mode) OR unified diffs
  (`--check`). NOTHING ELSE. In particular, no progress
  indicators, no INFO-level messages, no version banners (those
  go to stderr if any).
- **stderr**: diagnostics (errors and warnings) only. The
  diagnostic format matches `basic::DiagnosticEngine`'s human
  renderer (the same one `nslc` uses).
- **stdin**: read only when `--stdin` is set. Default mode and
  `-i` mode MUST NOT consume stdin.

This discipline ensures `nsl-fmt --check >/dev/null` is the
canonical CI usage (exit code carries the signal; stderr
carries the diagnostic for a human; stdout is silenceable).

---

## §6. Argv-parsing implementation

Per [research §8](../research.md#§8-cli-argv-parser--clopt-vs-custom),
`tools/nsl-fmt/main.cpp` uses LLVM's `cl::opt`:

```cpp
static cl::opt<bool>           InPlace ("i", cl::desc("Rewrite files in place"), cl::aliasopt(InPlaceLong));
static cl::alias               InPlaceLong("in-place", cl::desc("Alias for -i"), cl::aliasopt(InPlace));
static cl::opt<bool>           Check   ("c", cl::desc("Exit non-zero if any file would change"), cl::aliasopt(CheckLong));
static cl::alias               CheckLong  ("check", cl::desc("Alias for -c"), cl::aliasopt(Check));
static cl::opt<bool>           Stdin   ("stdin", cl::desc("Read source from stdin"));
static cl::opt<std::string>    Config  ("config", cl::desc("Path to .nsl-fmt.toml"));
static cl::opt<std::string>    Range   ("range", cl::desc("Format only LINE:LINE (1-indexed, inclusive)"));
static cl::list<std::string>   InputFiles(cl::Positional, cl::desc("<file>..."));
```

The mutual-exclusion check (§2) runs after
`cl::ParseCommandLineOptions()` returns and before any file
processing.

---

## §7. Help / version output

`--help` MUST list every flag in §1 with its description and
default value (LLVM `cl::PrintHelpMessage()` does this
automatically). `--version` MUST print:

```
nsl-fmt version <NSL_PROJECT_VERSION> (LLVM <LLVM_PROJECT_VERSION>)
```

where `NSL_PROJECT_VERSION` and `LLVM_PROJECT_VERSION` come from
the existing CMake variables already plumbed through `nslc` and
`nsl-opt`.

---

## Determinism note

Every CLI invocation with the same `(argv, stdin bytes, file
contents, .nsl-fmt.toml contents)` MUST produce byte-identical
stdout/stderr and the same exit code. Environment variables are
NOT input (Principle V). The exception is `--config <PATH>`,
where the file at PATH is part of the input.

---

## Spec cross-reference

| Spec FR | This contract section |
|---|---|
| FR-001 | §1, §3 row 1 |
| FR-002 | §1, §3 row 2 |
| FR-003 | §1, §3 rows 3-5 |
| FR-003a | §4 |
| FR-004 | §1, §5 stdin clause |
| FR-005 | §1 |
| FR-006 | §2 |
| FR-007 | §1, §3 range row |
| FR-013–FR-016 | §3 config-error rows |
| Principle V (determinism) | "Determinism note" |
