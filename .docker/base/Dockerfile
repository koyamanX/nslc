# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

# Common toolchain consumed by every higher layer (nsl/llvm-mlir,
# nsl/circt, nslc): host C++ compiler (clang+lld), build drivers
# (cmake+ninja), build cache (ccache), source/package fetch (git,
# curl, ca-certificates), and the Python the LLVM/MLIR test harness
# (lit) needs.
#
# Ubuntu 24.04's default repos do NOT ship clang's compiler-rt
# archives (libclang_rt.asan-x86_64.a etc.) — `libclang-rt-18-dev`
# is not in `noble/main`. The project's `NSL_ENABLE_ASAN=ON` default
# (CMakeLists.txt:53) requires those archives at link time, so we
# pull them from the LLVM apt source. Without this, every clang-cell
# CI build fails on `cannot find /usr/lib/llvm-18/lib/clang/18/lib/
# linux/libclang_rt.asan-x86_64.a` — the issue that surfaced post-M4
# merge and persisted through M5.
RUN apt-get update && apt-get install -y --no-install-recommends \
      ca-certificates curl gnupg \
    && curl -fsSL https://apt.llvm.org/llvm-snapshot.gpg.key \
        | gpg --dearmor -o /usr/share/keyrings/llvm.gpg \
    && echo "deb [signed-by=/usr/share/keyrings/llvm.gpg] http://apt.llvm.org/noble/ llvm-toolchain-noble-18 main" \
        > /etc/apt/sources.list.d/llvm.list \
    && apt-get update && apt-get install -y --no-install-recommends \
      build-essential \
      clang-18 lld-18 libclang-rt-18-dev \
      cmake ninja-build ccache \
      git \
      python3 python3-pip python3-venv \
      pkg-config zlib1g-dev libzstd-dev \
    && rm -rf /var/lib/apt/lists/* \
    && update-alternatives --install /usr/bin/clang   clang   /usr/bin/clang-18   100 \
    && update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-18 100 \
    && update-alternatives --install /usr/bin/lld     lld     /usr/bin/lld-18     100

ENV CC=clang \
    CXX=clang++ \
    CMAKE_GENERATOR=Ninja \
    CCACHE_DIR=/ccache

WORKDIR /workspace
CMD ["/bin/bash"]
