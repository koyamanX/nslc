// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/LSP/JSONTransport.cpp — Content-Length framing impl.

#include "JSONTransport.h"
#include "Logger.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdint>
#include <istream>
#include <ostream>
#include <string>

namespace nsl {
namespace lsp {

JSONTransport::JSONTransport(std::istream &in, std::ostream &out)
    : in_(in), out_(out) {}

namespace {

// Read one CRLF-terminated header line from `in`. Returns false
// on EOF or framing error; the line content (without the trailing
// CRLF) is written to `*out`.
bool readHeaderLine(std::istream &in, std::string *out) {
  out->clear();
  while (true) {
    int c = in.get();
    if (c == EOF) return false;
    if (c == '\r') {
      int next = in.get();
      if (next == '\n') return true;
      // Bare CR is a framing error in LSP base protocol.
      return false;
    }
    if (c == '\n') {
      // Bare LF (no preceding CR) — non-conformant but accept
      // defensively to handle minor client deviations. Some LSP
      // clients (notably older Neovim builds) emit bare LF.
      return true;
    }
    out->push_back(static_cast<char>(c));
  }
}

bool parseContentLength(llvm::StringRef line, uint64_t *out) {
  llvm::StringRef key("Content-Length:");
  if (!line.starts_with(key)) return false;
  llvm::StringRef rest = line.drop_front(key.size()).trim();
  return !rest.consumeInteger(10, *out) && rest.empty();
}

} // namespace

std::optional<llvm::json::Value> JSONTransport::readMessage() {
  // Header section: read CRLF-terminated lines until the empty
  // separator line. Per LSP 3.16 §base-protocol the only required
  // header is Content-Length; Content-Type is optional and
  // ignored at T3.
  uint64_t content_length = 0;
  bool got_length = false;
  std::string line;
  while (true) {
    if (!readHeaderLine(in_, &line)) {
      // EOF before complete header section.
      if (!got_length && line.empty()) return std::nullopt;
      NSL_LSP_LOG_ERROR("JSONTransport: framing error — unexpected "
                         "EOF in header section");
      return std::nullopt;
    }
    if (line.empty()) break; // separator
    uint64_t n;
    if (parseContentLength(line, &n)) {
      content_length = n;
      got_length = true;
    }
    // Other headers (Content-Type, etc.) are silently ignored at T3.
  }

  if (!got_length) {
    NSL_LSP_LOG_ERROR("JSONTransport: framing error — missing "
                       "Content-Length header");
    return std::nullopt;
  }

  // Body: read exactly content_length bytes.
  std::string body;
  body.resize(content_length);
  if (content_length > 0) {
    in_.read(body.data(), static_cast<std::streamsize>(content_length));
    auto got = in_.gcount();
    if (static_cast<uint64_t>(got) != content_length) {
      NSL_LSP_LOG_ERROR(llvm::formatv(
          "JSONTransport: framing error — short read ({0} of {1} bytes)",
          static_cast<uint64_t>(got), content_length).str());
      return std::nullopt;
    }
  }

  llvm::Expected<llvm::json::Value> parsed = llvm::json::parse(body);
  if (!parsed) {
    // Consume the Error explicitly. `os << err` streams the
    // diagnostic but in LLVM 18 doesn't always mark the Error
    // as handled — the destructor of an unhandled Error aborts
    // the process. `toString` is the canonical consume path.
    std::string err = llvm::toString(parsed.takeError());
    NSL_LSP_LOG_ERROR(llvm::formatv("JSONTransport: malformed JSON "
                                      "body: {0}", err).str());
    return std::nullopt;
  }
  return std::move(*parsed);
}

void JSONTransport::writeMessage(const llvm::json::Value &msg) {
  // Serialize compactly (no pretty-printing) into a buffer first
  // so we can emit Content-Length plus body atomically under the
  // mutex.
  std::string body;
  {
    llvm::raw_string_ostream os(body);
    os << msg;
  }

  std::lock_guard<std::mutex> guard(write_mtx_);
  out_ << "Content-Length: " << body.size() << "\r\n\r\n" << body;
  out_.flush();
}

} // namespace lsp
} // namespace nsl
