# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# tools/ — drivers and command-line front-ends shipped from this
# repository. `nslc` is the user-facing driver (M0+); `nsl-opt` is
# the M4 developer/test binary (Principle II §4 — NOT installed by
# default; gate via `-DNSL_INSTALL_DEV_TOOLS=ON`). Future T-track
# tools (`nsl-fmt` at T2, `nsl-lsp` at T3, `nsl-lint` at T6) drop in
# alongside as new sub-directories.

add_subdirectory(nslc)
add_subdirectory(nsl-opt)
add_subdirectory(nsl-lsp)
