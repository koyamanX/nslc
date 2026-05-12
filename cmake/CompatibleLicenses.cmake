# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# cmake/CompatibleLicenses.cmake — single source of truth for the
# Apache-2.0-WITH-LLVM-exception compatible SPDX identifier set
# (M7 FR-011).
#
# This module is consumed by `cmake/AuditedCorpusLint.cmake` (which
# enforces FR-011 on vendored audited-corpus PROVENANCE.md files)
# and may be reused by future tooling (e.g., M9 release license
# audit).
#
# **Provenance**: this list reflects upstream SPDX guidance on
# Apache-2.0 compatibility (the LLVM-exception is a permissive
# additive to Apache-2.0 that does not introduce new compatibility
# constraints). Compatible-with-Apache-2.0 SPDX identifiers cited:
#   - BSD-2-Clause      — permissive, attribution-only
#   - BSD-3-Clause      — permissive, attribution + no-endorsement
#   - MIT               — permissive, attribution-only
#   - Apache-2.0        — same family
#   - Apache-2.0 WITH LLVM-exception — the project's own license
#
# **What's NOT in the list (intentionally)**:
#   - GPL-3.0 / GPL-2.0 / LGPL-* — copyleft; would force the
#     project's distribution license. Out of scope for vendored
#     audited corpora.
#   - MPL-2.0           — file-level copyleft; complicates vendoring
#                         of per-file copies.
#   - CC-* (Creative Commons) — not designed for software licensing.
#   - "no LICENSE file" — per GitHub ToS, "all rights reserved" by
#     default. Vendoring requires explicit grant from upstream
#     author (e.g., the "original-author grant" pattern documented
#     in test/audited/*/PROVENANCE.md "Notes" blocks at M7 P-VEN
#     time).
#
# **Adding a new identifier**: requires a Constitution Principle V
# Licensing review + corresponding spec amendment. NOT a routine
# CMake edit.

set(NSL_LICENSE_COMPATIBLE
  "BSD-2-Clause"
  "BSD-3-Clause"
  "MIT"
  "Apache-2.0"
  "Apache-2.0 WITH LLVM-exception"
  CACHE STRING "SPDX identifiers compatible with Apache-2.0 WITH LLVM-exception")
mark_as_advanced(NSL_LICENSE_COMPATIBLE)
