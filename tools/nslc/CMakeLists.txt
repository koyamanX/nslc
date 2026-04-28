# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# tools/nslc/CMakeLists.txt — the `nslc` driver binary.
#
# At M0 the executable is a thin smoke wrapper around the configure-
# time `git describe`-derived version string (see
# cmake/NSLVersion.cmake). Per Principle II all real behavior lives in
# `nsl-driver`; this target owns no compiled NSL logic.

add_executable(nslc main.cpp)

# Pulls the Version.h include path transitively from nsl-driver's
# PUBLIC target_include_directories (which includes
# ${CMAKE_BINARY_DIR}/include where NSLVersion.cmake writes the
# generated header).
target_link_libraries(nslc PRIVATE nsl-driver)

# Per-Sn constraint TUs in lib/Sema/Constraints/ self-register at
# static-init time via NSL_REGISTER_CONSTRAINT. The TUs aren't
# referenced by symbol from anywhere — the only thing pulling them
# in is the static initializer side-effect — so without
# `--whole-archive` the static linker silently drops every Sn TU
# when libnsl-sema.a is consumed, and `Sema::runConstraintPasses`
# fans out over an empty registry. Wrap libnsl-sema specifically.
target_link_options(nslc PRIVATE
  "LINKER:--whole-archive,$<TARGET_FILE:nsl-sema>,--no-whole-archive")

set_target_properties(nslc PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  CXX_EXTENSIONS OFF)

target_compile_features(nslc PRIVATE cxx_std_17)
