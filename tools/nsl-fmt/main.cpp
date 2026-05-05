// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// tools/nsl-fmt/main.cpp — nsl-fmt driver entry point (T2 Phase 3a-CLI).
//
// Wires the seven CLI flags frozen by
// `specs/010-t2-formatter-v0/contracts/cli-surface.contract.md` §1
// + the mutually-exclusive rejection table in §2 + the exit-code
// matrix in §3 to the Phase-2c `format_buffer()` skeleton in
// `lib/Fmt/Format.cpp`.
//
// The argv parser is hand-rolled (matches `tools/nslc/main.cpp`'s
// convention). At Phase 3a-CLI the format step is byte-faithful
// (Phase 2c skeleton) — `nsl-fmt foo.nsl` reproduces foo.nsl on
// stdout. Phase 3 lands the LayoutPlanner that actually reformats.

#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceManager.h"
#include "nsl/Fmt/Fmt.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

// Frozen usage string. Help output goes to stdout (exit 0); error
// usage hints go to stderr (exit 2).
constexpr const char *kUsage =
    "usage: nsl-fmt [--version] [--help]\n"
    "               [--stdin]\n"
    "               [-i | --in-place]\n"
    "               [-c | --check]\n"
    "               [--config <path>]\n"
    "               [--range LINE:LINE]\n"
    "               [<file>...]\n"
    "\n"
    "Format NSL source. With no flags, prints the canonical formatting\n"
    "of each <file> to stdout in input order. See\n"
    "  docs/design/nsl_tooling_design.md §5.4\n"
    "for the full CLI contract.\n";

// -----------------------------------------------------------------------------
// Frozen diagnostic strings (cli-surface.contract.md §2 + §3).
// These match the lit fixtures under test/Fmt/cli/mutually-exclusive/.
// -----------------------------------------------------------------------------

constexpr const char *kErrCheckAndInPlace =
    "error: --check and --in-place are mutually exclusive\n";
constexpr const char *kErrInPlaceAndStdin =
    "error: --in-place cannot be combined with --stdin\n";
constexpr const char *kErrStdinAndPositional =
    "error: --stdin cannot be combined with positional file arguments\n";
constexpr const char *kErrRangeMultiFile =
    "error: --range requires exactly one input file\n";
constexpr const char *kErrCheckNoInput =
    "error: --check requires at least one input file or --stdin\n";

// -----------------------------------------------------------------------------
// argv helpers
// -----------------------------------------------------------------------------

bool starts(const char *s, const char *p) {
  return std::strncmp(s, p, std::strlen(p)) == 0;
}

// Read every byte of `path` into a string. On failure, write a
// per-spec diagnostic to stderr and return std::nullopt.
std::optional<std::string> readFileFully(llvm::StringRef path) {
  auto bufOrErr = llvm::MemoryBuffer::getFile(path);
  if (!bufOrErr) {
    llvm::errs() << "error: cannot open file '" << path
                 << "': " << bufOrErr.getError().message() << "\n";
    return std::nullopt;
  }
  auto &buf = *bufOrErr;
  return std::string(buf->getBufferStart(), buf->getBufferSize());
}

// Atomic write-to-temp + rename per FR-002. Returns true on
// success.
bool writeFileAtomically(llvm::StringRef path, llvm::StringRef content) {
  std::filesystem::path target(path.str());
  std::filesystem::path temp = target;
  temp += ".tmp.nslfmt";
  {
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    if (!out) {
      llvm::errs() << "error: cannot create temp file '" << temp.string()
                   << "'\n";
      return false;
    }
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!out) {
      llvm::errs() << "error: write failed to '" << temp.string() << "'\n";
      return false;
    }
  }
  std::error_code ec;
  std::filesystem::rename(temp, target, ec);
  if (ec) {
    llvm::errs() << "error: rename '" << temp.string() << "' -> '"
                 << target.string() << "' failed: " << ec.message() << "\n";
    return false;
  }
  return true;
}

// Mint a SourceManager + FileID for a file or stdin buffer. The
// formatter only needs FileID for SourceLocation packing — no
// SourceManager lookup is performed at Phase 2c.
nsl::FileID addBuffer(nsl::SourceManager &sm, llvm::StringRef name,
                      llvm::StringRef content) {
  std::vector<char> bytes(content.begin(), content.end());
  return sm.addBufferInMemory(name.str(), std::move(bytes));
}

// -----------------------------------------------------------------------------
// Per-input format step. Returns 0 on success, non-zero on failure.
// -----------------------------------------------------------------------------

int processInput(llvm::StringRef name, llvm::StringRef content,
                 const nsl::fmt::Configuration &cfg, bool checkMode,
                 bool inPlace, llvm::StringRef inPlacePath) {
  nsl::SourceManager sm;
  nsl::FileID fid = addBuffer(sm, name, content);

  nsl::fmt::FormatResult res =
      nsl::fmt::format_buffer(content, cfg, fid, /*range=*/std::nullopt);

  // Render any diagnostics carried out of format_buffer to stderr.
  // Full source-locating renderer (file:line:col) is wired in
  // Phase 3 proper when the SourceManager is plumbed through; for
  // now we emit `nsl-fmt: <severity>: <message>` per Diagnostic,
  // which is sufficient to surface "parse error" / "config
  // malformed" / etc. to the user.
  for (const nsl::Diagnostic &d : res.diagnostics) {
    const char *sev = "note";
    switch (d.severity) {
      case nsl::Severity::Error:   sev = "error";   break;
      case nsl::Severity::Warning: sev = "warning"; break;
      case nsl::Severity::Note:    sev = "note";    break;
    }
    llvm::errs() << "nsl-fmt: " << name << ": " << sev << ": " << d.message
                 << "\n";
  }

  if (res.status == nsl::fmt::FormatResult::Status::Refused ||
      res.status == nsl::fmt::FormatResult::Status::Error) {
    return 1; // continue-on-error per FR-003a
  }

  if (checkMode) {
    if (res.formattedText == content) {
      return 0; // clean
    }
    // Print unified diff (Phase 4 wires the real Myers diff; Phase
    // 2c emits a placeholder marker — still satisfies "exit
    // non-zero with diff per offending file" exit-code shape).
    std::string diff =
        nsl::fmt::emit_unified_diff(content, res.formattedText, name, name);
    llvm::outs() << diff;
    return 1;
  }

  if (inPlace) {
    if (!writeFileAtomically(inPlacePath, res.formattedText)) {
      return 1;
    }
    return 0;
  }

  // Default: write formatted text to stdout.
  llvm::outs() << res.formattedText;
  return 0;
}

} // namespace

int main(int argc, char **argv) {
  // Parsed flag state.
  bool useStdin   = false;
  bool inPlace    = false;
  bool checkMode  = false;
  std::optional<std::string> rangeArg;
  std::optional<std::string> configPath;
  std::vector<std::string>   inputs;

  // Parse argv.
  for (int i = 1; i < argc; ++i) {
    const char *a = argv[i];
    if (std::strcmp(a, "--version") == 0) {
      llvm::outs() << nsl::fmt::version_string() << "\n";
      return 0;
    }
    if (std::strcmp(a, "--help") == 0 || std::strcmp(a, "-h") == 0) {
      llvm::outs() << kUsage;
      return 0;
    }
    if (std::strcmp(a, "--stdin") == 0) {
      useStdin = true;
    } else if (std::strcmp(a, "-i") == 0 ||
               std::strcmp(a, "--in-place") == 0) {
      inPlace = true;
    } else if (std::strcmp(a, "-c") == 0 ||
               std::strcmp(a, "--check") == 0) {
      checkMode = true;
    } else if (std::strcmp(a, "--config") == 0 && i + 1 < argc) {
      configPath = std::string(argv[++i]);
    } else if (starts(a, "--config=")) {
      configPath = std::string(a + std::strlen("--config="));
    } else if (std::strcmp(a, "--range") == 0 && i + 1 < argc) {
      rangeArg = std::string(argv[++i]);
    } else if (starts(a, "--range=")) {
      rangeArg = std::string(a + std::strlen("--range="));
    } else if (a[0] != '-' || a[0] == '\0') {
      inputs.emplace_back(a);
    } else {
      llvm::errs() << "error: unknown argument: " << a << "\n" << kUsage;
      return 2;
    }
  }

  // Mutually-exclusive checks (cli-surface.contract.md §2).
  // Frozen strings; the matching lit fixtures under
  // test/Fmt/cli/mutually-exclusive/ assert these verbatim.
  if (checkMode && inPlace) {
    llvm::errs() << kErrCheckAndInPlace;
    return 2;
  }
  if (inPlace && useStdin) {
    llvm::errs() << kErrInPlaceAndStdin;
    return 2;
  }
  if (useStdin && !inputs.empty()) {
    llvm::errs() << kErrStdinAndPositional;
    return 2;
  }
  if (rangeArg.has_value() && inputs.size() > 1) {
    llvm::errs() << kErrRangeMultiFile;
    return 2;
  }
  if (checkMode && inputs.empty() && !useStdin) {
    llvm::errs() << kErrCheckNoInput;
    return 2;
  }

  // --range parsing is deferred to Phase 5 (T090). At Phase 3a-CLI
  // the flag is recognised but its presence is a no-op (the formatter
  // ignores the range and reformats the whole input).
  (void)rangeArg;

  // --config is recognised but TOML parsing lands at Phase 6 (T103).
  // Use built-in defaults for now.
  (void)configPath;
  nsl::fmt::Configuration cfg = nsl::fmt::default_configuration();

  // --stdin path.
  if (useStdin) {
    auto bufOrErr = llvm::MemoryBuffer::getSTDIN();
    if (!bufOrErr) {
      llvm::errs() << "error: cannot read stdin: "
                   << bufOrErr.getError().message() << "\n";
      return 1;
    }
    llvm::StringRef content = (*bufOrErr)->getBuffer();
    return processInput("<stdin>", content, cfg, checkMode, /*inPlace=*/false,
                        /*inPlacePath=*/"");
  }

  // No inputs + no --stdin: print usage to stderr + exit 2 (matches
  // gofmt behavior — the bare-invocation case is almost always a
  // mistake worth flagging).
  if (inputs.empty()) {
    llvm::errs() << kUsage;
    return 2;
  }

  // Default / -i / --check over multiple files: continue-on-error
  // per FR-003a.
  int aggregateExit = 0;
  for (const std::string &path : inputs) {
    auto contentOpt = readFileFully(path);
    if (!contentOpt) {
      aggregateExit = 1;
      continue;
    }
    int rc = processInput(path, *contentOpt, cfg, checkMode, inPlace, path);
    if (rc != 0) {
      aggregateExit = 1;
    }
  }
  return aggregateExit;
}
