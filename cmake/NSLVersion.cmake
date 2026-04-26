# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# NSLVersion.cmake
# ================
#
# **STUB.** Real `git describe`-driven version lands at task T027 (US1).
# This file exists so that Phase-2 configure resolves
# `include(NSLVersion)` before T027 lands.
#
# Once T027 lands, this file resolves NSLC_GIT_DESCRIBE via
#   execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty ...)
# and forwards the result to include/nsl/Driver/Version.h.in via
# configure_file().

include_guard(GLOBAL)

set(NSLC_GIT_DESCRIBE "unknown" CACHE INTERNAL
  "git-describe output baked into nslc --version. Replaced by T027.")
