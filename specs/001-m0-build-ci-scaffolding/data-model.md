<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Data Model: M0 — Build & CI Scaffolding (with P-CI)

**Branch**: `001-m0-build-ci-scaffolding` | **Date**: 2026-04-26
**Plan**: [plan.md](./plan.md) | **Spec**: [spec.md](./spec.md)

This is a build-system + CI feature, so the "data model" is
configuration data, not runtime database schemas. Each entity below
expresses a structured artifact the implementation must produce or
honor. Field types are at the spec level (string, enum, list-of) —
exact CMake / YAML / JSON encoding lives in the contracts and the
implementation.

---

## Entity 1: `LibrarySkeleton`

Represents one of the nine compiler-track static library archives
(spec key entity "Library skeleton ×9").

**Fields**:

| Field | Type | Validation | Source |
|---|---|---|---|
| `name` | string, kebab-case | MUST be one of the 9 names from `docs/design/nsl_compiler_design.md` §3, in implementation order: `nsl-basic`, `nsl-preprocess`, `nsl-lex`, `nsl-ast`, `nsl-parse`, `nsl-sema`, `nsl-dialect`, `nsl-lower`, `nsl-driver`. | spec FR-001 |
| `layer_index` | integer 1..9 | MUST match the row index in the §3 table (drives dependency-direction validation in `add_nsl_library`). | Principle II |
| `header_dir` | path | MUST equal `include/nsl/<Layer>/` where `<Layer>` is the §3-table layer column (e.g., `include/nsl/Basic/`). | Principle II header convention |
| `headers` | list of paths under `header_dir` | At M0, empty (just `.keep`). For `nsl-basic`: `SourceLocation.h`, `Diagnostic.h` (per §3 multiplicity exception). For `nsl-ast`: per-node-kind headers (Principle II "AST exception"). | Principle II |
| `source_dir` | path | MUST equal `lib/<Layer>/`. | Principle II |
| `sources` | list of paths under `source_dir` | At M0, empty (skeleton library). | spec FR-001 |
| `depends_on` | list of `LibrarySkeleton.name` | Each entry MUST be at a strictly lower `layer_index` than the declaring library; the `add_nsl_library` macro MUST refuse otherwise (FR-004). | spec FR-004; Principle II |
| `participates_in_libnslfrontend` | boolean | True for layers 1–6 (per Principle II's `libNSLFrontend.a` aggregate); False for `nsl-dialect`/`nsl-lower`/`nsl-driver` (those depend on MLIR/CIRCT and aren't part of the front-end aggregate by Principle II's wording). | Principle II |

**M0 instances** (all 9, layer index ordered by Principle II downward-only direction):

| name | layer | header_dir | depends_on |
|---|---|---|---|
| `nsl-basic` | 1 | `include/nsl/Basic/` | (none) |
| `nsl-preprocess` | 2 | `include/nsl/Preprocess/` | `nsl-basic` |
| `nsl-lex` | 3 | `include/nsl/Lex/` | `nsl-basic` |
| `nsl-ast` | 4 | `include/nsl/AST/` | `nsl-basic` |
| `nsl-parse` | 5 | `include/nsl/Parse/` | `nsl-lex`, `nsl-ast` |
| `nsl-sema` | 6 | `include/nsl/Sema/` | `nsl-ast` |
| `nsl-dialect` | 7 | `include/nsl/Dialect/NSL/IR/` | (MLIR, CIRCT — external) |
| `nsl-lower` | 8 | `include/nsl/Lower/` | `nsl-sema`, `nsl-dialect` (+ CIRCT) |
| `nsl-driver` | 9 | `include/nsl/Driver/` | all of 1–8 |

> **Note (2026-04-27, post-implementation patch).** An earlier draft of
> this table listed `nsl-parse` at index 4 and `nsl-ast` at index 5 to
> match a literal reading of `nsl_compiler_design.md` §3's row order.
> That ordering would make `nsl-parse DEPENDS nsl-ast` an upward dep
> (4 → 5) and violate Principle II. The conflict was resolved in
> favour of Principle II in two atomic places: `cmake/AddNSLLibrary.cmake`
> (the layer-index lookup) and `docs/design/nsl_compiler_design.md`
> §3 (rows 4 and 5 swapped in this PR). spec.md FR-001 was patched
> in lockstep. AST is layer 4, Parse is layer 5; all three sources now
> agree.

---

## Entity 2: `TestLayer`

Represents one of the per-library test directories rooted at
`test/<Layer>/`, plus the future End-to-End directory.

**Fields**:

| Field | Type | Validation | Source |
|---|---|---|---|
| `name` | string | MUST mirror a `LibrarySkeleton.name` *or* be `EndToEnd`. | Principle VI |
| `directory` | path | MUST equal `test/<Layer>/`. | Principle VI; LLVM/CIRCT convention |
| `library_ref` | `LibrarySkeleton.name` or null (E2E) | Each unit-level test layer points at its library. | Principle VI |
| `driver` | enum `{lit-filecheck, gtest}` | Lowering and end-to-end MUST be `lit-filecheck` (Principle VI mandate). Lex/Parse/Sema/Dialect MAY use `gtest` for unit tests; their lit smoke fixture is still mandatory at M0. | Principle VI per-layer accepted-drivers list |
| `m0_smoke_fixture` | path | MUST exist at M0; convention `test/<Layer>/smoke.test` for layers without other fixtures yet. | spec FR-007 |
| `at_m0_status` | enum `{wired-with-smoke, wired-but-empty}` | All 9 layers + E2E start at M0 as `wired-with-smoke` *except* the End-to-End directory which is `wired-but-empty` until M7 (P-VEN + P-VCD lands). | spec FR-007, FR-015 |

---

## Entity 3: `CIStage`

Represents one of the six CI stages mandated by Principle IX +
spec FR-014.

**Fields**:

| Field | Type | Validation | Source |
|---|---|---|---|
| `ord` | integer 1..6 | MUST be the position in Principle IX's enumerated order. | Principle IX |
| `name` | string | MUST be the canonical stage name. | Principle IX |
| `command` | string | The shell command the stage runs (defined in `scripts/ci.sh` and mirrored in `.github/workflows/ci.yml`). | spec FR-014, FR-017, FR-021 |
| `gating` | enum `{blocks-merge, non-blocking}` | At M0: stages 1–4 are `blocks-merge`; stages 5–6 are `non-blocking` because their bodies are empty. Once M7 / M8 land, those stages flip to `blocks-merge` (deliberate amendment via that milestone's spec). | spec FR-015, FR-016 |
| `status_kind` | enum `{pass, fail, wired-but-empty}` | Stages 5 and 6 emit `wired-but-empty` (rendered as GitHub-Actions `skipped`) until their bodies land. | spec FR-015 |
| `matrix_dimensions` | list (only stage 1) | For stage 1 only: `Debug × Release × {GCC, Clang}` × Linux x86_64 = 4 builds. | spec Q2, FR-014 stage 1 |

**M0 instances** (all 6, in order):

| ord | name | command (essence) | gating | status_kind |
|---|---|---|---|---|
| 1 | Build matrix | `./scripts/ci.sh build-matrix` (4 cells) | blocks-merge | pass |
| 2 | Static checks | `./scripts/ci.sh static-checks` (clang-format + clang-tidy + check_spdx.py) | blocks-merge | pass |
| 3 | Unit & layer tests | `./scripts/ci.sh unit-tests` (gtest + per-layer lit smokes) | blocks-merge | pass |
| 4 | Lowering tests | `./scripts/ci.sh lowering-tests` (lit + FileCheck on `-emit=` outputs) | blocks-merge | pass (smoke only at M0; grows from M5) |
| 5 | End-to-end | `./scripts/ci.sh e2e` (audited-project compile + simulate vs. golden VCDs) | non-blocking at M0 | wired-but-empty (gates land M7) |
| 6 | Formal | `./scripts/ci.sh formal` (riscv-formal on `rv32x_dev`) | non-blocking at M0 | wired-but-empty (gates land M8) |

---

## Entity 4: `SPDXHeaderConvention`

Represents the per-extension recipe `scripts/check_spdx.py` uses to
locate and validate the SPDX header line.

**Fields**:

| Field | Type | Validation | Source |
|---|---|---|---|
| `extension` | string (lowercase, leading `.`) | MUST match a real file extension present in the repo. | spec FR-010 |
| `comment_opener` | string | The literal characters that begin a comment of this kind on the first line. | `CONTRIBUTING.md` §2 |
| `comment_closer` | string or null | Closer for paired-comment syntaxes (e.g., HTML `-->`, EBNF `*)`); null for line-comment syntaxes. | `CONTRIBUTING.md` §2 |
| `expected_identifier` | constant string | Always `Apache-2.0 WITH LLVM-exception` — the only acceptable SPDX value. | Constitution Build/Code/Licensing |
| `is_exception_path` | function (path → bool) | Driven by an explicit list (entity 4b below) — never heuristic. | spec FR-012 |

**M0 recipes** (drawn from `CONTRIBUTING.md` §2 + the project's
actual file inventory):

| extension | comment_opener | comment_closer | example header line |
|---|---|---|---|
| `.md` | `<!--` | `-->` | `<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->` |
| `.cpp`, `.cc`, `.cxx`, `.c`, `.h`, `.hpp`, `.hh` | `//` | (null) | `// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception` |
| `.cmake`, `CMakeLists.txt` | `#` | (null) | `# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception` |
| `.py`, `.sh`, `.bash` | `#` | (null) | `# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception` |
| `.ebnf` | `(*` | `*)` | `(* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception *)` |
| `.nsl` | `//` | (null) | `// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception` |
| `.yml`, `.yaml`, `.toml` | `#` | (null) | `# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception` |
| `.txt`, `.cfg`, `.lock` | `#` | (null) | `# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception` |
| `.test`, `.td`, `.mlir` | `//` | (null) | `// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception` |
| `.gitignore`, `.clang-format`, `.clang-tidy`, `.dockerignore`, `Dockerfile`, `Makefile` | `#` | (null) | `# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception` |
| `.json` | (no comment syntax — fail-loud) | — | (must be on the exception list, e.g., `.specify/feature.json`) |
| `.h.in` and other `.in`-suffixed templates | (resolves to underlying ext) | — | template files use the recipe of the suffix-stripped extension |

**Files for which no recipe is registered MUST fail loudly** (spec
FR-010); silent skip is forbidden.

### Entity 4b: `SPDXExceptionList`

A version-controlled list of paths exempt from the SPDX check.
**Format**: one entry per line. Entries ending with `/` are directory
prefixes; entries without are exact file paths. `#`-prefixed lines
and blank lines are ignored. Paths are repo-relative or absolute.

**M0 contents** (per `scripts/spdx_exceptions.txt`):

- `.specify/` — vendored Speckit tooling (templates, scripts,
  manifests, JSON metadata). JSON has no comment syntax (per Entity 4
  row "json"), and the speckit-managed YAML/MD files are authored
  upstream without a project SPDX line.
- `.claude/` — Claude Code project-level configuration
  (agents/, commands/, skills/, settings.json). Claude-managed.
- `.github/branch-protection.json` — single explicit JSON file
  with the SPDX identifier embedded as a `_comment_top` key
  inside the object.

**Auto-exempt by basename** (no entry required): `LICENSE`, `.keep`,
`.gitkeep`. **Auto-exempt by content**: zero-byte files.

> **Note (2026-04-27, post-implementation patch).** The original draft
> claimed the list was "empty at M0" because the audit at the time
> only enumerated authentic project files (README.md, CONTRIBUTING.md,
> CLAUDE.md, docs/, examples/, LICENSE — all of which either carry a
> header or are auto-exempt). The audit missed the vendored Speckit
> and Claude Code trees that ship without project SPDX, plus
> `.specify/feature.json` whose JSON syntax precludes a comment.
> Directory-prefix exception support (the trailing-`/` form) was
> added to the script so the three entries above cover ~70 files
> without expanding the list to per-file granularity.

**Validation**: every path in the exception list MUST exist; stale
entries fail the script (so the exception list cannot silently rot).

---

## Entity 5: `LocalReproductionEntryPoint`

Represents `scripts/ci.sh` (FR-017, FR-021).

**Fields**:

| Field | Type | Validation | Source |
|---|---|---|---|
| `path` | path | `scripts/ci.sh`. | spec FR-017 |
| `referenced_in` | list of paths | MUST include `README.md` (Building section) and `CONTRIBUTING.md` (Local CI sub-section). | spec FR-021 |
| `stages` | list of `CIStage.name` | The same 6 stages, in the same order, that `.github/workflows/ci.yml` runs. Divergence between this list and the YAML is a CI bug per FR-021. | spec FR-021 |
| `dispatch` | function (stage-name → command) | Returns the same command `CIStage.command` defines; reused by both `ci.sh` and the YAML. | research §11 |

---

## Entity relationships

```
LibrarySkeleton ──one-to-one── TestLayer
       │                            │
       │                            ├── m0_smoke_fixture
       │                            └── (lit-filecheck or gtest driver)
       │
       └── declared via add_nsl_library macro (cmake/AddNSLLibrary.cmake)

CIStage ──many-to-one── LocalReproductionEntryPoint
   │
   └── dispatches into shell snippets shared with .github/workflows/ci.yml

SPDXHeaderConvention ──many-to-one── scripts/check_spdx.py
       │
       └── pairs with SPDXExceptionList (currently empty)
```

No state machines; no lifecycle transitions at M0. (Stage 5 / 6's
`wired-but-empty → wired-with-tests` transition lands in M7 / M8
respectively, in a future spec.)
