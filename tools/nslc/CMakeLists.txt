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

set_target_properties(nslc PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  CXX_EXTENSIONS OFF)

target_compile_features(nslc PRIVATE cxx_std_17)
