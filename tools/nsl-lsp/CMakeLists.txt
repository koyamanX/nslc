# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# tools/nsl-lsp/CMakeLists.txt — the `nsl-lsp` LSP server binary
# (T3). Per Constitution Principle II §3, `nsl-lsp` is a user-
# facing T-track tooling binary; release tarballs DO bundle it
# (contrast `nsl-opt`, which is gated behind NSL_INSTALL_DEV_TOOLS).
#
# **Specification anchors**:
#   - `specs/010-t3-lsp-skeleton/plan.md` §Project Structure
#   - `specs/010-t3-lsp-skeleton/contracts/lsp-protocol.contract.md`
#   - Constitution Principle II §3 — `nsl-lsp` is a user-facing
#     tooling binary that reuses libNSLFrontend.a.
#
# At T3 the binary is a thin entry point (≤ 70 lines) — all real
# logic lives in `lib/LSP/` (`libNSLLSP.a`).

add_executable(nsl-lsp main.cpp)

target_link_libraries(nsl-lsp PRIVATE NSLLSP)

# CRITICAL: per-Sn constraint TUs in lib/Sema/Constraints/ self-
# register at static-init time via NSL_REGISTER_CONSTRAINT. Without
# `--whole-archive` around libnsl-sema.a, the static linker silently
# drops every Sn TU and `Sema::runConstraintPasses` fans out over
# an empty registry. That would silently FAIL the FR-021 test gate
# (the LSP server would emit zero Sn diagnostics for any open
# document). Mirrors the identical pattern in tools/nslc/CMakeLists.
target_link_options(nsl-lsp PRIVATE
  "LINKER:--whole-archive,$<TARGET_FILE:nsl-sema>,--no-whole-archive")

set_target_properties(nsl-lsp PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  CXX_EXTENSIONS OFF)

target_compile_features(nsl-lsp PRIVATE cxx_std_17)

# Unconditional install — `nsl-lsp` is a T-track user-facing
# deliverable per Constitution Principle II §3, distinct from
# `nsl-opt` which is a developer tool gated behind
# NSL_INSTALL_DEV_TOOLS.
install(TARGETS nsl-lsp RUNTIME DESTINATION bin)
