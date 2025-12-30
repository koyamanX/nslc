#include "ast.h"

// ============================================================================
// TOP-LEVEL
// ============================================================================

void NSLNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void ModuleDeclarationNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void ModuleImplementationNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void StructDeclarationNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

// ============================================================================
// PORT DECLARATIONS
// ============================================================================

void PortDeclarationNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void FunctionPortDeclarationNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

#if 0
void ParameterDeclarationNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}
#endif

// ============================================================================
// SIGNAL DECLARATIONS
// ============================================================================

void WireDeclarationNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void RegDeclarationNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void VariableDeclarationNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void IntegerDeclarationNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void MemoryDeclarationNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

#if 0
// ============================================================================
// FUNCTION/PROCEDURE
// ============================================================================

void FuncSelfDeclarationNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void FunctionDefinitionNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void ProcedureDeclarationNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void ProcedureDefinitionNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

// ============================================================================
// STATE MACHINE
// ============================================================================

void StateNameDeclarationNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void StateBehaviorNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

// ============================================================================
// MODULE INSTANTIATION
// ============================================================================

void ModuleInstantiationNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

// ============================================================================
// STATEMENTS
// ============================================================================

void BlockStatementNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void AssignmentStatementNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void IfStatementNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void AnyStatementNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void AltStatementNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void SeqStatementNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void WhileStatementNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void ForStatementNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void GotoStatementNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void GenerateStatementNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void ReturnStatementNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void InvokeStatementNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void FinishStatementNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void FunctionCallStatementNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

// ============================================================================
// EXPRESSIONS
// ============================================================================

void BinaryExpressionNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void UnaryExpressionNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void ConditionalExpressionNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void IdentifierExpressionNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void LiteralExpressionNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void IndexExpressionNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void SliceExpressionNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void MemberAccessExpressionNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void ConcatenationExpressionNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void ReplicationExpressionNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void SignExtensionExpressionNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void ZeroExtensionExpressionNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}

void FunctionCallExpressionNode::accept(ASTVisitor& visitor) {
    visitor.visit(this);
}
#endif
