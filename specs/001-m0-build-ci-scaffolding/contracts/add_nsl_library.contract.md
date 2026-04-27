<!-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception -->

# Contract: `add_nsl_library` CMake macro

**File**: `cmake/AddNSLLibrary.cmake`
**Plan**: [../plan.md](../plan.md) §research §3
**Spec FRs covered**: FR-001 (9 libs), FR-002 (sole declaration mechanism), FR-003 (per-layer header path), FR-004 (downward-only deps)

## Signature

```cmake
add_nsl_library(<name>
  <source files...>
  [HEADERS <header files...>]
  [DEPENDS <intra-project lib targets...>]
  [LINK_LIBS <external targets...>]
  [EXCLUDE_FROM_LIBNSLFRONTEND]
)
```

## Arguments

| Arg | Required | Validation |
|---|---|---|
| `<name>` (positional 1) | yes | MUST match one of the 9 names in `docs/design/nsl_compiler_design.md` §3. Configure-time fatal error otherwise. |
| `<source files...>` (positional rest) | yes (≥0) | Paths relative to the calling `CMakeLists.txt`. May be empty at M0 (skeleton libraries). |
| `HEADERS` | no | Paths relative to `${CMAKE_SOURCE_DIR}/include/nsl/<Layer>/`. Multiple headers allowed (Principle II `nsl-basic` and `nsl-ast` exceptions). |
| `DEPENDS` | no | Each entry MUST be a previously-declared `add_nsl_library` target with a strictly lower `layer_index`. Configure-time fatal error otherwise, citing the §3 row of the violating dep. |
| `LINK_LIBS` | no | External CMake targets (MLIR, CIRCT, GoogleTest, etc.). No NSL-internal targets allowed here — those go in `DEPENDS` for the dependency-direction guard to fire. |
| `EXCLUDE_FROM_LIBNSLFRONTEND` | no | Boolean flag. When set, the library is NOT aggregated into `libNSLFrontend.a`. Use only for forward-compatibility — at M0 nothing sets this. |

## Behavior

1. **Layer-name validation.** `<name>` is looked up in the macro's
   internal `NSL_LAYER_TABLE` (a CMake map mirroring §3). Unknown
   names produce `message(FATAL_ERROR "add_nsl_library: '${name}' is
   not in the §3 layer table; the 9 valid names are ...")`.
2. **Library creation.** `add_library(${name} STATIC ${sources})`.
3. **Standard-features pin.** `target_compile_features(${name}
   PUBLIC cxx_std_17)` and `set_target_properties(${name}
   PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)`.
4. **Header install.** Each header in `HEADERS` is installed into
   `${CMAKE_INSTALL_INCLUDEDIR}/nsl/<Layer>/<basename>` and added as
   a `PUBLIC` `target_sources` `FILE_SET HEADERS BASE_DIRS
   ${CMAKE_SOURCE_DIR}/include`.
5. **Intra-project deps validation.** For each entry in `DEPENDS`,
   look up its layer index. If the entry's index ≥ `<name>`'s index,
   emit `message(FATAL_ERROR "add_nsl_library: layer '${name}'
   (index N) cannot depend on '${dep}' (index M ≥ N) — see
   docs/design/nsl_compiler_design.md §3.")`.
6. **Dep linkage.** `target_link_libraries(${name} PUBLIC
   ${DEPENDS} ${LINK_LIBS})`.
7. **Aggregate registration.** Unless
   `EXCLUDE_FROM_LIBNSLFRONTEND` is set, append `${name}` to a
   global property `NSL_FRONTEND_LIBS` so a future
   `add_library(NSLFrontend INTERFACE)` target (M3-era) can link
   them all.
8. **clang-tidy / clang-format hook.** Add `${sources}` to the
   project-wide format/lint target lists so the static-checks CI
   stage exercises them.

## Test contract

Tests live in `test_unit/add_nsl_library_test/`. Per Principle VIII,
each test is written and observed failing before the macro
implementation lands.

| Test | Asserts |
|---|---|
| `valid_layer_name_succeeds` | `add_nsl_library(nsl-basic ...)` configures cleanly. |
| `unknown_layer_name_fails` | `add_nsl_library(nsl-bogus ...)` fails configure with the §3 citation in the error string. |
| `downward_dep_succeeds` | `add_nsl_library(nsl-lex ... DEPENDS nsl-basic)` configures cleanly. |
| `upward_dep_fails` | `add_nsl_library(nsl-basic ... DEPENDS nsl-lex)` fails configure with index-mismatch error. |
| `sibling_bypass_fails` | `add_nsl_library(nsl-lex ... DEPENDS nsl-parse)` fails (parse depends on lex, so lex→parse would be cyclic). |
| `multi_header_succeeds_for_basic` | `nsl-basic` declares both `SourceLocation.h` and `Diagnostic.h`. |
| `per_node_headers_succeed_for_ast` | `nsl-ast` declares e.g. `Decl.h`, `Stmt.h`, `Expr.h` in one call. |

## Failure modes & diagnostics

All failures emit `message(FATAL_ERROR ...)` at configure time
(never at build time) so that no contributor wastes a build cycle
on a structurally-invalid `add_nsl_library` invocation.

Diagnostic format: `add_nsl_library: <reason> — see
docs/design/nsl_compiler_design.md §3` (Principle IV "diagnostics
point back to the spec").
