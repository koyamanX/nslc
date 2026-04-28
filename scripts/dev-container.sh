#!/bin/bash
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# scripts/dev-container.sh — wraps `docker run` with the project's
# dev container image and a 2GB cgroup memory limit, so a runaway
# leak gets SIGKILLed early instead of climbing past the host's
# RAM and tripping the OS OOM-killer (which has historically
# taken down agent processes mid-build).
#
# Usage:
#   scripts/dev-container.sh '<command>'
#   scripts/dev-container.sh --memory 4g '<command>'   # override default cap
#
# The script:
#   1. Pins memory + swap at 2GB by default (override with --memory).
#   2. Bind-mounts $PWD as /work and cd's into it.
#   3. Ensures libclang-rt-18-dev is installed (one-shot apt for
#      ASan; the next dev-container image rebuild bakes it in).
#   4. Runs the supplied command via `sh -c`.
#
# Combined with the project's `NSL_ENABLE_ASAN=ON` CMake default,
# this turns every dev/agent build into a leak-checked, memory-
# capped run.

set -euo pipefail

MEMORY_LIMIT="2g"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --memory)
      MEMORY_LIMIT="$2"
      shift 2
      ;;
    --memory=*)
      MEMORY_LIMIT="${1#*=}"
      shift
      ;;
    --)
      shift
      break
      ;;
    *)
      break
      ;;
  esac
done

if [[ $# -lt 1 ]]; then
  echo "usage: $0 [--memory <size>] '<command>'" >&2
  exit 2
fi

CMD="$*"

# Wrap the supplied command with the libclang-rt-18-dev install
# step. The dev-container image at ghcr.io/koyamanx/nsl-nslc:dev
# does not yet bake the sanitizer runtime; once the next image
# rebuild publishes (.docker/base/Dockerfile already updated), the
# `apt-get` calls become no-ops.
PRELUDE='if [ ! -f /usr/lib/llvm-18/lib/clang/18/lib/linux/libclang_rt.asan-x86_64.a ]; then
  apt-get update >/dev/null 2>&1
  apt-get install -y --no-install-recommends libclang-rt-18-dev >/dev/null 2>&1
fi'

exec sg docker -c "docker run --rm \
  --memory ${MEMORY_LIMIT} \
  --memory-swap ${MEMORY_LIMIT} \
  --oom-kill-disable=false \
  -v ${PWD}:/work -w /work \
  ghcr.io/koyamanx/nsl-nslc:dev \
  sh -c '${PRELUDE} && ${CMD//\'/\'\\\'\'}'"
