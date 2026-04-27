// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/Driver/EmitTokens.h
//
// Public interface for the `nslc -emit=tokens` code path. The
// `tools/nslc/main.cpp` driver delegates to `emitTokens()` after
// parsing its CLI; per Constitution Principle II all real behavior
// lives here in `nsl-driver` so the driver binary stays ≤60 lines.
//
// The contract for this CLI surface is
// `specs/002-m1-lex-preprocess/contracts/nslc-emit-tokens.contract.md`
// (FR-029, FR-030, FR-038). Stdout schema, exit codes, and
// determinism guarantees are pinned there; this header documents only
// the C++ entry-point shape.

#ifndef NSL_DRIVER_EMITTOKENS_H
#define NSL_DRIVER_EMITTOKENS_H

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

#include <string>
#include <vector>

namespace nsl::driver {

/// Options controlling `emitTokens()` behavior. Owned by the caller
/// (`tools/nslc/main.cpp`), populated from CLI flags.
struct EmitTokensOptions {
  /// Quote-form `#include` search directories (`-I <dir>`, repeatable).
  /// Accepted at M1 but unused until Phase 4 wires the preprocessor.
  std::vector<std::string> include_paths;

  /// Predefined macros (`-D NAME=value`, repeatable). Accepted at M1
  /// but unused until Phase 4 wires the preprocessor.
  std::vector<std::string> predefined_macros;

  /// Emit diagnostics in JSON (NDJSON, smoke-only at M1) rather than
  /// the canonical text format. Set by `--diagnostic-format=json`.
  bool diagnostic_json = false;
};

/// Run `-emit=tokens` over `input_path`. Loads the file via the
/// `SourceManager`, runs the lexer, prints tokens to `os` in the
/// canonical contract format, and renders any diagnostics to `err`.
///
/// **Phase 3 form**: lex-only — the preprocessor wiring lands in
/// Phase 4 (T061), which extends this function to construct the
/// `Preprocessor` from `opts.include_paths` and
/// `opts.predefined_macros`.
///
/// Returns exit codes per the contract:
///   - 0: success.
///   - 1: lex (or, post-Phase-4, preprocess) raised at least one
///        error. Stdout receives no token output (FR "no partial
///        output").
///   - 3: input file could not be opened.
int emitTokens(llvm::StringRef input_path, const EmitTokensOptions &opts,
               llvm::raw_ostream &os, llvm::raw_ostream &err);

} // namespace nsl::driver

#endif // NSL_DRIVER_EMITTOKENS_H
