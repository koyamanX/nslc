// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Sema/TypeSystem.cpp — out-of-line definitions for the `Type`
// hierarchy and the `TypeSystem` interner declared in
// `include/nsl/Sema/TypeSystem.h`.
//
// `FieldInfo` is defined directly in `TypeSystem.h` (not in
// `SymbolTable.h`) so this TU does not need a SymbolTable.h
// include — TypeSystem is foundational to SymbolTable, not the
// other way around.

#include "nsl/Sema/TypeSystem.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/ADT/StringRef.h"

#include <cassert>
#include <cstdint>
#include <memory>
#include <utility>

namespace llvm {

// DenseMapInfo specialization for `(uint64_t, const Type*)` —
// needed for the memory cache. Uses llvm::hash_combine over the
// two scalar fields; both `0xFFFF…FE` and `0xFFFF…FF` are reserved
// as empty/tombstone keys.
template <>
struct DenseMapInfo<std::pair<uint64_t, const nsl::sema::Type *>> {
  using Key = std::pair<uint64_t, const nsl::sema::Type *>;
  static inline Key getEmptyKey() {
    return {~uint64_t{0}, nullptr};
  }
  static inline Key getTombstoneKey() {
    return {~uint64_t{0} - 1, nullptr};
  }
  static unsigned getHashValue(const Key &k) {
    return static_cast<unsigned>(
        llvm::hash_combine(k.first, k.second));
  }
  static bool isEqual(const Key &a, const Key &b) {
    return a.first == b.first && a.second == b.second;
  }
};

} // namespace llvm

namespace nsl::sema {

// -----------------------------------------------------------------
// Out-of-line virtual destructors (vtable anchors)
// -----------------------------------------------------------------

Type::~Type() = default;

// -----------------------------------------------------------------
// StructType out-of-line members (FieldInfo full definition only
// available in this TU since the header forward-declared it)
// -----------------------------------------------------------------

StructType::StructType(ast::Identifier n, std::vector<FieldInfo> fields,
                       uint64_t totalWidth) noexcept
    : Type(TypeKind::Struct), name_(n), fields_(std::move(fields)),
      totalWidth_(totalWidth) {}

llvm::ArrayRef<FieldInfo> StructType::fields() const noexcept {
  return fields_;
}

// -----------------------------------------------------------------
// TypeSystem::Impl (private state)
// -----------------------------------------------------------------

struct TypeSystem::Impl {
  /// Singleton 1-bit type. Allocated once at `TypeSystem`
  /// construction and never replaced.
  std::unique_ptr<BitType> bitSingleton;

  /// Singleton "unresolved" sentinel. Allocated once.
  std::unique_ptr<UnresolvedType> unresolvedSingleton;

  /// `bit[N]` cache keyed by width (per design §6.x line 849).
  llvm::DenseMap<uint64_t, std::unique_ptr<BitVectorType>> bvCache;

  /// `struct` cache keyed by name. Per research §3, struct names
  /// are unique per compilation unit (the `ResolutionPass` enforces
  /// this by declaring each `StructTypeSymbol` in the global scope),
  /// so the name alone is the cache key.
  llvm::DenseMap<llvm::StringRef, std::unique_ptr<StructType>>
      structCache;

  /// `mem` cache keyed by `(depth, element)`.
  llvm::DenseMap<std::pair<uint64_t, TypeRef>,
                 std::unique_ptr<MemoryType>>
      memCache;

  Impl()
      : bitSingleton(std::make_unique<BitType>()),
        unresolvedSingleton(std::make_unique<UnresolvedType>()) {}
};

// -----------------------------------------------------------------
// TypeSystem
// -----------------------------------------------------------------

TypeSystem::TypeSystem() : impl_(std::make_unique<Impl>()) {}
TypeSystem::~TypeSystem() = default;

TypeRef TypeSystem::bit() const noexcept {
  return impl_->bitSingleton.get();
}

TypeRef TypeSystem::unresolved() const noexcept {
  return impl_->unresolvedSingleton.get();
}

TypeRef TypeSystem::bitVector(uint64_t width) {
  assert(width >= 1 && "BitVectorType width must be >= 1");
  auto it = impl_->bvCache.find(width);
  if (it != impl_->bvCache.end()) {
    return it->second.get();
  }
  auto fresh = std::make_unique<BitVectorType>(width);
  TypeRef raw = fresh.get();
  impl_->bvCache.insert({width, std::move(fresh)});
  return raw;
}

TypeRef TypeSystem::structType(ast::Identifier name,
                               std::vector<FieldInfo> fields,
                               uint64_t totalWidth) {
  auto it = impl_->structCache.find(name);
  if (it != impl_->structCache.end()) {
    // Re-interning by name — the first registration's fields are
    // authoritative (per research §3).
    return it->second.get();
  }
  auto fresh =
      std::make_unique<StructType>(name, std::move(fields), totalWidth);
  TypeRef raw = fresh.get();
  impl_->structCache.insert({name, std::move(fresh)});
  return raw;
}

TypeRef TypeSystem::memory(uint64_t depth, TypeRef element) {
  std::pair<uint64_t, TypeRef> const key{depth, element};
  auto it = impl_->memCache.find(key);
  if (it != impl_->memCache.end()) {
    return it->second.get();
  }
  auto fresh = std::make_unique<MemoryType>(depth, element);
  TypeRef raw = fresh.get();
  impl_->memCache.insert({key, std::move(fresh)});
  return raw;
}

} // namespace nsl::sema
