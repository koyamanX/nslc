# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
FROM nsl/circt:dev

ARG DEBIAN_FRONTEND=noninteractive

# `lit` is a Python module the LLVM build's `LLVM_INSTALL_UTILS=ON`
# does NOT install (it ships only the C++ helpers FileCheck, count,
# not, split-file). Pin via PyPI to a stable LLVM-18 release so docker
# image builds are deterministic — if a newer lit changes test-runner
# semantics we want to bump it explicitly with a deps.lock change.
ARG LIT_VERSION=18.1.8

# Toolchain for building, testing, and linting nslc.
# clang / cmake / ninja / lld / ccache / python3 come from nsl/base:dev;
# lit / FileCheck / not / count / split-file / yaml2obj / obj2yaml are
# already staged in /opt/llvm/bin by nsl/llvm-mlir:dev.
#
# This layer adds:
#   - clang-format / clang-tidy (FR-008, FR-009 static-checks stage)
#   - g++              — the matrix's gcc cells (spec Q2)
#   - libgtest-dev / libgmock-dev — GoogleTest unit-test framework
#                       (research §1; consumed by find_package(GTest)
#                        in test_unit/CMakeLists.txt)
#   - python3-lit      — lit test runner (LLVM_INSTALL_UTILS=ON in
#                        nsl/llvm-mlir installs the C++ helpers
#                        FileCheck/count/not/split-file but not lit
#                        itself, since lit is a Python module)
#   - python3-pytest   — drives the test_unit/* CTest fixtures (US2/US3)
#   - python3-yaml     — required by test_unit/ci_yml_test
#   - gdb / less       — interactive debug helpers
RUN apt-get update && apt-get install -y --no-install-recommends \
      clang-format clang-tidy \
      g++ \
      libgtest-dev libgmock-dev \
      python3-pytest python3-yaml \
      gdb less \
 && python3 -m pip install --break-system-packages --no-cache-dir \
      "lit==${LIT_VERSION}" \
 && rm -rf /var/lib/apt/lists/*

# Mount point for the nslc source tree.
WORKDIR /workspace

# Make `cmake -B build -G Ninja` Just Work against the prebuilt stack.
ENV LLVM_DIR=/opt/llvm/lib/cmake/llvm \
    MLIR_DIR=/opt/llvm/lib/cmake/mlir \
    CIRCT_DIR=/opt/circt/lib/cmake/circt \
    CC=clang \
    CXX=clang++ \
    CMAKE_GENERATOR=Ninja \
    CMAKE_BUILD_TYPE=Debug

CMD ["/bin/bash"]
