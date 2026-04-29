# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# test_unit/constructive_sn_test/CMakeLists.txt — gtest suite for the
# 6 constructive `Sn` introspection observables (per Clarifications
# session 2026-04-28 Q1 → Option B; `sema-api.contract.md` Invariant 4;
# `research.md` §6).
#
# Per-`Sn` files:
#   - s13_test.cc — `AltBlock::cases()` priority order vs
#                   `AnyBlock::cases()` declaration order
#   - s18_test.cc — `StructTypeSymbol::fields()` MSB-first
#   - s19_test.cc — `SeqBlock::clockBudget()` (M3 stub)
#   - s23_test.cc — `RegSymbol::type()` == `BitVectorType{1}` for
#                   width-omitted-with-init
#   - s24_test.cc — `MemSymbol::initValues()` zero-padded to depth
#   - s27_test.cc — `Sema::classifyIdentifierExpr()` returns
#                   `ControlTerminalTap` for control-shaped symbols
#
# Authored at Phase 4a (TDD red phase per Principle VIII): the
# fixtures FAIL because the Phase 4b checkers (T065-T070) haven't
# populated the introspection state yet. Phase 4b lands implementations
# that turn the assertions green.

include(GoogleTest)

add_executable(constructive_sn_test
  s13_test.cc
  s18_test.cc
  s19_test.cc
  s23_test.cc
  s24_test.cc
  s27_test.cc)

target_link_libraries(constructive_sn_test
  PRIVATE
    nsl-basic
    nsl-ast
    nsl-sema
    GTest::gtest
    GTest::gtest_main)

gtest_discover_tests(constructive_sn_test
  PROPERTIES TIMEOUT 30)
