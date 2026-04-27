// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// AssertImpl.h — private to lib/Basic/. Always-active invariant guard
// for the spec invariants documented in
// `specs/002-m1-lex-preprocess/data-model.md` (entities 1, 2, 5).
//
// `assert()` from `<cassert>` compiles out under `-DNDEBUG` (Release
// builds), but the data-model invariants it guards are SPEC-level
// constraints — `SourceLocation::make()`'s 16 MiB offset cap is
// documented as a "hard fatal" in entity 1, the SourceRange same-
// file constraint in entity 2, etc. These must fire in every build,
// not just Debug. NSL_ABORT is the always-active replacement.
//
// This header is NOT installed (not listed in lib/Basic/
// CMakeLists.txt's HEADERS clause). Public-API users see the
// behavior — "make() aborts on out-of-range offset" — but not the
// macro itself.

#ifndef NSL_LIB_BASIC_ASSERTIMPL_H
#define NSL_LIB_BASIC_ASSERTIMPL_H

#include <cstdio>
#include <cstdlib>

namespace nsl::basic::detail {

[[noreturn]] inline void abortWithMessage(const char *expr, const char *file,
                                          int line, const char *msg) {
  std::fprintf(stderr, "%s:%d: NSL_ABORT(%s): %s\n", file, line, expr, msg);
  std::abort();
}

} // namespace nsl::basic::detail

#define NSL_ABORT(cond, msg)                                                   \
  do {                                                                         \
    if (!(cond)) {                                                             \
      ::nsl::basic::detail::abortWithMessage(#cond, __FILE__, __LINE__, (msg));\
    }                                                                          \
  } while (false)

#endif // NSL_LIB_BASIC_ASSERTIMPL_H
