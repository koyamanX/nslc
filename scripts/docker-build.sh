#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# scripts/docker-build.sh — local helper that builds the four-stage
# Docker image set under .docker/ using the pins in cmake/deps.lock.
#
# Usage:
#   ./scripts/docker-build.sh              # build all four locally (no push)
#   ./scripts/docker-build.sh base         # build a single layer
#   ./scripts/docker-build.sh nslc
#
# Override the registry / owner with env vars when running locally:
#   NSLC_DOCKER_OWNER=mycache NSLC_DOCKER_REGISTRY=ghcr.io \
#     ./scripts/docker-build.sh
#
# CI uses .github/workflows/publish-images.yml — same pins, same tag
# scheme, with a `--push` step appended.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_LOCK="${REPO_ROOT}/cmake/deps.lock"

# Load pins from cmake/deps.lock.
# shellcheck disable=SC1090
{ set -a; . "${DEPS_LOCK}"; set +a; }

# Local-build defaults. CI overrides via env in publish-images.yml.
NSLC_DOCKER_REGISTRY="${NSLC_DOCKER_REGISTRY:-${DOCKER_REGISTRY}}"
NSLC_DOCKER_OWNER="${NSLC_DOCKER_OWNER:-local}"

img() {
  printf '%s/%s/%s:%s' "${NSLC_DOCKER_REGISTRY}" \
    "${NSLC_DOCKER_OWNER}" "$1" "${DOCKER_TAG}"
}

BASE_IMG="$(img "${DOCKER_BASE_IMAGE}")"
LLVM_IMG="$(img "${DOCKER_LLVM_MLIR_IMAGE}")"
CIRCT_IMG="$(img "${DOCKER_CIRCT_IMAGE}")"
NSLC_IMG="$(img "${DOCKER_NSLC_IMAGE}")"

# The Dockerfiles' FROM lines reference `nsl/base:dev`, `nsl/llvm-mlir:dev`
# etc. by their local-only short tags. Tag every image with both the
# fully-qualified name AND the short FROM-target name so the next
# layer's docker build resolves the parent.

build_base() {
  echo "==> ${BASE_IMG}"
  docker build -t "${BASE_IMG}" -t nsl/base:dev "${REPO_ROOT}/.docker/base/"
}

build_llvm_mlir() {
  echo "==> ${LLVM_IMG}  (LLVM_COMMIT=${LLVM_COMMIT:-main})"
  docker build -t "${LLVM_IMG}" -t nsl/llvm-mlir:dev \
    --build-arg LLVM_COMMIT="${LLVM_COMMIT}" \
    --build-arg LLVM_PARALLEL_LINK_JOBS="${LLVM_PARALLEL_LINK_JOBS:-2}" \
    "${REPO_ROOT}/.docker/llvm-mlir/"
}

build_circt() {
  echo "==> ${CIRCT_IMG}  (CIRCT_COMMIT=${CIRCT_COMMIT:-main})"
  docker build -t "${CIRCT_IMG}" -t nsl/circt:dev \
    --build-arg CIRCT_COMMIT="${CIRCT_COMMIT}" \
    --build-arg CIRCT_PARALLEL_LINK_JOBS="${CIRCT_PARALLEL_LINK_JOBS:-2}" \
    "${REPO_ROOT}/.docker/circt/"
}

build_nslc() {
  echo "==> ${NSLC_IMG}"
  docker build -t "${NSLC_IMG}" -t nsl/nslc:dev "${REPO_ROOT}/.docker/nslc/"
}

target="${1:-all}"
case "${target}" in
  base)      build_base ;;
  llvm-mlir) build_llvm_mlir ;;
  circt)     build_circt ;;
  nslc)      build_nslc ;;
  all)       build_base && build_llvm_mlir && build_circt && build_nslc ;;
  *)
    echo "docker-build.sh: unknown target '${target}'" >&2
    echo "  valid: base | llvm-mlir | circt | nslc | all" >&2
    exit 2
    ;;
esac

echo
echo "OK. Local images:"
docker images --filter=reference='nsl/*:dev' \
  --format 'table {{.Repository}}\t{{.Tag}}\t{{.Size}}'
echo
echo "To run the dev environment:"
echo "  docker run --rm -it -v \"\$(pwd)\":/workspace ${NSLC_IMG}"
