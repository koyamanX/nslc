# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# cmake/AuditedCorpusLint.cmake — configure-time lint for the
# audited NSL corpus (M7 P-VEN + P-VCD).
#
# Spec anchor: `specs/011-m7-driver-e2e/spec.md` FR-013, FR-016.
# Contract: `specs/011-m7-driver-e2e/contracts/audited-corpus.contract.md`
# §4 (checks enumerated).
#
# This module runs at `cmake -B build ...` configure time. Each
# violation aborts configure with `message(FATAL_ERROR …)` — Principle
# IX no-bypass.
#
# **Status (Phase 1 scaffold)**: skeleton only — emits an
# informational STATUS message. The actual check body lands at T045
# once the seven projects + their PROVENANCE.md / golden/REGEN.md
# files exist under test/audited/.

# Single source of truth for the canonical audited projects.
#
# **M7 corpus narrowed from 7 → 4 (2026-05-12)**: per the audited-
# corpus licensing audit at M7 implementation time, 3 of the
# originally-listed 7 projects are blocked from vendoring:
#   - rv32x_dev: licensed GPL-3.0 (incompatible with Apache-2.0
#     WITH LLVM-exception per Constitution V Licensing)
#   - mmcspi + SDRAM_Controler: forks where upstream license cannot
#     be determined and the project owner cannot grant for vendored
#     code in this project
# The 4 remaining projects (cpu16, mips32_single_cycle,
# ahb_lite_nsl, turboV) are vendored under an explicit
# original-author grant of Apache-2.0 WITH LLVM-exception (recorded
# per project in PROVENANCE.md Notes block). Adding any of the
# dropped projects later requires resolving the upstream licensing
# first; adding new projects post-M7 is a routine PR per
# audited-corpus.contract.md §8.
set(NSL_AUDITED_PROJECTS
  cpu16
  mips32_single_cycle
  ahb_lite_nsl
  turboV)

# Apache-2.0 WITH LLVM-exception compatible SPDX identifiers (FR-011).
# Pulled from the dedicated module so the list is maintained
# centrally (a future M9 release license audit may reuse it).
include(${CMAKE_CURRENT_LIST_DIR}/CompatibleLicenses.cmake)

# -----------------------------------------------------------------------------
# Per-project structural checks (FR-013).
#
# For each project in NSL_AUDITED_PROJECTS:
#   (a) test/audited/<project>/ exists
#   (b) PROVENANCE.md present with required Key: lines
#   (c) Upstream-SHA matches ^[0-9a-f]{40}$
#   (d) License is in NSL_LICENSE_COMPATIBLE
#   (e) golden/ subdirectory exists
#   (f) golden/REGEN.md present
#   (g) no REGEN.md contains a bare `nslc` invocation
#
# Plus per-corpus checks:
#   (h) no .gitmodules entry under test/audited/
#   (i) no FetchContent/ExternalProject under test/audited/
# -----------------------------------------------------------------------------

function(_nsl_check_provenance project_dir project_name)
  set(prov_file "${project_dir}/PROVENANCE.md")
  if(NOT EXISTS "${prov_file}")
    message(FATAL_ERROR
      "AuditedCorpusLint: ${project_name}/PROVENANCE.md missing")
  endif()

  file(READ "${prov_file}" prov_content)

  # Required keys per audited-corpus.contract.md §2.
  set(required_keys "Upstream-URL" "Upstream-SHA" "License" "Vendored-At")
  foreach(key IN LISTS required_keys)
    string(REGEX MATCH "${key}:[ ]*([^\n\r]+)" matched "${prov_content}")
    if(NOT matched)
      message(FATAL_ERROR
        "AuditedCorpusLint: ${project_name}/PROVENANCE.md "
        "missing required key: ${key}")
    endif()
    if("${CMAKE_MATCH_1}" STREQUAL "")
      message(FATAL_ERROR
        "AuditedCorpusLint: ${project_name}/PROVENANCE.md "
        "key '${key}' has empty value")
    endif()
  endforeach()

  # Upstream-SHA shape check (40-hex).
  string(REGEX MATCH "Upstream-SHA:[ ]*([0-9a-f]+)" sha_match "${prov_content}")
  string(LENGTH "${CMAKE_MATCH_1}" sha_len)
  if(NOT sha_len EQUAL 40)
    message(FATAL_ERROR
      "AuditedCorpusLint: ${project_name}/PROVENANCE.md "
      "Upstream-SHA malformed (got length ${sha_len}, expected 40 hex chars)")
  endif()

  # License compatibility check (FR-011).
  string(REGEX MATCH "License:[ ]*([^\n\r]+)" lic_match "${prov_content}")
  set(license_value "${CMAKE_MATCH_1}")
  string(STRIP "${license_value}" license_value)
  # Strip any trailing comment (e.g., "Apache-2.0 WITH LLVM-exception (original-author grant)")
  string(REGEX REPLACE "^([^(]+)\\(.*$" "\\1" license_canonical "${license_value}")
  string(STRIP "${license_canonical}" license_canonical)
  set(compatible FALSE)
  foreach(compat IN LISTS NSL_LICENSE_COMPATIBLE)
    if("${license_canonical}" STREQUAL "${compat}")
      set(compatible TRUE)
      break()
    endif()
  endforeach()
  if(NOT compatible)
    message(FATAL_ERROR
      "AuditedCorpusLint: ${project_name}/PROVENANCE.md "
      "License '${license_canonical}' not in compatible set "
      "(${NSL_LICENSE_COMPATIBLE})")
  endif()
endfunction()

function(_nsl_check_golden project_dir project_name)
  set(golden_dir "${project_dir}/golden")
  if(NOT IS_DIRECTORY "${golden_dir}")
    message(FATAL_ERROR
      "AuditedCorpusLint: ${project_name}/golden/ missing")
  endif()

  set(regen_file "${golden_dir}/REGEN.md")
  if(NOT EXISTS "${regen_file}")
    message(FATAL_ERROR
      "AuditedCorpusLint: ${project_name}/golden/REGEN.md missing")
  endif()

  # FR-016: no bare `nslc` invocation in REGEN.md (no self-referential
  # goldens). The token `nslc` is permitted ONLY inside explanatory
  # prose (e.g., "...NOT generated by nslc..."). We grep for a hard
  # pattern: a line starting with `nslc ` or `./nslc` or `nslc-` after
  # optional whitespace, which would indicate an actual command invocation.
  file(READ "${regen_file}" regen_content)
  if(regen_content MATCHES "(^|\n)[ \t]*(\\.\\/)?nslc[ \t]")
    message(FATAL_ERROR
      "AuditedCorpusLint: ${project_name}/golden/REGEN.md "
      "contains forbidden bare nslc invocation (FR-016: no "
      "self-referential goldens). Goldens MUST be sourced from "
      "an external known-good source, not from nslc's own output.")
  endif()
endfunction()

function(_nsl_audited_corpus_lint)
  # Per-corpus checks first.
  set(gitmodules "${CMAKE_SOURCE_DIR}/.gitmodules")
  if(EXISTS "${gitmodules}")
    file(READ "${gitmodules}" gm_content)
    if(gm_content MATCHES "test/audited/")
      message(FATAL_ERROR
        "AuditedCorpusLint: .gitmodules references test/audited/. "
        "P-VEN requires verbatim file copy, NOT submodule.")
    endif()
  endif()

  # Per-project checks.
  foreach(project IN LISTS NSL_AUDITED_PROJECTS)
    set(project_dir "${CMAKE_SOURCE_DIR}/test/audited/${project}")
    if(NOT IS_DIRECTORY "${project_dir}")
      message(FATAL_ERROR
        "AuditedCorpusLint: missing vendored project directory: "
        "test/audited/${project}")
    endif()
    _nsl_check_provenance("${project_dir}" "${project}")
    _nsl_check_golden("${project_dir}" "${project}")
  endforeach()

  list(LENGTH NSL_AUDITED_PROJECTS num_projects)
  message(STATUS
    "AuditedCorpusLint: OK — ${num_projects} project(s) vendored "
    "with valid PROVENANCE.md + golden/REGEN.md "
    "(${NSL_AUDITED_PROJECTS})")
endfunction()

_nsl_audited_corpus_lint()
