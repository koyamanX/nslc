#include "ast.h"

void NSLNode::accept(ASTVisitor &_visitor) {
    _visitor.visit_nsl(this);
}

void ModuleDeclarationNode::accept(ASTVisitor &_visitor) {
    _visitor.visit_module_declaration(this);
}

void DeclarationNode::accept(ASTVisitor &_visitor) {
    _visitor.visit_declaration(this);
}

void WireDeclarationNode::accept(ASTVisitor &visitor) {
    visitor.visit_wire_declaration(this);
}

void PortDeclarationNode::accept(ASTVisitor &visitor) {
    visitor.visit_port_declaration(this);
}

void FunctionDeclarationNode::accept(class ASTVisitor& visitor) {
    visitor.visit_function_declaration(this);
}

void ModuleImplementationNode::accept(class ASTVisitor& visitor) {
    visitor.visit_module_implementation(this);
}

void StructDeclarationNode::accept(class ASTVisitor& visitor) {
    visitor.visit_struct_declaration(this);
}
