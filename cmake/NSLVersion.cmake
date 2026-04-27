# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# NSLVersion.cmake — derive the `nslc --version` string at configure
# time from `git describe --tags --always --dirty` and forward it into
# include/nsl/Driver/Version.h.in via configure_file().
#
# Contract:  specs/001-m0-build-ci-scaffolding/contracts/nslc-version.contract.md
# Research:  specs/001-m0-build-ci-scaffolding/research.md §5
#
# Output forms (per FR-006 / spec Q5):
#   pre-tag, clean             nslc 0.0.0-dev+g<sha>
#   pre-tag, dirty             nslc 0.0.0-dev+g<sha>-dirty
#   tagged, clean              nslc <tag>
#   post-tag commits, clean    nslc <tag>-<n>-g<sha>
#   post-tag, dirty            nslc <tag>-<n>-g<sha>-dirty
#   tarball (no .git)          nslc unknown

include_guard(GLOBAL)

find_package(Git QUIET)

set(NSLC_GIT_DESCRIBE "unknown")

if(Git_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
  # `-c safe.directory=*` tolerates the dubious-ownership case that
  # arises when the source tree is mounted into a Docker container
  # (the volume's uid/gid mismatch the running user). The check
  # exists as a defence against attackers planting a hostile .git in
  # a writable parent dir; we are not in that situation here.
  execute_process(
    COMMAND ${GIT_EXECUTABLE} -c safe.directory=* describe --tags --always --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE _git_describe
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE _git_rc)
  if(_git_rc EQUAL 0 AND _git_describe)
    # `git describe --always` falls back to a bare hex SHA when no tag
    # exists. Re-format that to the FR-006 pre-tag canonical form.
    if(_git_describe MATCHES "^[0-9a-f]+(-dirty)?$")
      set(NSLC_GIT_DESCRIBE "0.0.0-dev+g${_git_describe}")
    else()
      set(NSLC_GIT_DESCRIBE "${_git_describe}")
    endif()
  endif()
endif()

message(STATUS "nslc version: nslc ${NSLC_GIT_DESCRIBE}")

configure_file(
  "${CMAKE_SOURCE_DIR}/include/nsl/Driver/Version.h.in"
  "${CMAKE_BINARY_DIR}/include/nsl/Driver/Version.h"
  @ONLY)
