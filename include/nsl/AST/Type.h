// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/Type.h
//
// Forward-declared `TypeRef` for the `Expr::inferredType_` slot
// (data-model §1.1). At M2 the slot exists but is always nullptr —
// M3 (Sema) introduces concrete `Type` subclasses and fills the
// slot during name-resolution + width-inference. Per FR-004:
// "`Expr` MUST carry a `TypeRef inferredType()` slot writable by
// Sema at M3."
//
// `TypeRef` is a non-owning pointer to a `Type` instance allocated
// elsewhere. The "unresolved" sentinel is `nullptr`. M2 never
// dereferences it — only writes nullptr on construction and reads
// the pointer-equality test in the printer (Invariant 7: future
// additive `(type=...)` fields).

#ifndef NSL_AST_TYPE_H
#define NSL_AST_TYPE_H

namespace nsl::sema {
/// Forward declaration of the M3 Sema type hierarchy. Defined in
/// `include/nsl/Sema/TypeSystem.h`.
class Type;
} // namespace nsl::sema

namespace nsl::ast {

/// Type alias bridging `nsl::ast::Type` to the M3 `nsl::sema::Type`.
/// At M2 this aliased nothing concrete (the M2 spec used `Type` only
/// as a forward-declared placeholder); M3 wires the alias to the
/// `nsl::sema::Type` hierarchy so `Expr::setInferredType()` can store
/// a `sema::TypeRef` without a cast.
///
/// This is a forward-declaration alias — including this header still
/// does NOT pull in the full `TypeSystem.h`. Concrete `Type`
/// definitions live in `include/nsl/Sema/TypeSystem.h`.
using Type = ::nsl::sema::Type;

/// Non-owning reference to an `ast::Type` (a.k.a. `sema::Type`).
/// nullptr = unresolved. Pointer equality implies type equality
/// (per `sema-api.contract.md` Invariant 5).
using TypeRef = const Type *;

} // namespace nsl::ast

#endif // NSL_AST_TYPE_H
