// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test_unit/type_system_test/interning_test.cpp
//
// TDD fixtures (M3 Phase 2, T008) for `nsl::sema::TypeSystem`.
//
// **Specification anchors**:
//   - data-model §3 (Type hierarchy + interner): `bit()` and
//     `unresolved()` are singletons; `bitVector(N)` interns by
//     width; `structType(name, fields)` interns by name; `memory(d,
//     elt)` interns by `(depth, element)`.
//   - `sema-api.contract.md` Invariant 5: `equal(a, b) == (a == b)`.
//   - `sema-stability.contract.md` Invariant 3: pointer equality
//     implies type equality.
//
// **TDD evidence (Principle VIII NON-NEGOTIABLE)**: this file is
// authored before `lib/Sema/TypeSystem.cpp` lands. Against the
// unchanged tree the translation unit FAILS TO LINK because
// `nsl::sema::TypeSystem` ctor / `bit` / `unresolved` / `bitVector`
// / `structType` / `memory` are unresolved symbols (lib/Sema only
// ships the M0 anchor TU). The expected red→green observation is
// encoded in the assertions below.

#include "nsl/Sema/SymbolTable.h"
#include "nsl/Sema/TypeSystem.h"

#include <gtest/gtest.h>

namespace {

using nsl::ast::Identifier;
using nsl::sema::FieldInfo;
using nsl::sema::TypeRef;
using nsl::sema::TypeSystem;

// ---------------------------------------------------------------
// (a) bitVector(8) returns the same TypeRef across two calls
//     (Invariant 3 of `sema-stability.contract.md`).
// ---------------------------------------------------------------

TEST(TypeSystemInterningTest, BitVectorSameWidthIsSamePointer) {
  TypeSystem ts;
  TypeRef a = ts.bitVector(8);
  TypeRef b = ts.bitVector(8);
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(a, b);
  EXPECT_TRUE(ts.equal(a, b));
}

// ---------------------------------------------------------------
// (b) bitVector(8) != bitVector(16)
// ---------------------------------------------------------------

TEST(TypeSystemInterningTest, BitVectorDifferentWidthIsDifferentPointer) {
  TypeSystem ts;
  TypeRef bv8 = ts.bitVector(8);
  TypeRef bv16 = ts.bitVector(16);
  EXPECT_NE(bv8, bv16);
  EXPECT_FALSE(ts.equal(bv8, bv16));
}

// ---------------------------------------------------------------
// (c) bit() is a singleton
// ---------------------------------------------------------------

TEST(TypeSystemInterningTest, BitSingleton) {
  TypeSystem ts;
  TypeRef a = ts.bit();
  TypeRef b = ts.bit();
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a, b);
  // The singleton 1-bit `BitType` is NOT the same as
  // `BitVectorType{1}` (per design §6.x lines 813–818).
  EXPECT_NE(a, ts.bitVector(1));
}

// ---------------------------------------------------------------
// (d) unresolved() is a singleton
// ---------------------------------------------------------------

TEST(TypeSystemInterningTest, UnresolvedSingleton) {
  TypeSystem ts;
  TypeRef a = ts.unresolved();
  TypeRef b = ts.unresolved();
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a, b);
  // Unresolved is distinct from both Bit and BitVector(1).
  EXPECT_NE(a, ts.bit());
  EXPECT_NE(a, ts.bitVector(1));
}

// ---------------------------------------------------------------
// (e) equal(a, b) is exactly a == b (Invariant 5)
// ---------------------------------------------------------------

TEST(TypeSystemInterningTest, EqualIsPointerEquality) {
  TypeSystem ts;
  TypeRef a = ts.bitVector(32);
  TypeRef b = ts.bitVector(32);
  TypeRef c = ts.bitVector(64);
  EXPECT_EQ(ts.equal(a, b), a == b);
  EXPECT_EQ(ts.equal(a, c), a == c);
  EXPECT_TRUE(ts.equal(a, b));
  EXPECT_FALSE(ts.equal(a, c));
}

// ---------------------------------------------------------------
// (f) structType(name, fields) interns by name (research §3:
//     struct names are unique per compilation unit)
// ---------------------------------------------------------------

TEST(TypeSystemInterningTest, StructTypeInternedByName) {
  TypeSystem ts;
  std::vector<FieldInfo> fields_a;
  fields_a.push_back({Identifier("hi"), 8U, 8U});
  fields_a.push_back({Identifier("lo"), 8U, 0U});

  TypeRef a = ts.structType(Identifier("packet_t"), fields_a, 16U);
  ASSERT_NE(a, nullptr);

  // Second call with the same name returns the same TypeRef (the
  // first-registered fields/totalWidth are authoritative).
  std::vector<FieldInfo> fields_b;
  fields_b.push_back({Identifier("hi"), 8U, 8U});
  fields_b.push_back({Identifier("lo"), 8U, 0U});
  TypeRef b = ts.structType(Identifier("packet_t"), fields_b, 16U);
  EXPECT_EQ(a, b);

  // Different name is a different TypeRef.
  TypeRef c = ts.structType(Identifier("other_t"), fields_a, 16U);
  EXPECT_NE(a, c);
}

// ---------------------------------------------------------------
// (g) memory(depth, element) interns by (depth, TypeRef)
// ---------------------------------------------------------------

TEST(TypeSystemInterningTest, MemoryInternedByDepthAndElement) {
  TypeSystem ts;
  TypeRef bv8 = ts.bitVector(8);
  TypeRef bv16 = ts.bitVector(16);

  TypeRef m1 = ts.memory(1024, bv8);
  TypeRef m2 = ts.memory(1024, bv8);
  EXPECT_EQ(m1, m2);

  TypeRef m3 = ts.memory(2048, bv8);
  EXPECT_NE(m1, m3);

  TypeRef m4 = ts.memory(1024, bv16);
  EXPECT_NE(m1, m4);
}

} // namespace
