# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
# Parent image: local builds default to the short `nsl/base:dev` tag
# `scripts/docker-build.sh` produces; CI overrides via build-arg to the
# fully-qualified ghcr.io name so buildx (docker-container driver,
# isolated from the host docker daemon) can pull it directly.
ARG PARENT_IMAGE=nsl/base:dev
FROM ${PARENT_IMAGE}

ARG DEBIAN_FRONTEND=noninteractive

# Build LLVM+MLIR from source and install under /opt/llvm. The CIRCT
# layer above pins the LLVM commit via its `llvm/` submodule — keep
# LLVM_COMMIT in sync with that pin or CIRCT will fail to configure
# against this image.
ARG LLVM_REPO=https://github.com/llvm/llvm-project.git
ARG LLVM_COMMIT=main
ARG LLVM_INSTALL_PREFIX=/opt/llvm
ARG LLVM_BUILD_TYPE=Release
ARG LLVM_PARALLEL_LINK_JOBS=2

# `LLVM_INSTALL_UTILS=ON` puts lit / FileCheck / not / count /
# split-file / yaml2obj / obj2yaml under ${LLVM_INSTALL_PREFIX}/bin —
# the contract that nsl/circt:dev and nslc:dev consume.
RUN git clone --filter=blob:none ${LLVM_REPO} /tmp/llvm-project \
    && git -C /tmp/llvm-project checkout ${LLVM_COMMIT} \
    && cmake -S /tmp/llvm-project/llvm -B /tmp/llvm-project/build -G Ninja \
        -DCMAKE_BUILD_TYPE=${LLVM_BUILD_TYPE} \
        -DCMAKE_INSTALL_PREFIX=${LLVM_INSTALL_PREFIX} \
        -DLLVM_ENABLE_PROJECTS="mlir" \
        -DLLVM_TARGETS_TO_BUILD="host" \
        -DLLVM_ENABLE_ASSERTIONS=ON \
        -DLLVM_INSTALL_UTILS=ON \
        -DLLVM_BUILD_EXAMPLES=OFF \
        -DLLVM_INCLUDE_EXAMPLES=OFF \
        -DLLVM_BUILD_TESTS=OFF \
        -DLLVM_INCLUDE_TESTS=OFF \
        -DLLVM_USE_LINKER=lld \
        -DLLVM_CCACHE_BUILD=ON \
        -DLLVM_PARALLEL_LINK_JOBS=${LLVM_PARALLEL_LINK_JOBS} \
    && cmake --build /tmp/llvm-project/build --target install \
    && rm -rf /tmp/llvm-project

ENV PATH="${LLVM_INSTALL_PREFIX}/bin:${PATH}" \
    LLVM_DIR=${LLVM_INSTALL_PREFIX}/lib/cmake/llvm \
    MLIR_DIR=${LLVM_INSTALL_PREFIX}/lib/cmake/mlir

CMD ["/bin/bash"]
