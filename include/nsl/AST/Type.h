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

namespace nsl::ast {

/// Forward declaration. M3 (Sema) supplies the definition.
class Type;

/// Non-owning reference to an `ast::Type`. nullptr = unresolved.
///
/// Why a typedef rather than a wrapper class: keeping the slot a
/// raw `const Type*` avoids forcing every per-node-kind header to
/// include a heavier "TypeRef" class definition; M2 only stores
/// nullptr, and M3 will revisit if a richer wrapper (e.g., qualified
/// types) is needed.
using TypeRef = const Type *;

} // namespace nsl::ast

#endif // NSL_AST_TYPE_H
