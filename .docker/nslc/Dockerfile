# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
FROM nsl/circt:dev

ARG DEBIAN_FRONTEND=noninteractive

# Toolchain for building, testing, and linting nslc.
# clang / cmake / ninja / lld / ccache / python3 come from nsl/base:dev;
# lit / FileCheck / not / count / split-file / yaml2obj / obj2yaml are
# already staged in /opt/llvm/bin by nsl/llvm-mlir:dev. Add formatter,
# static-analyzer, and debug helpers on top.
RUN apt-get update && apt-get install -y --no-install-recommends \
      clang-format clang-tidy \
      gdb less \
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
