//===- Fmt.h - NSL formatter public umbrella header --------------*- C++ -*-=//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Public umbrella header for `libNslFmt.a` — the NSL formatter library.
// This is the SOLE public header for the `nsl-fmt` tool library
// (Constitution Principle II — single-public-header rule; nsl-fmt is
// NOT one of the named exceptions for `nsl-ast` / `nsl-sema`).
//
// At T2 acceptance this header exposes exactly 10 public symbols
// frozen by `specs/010-t2-formatter-v0/contracts/format-api.contract.md`
// §3 (3 types + 7 free functions). Public-symbol declarations are
// added INCREMENTALLY by the implementation tasks listed below; the
// final 10-symbol shape is verified by `scripts/audit_fmt_api.sh`
// (added in T029, wired into CI by T092).
//
// Implementation-task → declaration map (from
// `specs/010-t2-formatter-v0/tasks.md`):
//   - T026 → format_buffer, FormatResult, LineRange
//   - T076 → emit_unified_diff
//   - T087 → version_string
//   - T088 → config_key_names
//   - T089 → default_configuration
//   - T102 → Configuration
//   - T103 → parse_config_file
//   - T106 → discover_config
//
// At Phase 1 (T004) this header is intentionally empty — only the
// SPDX header, include guard, and namespace shell. No declarations
// land here yet.
//
//===----------------------------------------------------------------------===//

#ifndef NSL_FMT_FMT_H
#define NSL_FMT_FMT_H

namespace nsl::fmt {

// Public-symbol declarations are added incrementally by later tasks
// (see file header above). At Phase 1 this namespace is intentionally
// empty.

} // namespace nsl::fmt

#endif // NSL_FMT_FMT_H
