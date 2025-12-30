#pragma once

#include "token.h"
#include <memory>
#include <vector>
#include <string>

enum class ASTNodeType {
    /* ========== TOP-LEVEL ========== */
    AST_NSL,                        // <nsl_source_file>
    AST_MODULE_DECLARATION,         // <declare> block
    AST_MODULE_IMPLEMENTATION,      // <module> block
    AST_STRUCT_DECLARATION,         // <struct_declaration>

    /* ========== PORT DECLARATIONS (in declare block) ========== */
    AST_PORT_DECLARATION,           // <port_declaration> (input/output/inout)
    AST_FUNCTION_PORT_DECLARATION,  // <function_port_declaration> (func_in/func_out)
    AST_PARAMETER_DECLARATION,      // <parameter_declaration> (param_int/param_str)

    /* ========== SIGNAL DECLARATIONS (in module block) ========== */
    AST_WIRE_DECLARATION,           // <signal_declaration> wire
    AST_REG_DECLARATION,            // <signal_declaration> reg
    AST_VARIABLE_DECLARATION,       // <signal_declaration> variable
    AST_INTEGER_DECLARATION,        // <signal_declaration> integer
    AST_MEMORY_DECLARATION,         // <memory_declaration>

    /* ========== FUNCTION/PROCEDURE ========== */
    AST_FUNC_SELF_DECLARATION,      // <func_self_declaration>
    AST_FUNCTION_DEFINITION,        // <function_definition> (func block)
    AST_PROCEDURE_DECLARATION,      // <procedure_declaration> (proc_name)
    AST_PROCEDURE_DEFINITION,       // <procedure_definition> (proc block)

    /* ========== STATE MACHINE ========== */
    AST_STATE_NAME_DECLARATION,     // <state_name_declaration>
    AST_STATE_BEHAVIOR,             // <state_behavior>

    /* ========== MODULE INSTANTIATION ========== */
    AST_MODULE_INSTANTIATION,       // <module_instantiation>

    /* ========== STATEMENTS ========== */
    AST_BLOCK_STATEMENT,            // <block_statement>
    AST_ASSIGNMENT_STATEMENT,       // <assignment_statement>
    AST_IF_STATEMENT,               // <if_statement>
    AST_ANY_STATEMENT,              // <any_statement>
    AST_ALT_STATEMENT,              // <alt_statement>
    AST_SEQ_STATEMENT,              // <seq_statement>
    AST_WHILE_STATEMENT,            // <while_statement>
    AST_FOR_STATEMENT,              // <for_statement>
    AST_GOTO_STATEMENT,             // <goto_statement>
    AST_GENERATE_STATEMENT,         // <generate_statement>
    AST_RETURN_STATEMENT,           // <return_statement>
    AST_INVOKE_STATEMENT,           // <invoke_statement>
    AST_FINISH_STATEMENT,           // <finish_statement>
    AST_FUNCTION_CALL_STATEMENT,    // <function_call_statement>

    /* ========== EXPRESSIONS ========== */
    AST_BINARY_EXPRESSION,          // Binary operators (+, -, *, <<, >>, etc.)
    AST_UNARY_EXPRESSION,           // <unary_expression> (!, ~, -, +, reductions)
    AST_CONDITIONAL_EXPRESSION,     // <conditional_expression> (if-else expr)
    AST_IDENTIFIER_EXPRESSION,      // <identifier> in expression context
    AST_LITERAL_EXPRESSION,         // <literal> (integer, string)
    AST_INDEX_EXPRESSION,           // Array index: id[expr]
    AST_SLICE_EXPRESSION,           // Bit slice: id[expr:expr]
    AST_MEMBER_ACCESS_EXPRESSION,   // Member access: id.id
    AST_CONCATENATION_EXPRESSION,   // <concatenation> {a, b, c}
    AST_REPLICATION_EXPRESSION,     // <replication> n{expr}
    AST_SIGN_EXTENSION_EXPRESSION,  // <sign_extension> n#(expr)
    AST_ZERO_EXTENSION_EXPRESSION,  // <zero_extension> n'(expr)
    AST_FUNCTION_CALL_EXPRESSION,   // <function_call> in expression context
};

using ASTNodePtr = std::unique_ptr<class ASTNode>;
using ASTNodeList = std::vector<ASTNodePtr>;

// Top-Level
using NSLNodePtr = std::unique_ptr<class NSLNode>;
using ModuleDeclarationNodePtr = std::unique_ptr<class ModuleDeclarationNode>;
using ModuleImplementationNodePtr = std::unique_ptr<class ModuleImplementationNode>;
using StructDeclarationNodePtr = std::unique_ptr<class StructDeclarationNode>;

// Port Declarations
using PortDeclarationNodePtr = std::unique_ptr<class PortDeclarationNode>;
using FunctionPortDeclarationNodePtr = std::unique_ptr<class FunctionPortDeclarationNode>;
using ParameterDeclarationNodePtr = std::unique_ptr<class ParameterDeclarationNode>;

// Signal Declarations
using WireDeclarationNodePtr = std::unique_ptr<class WireDeclarationNode>;
using RegDeclarationNodePtr = std::unique_ptr<class RegDeclarationNode>;
using VariableDeclarationNodePtr = std::unique_ptr<class VariableDeclarationNode>;
using IntegerDeclarationNodePtr = std::unique_ptr<class IntegerDeclarationNode>;
using MemoryDeclarationNodePtr = std::unique_ptr<class MemoryDeclarationNode>;

// Function/Procedure
using FuncSelfDeclarationNodePtr = std::unique_ptr<class FuncSelfDeclarationNode>;
using FunctionDefinitionNodePtr = std::unique_ptr<class FunctionDefinitionNode>;
using ProcedureDeclarationNodePtr = std::unique_ptr<class ProcedureDeclarationNode>;
using ProcedureDefinitionNodePtr = std::unique_ptr<class ProcedureDefinitionNode>;

// State Machine
using StateNameDeclarationNodePtr = std::unique_ptr<class StateNameDeclarationNode>;
using StateBehaviorNodePtr = std::unique_ptr<class StateBehaviorNode>;

// Module Instantiation
using ModuleInstantiationNodePtr = std::unique_ptr<class ModuleInstantiationNode>;

// Statements
using StatementNodePtr = std::unique_ptr<class StatementNode>;
using BlockStatementNodePtr = std::unique_ptr<class BlockStatementNode>;
using AssignmentStatementNodePtr = std::unique_ptr<class AssignmentStatementNode>;
using IfStatementNodePtr = std::unique_ptr<class IfStatementNode>;
using AnyStatementNodePtr = std::unique_ptr<class AnyStatementNode>;
using AltStatementNodePtr = std::unique_ptr<class AltStatementNode>;
using SeqStatementNodePtr = std::unique_ptr<class SeqStatementNode>;
using WhileStatementNodePtr = std::unique_ptr<class WhileStatementNode>;
using ForStatementNodePtr = std::unique_ptr<class ForStatementNode>;
using GotoStatementNodePtr = std::unique_ptr<class GotoStatementNode>;
using GenerateStatementNodePtr = std::unique_ptr<class GenerateStatementNode>;
using ReturnStatementNodePtr = std::unique_ptr<class ReturnStatementNode>;
using InvokeStatementNodePtr = std::unique_ptr<class InvokeStatementNode>;
using FinishStatementNodePtr = std::unique_ptr<class FinishStatementNode>;
using FunctionCallStatementNodePtr = std::unique_ptr<class FunctionCallStatementNode>;

// Expressions
using ExpressionNodePtr = std::unique_ptr<class ExpressionNode>;
using BinaryExpressionNodePtr = std::unique_ptr<class BinaryExpressionNode>;
using UnaryExpressionNodePtr = std::unique_ptr<class UnaryExpressionNode>;
using ConditionalExpressionNodePtr = std::unique_ptr<class ConditionalExpressionNode>;
using IdentifierExpressionNodePtr = std::unique_ptr<class IdentifierExpressionNode>;
using LiteralExpressionNodePtr = std::unique_ptr<class LiteralExpressionNode>;
using IndexExpressionNodePtr = std::unique_ptr<class IndexExpressionNode>;
using SliceExpressionNodePtr = std::unique_ptr<class SliceExpressionNode>;
using MemberAccessExpressionNodePtr = std::unique_ptr<class MemberAccessExpressionNode>;
using ConcatenationExpressionNodePtr = std::unique_ptr<class ConcatenationExpressionNode>;
using ReplicationExpressionNodePtr = std::unique_ptr<class ReplicationExpressionNode>;
using SignExtensionExpressionNodePtr = std::unique_ptr<class SignExtensionExpressionNode>;
using ZeroExtensionExpressionNodePtr = std::unique_ptr<class ZeroExtensionExpressionNode>;
using FunctionCallExpressionNodePtr = std::unique_ptr<class FunctionCallExpressionNode>;

class ASTVisitor;

class ASTNode {
public:
  ASTNode(ASTNodeType _type, const SourceLocation &_loc) : type_(_type), loc_(_loc) {}
  virtual ~ASTNode() = default;

  ASTNodeType get_type() const {
    return type_;
  }

  const SourceLocation& get_location() const {
    return loc_;
  }

  virtual void accept(ASTVisitor &_visitor) = 0;

protected:
  ASTNodeType type_;
  SourceLocation loc_;
};

class NSLNode : public ASTNode {
public:
    NSLNode(const SourceLocation &_loc)
        : ASTNode(ASTNodeType::AST_NSL, _loc)
        , modules_(), declarations_(), structs_() {}

    ~NSLNode() override = default;

    void accept(ASTVisitor &_visitor) override;

    void add_module(ASTNodePtr _module) {
        modules_.push_back(std::move(_module));
    }

    const ASTNodeList &get_modules() const {
        return modules_;
    }

    void add_declaration(ASTNodePtr _declaration) {
        declarations_.push_back(std::move(_declaration));
    }

    const ASTNodeList &get_declarations() const {
        return declarations_;
    }

    void add_struct(ASTNodePtr _struct) {
        structs_.push_back(std::move(_struct));
    }

    const ASTNodeList &get_structs() const {
        return structs_;
    }

private:
    ASTNodeList modules_;
    ASTNodeList declarations_;
    ASTNodeList structs_;
};

class DeclarationNode : public ASTNode {
public:
    DeclarationNode(ASTNodeType _type, const SourceLocation& _loc, std::string _name)
    : ASTNode(_type, _loc), name_(_name) {}

    ~DeclarationNode() override = default;

    const std::string& get_name() const {
        return name_;
    }

    void accept(ASTVisitor& _visitor) override = 0;

private:
    std::string name_;
};

class ModuleDeclarationNode : public DeclarationNode {
public:
    ModuleDeclarationNode(const std::string& _name,
                        const SourceLocation& _loc,
                        bool _is_interface = false,
                        bool _is_simulation = false)
    : DeclarationNode(ASTNodeType::AST_MODULE_DECLARATION, _loc, _name)
    , is_interface_(_is_interface), is_simulation_(_is_simulation) {}

    ~ModuleDeclarationNode() override = default;

    void add_port_declaration(ASTNodePtr _portDecl) {
        port_declarations_.push_back(std::move(_portDecl));
    }

    const std::vector<ASTNodePtr>& get_port_declarations() const {
        return port_declarations_;
    }

    bool is_interface() const {
        return is_interface_;
    }

    bool is_simulation() const {
        return is_simulation_;
    }

    void accept(ASTVisitor& _visitor) override;

private:
    std::vector<ASTNodePtr> port_declarations_;
    bool is_interface_;
    bool is_simulation_;
};

class WireDeclarationNode : public DeclarationNode {
public:
    WireDeclarationNode(const std::string& _name,
                        int _width,
                        const SourceLocation& _loc)
      : DeclarationNode(ASTNodeType::AST_WIRE_DECLARATION, _loc, _name), width_(_width) {}

    int get_width() const {
      return width_;
    }

    void accept(ASTVisitor& visitor) override;

private:
    int width_; // 0 means 1-bit (default)
};

class RegDeclarationNode : public DeclarationNode {
public:
    RegDeclarationNode(const std::string& _name,
                        int _width,
                        const SourceLocation& _loc)
      : DeclarationNode(ASTNodeType::AST_WIRE_DECLARATION, _loc, _name), width_(_width)
      , has_init_value_(false) {}

    RegDeclarationNode(const std::string& _name,
                        int _width,
                        const std::string& _init_value,
                        const SourceLocation& _loc)
      : DeclarationNode(ASTNodeType::AST_WIRE_DECLARATION, _loc, _name), width_(_width)
      , has_init_value_(true), init_value_(_init_value) {}

    int get_width() const {
        return width_;
    }

    bool has_init_value() const {
        return has_init_value_;
    }

    void accept(ASTVisitor& visitor) override;

private:
    int width_; // 0 means 1-bit (default)
    bool has_init_value_;
    std::string init_value_;
};

class MemoryDeclarationNode : public DeclarationNode {
public:
    MemoryDeclarationNode(const std::string& _name,
                          int _width,
                          int _depth,
                          const SourceLocation& _loc)
      : DeclarationNode(ASTNodeType::AST_WIRE_DECLARATION, _loc, _name), width_(_width)
      , depth_(_depth), has_init_values_(false) {}

    MemoryDeclarationNode(const std::string& _name,
                          int _width,
                          int _depth,
                          const std::vector<std::string>& _init_values,
                          const SourceLocation& _loc)
      : DeclarationNode(ASTNodeType::AST_WIRE_DECLARATION, _loc, _name), width_(_width)
      , depth_(_depth), has_init_values_(true), init_values_(_init_values) {}

    int get_width() const {
        return width_;
    }

    int get_depth() const {
        return depth_;
    }

    bool has_init_values() const {
        return has_init_values_;
    }

    const std::vector<std::string>& get_init_values() const {
        return init_values_;
    }

    void accept(ASTVisitor& visitor) override;

private:
    int width_; // 0 means 1-bit (default)
    int depth_;
    bool has_init_values_;
    std::vector<std::string> init_values_;
};

enum class PortDirection { INPUT, OUTPUT, INOUT };

class PortDeclarationNode : public WireDeclarationNode {
public:
    PortDeclarationNode(const std::string& _name,
                        PortDirection _direction,
                        int _width,
                        const SourceLocation& _loc)
      : WireDeclarationNode(_name, _width, _loc), direction_(_direction) {}

    PortDirection get_direction() const {
      return direction_;
    }

    void accept(ASTVisitor& visitor) override;

private:
    PortDirection direction_;
};

enum class FunctionType { FUNC_IN, FUNC_OUT };

class FunctionPortDeclarationNode : public DeclarationNode {
public:
    FunctionPortDeclarationNode(FunctionType _type,
                              const std::string &_name,
                              const std::vector<std::string> &_params,
                              const SourceLocation &_loc)
        : DeclarationNode(ASTNodeType::AST_FUNCTION_PORT_DECLARATION, _loc, _name), type_(_type)
        , parameters_(_params), return_value_("") {}

    FunctionPortDeclarationNode(FunctionType _type,
                              const std::string &_name,
                              const std::vector<std::string> &_params,
                              const std::string &_return_value,
                              const SourceLocation &_loc)
        : DeclarationNode(ASTNodeType::AST_FUNCTION_PORT_DECLARATION, _loc, _name), type_(_type)
        , parameters_(_params), return_value_(_return_value) {}

    FunctionType get_type() const {
        return type_;
    }
    const std::vector<std::string> &get_paramaters() const {
        return parameters_;
    }
    const std::string &get_return_value() const {
        return return_value_;
    }

    void accept(class ASTVisitor& visitor) override;

private:
    FunctionType type_;
    std::vector<std::string> parameters_;
    std::string return_value_;
};

class ModuleImplementationNode : public ASTNode {
public:
    ModuleImplementationNode(const std::string& _name, const SourceLocation& _loc)
        : ASTNode(ASTNodeType::AST_MODULE_IMPLEMENTATION, _loc), name_(_name) {}

    void add_declaration(ASTNodePtr _decl) {
        declarations_.push_back(std::move(_decl));
    }

    void add_declarations(ASTNodeList _declarations) {
        for (auto &decl : _declarations) {
            declarations_.push_back(std::move(decl));
        }
    }

    void add_statement(ASTNodePtr _stmt) {
        statements_.push_back(std::move(_stmt));
    }

    const std::string& get_module_name() const {
        return name_;
    }

    const std::vector<ASTNodePtr>& get_declarations() const {
        return declarations_;
    }

    const std::vector<ASTNodePtr>& get_statements() const {
        return statements_;
    }

    void accept(class ASTVisitor& visitor) override;

private:
    std::string name_;
    std::vector<ASTNodePtr> declarations_;
    std::vector<ASTNodePtr> statements_;
};

class StructDeclarationNode : public DeclarationNode {
public:
    struct StructField {
        std::string name;
        int width; // 0 means 1-bit (default)
        SourceLocation location;
    };

    StructDeclarationNode(const std::string& _name, const SourceLocation& _loc)
        : DeclarationNode(ASTNodeType::AST_STRUCT_DECLARATION, _loc, _name) {}

    void add_field(const std::string& _name, int _width, const SourceLocation& _loc) {
        fields_.push_back({ _name, _width, _loc });
    }

    const std::vector<StructField>& get_fields() const {
        return fields_;
    }

    void accept(class ASTVisitor& visitor) override;

private:
    std::vector<StructField> fields_;
};

class VariableDeclarationNode : public DeclarationNode {
public:
    VariableDeclarationNode(const std::string& _name,
                            int _width,
                            const SourceLocation& _loc)
      : DeclarationNode(ASTNodeType::AST_WIRE_DECLARATION, _loc, _name), width_(_width) {}

    int get_width() const {
        return width_;
    }
    void accept(ASTVisitor& visitor) override;
private:
    int width_; // 0 means 1-bit (default)
};

class IntegerDeclarationNode : public DeclarationNode {
public:
    IntegerDeclarationNode(const std::string& _name, SourceLocation& _loc)
        : DeclarationNode(ASTNodeType::AST_WIRE_DECLARATION,  _loc, _name) {}
    void accept(ASTVisitor& visitor) override;
};

class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;

   // Top-Level
    virtual void visit(NSLNode* node) = 0;
    virtual void visit(ModuleDeclarationNode* node) = 0;
    virtual void visit(ModuleImplementationNode* node) = 0;
    virtual void visit(StructDeclarationNode* node) = 0;

    // Port Declarations
    virtual void visit(PortDeclarationNode* node) = 0;
    virtual void visit(FunctionPortDeclarationNode* node) = 0;
    virtual void visit(ParameterDeclarationNode* node) = 0;

    // Signal Declarations
    virtual void visit(WireDeclarationNode* node) = 0;
    virtual void visit(RegDeclarationNode* node) = 0;
    virtual void visit(VariableDeclarationNode* node) = 0;
    virtual void visit(IntegerDeclarationNode* node) = 0;
    virtual void visit(MemoryDeclarationNode* node) = 0;

    // Function/Procedure
    virtual void visit(FuncSelfDeclarationNode* node) = 0;
    virtual void visit(FunctionDefinitionNode* node) = 0;
    virtual void visit(ProcedureDeclarationNode* node) = 0;
    virtual void visit(ProcedureDefinitionNode* node) = 0;

    // State Machine
    virtual void visit(StateNameDeclarationNode* node) = 0;
    virtual void visit(StateBehaviorNode* node) = 0;

    // Module Instantiation
    virtual void visit(ModuleInstantiationNode* node) = 0;

    // Statements
    virtual void visit(BlockStatementNode* node) = 0;
    virtual void visit(AssignmentStatementNode* node) = 0;
    virtual void visit(IfStatementNode* node) = 0;
    virtual void visit(AnyStatementNode* node) = 0;
    virtual void visit(AltStatementNode* node) = 0;
    virtual void visit(SeqStatementNode* node) = 0;
    virtual void visit(WhileStatementNode* node) = 0;
    virtual void visit(ForStatementNode* node) = 0;
    virtual void visit(GotoStatementNode* node) = 0;
    virtual void visit(GenerateStatementNode* node) = 0;
    virtual void visit(ReturnStatementNode* node) = 0;
    virtual void visit(InvokeStatementNode* node) = 0;
    virtual void visit(FinishStatementNode* node) = 0;
    virtual void visit(FunctionCallStatementNode* node) = 0;
};
