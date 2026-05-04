// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/parse_test/parser_smoke_test.cpp
//
// TDD fixture (T021 of `specs/005-m2-parser/tasks.md`) for the
// `nsl-parse` library's public entry point
// `nsl::parse::parseCompilationUnit(lex::Lexer&, basic::DiagnosticEngine&)`.
//
// **Specification anchors**:
//   - FR-002 / FR-006: `nsl-parse` MUST produce a `CompilationUnit`
//     AST whose `items` vector contains one node per surviving
//     top-level item in declaration order.
//   - FR-018: every AST node carries a `SourceRange`.
//   - `data-model.md §2`: the parser's public surface is
//
//         namespace nsl::parse {
//           std::unique_ptr<ast::CompilationUnit>
//           parseCompilationUnit(Lexer& lex, DiagnosticEngine& diag);
//         }
//
//     (`Lexer` and `DiagnosticEngine` live in `namespace nsl` per the
//     M1 lexer / basic libraries — confirmed against
//     `include/nsl/Lex/Lexer.h` and `include/nsl/Basic/Diagnostic.h`.)
//
// **Failing-state evidence (Principle VIII NON-NEGOTIABLE; FR-029)**:
// against the unchanged tree at `7bc1aea`,
//   `include/nsl/Parse/Parser.h` is empty (Track D has not authored
//   it yet) and the `nsl-parse` library has no `Parser.cpp` /
//   `ParseDecl.cpp` / `ParseStmt.cpp` / `ParseExpr.cpp` sources, so
//   `lib/Parse/CMakeLists.txt` produces no library target. Linking
//   `parse_test` fails with an unresolved-symbol error for
//   `nsl::parse::parseCompilationUnit`. That link failure IS the
//   failing-state evidence required by FR-029.
//
// Once Track D lands the parser, the assertions below should pass
// without further fixture edits — this is the smoke-level
// well-formed-input check parallel to M1's lexer smoke test.

#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/Decl.h"
#include "nsl/AST/ModuleBlock.h"
#include "nsl/AST/NodeKind.h"
#include "nsl/AST/RegDecl.h"
#include "nsl/Basic/Diagnostic.h"
#include "nsl/Basic/SourceManager.h"
#include "nsl/Lex/Lexer.h"
#include "nsl/Parse/Parser.h"

#include "llvm/Support/Casting.h"

#include "gtest/gtest.h"
#include <memory>
#include <vector>

namespace {

std::vector<char> bytesOf(const char *literal) {
  std::vector<char> out;
  while (*literal != 0) {
    out.push_back(*literal++);
  }
  return out;
}

// US1 acceptance scenario 2 (`spec.md` §"User Story 1") boiled down
// to a single in-memory buffer: a `module` containing a single
// `reg` declaration. The parser MUST produce a `CompilationUnit`
// whose only item is a `ModuleBlock` whose only `internals` entry
// is a `RegDecl` whose `name` is `q`.
//
// Source layout:
//   line 1, cols 1-15: "module hello {"
//   line 2, cols 1-15: "  reg q[8] = 0;"
//   line 3, col  1:    "}"
//
// The smoke test asserts STRUCTURE, not byte-exact `SourceRange`
// coordinates — those are exercised by the printer's determinism
// gate in `test_unit/ast_printer_test/`.
TEST(ParserSmokeTest, ParsesModuleWithSingleRegDecl) {
  nsl::SourceManager sm;
  nsl::FileID const fid = sm.addBufferInMemory(
      "/virt/smoke.nsl", bytesOf("module hello {\n  reg q[8] = 0;\n}\n"));
  ASSERT_TRUE(fid.isValid());

  nsl::DiagnosticEngine diag(sm);
  // Construct the lexer on the in-memory buffer per the M1
  // lexer's pull-model interface.
  nsl::Lexer lex(sm, fid, diag);

  std::unique_ptr<nsl::ast::CompilationUnit> cu =
      nsl::parse::parseCompilationUnit(lex, diag);

  // The parser MUST return a non-null root for well-formed input
  // (FR-006 — parser only returns null if recovery exhausts to EOF
  // on a corrupted token stream, which this input does not exhibit).
  ASSERT_NE(cu, nullptr);

  // No error-severity diagnostics on a well-formed input
  // (`spec.md §"Acceptance Scenarios"` US1 #6 — well-formed-input
  // path only).
  EXPECT_FALSE(diag.hasError());

  // Exactly one top-level item: the `ModuleBlock`.
  ASSERT_EQ(cu->items().size(), 1U);

  // The first item is a `ModuleBlock` (NodeKind::NK_ModuleBlock).
  const nsl::ast::Decl &item = *cu->items()[0];
  ASSERT_EQ(item.kind(), nsl::ast::NodeKind::NK_ModuleBlock);

  const auto *mod = llvm::cast<nsl::ast::ModuleBlock>(&item);
  EXPECT_EQ(mod->name(), "hello");

  // The module's body has exactly one internal declaration:
  // the `reg q[8] = 0;`. Per `data-model.md §1.4`, internals live
  // under `ModuleBlock::internals()`.
  ASSERT_EQ(mod->internals().size(), 1U);
  const nsl::ast::Decl &intern0 = *mod->internals()[0];
  ASSERT_EQ(intern0.kind(), nsl::ast::NodeKind::NK_RegDecl);

  const auto *reg = llvm::cast<nsl::ast::RegDecl>(&intern0);
  EXPECT_EQ(reg->name(), "q");
  // Width is `[8]`, init is `= 0` — both expressions are non-null.
  EXPECT_NE(reg->width(), nullptr);
  EXPECT_NE(reg->init(), nullptr);
}

// Edge case from `spec.md §"Edge Cases"`: an empty compilation
// unit (zero top-level items) MUST produce a `CompilationUnit`
// with `items` empty and exit successfully. Asserts FR-006 boundary
// behavior — the parser doesn't crash on an empty buffer.
TEST(ParserSmokeTest, ParsesEmptyCompilationUnit) {
  nsl::SourceManager sm;
  nsl::FileID const fid = sm.addBufferInMemory("/virt/empty.nsl", bytesOf(""));
  ASSERT_TRUE(fid.isValid());

  nsl::DiagnosticEngine diag(sm);
  nsl::Lexer lex(sm, fid, diag);

  std::unique_ptr<nsl::ast::CompilationUnit> cu =
      nsl::parse::parseCompilationUnit(lex, diag);

  ASSERT_NE(cu, nullptr);
  EXPECT_FALSE(diag.hasError());
  EXPECT_EQ(cu->items().size(), 0U);
}

} // namespace
