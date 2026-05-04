# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# tools/nsl-fmt/CMakeLists.txt — the `nsl-fmt` driver binary (T2).
#
# `nsl-fmt` is the first of the three Constitution Principle-II-named
# user-facing tooling binaries (`nsl-fmt`, `nsl-lsp`, `nsl-lint`).
# Per Principle II's no-duplication rule, the binary is a thin shell
# over `libNslFmt.a` (defined under `lib/Fmt/`) and the underlying
# `libNSLFrontend.a` layers. Real CLI behaviour lands in Phases 3–6
# per `specs/010-t2-formatter-v0/contracts/cli-surface.contract.md`.
#
# At Phase 1 the binary is a five-line stub (returns 0); the test
# fixture under T001 in `quickstart.md` §3 asserts only that
# `--version` prints a banner — not yet wired to actual logic.

add_executable(nsl-fmt main.cpp)

# Library target name is `NslFmt` (mixed-case) so the resulting
# archive is `libNslFmt.a` per spec FR-017; see lib/Fmt/CMakeLists.txt
# for the rationale (avoids collision with this `nsl-fmt` executable
# target).
target_link_libraries(nsl-fmt
  PRIVATE
    NslFmt)

# Place alongside `nslc` and `nsl-opt` in `bin/` so contributors
# invoking from `build/bin/` find every project binary side-by-side.
set_target_properties(nsl-fmt PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  CXX_EXTENSIONS OFF)

target_compile_features(nsl-fmt PRIVATE cxx_std_17)
