// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test/lsp/JSONTransport_test.cpp — focused unit test for the
// `Content-Length`-framed JSON-RPC transport (T019). Per
// `specs/010-t3-lsp-skeleton/contracts/lsp-protocol.contract.md`
// §1 (framing) and `data-model.md` §2.1.
//
// Uses `std::stringstream` for both in and out so the tests run
// in-process (no subprocess); the integration tests in
// lifecycle_test.cpp / diagnostics_test.cpp / folding_test.cpp
// cover the same transport over real pipes.

#include "../../lib/LSP/JSONTransport.h"

#include "llvm/Support/JSON.h"

#include <gtest/gtest.h>
#include <sstream>
#include <string>

using nsl::lsp::JSONTransport;

namespace {

// Frame `body_text` with a Content-Length header.
std::string frame(llvm::StringRef body) {
  return "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" +
          body.str();
}

} // namespace

TEST(JSONTransportTest, RoundTripSimple) {
  std::stringstream in_stream;
  std::stringstream out_stream;
  in_stream << frame(R"({"jsonrpc":"2.0","method":"foo","params":42})");

  JSONTransport t(in_stream, out_stream);
  auto msg = t.readMessage();
  ASSERT_TRUE(msg.has_value());
  auto *obj = msg->getAsObject();
  ASSERT_NE(obj, nullptr);
  EXPECT_EQ(obj->getString("jsonrpc").value_or(""), "2.0");
  EXPECT_EQ(obj->getString("method").value_or(""), "foo");
  EXPECT_EQ(obj->getInteger("params").value_or(0), 42);
}

TEST(JSONTransportTest, RoundTripNestedObject) {
  std::stringstream in_stream;
  std::stringstream out_stream;
  in_stream << frame(
      R"({"id":7,"params":{"textDocument":{"uri":"file:///x","version":3}}})");

  JSONTransport t(in_stream, out_stream);
  auto msg = t.readMessage();
  ASSERT_TRUE(msg.has_value());
  auto *p = msg->getAsObject()->getObject("params");
  ASSERT_NE(p, nullptr);
  auto *td = p->getObject("textDocument");
  ASSERT_NE(td, nullptr);
  EXPECT_EQ(td->getString("uri").value_or(""), "file:///x");
  EXPECT_EQ(td->getInteger("version").value_or(0), 3);
}

TEST(JSONTransportTest, MultipleMessagesInOneStream) {
  std::stringstream in_stream;
  std::stringstream out_stream;
  in_stream << frame(R"({"id":1})") << frame(R"({"id":2})");

  JSONTransport t(in_stream, out_stream);
  auto m1 = t.readMessage();
  ASSERT_TRUE(m1.has_value());
  EXPECT_EQ(m1->getAsObject()->getInteger("id").value_or(0), 1);
  auto m2 = t.readMessage();
  ASSERT_TRUE(m2.has_value());
  EXPECT_EQ(m2->getAsObject()->getInteger("id").value_or(0), 2);
}

TEST(JSONTransportTest, EOFReturnsNullopt) {
  std::stringstream in_stream;  // empty
  std::stringstream out_stream;

  JSONTransport t(in_stream, out_stream);
  auto msg = t.readMessage();
  EXPECT_FALSE(msg.has_value());
}

TEST(JSONTransportTest, MissingContentLengthRejected) {
  std::stringstream in_stream;
  std::stringstream out_stream;
  // Header section without Content-Length, then empty separator.
  in_stream << "Content-Type: application/json\r\n\r\n{}";

  JSONTransport t(in_stream, out_stream);
  auto msg = t.readMessage();
  EXPECT_FALSE(msg.has_value())
      << "missing Content-Length must be rejected per contract §7.4";
}

TEST(JSONTransportTest, MalformedJSONBodyRejected) {
  std::stringstream in_stream;
  std::stringstream out_stream;
  // 5-byte body that's not valid JSON.
  in_stream << "Content-Length: 5\r\n\r\n{abc:";

  JSONTransport t(in_stream, out_stream);
  auto msg = t.readMessage();
  EXPECT_FALSE(msg.has_value())
      << "malformed JSON body must be rejected per contract §7.4";
}

TEST(JSONTransportTest, WriteEmitsContentLengthFraming) {
  std::stringstream in_stream;
  std::stringstream out_stream;

  JSONTransport t(in_stream, out_stream);
  t.writeMessage(llvm::json::Object{{"id", 42}, {"result", true}});

  std::string out = out_stream.str();
  // Must start with "Content-Length:" and have the \r\n\r\n
  // separator.
  EXPECT_TRUE(out.find("Content-Length:") == 0)
      << "wire output must begin with Content-Length; got: " << out;
  EXPECT_NE(out.find("\r\n\r\n"), std::string::npos);
  // Body must be valid JSON containing our keys.
  auto sep = out.find("\r\n\r\n");
  std::string body = out.substr(sep + 4);
  auto parsed = llvm::json::parse(body);
  ASSERT_TRUE((bool)parsed);
  auto *o = parsed->getAsObject();
  ASSERT_NE(o, nullptr);
  EXPECT_EQ(o->getInteger("id").value_or(0), 42);
  EXPECT_EQ(o->getBoolean("result").value_or(false), true);
}

TEST(JSONTransportTest, WriteContentLengthMatchesBodySize) {
  std::stringstream in_stream;
  std::stringstream out_stream;

  JSONTransport t(in_stream, out_stream);
  t.writeMessage(llvm::json::Value("hello"));

  std::string out = out_stream.str();
  auto sep = out.find("\r\n\r\n");
  ASSERT_NE(sep, std::string::npos);
  // Parse the Content-Length value.
  auto cl_start = out.find("Content-Length: ");
  ASSERT_EQ(cl_start, 0u);
  auto cl_end = out.find("\r\n", cl_start);
  ASSERT_NE(cl_end, std::string::npos);
  std::string cl = out.substr(cl_start + 16, cl_end - cl_start - 16);
  size_t expected_n = std::stoul(cl);
  std::string body = out.substr(sep + 4);
  EXPECT_EQ(body.size(), expected_n)
      << "Content-Length header (" << expected_n
      << ") must equal body size (" << body.size() << ")";
}
