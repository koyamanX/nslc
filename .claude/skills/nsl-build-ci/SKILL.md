---
name: "nsl-build-ci"
description: "Author CMake LLVM-style scaffolding (add_nsl_library), GitHub Actions matrix, and reproducibility checks — gates P-CI, M0, and the Principle IX merge gate."
argument-hint: "Build/CI task (e.g., 'add Release build matrix to CI' or 'wire add_nsl_library for nsl-sema')"
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

Owns the build system and CI pipeline. Two milestones depend directly on this skill:

| Milestone | Concern |
|---|---|
| **P-CI** | CI pipeline online with all six Principle IX stages |
| **M0** | Nine library CMake skeletons; lit + FileCheck wiring; SPDX-header check; smoke `nslc --version` binary |
| **M9** | CI-built reproducible release tarball + binaries (no human-built artifacts) |

Constitution **Principle IX** is the load-bearing principle here: until P-CI lands, the *transitional clause* requires every PR submitter to run the full local-equivalent of CI and link the run output (or hash) in the PR description. Your job is to make that transitional clause go away by landing P-CI early.

## Outline

1. **Identify the task type.**
   - **CMake scaffolding** — adding `add_nsl_library(<name>)` for one of the nine libraries, or for a tooling binary
   - **CI stage** — wiring one of Principle IX's six stages (build matrix → static checks → unit/layer → lowering → e2e → formal)
   - **Reproducibility** — verifying byte-stability across two builds (Principle V)
   - **SPDX-header check** — the M0 deliverable that lints every new file

2. **CMake conventions (LLVM-style).**
   - Read `docs/design/nsl_compiler_design.md` §13 (lines **1199–1256**) for CMake layout + dependencies
   - Use `add_nsl_library(<name> SOURCES … LINK_LIBS …)` per the layered architecture in §3 (lines **132–148**)
   - **Layer-direction enforcement (Principle II):** the LINK_LIBS list MUST flow downward — `nsl-parse` may link `nsl-lex`, never the other way around
   - C++17 only (Constitution "Build, Code, and Licensing Standards"). Do NOT enable C++20 features without an explicit constitutional amendment.

3. **The six CI stages (Principle IX).** These run in order on every PR and every push to `main`:
   1. **Build matrix** — `Debug` and `Release` builds on Linux x86_64. Adding more platforms is fine; **dropping any requires a constitutional amendment.**
   2. **Static checks** — `clang-format` + `clang-tidy` (against the project's `.clang-tidy` profile; create one in M0 if not present) + SPDX-header presence check on every new/modified file
   3. **Unit & layer tests** — lexer, parser, sema (every `Sn`/`Nn`), and `nsl-opt` round-trip
   4. **Lowering tests** — lit + FileCheck on `-emit=mlir` and `-emit=hw` outputs
   5. **End-to-end tests** — the seven audited projects compiled to Verilog and simulated against reference VCDs (Icarus and/or Verilator)
   6. **Formal** — riscv-formal on `rv32x_dev` once `/nsl-formal` lands the integration

4. **Reproducibility (Principle V + IX).**
   - CI MUST be re-runnable locally via a single documented entry point (e.g., `./scripts/ci.sh`)
   - A failure that only manifests in CI is a CI bug, not a feature
   - **Build caches MAY accelerate CI but MUST NOT mask non-determinism** — a cache miss MUST produce byte-identical output to a cache hit. Add a determinism check that compares two fresh builds.

5. **No-bypass discipline (Principle IX).**
   - `--no-verify`, `--no-gpg-sign`, and equivalent commit-time / push-time bypasses MUST NOT be used to dodge pre-commit, pre-push, or CI checks
   - If a hook fails, fix the underlying issue. Bypasses are permitted ONLY with explicit user authorization for a specific, named reason recorded in the PR description.

6. **SPDX-header check (M0 deliverable).** Every new file in the project MUST carry the appropriate SPDX identifier on its first line:
   - Markdown: `<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->`
   - EBNF: `(* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception *)`
   - C++ / TableGen / build files: LLVM-style block (see `CONTRIBUTING.md` §2)

   The CI check rejects any PR that adds files without one.

7. **Release-pipeline (M9).**
   - Tagged releases MUST publish reproducible binaries and source tarballs **from CI**
   - **No human-built artifacts** are attached to releases (Principle IX, "Release artifacts")
   - Verify byte-reproducibility of release artifacts across two CI builds before publishing

8. **Verify.** Confirm:
   - [ ] If new library: `add_nsl_library` skeleton present; LINK_LIBS respects Principle II layer direction
   - [ ] If CI stage: stage runs on every PR and push to `main`; failure blocks merge
   - [ ] If determinism work: byte-stability verified across cache-miss + cache-hit
   - [ ] If new file: SPDX-header check passes
   - [ ] No `--no-verify` / `--no-gpg-sign` bypass introduced
   - [ ] `./scripts/ci.sh` (or equivalent single entry point) reproduces CI locally

## Constitutional anchors

- **Principle II** — Layered Library Architecture (CMake LINK_LIBS direction)
- **Principle V** — Inspectable, Deterministic Pipeline (reproducibility)
- **Principle IX** — Continuous Integration & Delivery (six stages; no-bypass; release artifacts from CI)
- **Build, Code, and Licensing Standards** — C++17, SPDX, CMake LLVM-style
