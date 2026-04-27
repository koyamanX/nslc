# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# nsl-ast — layer 4. AST node hierarchy with the per-node-kind
# headers exception (Constitution Principle II §3). M2 lands the
# umbrella headers (`ASTNode.h`, `ASTVisitor.h`, `NodeKind.h` /
# `.def`, `Type.h`, `Decl.h`, `Stmt.h`, `Expr.h`, `Printer.h`) plus
# one header per concrete `Decl` / `Stmt` / `Expr` subclass listed in
# `include/nsl/AST/NodeKind.def`. Real sources arrive incrementally
# with M3 (Sema fills `Expr::inferredType_`) and later milestones.
#
# Per `cmake/AddNSLLibrary.cmake`, the macro auto-validates layer-
# direction (Principle II downward-only flow) on `DEPENDS`. Listing
# `nsl-basic` here covers `SourceLocation` / `SourceRange` /
# `SourceManager` consumed by the printer.

add_nsl_library(nsl-ast
  ASTNode.cpp
  Printer.cpp
  NodeKindNames.cpp
  HEADERS
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/ASTNode.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/ASTVisitor.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/NodeKind.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/NodeKind.def
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/Type.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/Decl.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/Stmt.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/Expr.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/Printer.h
    # ---- Decl-family per-kind headers (data-model §§1.2–1.4) ----
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/CompilationUnit.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/StructDecl.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/TopLevelParamDecl.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/DeclareBlock.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/PortDecl.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/ModuleBlock.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/RegDecl.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/WireDecl.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/VariableDecl.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/IntegerDecl.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/MemDecl.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/FuncSelfDecl.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/ProcNameDecl.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/StateNameDecl.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/FirstStateDecl.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/SubmoduleDecl.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/StructInstDecl.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/FuncDefn.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/ProcDefn.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/StateDefn.h
    # ---- Stmt-family per-kind headers (data-model §1.5) ----
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/TransferStmt.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/IncDecStmt.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/ControlCallStmt.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/BareFinishStmt.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/SystemTaskStmt.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/ReturnStmt.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/EmptyStmt.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/LabeledStmt.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/GotoStmt.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/InitBlockStmt.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/DelayTaskStmt.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/ParallelBlock.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/AltBlock.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/AnyBlock.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/SeqBlock.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/WhileBlock.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/ForBlock.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/IfStmt.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/StructuralGenerate.h
    # ---- Expr-family per-kind headers (data-model §1.6) ----
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/LiteralExpr.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/IdentifierExpr.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/SystemVarExpr.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/UnaryExpr.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/BinaryExpr.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/ConditionalExpr.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/ConcatExpr.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/RepeatExpr.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/SignExtendExpr.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/ZeroExtendExpr.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/SliceExpr.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/FieldAccessExpr.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/CallExpr.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/StructCastExpr.h
    ${CMAKE_SOURCE_DIR}/include/nsl/AST/IncDecExpr.h
  DEPENDS
    nsl-basic
  LINK_LIBS
    LLVMSupport)
