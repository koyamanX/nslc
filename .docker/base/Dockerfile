# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

# Common toolchain consumed by every higher layer (nsl/llvm-mlir,
# nsl/circt, nslc): host C++ compiler (clang+lld), build drivers
# (cmake+ninja), build cache (ccache), source/package fetch (git,
# curl, ca-certificates), and the Python the LLVM/MLIR test harness
# (lit) needs. Distro packages only — anything project-specific is
# layered on top.
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential \
      clang lld libclang-rt-18-dev \
      cmake ninja-build ccache \
      git curl ca-certificates \
      python3 python3-pip python3-venv \
      pkg-config zlib1g-dev libzstd-dev \
    && rm -rf /var/lib/apt/lists/*

ENV CC=clang \
    CXX=clang++ \
    CMAKE_GENERATOR=Ninja \
    CCACHE_DIR=/ccache

WORKDIR /workspace
CMD ["/bin/bash"]
