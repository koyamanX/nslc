//===- Stub.cpp - Anchor TU for libNslFmt.a (Phase 1 scaffold) ---*- C++ -*-=//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Phase-1 anchor translation unit for `nsl-fmt`. CMake STATIC
// libraries need at least one object file; this TU exists so the
// archive can be built before any real implementation lands.
//
// **DELETE THIS FILE** as soon as the first real source file lands
// in Phase 2 (the natural candidate is `DirectiveSplitter.cpp` per
// T015 of `specs/010-t2-formatter-v0/tasks.md`). Per Constitution
// Principle II, the library is NOT one of the named header-layout
// exceptions; its public surface is `include/nsl/Fmt/Fmt.h` only.
//
//===----------------------------------------------------------------------===//

#include "nsl/Fmt/Fmt.h"

namespace nsl::fmt::detail {

// Anchor symbol so the TU is not optimised away under `--gc-sections`.
// The function has no callers and no side effects; it exists purely to
// give the linker something to keep alive in the archive.
[[maybe_unused]] int phase1_anchor() noexcept { return 0; }

} // namespace nsl::fmt::detail
