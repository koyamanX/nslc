# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
FROM nsl/llvm-mlir:dev

ARG DEBIAN_FRONTEND=noninteractive

# Build CIRCT against the LLVM/MLIR install staged in /opt/llvm by
# the parent layer. Install under /opt/circt so the cmake configs
# land at /opt/circt/lib/cmake/circt — the path nslc:dev's CIRCT_DIR
# expects. CIRCT_COMMIT must match the LLVM_COMMIT used to build
# nsl/llvm-mlir:dev (see CIRCT's `llvm/` submodule).
ARG CIRCT_REPO=https://github.com/llvm/circt.git
ARG CIRCT_COMMIT=main
ARG CIRCT_INSTALL_PREFIX=/opt/circt
ARG CIRCT_BUILD_TYPE=Release
ARG CIRCT_PARALLEL_LINK_JOBS=2

RUN git clone --filter=blob:none ${CIRCT_REPO} /tmp/circt \
    && git -C /tmp/circt checkout ${CIRCT_COMMIT} \
    && cmake -S /tmp/circt -B /tmp/circt/build -G Ninja \
        -DCMAKE_BUILD_TYPE=${CIRCT_BUILD_TYPE} \
        -DCMAKE_INSTALL_PREFIX=${CIRCT_INSTALL_PREFIX} \
        -DMLIR_DIR=${MLIR_DIR} \
        -DLLVM_DIR=${LLVM_DIR} \
        -DLLVM_EXTERNAL_LIT=/opt/llvm/bin/llvm-lit \
        -DLLVM_USE_LINKER=lld \
        -DLLVM_CCACHE_BUILD=ON \
        -DLLVM_PARALLEL_LINK_JOBS=${CIRCT_PARALLEL_LINK_JOBS} \
    && cmake --build /tmp/circt/build --target install \
    && rm -rf /tmp/circt

ENV PATH="${CIRCT_INSTALL_PREFIX}/bin:${PATH}" \
    CIRCT_DIR=${CIRCT_INSTALL_PREFIX}/lib/cmake/circt

CMD ["/bin/bash"]
