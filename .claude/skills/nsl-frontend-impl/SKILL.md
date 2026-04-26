---
name: "nsl-frontend-impl"
description: "Implement compiler front-end layers (nsl-basic / nsl-preprocess / nsl-lex / nsl-parse / nsl-ast / nsl-sema) in C++17 LLVM-style — gates the M0–M3 critical path."
argument-hint: "Layer + feature description (e.g., 'lex: add %IDENT% token recognition')"
metadata:
  author: "nslc-project"
user-invocable: true
disable-model-invocation: false
---

## User Input

```text
$ARGUMENTS
```

You **MUST** consider the user input before proceeding (if not empty).

## Role

Implements the six front-end libraries that form `libNSLFrontend.a`:

| # | Library | Milestone | Public header path |
|---|---|---|---|
| 1 | `nsl-basic` | M1 | `include/nsl/Basic/` |
| 2 | `nsl-preprocess` | M1 | `include/nsl/Preprocess/` |
| 3 | `nsl-lex` | M1 | `include/nsl/Lex/` |
| 4 | `nsl-parse` | M2 | `include/nsl/Parse/` |
| 5 | `nsl-ast` | M2 | `include/nsl/AST/` |
| 6 | `nsl-sema` | M3 | `include/nsl/Sema/` |

Use this skill when implementing any of the above. **M3 is the unlock point** for the entire tooling track — prioritize landing it.

## Outline

1. **Identify the layer and milestone.** Check `README.md` §Roadmap M-track table to confirm the right library and the test gate that must turn green.

2. **Route to the spec.** Before writing C++, consult `docs/CLAUDE.md` §3 task → section map for the relevant spec sections:
   - Lexer → `docs/spec/nsl_lang.ebnf` lines **714–824**, `nsl_pp.ebnf` **345–390**
   - Preprocessor → `nsl_pp.ebnf` whole file (read in the order given in §3)
   - Parser → `nsl_lang.ebnf` **65–712** + N1–N14 disambiguation **1011–1149**
   - Sema → `nsl_lang.ebnf` **826–1009** (`S1`–`S29` is the entire Sema spec)
   - AST shape → `docs/design/nsl_compiler_design.md` **299–682**

3. **TDD entry (Principle VIII, NON-NEGOTIABLE).** Hand off to `/nsl-test-author` first — the test MUST be observed failing against the unchanged tree before any production code lands.
   - Lexer change → test on a token stream
   - Parser change → AST-snapshot test covering the affected production
   - Sema (`Sn`) → exactly one pass-case + one fail-case test, fail-case asserting on the specific diagnostic string

4. **Implement in LLVM/CIRCT style.** Per Constitution "Build, Code, and Licensing Standards":
   - **C++17 only.** No concepts / ranges / `std::format` / other C++20 features. Use `std::variant`, `std::optional`, RAII for ownership.
   - Single public header per library (or per-node-kind under `include/nsl/AST/` for `nsl-ast`); private headers stay in `lib/`
   - LLVM naming and brace style; SPDX header on every new file (`Apache-2.0 WITH LLVM-exception`)
   - Every AST node, symbol-table entry, and downstream IR carrier MUST hold a `SourceRange` (Principle IV)
   - `DiagnosticEngine` calls render to `file:line:col`; `#line` directive metadata MUST survive every later stage

5. **Respect layer boundaries (Principle II).** No upward dependencies (e.g., `nsl-lex` MUST NOT include `nsl-parse` headers). If you need to introduce a sibling dependency that bypasses the layer table in `docs/design/nsl_compiler_design.md` §3, that's a constitutional concern — file an issue, don't paper over it.

6. **Add the CMake skeleton.** Every library uses `add_nsl_library(<name> ...)` per `docs/design/nsl_compiler_design.md` §13. Hand off to `/nsl-build-ci` if the skeleton doesn't exist yet.

7. **Run the test gate.** Until P-CI lands, run the local-equivalent of CI per Principle IX transitional clause:
   - Build (Debug + Release) on Linux x86_64
   - clang-format + clang-tidy + SPDX-header check
   - The relevant per-layer tests from Principle VI

8. **Verify.** Confirm:
   - [ ] TDD: test was observed failing first (link the failing-state commit hash in PR description)
   - [ ] No upward-layer dependency introduced
   - [ ] SPDX header on every new file
   - [ ] All artifacts byte-stable across two builds (Principle V)
   - [ ] Source-locating diagnostics work end-to-end through the change
   - [ ] Test gate from `README.md` §Roadmap row turned green

## Constitutional anchors

- **Principle II** — Layered Library Architecture
- **Principle IV** — Source-Locating Diagnostics
- **Principle V** — Inspectable, Deterministic Pipeline
- **Principle VI** — Layered Test Discipline (NON-NEGOTIABLE)
- **Principle VIII** — Test-First Development (NON-NEGOTIABLE)
