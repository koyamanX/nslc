// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/recovery_set_test/recovery_partial_ast_test.cpp
//
// TDD fixture (T053 of `specs/005-m2-parser/tasks.md`) for
// **partial-AST preservation under recovery** — the load-bearing
// invariant of M2 Phase 5 / US3 that lit fixtures cannot directly
// observe (per `nslc-emit-ast.contract.md` step 5: errors → no AST
// on stdout, so the surviving siblings live only in the in-memory
// AST).
//
// **Specification anchors**:
//   - spec.md US3 acceptance scenario 3: recovery preserves
//     `module_item`s after the error site — they "are still parsed
//     and appear in the AST under that `ModuleBlock`."
//   - parser-recovery.contract.md §"What recovery does NOT do":
//     "Recovery does NOT produce synthetic AST nodes for the
//     malformed construct. The malformed construct is simply absent
//     from the parent's vector; well-formed siblings remain."
//   - FR-006: parser produces a `CompilationUnit` whose `items`
//     vector contains one node per surviving top-level item in
//     declaration order. "Surviving" is the operative word — under
//     recovery the malformed item is dropped, but the well-formed
//     prefix and suffix remain.
//   - /speckit-clarify Q1 (2026-04-27): full multi-error recovery
//     at every grammar level.
//
// **Failing-state evidence (Principle VIII NON-NEGOTIABLE; FR-029)**:
// against parent commit `39cc618`, the parser does single-error
// bail — `parseModuleItem()` returns false on the first syntax
// error, `parseModuleBlock()` propagates `nullptr`, and
// `parseCompilationUnit()` typically returns a `CompilationUnit`
// whose `items` is missing the malformed `ModuleBlock` AS A WHOLE
// (not just the single malformed item). Therefore the assertions
// below — that the well-formed `reg/wire` siblings on EITHER side
// of the malformed `reg` survive inside the same `ModuleBlock` —
// FAIL. That observed failure IS the FR-029 evidence Track K
// commits ahead of Track L's implementation.

#include "nsl/AST/CompilationUnit.h"
#include "nsl/AST/Decl.h"
#include "nsl/AST/ModuleBlock.h"
#include "nsl/AST/NodeKind.h"
#include "nsl/AST/RegDecl.h"
#include "nsl/AST/WireDecl.h"
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

// US3 acceptance scenario 3 boiled down to an in-memory buffer.
// One `module` whose body has THREE `module_item`s in source order:
//   1. Well-formed `wire a;`        (before the error)
//   2. Malformed  `reg`             (no identifier — syntax error)
//   3. Well-formed `wire c;`        (after the error)
//
// The contract's promise: items 1 and 3 MUST survive in the parent
// `ModuleBlock`'s `internals()` vector under recovery. Item 2 is
// simply absent (no synthetic placeholder per the contract).
//
// We assert STRUCTURE on the surviving siblings only — recovery is
// what's under test, not the name-recognition pathway.
TEST(RecoveryPartialAST, ModuleItemRecoveryPreservesSiblings) {
  nsl::SourceManager sm;
  nsl::FileID const fid = sm.addBufferInMemory(
      "/virt/recovery-mid-module.nsl",
      bytesOf("module foo {\n"
              "  wire a;\n" // surviving sibling #1 (before error)
              "  reg\n"     // malformed — missing identifier
              "  wire c;\n" // surviving sibling #2 (after error)
              "}\n"));
  ASSERT_TRUE(fid.isValid());

  nsl::DiagnosticEngine diag(sm);
  nsl::Lexer lex(sm, fid, diag);

  std::unique_ptr<nsl::ast::CompilationUnit> cu =
      nsl::parse::parseCompilationUnit(lex, diag);

  // (a) Per the contract's "What recovery does NOT do" §, recovery
  // preserves the partial AST: the returned root MUST be non-null.
  ASSERT_NE(cu, nullptr) << "parseCompilationUnit MUST return a "
                            "non-null root under recovery — single-"
                            "error bail is forbidden by FR-021 / Q1.";

  // (b) Exactly one error-severity diagnostic emitted: the malformed
  //     `reg`. The well-formed wires raise nothing.
  EXPECT_EQ(diag.numErrors(), 1U)
      << "Expected exactly 1 error from the malformed `reg`. "
         "If >1, recovery cascaded; if 0, the parser silently "
         "swallowed the error (Principle IV violation).";

  // (c) The root's `items` vector contains exactly one top-level
  //     item — the `ModuleBlock foo`. The malformed item lives
  //     INSIDE the module, so the module itself survives.
  ASSERT_EQ(cu->items().size(), 1U);
  const nsl::ast::Decl &item = *cu->items()[0];
  ASSERT_EQ(item.kind(), nsl::ast::NodeKind::NK_ModuleBlock);
  const auto *mod = llvm::cast<nsl::ast::ModuleBlock>(&item);
  EXPECT_EQ(mod->name(), "foo");

  // (d) The load-bearing assertion: the module's `internals` vector
  //     contains the TWO well-formed wires that flanked the error.
  //     Per the contract, the malformed `reg` produces NO synthetic
  //     node — the vector is exactly size 2, not 3.
  ASSERT_EQ(mod->internals().size(), 2U)
      << "Recovery MUST preserve well-formed siblings on either "
         "side of the malformed `module_item` (parser-recovery."
         "contract.md §\"What recovery does NOT do\": malformed "
         "construct absent; well-formed siblings remain).";

  // (e) Verify declaration order is preserved (sibling #1 is the
  //     `wire a` before the error; sibling #2 is the `wire c`
  //     after).
  const nsl::ast::Decl &intern0 = *mod->internals()[0];
  ASSERT_EQ(intern0.kind(), nsl::ast::NodeKind::NK_WireDecl);
  const auto *wire0 = llvm::cast<nsl::ast::WireDecl>(&intern0);
  EXPECT_EQ(wire0->name(), "a");

  const nsl::ast::Decl &intern1 = *mod->internals()[1];
  ASSERT_EQ(intern1.kind(), nsl::ast::NodeKind::NK_WireDecl);
  const auto *wire1 = llvm::cast<nsl::ast::WireDecl>(&intern1);
  EXPECT_EQ(wire1->name(), "c");
}

// Multi-error variant: two independent syntax errors in separate
// `module_item`s. Per `parser-recovery.contract.md` §"Multi-error
// fixture corpus minimum" item 2 + the FR-021 minimum, both
// diagnostics MUST emit, and the well-formed siblings between/after
// the errors MUST survive.
//
// Source layout (5 module_items):
//   1. `wire a;`      well-formed
//   2. `reg`          malformed (missing identifier)
//   3. `wire b;`      well-formed
//   4. `func g`       malformed (no `(` / no body)
//   5. `wire c;`      well-formed
//
// Expected: `internals()` size 3 (the three wires); `numErrors() == 2`;
// no synthetic placeholder for the dropped `reg` or `func`.
TEST(RecoveryPartialAST, TwoErrorsInModuleBodyPreserveSiblings) {
  nsl::SourceManager sm;
  nsl::FileID const fid = sm.addBufferInMemory("/virt/recovery-two-errors.nsl",
                                               bytesOf("module foo {\n"
                                                       "  wire a;\n"
                                                       "  reg\n"
                                                       "  wire b;\n"
                                                       "  func g\n"
                                                       "  wire c;\n"
                                                       "}\n"));
  ASSERT_TRUE(fid.isValid());

  nsl::DiagnosticEngine diag(sm);
  nsl::Lexer lex(sm, fid, diag);

  std::unique_ptr<nsl::ast::CompilationUnit> cu =
      nsl::parse::parseCompilationUnit(lex, diag);

  ASSERT_NE(cu, nullptr);

  // BOTH errors must emit (Q1 — full recovery, no fail-fast). If the
  // count is 1, recovery aborted after the first; if 0, errors were
  // swallowed; if >2, recovery cascaded.
  EXPECT_EQ(diag.numErrors(), 2U)
      << "FR-021 multi-error contract: BOTH `reg`-with-no-ident and "
         "`func g`-with-no-body must emit diagnostics in a single run.";

  ASSERT_EQ(cu->items().size(), 1U);
  const auto *mod = llvm::cast<nsl::ast::ModuleBlock>(cu->items()[0].get());

  // Three surviving wires; no synthetic placeholders.
  ASSERT_EQ(mod->internals().size(), 3U)
      << "Three well-formed `wire` declarations flank the two "
         "malformed items; ALL three must survive in `internals()` "
         "under full recovery.";

  // Order is `a`, `b`, `c` (declaration-order preservation, FR-006).
  const auto *w0 = llvm::cast<nsl::ast::WireDecl>(mod->internals()[0].get());
  const auto *w1 = llvm::cast<nsl::ast::WireDecl>(mod->internals()[1].get());
  const auto *w2 = llvm::cast<nsl::ast::WireDecl>(mod->internals()[2].get());
  EXPECT_EQ(w0->name(), "a");
  EXPECT_EQ(w1->name(), "b");
  EXPECT_EQ(w2->name(), "c");
}

} // namespace
