// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// tools/nslc/main.cpp — nslc driver entry point (Principle II target
// ≤62 lines + per-`-emit=*` glue). Real work lives in nsl-driver.
//
// **Stdin support (M6 follow-on, T139 close-out)**: `nslc -emit=<stage> -`
// reads NSL source from stdin. The driver layer's emit functions all
// take a file path (`sm.loadFile(path)`) so we trampoline `-` through
// a temp file: read stdin, write to a `mkstemp` path, pass that path
// to `emitX(...)`. The temp file is unlinked at process exit via an
// `atexit` registered handler. Same shape works across the four
// distinct emit stages — `tokens`, `ast`, `mlir`, and `hw` (with
// `circt` as the alias for `hw` per `driver-emit-hw.contract.md` §1,
// so the dispatch arm count is 4 even though there are 5 accepted
// stage spellings).
//
// **Known limitation (Copilot review #3)**: `mkstemps` returns a
// random suffix in the path, so `nslc -emit=tokens -` token-stream
// output (which embeds the input path in each token's location)
// varies per invocation. Out of scope for this PR — fixing
// requires plumbing a virtual `<stdin>` label through SourceManager
// rather than handing it a real filesystem path. Tracked as a
// post-merge follow-on.

#include "nsl/Driver/EmitAST.h"
#include "nsl/Driver/EmitHW.h"
#include "nsl/Driver/EmitMLIR.h"
#include "nsl/Driver/EmitTokens.h"
#include "nsl/Driver/Version.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

namespace {
constexpr const char *kUsage =
    "usage: nslc [--version] [-I <dir>]... [-D NAME=value]... "
    "[--diagnostic-format=text|json] -emit=<stage> <input>\n"
    "  -emit=<stage>   Stop after stage. Stages:\n"
    "                    tokens   M1 lex output\n"
    "                    ast      M2/M3 AST snapshot\n"
    "                    mlir     M5 nsl::* MLIR (post-structural-expansion)\n"
    "                    hw       M6 CIRCT MLIR (hw/comb/seq/fsm/sv;\n"
    "                             also accepts -emit=circt as an alias)\n"
    "                    verilog  (M7+) — not yet implemented\n";
bool starts(const char *s, const char *p) {
  return std::strncmp(s, p, std::strlen(p)) == 0;
}

// Persistent storage for the temp-file path so the atexit handler
// (registered after mkstemp succeeds) sees it. Static-storage-
// duration string is fine — only one temp file per process.
std::string g_stdin_temp_path;

void cleanupStdinTempFile() {
  if (!g_stdin_temp_path.empty()) {
    std::remove(g_stdin_temp_path.c_str());
  }
}

/// Read the entirety of stdin into a freshly-`mkstemp`-created file
/// and return its path. Returns an empty string on error (caller
/// emits a diagnostic). On success, registers an atexit handler that
/// unlinks the file when the process exits.
std::string slurpStdinToTempFile(llvm::raw_ostream &err) {
  // Honor TMPDIR when present + non-empty, fall back to /tmp.
  // (Copilot review #2: empty TMPDIR previously turned into a
  // root-relative "/nslc-stdin-..." path that EACCES'd.)
  const char *tmpdir = std::getenv("TMPDIR");
  std::string path = (tmpdir != nullptr && tmpdir[0] != '\0') ? tmpdir : "/tmp";
  if (!path.empty() && path.back() == '/') {
    path.pop_back();
  }
  path += "/nslc-stdin-XXXXXX.nsl";

  // mkstemps wants a non-const pointer; std::string::data() is
  // contiguous + writable since C++17 so this is well-defined.
  int fd = mkstemps(path.data(), static_cast<int>(std::strlen(".nsl")));
  if (fd < 0) {
    err << "could not create temp file for stdin: " << std::strerror(errno)
        << "\n";
    return {};
  }
  g_stdin_temp_path = path;
  std::atexit(cleanupStdinTempFile);

  // Stream stdin → fd in 64 KB chunks.
  char buf[64 * 1024];
  while (true) {
    // Copilot review #4: `errno` after `fread` is not guaranteed to
    // be set even on partial-read-into-EOF; clear it first so the
    // post-read check distinguishes real I/O errors from spurious
    // stale errno values.
    errno = 0;
    std::size_t n = std::fread(buf, 1, sizeof(buf), stdin);
    if (n == 0) {
      if (std::ferror(stdin)) {
        err << "error reading stdin"
            << (errno != 0 ? std::string(": ") + std::strerror(errno) : "")
            << "\n";
        ::close(fd);
        return {};
      }
      break; // EOF
    }
    char *p = buf;
    std::size_t remaining = n;
    while (remaining > 0) {
      ssize_t w = ::write(fd, p, remaining);
      if (w < 0) {
        if (errno == EINTR) {
          continue;
        }
        err << "error writing stdin to temp file " << path << ": "
            << std::strerror(errno) << "\n";
        ::close(fd);
        return {};
      }
      p += w;
      remaining -= static_cast<std::size_t>(w);
    }
  }
  if (::close(fd) < 0) {
    err << "error closing temp file " << path << ": " << std::strerror(errno)
        << "\n";
    return {};
  }
  return path;
}
} // namespace

int main(int argc, char **argv) {
  nsl::driver::EmitTokensOptions opts;
  llvm::StringRef stage;
  llvm::StringRef input;
  for (int i = 1; i < argc; ++i) {
    const char *a = argv[i];
    if ((std::strcmp(a, "--version") == 0) || (std::strcmp(a, "-v") == 0)) {
      llvm::outs() << "nslc " << NSLC_VERSION_STRING << "\n";
      return 0;
    }
    if (starts(a, "-emit=")) {
      stage = a + 6;
    } else if ((std::strcmp(a, "-I") == 0) && i + 1 < argc) {
      opts.include_paths.emplace_back(argv[++i]);
    } else if (starts(a, "-I") && a[2] != '\0') {
      opts.include_paths.emplace_back(a + 2);
    } else if ((std::strcmp(a, "-D") == 0) && i + 1 < argc) {
      opts.predefined_macros.emplace_back(argv[++i]);
    } else if (starts(a, "-D") && a[2] != '\0') {
      opts.predefined_macros.emplace_back(a + 2);
    } else if (std::strcmp(a, "--diagnostic-format=json") == 0) {
      opts.diagnostic_json = true;
    } else if (std::strcmp(a, "--diagnostic-format=text") == 0) {
      opts.diagnostic_json = false;
    } else if (std::strcmp(a, "-") == 0 && input.empty()) {
      // Stdin marker — recognized for every -emit=<stage>. The
      // actual stdin slurping happens after arg parsing finishes
      // (so a parse-error or missing -emit doesn't waste a stdin
      // read). We mark `input = "-"` and resolve it below.
      input = "-";
    } else if (a[0] != '-' && input.empty()) {
      input = a;
    } else {
      llvm::errs() << "unknown argument: " << a << "\n" << kUsage;
      return 2;
    }
  }
  if (stage.empty() || input.empty()) {
    llvm::errs() << "input file required\n" << kUsage;
    return 2;
  }
  // Resolve `-` → temp file holding stdin contents.
  std::string stdin_path_storage; // keep alive for the StringRef below
  if (input == "-") {
    stdin_path_storage = slurpStdinToTempFile(llvm::errs());
    if (stdin_path_storage.empty()) {
      return 3; // matches emitX's "could not open input" exit code
    }
    input = stdin_path_storage;
  }
  if (stage == "tokens") {
    return nsl::driver::emitTokens(input, opts, llvm::outs(), llvm::errs());
  }
  if (stage == "ast") {
    return nsl::driver::emitAST(input, opts, llvm::outs(), llvm::errs());
  }
  if (stage == "mlir") {
    return nsl::driver::emitMLIR(input, opts, llvm::outs(), llvm::errs());
  }
  // M6: -emit=hw (canonical) and -emit=circt (alias per
  // driver-emit-hw.contract.md §1) both invoke emitHW.
  if (stage == "hw" || stage == "circt") {
    return nsl::driver::emitHW(input, opts, llvm::outs(), llvm::errs());
  }
  if (stage == "verilog") {
    llvm::errs()
        << "error: '-emit=verilog' is not yet implemented (planned for M7)\n";
    return 2;
  }
  llvm::errs() << "unknown emit stage: " << stage << "\n" << kUsage;
  return 2;
}
