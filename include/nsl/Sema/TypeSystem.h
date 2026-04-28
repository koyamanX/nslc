// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/Sema/TypeSystem.h — `Type` hierarchy + interner.
//
// Public surface (per `contracts/sema-api.contract.md` Invariant 1):
// this header is one of three permitted public umbrella headers under
// `include/nsl/Sema/` (Constitution Principle II §3 amended at
// v1.6.0). The other two are `Sema.h` and `SymbolTable.h`.
//
// Per the same constitutional amendment, every concrete `Type`
// subclass nests in this header — NOT in a separate per-kind header.
//
// Mirrors `docs/design/nsl_compiler_design.md` §6.x lines 797–856
// verbatim per Constitution Principle VII (spec/design coupling).

#ifndef NSL_SEMA_TYPE_SYSTEM_H
#define NSL_SEMA_TYPE_SYSTEM_H

#include "nsl/AST/ASTNode.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace nsl::sema {

/// Per-field record inside a `StructType` (and the parallel
/// `StructTypeSymbol` in `SymbolTable.h`). Stored in MSB-first
/// order per `S18`; `offset` is the bit position from MSB so
/// `fields[0].offset == totalWidth - fields[0].width` (research §6).
///
/// Defined here (`TypeSystem.h`) — not in `SymbolTable.h` — to
/// avoid a circular include: `SymbolTable.h` already includes
/// `TypeSystem.h` for `TypeRef`, and `StructType`'s private
/// `std::vector<FieldInfo> fields_` member needs the full type at
/// the point of the class declaration. Both `StructType` and
/// `StructTypeSymbol` consume the same record.
struct FieldInfo {
  ast::Identifier name;
  uint64_t        width;
  uint64_t        offset; ///< bit position from MSB (per S18)
};

/// Closed set of concrete `Type` kinds (data-model §3.2).
enum class TypeKind : uint8_t {
  Bit,         ///< Singleton 1-bit type (`BitType`)
  BitVector,   ///< `BitVectorType{N}` for N ≥ 1
  Struct,      ///< `StructType` — interned by name
  Memory,      ///< `MemoryType` — interned by `(depth, element)`
  Unresolved,  ///< Singleton sentinel for FR-017 no-cascade
};

// -----------------------------------------------------------------
// Type — abstract base
// -----------------------------------------------------------------

/// Abstract base of every concrete `Type` kind. Every `Type`
/// instance is owned by a `TypeSystem` and shared via interning;
/// pointer equality implies type equality (`sema-api.contract.md`
/// Invariant 5; design §6.x line 846).
///
/// Lifetime: `Type`s are never copied or moved; they live for the
/// lifetime of the surrounding `TypeSystem` (which is in turn
/// owned by `SemaResult::types`).
class Type {
public:
  Type() = delete;

  /// Out-of-line virtual destructor (anchored in `TypeSystem.cpp`).
  virtual ~Type();

  Type(const Type &)            = delete;
  Type &operator=(const Type &) = delete;
  Type(Type &&)                 = delete;
  Type &operator=(Type &&)      = delete;

  /// Runtime kind discriminator (closed set; one per `TypeKind`
  /// enumerator).
  [[nodiscard]] TypeKind kind() const noexcept { return kind_; }

protected:
  explicit Type(TypeKind k) noexcept : kind_(k) {}

private:
  TypeKind kind_;
};

/// Non-owning handle to an interned `Type`. Per design §6.x line
/// 838: `using TypeRef = const Type*`. Pointer equality implies
/// type equality (Invariant 5).
///
/// Sentinel meanings:
///   - `nullptr`: "no type yet" (M2 leaves `Expr::inferredType_ ==
///     nullptr`); after `Sema::run()`, every `Expr::inferredType()`
///     is either non-null or `TypeSystem::unresolved()`.
///   - `TypeSystem::unresolved()`: poison sentinel for FR-017
///     no-cascade — an unresolved subtree's downstream consumers
///     skip silently.
using TypeRef = const Type *;

/// Singleton 1-bit type. Returned by `TypeSystem::bit()`.
class BitType final : public Type {
public:
  BitType() noexcept : Type(TypeKind::Bit) {}

  static constexpr TypeKind kKind = TypeKind::Bit;
  static bool classof(const Type *t) noexcept { return t->kind() == kKind; }
};

/// `bit[N]` type — interned per width. Width is the number of
/// bits (`width == 1` is *not* the same TypeRef as `BitType` per
/// design §6.x lines 813–818; the tooling uses `BitType` for
/// "control-shaped" 1-bit values and `BitVectorType{1}` for
/// "value-shaped" 1-bit registers, e.g., `S23` reg-omitted-width).
class BitVectorType final : public Type {
public:
  explicit BitVectorType(uint64_t w) noexcept
      : Type(TypeKind::BitVector), width_(w) {}

  /// Bit-width — always ≥ 1.
  [[nodiscard]] uint64_t width() const noexcept { return width_; }

  static constexpr TypeKind kKind = TypeKind::BitVector;
  static bool classof(const Type *t) noexcept { return t->kind() == kKind; }

private:
  uint64_t width_;
};

/// `struct` type — interned by name. Field information (MSB-first
/// order; `FieldInfo` defined in `SymbolTable.h`) is supplied at
/// interning time and stored on the `Type` instance for use by
/// downstream layers (M4+ dialect / lowering).
class StructType final : public Type {
public:
  StructType(ast::Identifier n, std::vector<FieldInfo> fields,
             uint64_t totalWidth) noexcept;

  /// User-source name (e.g., `header_t`).
  [[nodiscard]] ast::Identifier name() const noexcept { return name_; }

  /// MSB-first ordered field records (per `S18`).
  [[nodiscard]] llvm::ArrayRef<FieldInfo> fields() const noexcept;

  /// Total bit-width of the packed struct.
  [[nodiscard]] uint64_t totalWidth() const noexcept { return totalWidth_; }

  static constexpr TypeKind kKind = TypeKind::Struct;
  static bool classof(const Type *t) noexcept { return t->kind() == kKind; }

  // The fields vector is opaque to the public header (FieldInfo is
  // a forward declaration here); definitions live in TypeSystem.cpp
  // via the SymbolTable.h include there.
private:
  ast::Identifier        name_;
  std::vector<FieldInfo> fields_;
  uint64_t               totalWidth_;
};

/// `mem` type — interned by `(depth, element)`. Per design §6.x
/// lines 829–836.
class MemoryType final : public Type {
public:
  MemoryType(uint64_t depth, TypeRef element) noexcept
      : Type(TypeKind::Memory), depth_(depth), element_(element) {}

  /// Number of words.
  [[nodiscard]] uint64_t depth() const noexcept { return depth_; }

  /// Per-word element type — typically a `BitVectorType` but may
  /// be a `StructType` for memories of structs.
  [[nodiscard]] TypeRef element() const noexcept { return element_; }

  static constexpr TypeKind kKind = TypeKind::Memory;
  static bool classof(const Type *t) noexcept { return t->kind() == kKind; }

private:
  uint64_t depth_;
  TypeRef  element_;
};

/// Singleton "unresolved" sentinel — used as the `inferredType()` of
/// any subtree containing an unresolved name (FR-017 no-cascade).
/// Returned by `TypeSystem::unresolved()`. Two identifiers under the
/// same unresolved root produce only one diagnostic because the
/// downstream `Sn` walkers see this sentinel and skip.
class UnresolvedType final : public Type {
public:
  UnresolvedType() noexcept : Type(TypeKind::Unresolved) {}

  static constexpr TypeKind kKind = TypeKind::Unresolved;
  static bool classof(const Type *t) noexcept { return t->kind() == kKind; }
};

// -----------------------------------------------------------------
// TypeSystem — interner
// -----------------------------------------------------------------

/// Owns and interns every `Type` instance produced during a Sema
/// run. After `Sema::run()` returns, ownership is moved into
/// `SemaResult::types` (`sema-api.contract.md` Invariant 6).
///
/// Interning contract (Invariant 5; design §6.x line 799):
///   For any two `TypeRef a, b` returned from the same
///   `TypeSystem` instance, `a == b ⟺ structurally_equal(a, b)`.
class TypeSystem {
public:
  TypeSystem();
  ~TypeSystem();

  TypeSystem(const TypeSystem &)            = delete;
  TypeSystem &operator=(const TypeSystem &) = delete;
  TypeSystem(TypeSystem &&)                 = delete;
  TypeSystem &operator=(TypeSystem &&)      = delete;

  /// Singleton 1-bit type. Always returns the same `TypeRef` for
  /// the lifetime of `*this`.
  [[nodiscard]] TypeRef bit() const noexcept;

  /// Singleton "unresolved" sentinel (per FR-017 no-cascade).
  /// Always returns the same `TypeRef` for the lifetime of `*this`.
  [[nodiscard]] TypeRef unresolved() const noexcept;

  /// Interned `bit[N]` type. `bitVector(N)` returns the same
  /// `TypeRef` across every invocation with the same `N` for the
  /// lifetime of `*this` (Invariant 3).
  [[nodiscard]] TypeRef bitVector(uint64_t width);

  /// Interned struct type. `name` is the unique key (per research
  /// §3: struct names are unique per compilation unit; the
  /// `ResolutionPass` enforces this by declaring each
  /// `StructTypeSymbol` in the global scope). `fields` is taken in
  /// MSB-first order; `totalWidth` is the sum of field widths.
  [[nodiscard]] TypeRef structType(ast::Identifier name,
                                   std::vector<FieldInfo> fields,
                                   uint64_t totalWidth);

  /// Interned memory type. Key is `(depth, element)`.
  [[nodiscard]] TypeRef memory(uint64_t depth, TypeRef element);

  /// Type equality — exactly pointer equality per Invariant 5.
  /// Inline so the compiler can fold to a single `cmp` instruction
  /// at every consumer site.
  [[nodiscard]] bool equal(TypeRef a, TypeRef b) const noexcept {
    return a == b;
  }

private:
  // Implementation detail (private to make the contract enforceable
  // without leaking the cache layout into the header). Definitions
  // in `lib/Sema/TypeSystem.cpp`.
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace nsl::sema

#endif // NSL_SEMA_TYPE_SYSTEM_H
