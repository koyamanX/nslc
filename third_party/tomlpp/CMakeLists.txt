# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# third_party/tomlpp/CMakeLists.txt — wraps the vendored toml++ v3.4
# single-header library (toml.hpp) as a CMake INTERFACE target so
# downstream consumers (lib/Fmt) can write `target_link_libraries(...
# PRIVATE tomlpp)` and inherit the include path automatically.
#
# Provenance: third_party/tomlpp/PROVENANCE.md (upstream URL, commit
# SHA, sha256 sums, MIT license).
#
# Per Constitution Principle V vendoring discipline: this directory
# is the only mechanism by which `<toml.hpp>` enters the build —
# there is no configure-time download, no submodule, no
# FetchContent. To update, follow the recipe in PROVENANCE.md.

add_library(tomlpp INTERFACE)

target_include_directories(tomlpp
  SYSTEM INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}")

# C++17 minimum (matches the project baseline; toml++ v3.4 supports
# C++17/20/23 — we pin to 17 per Constitution "Build, Code, and
# Licensing Standards").
target_compile_features(tomlpp INTERFACE cxx_std_17)
