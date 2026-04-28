// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// include/nsl/AST/NodeKind.h
//
// `NodeKind` — the closed enumeration of every concrete AST node
// kind in `nsl-ast`. The enum body is generated from the X-macro
// source-of-truth `NodeKind.def` (per
// `specs/005-m2-parser/research.md` §6).
//
// Consumers of the enum:
//   - `ASTNode` (in `ASTNode.h`) stores a `NodeKind` for runtime
//     dispatch (`kind()` accessor; printer enum-to-string lookup).
//   - `ASTVisitor` (in `ASTVisitor.h`) uses the same `.def` to
//     declare one pure-virtual `visit(T&)` per kind.
//   - `lib/AST/NodeKindNames.cpp` includes the `.def` to build the
//     deterministic enum-to-string table.

#ifndef NSL_AST_NODEKIND_H
#define NSL_AST_NODEKIND_H

#include "llvm/ADT/StringRef.h"

#include <cstdint>

namespace nsl::ast {

/// The closed set of concrete AST node kinds.
///
/// Enumerator names are `NK_<EnumName>` where `<EnumName>` matches
/// the C++ class. Source order of `NodeKind.def` IS the enum order
/// (Principle V determinism).
enum class NodeKind : uint8_t {
#define NSL_NODE_KIND(EnumName, BaseClass) NK_##EnumName,
#include "nsl/AST/NodeKind.def"
#undef NSL_NODE_KIND

  /// Sentinel for fixed-size tables keyed by kind. Always last.
  NK_count
};

/// Convert a `NodeKind` to its enumerator name (e.g.,
/// `"CompilationUnit"`, `"BinaryExpr"`). Used by the AST printer's
/// `(NodeKind ...)` opening token (FR-022 schema; Invariant 6 in
/// `contracts/ast-stability.contract.md`). Stable: the string IS
/// the enumerator name without the `NK_` prefix.
llvm::StringRef toString(NodeKind k);

} // namespace nsl::ast

#endif // NSL_AST_NODEKIND_H
