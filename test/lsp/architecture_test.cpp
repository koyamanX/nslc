// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test/lsp/architecture_test.cpp — Phase 6 / US4 architectural
// verification. Per
// `specs/010-t3-lsp-skeleton/spec.md` §US4 + SC-005 +
// `specs/010-t3-lsp-skeleton/contracts/lsp-protocol.contract.md`.
//
// These tests are structural — they invoke the audit shell script
// from CMake at test time. The script (`scripts/lsp_link_audit.sh`)
// performs source-tree grep + public-header count and exits 0
// when both pass.

#include <cstdlib>
#include <gtest/gtest.h>

#ifndef NSL_LSP_LINK_AUDIT_SCRIPT
#error "NSL_LSP_LINK_AUDIT_SCRIPT must be defined by CMake"
#endif

TEST(ArchitectureSuite, LinkAudit_NoFrontendDuplication) {
  // SC-005 + Principle II §3: lib/LSP/ does not redefine any
  // frontend class. Public header surface is exactly Server.h.
  int rc = std::system(NSL_LSP_LINK_AUDIT_SCRIPT);
  EXPECT_EQ(rc, 0)
      << "lsp_link_audit failed; see stderr for the violation list";
}
