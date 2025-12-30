#pragma once

#include "token.h"
#include <memory>
#include <vector>
#include <string>

enum class ASTNodeType {
    AST_NSL,
    AST_MODULE_DECLARATION,
    AST_MODULE_IMPLEMENTATION,
    AST_STRUCT_DECLARATION,

    AST_WIRE_DECLRATION,
    AST_FUNCTION_DECLARATION,

#if 0
    AST_PORT_DECLARATION,
    AST_SIGNAL_DECLARATION,
    AST_MEMORY_DECLARATION,
    AST_FUNCTION_PORT_DECLARATION,
    AST_FUNC_SELF_DECLARATION,
    AST_FUNCTION_DECLARATION,
    AST_STATE_DECLARATION,
    AST_STATE_NAME_DECLARATION,
    AST_PROC_NAME_DECLARATION,
    AST_PROCEDURE_DECLARATION,

    AST_ASSIGNMENT_STATEMENT,
    AST_IF_STATEMENT,
    AST_ALT_STATEMENT,
    AST_ANY_STATEMENT,
    AST_SEQ_STATEMENT,
    AST_WHILE_STATEMENT,
    AST_FOR_STATEMENT,
    AST_GOTO_STATEMENT,
    AST_FINISH_STATEMENT,
    AST_RETURN_STATEMENT,
    AST_INVOKE_STATEMENT,
    AST_LABEL_STATEMENT,
    AST_FUNCTION_CALL_STATEMENT,
    AST_PROCEDURE_CALL_STATEMENT,
    AST_STATE_BEHAVIOR,

    AST_BINARY_EXPRESSION,
    AST_UNARY_EXPRESSION,
    AST_CONDITIONAL_EXPRESSION,
    AST_IDENTIFIER_EXPRESSION,
    AST_LITERAL_EXPRESSION,
    AST_INDEX_EXPRESSION,
    AST_SLICE_EXPRESSION,
    AST_MEMBER_ACCESS_EXPRESSION,
    AST_CONCATENATION_EXPRESSION,
    AST_REPLICATION_EXPRESSION,
    AST_SIGN_EXTENSION_EXPRESSION,
    AST_ZERO_EXTENSION_EXPRESSION,
    AST_FUNCTION_CALL_EXPRESSION,

    AST_MODULE_INSTANTIATION
#endif
};

using ASTNodePtr = std::unique_ptr<class ASTNode>;
using ASTNodeList = std::vector<ASTNodePtr>;
using NSLNodePtr = std::unique_ptr<class NSLNode>;
using DeclarationNodePtr = std::unique_ptr<class DeclarationNode>;
using FunctionDeclarationNodePtr = std::unique_ptr<class FunctionDeclarationNode>;
using ModuleImplementationNodePtr = std::unique_ptr<class ModuleImplementationNode>;
using ModuleDeclarationNodePtr = std::unique_ptr<class ModuleDeclarationNode>;
using WireDeclarationNodePtr = std::unique_ptr<class WireDeclarationNode>;
using PortDeclarationNodePtr = std::unique_ptr<class PortDeclarationNode>;

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

    void accept(ASTVisitor& _visitor) override;

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
      : DeclarationNode(ASTNodeType::AST_WIRE_DECLRATION, _loc, _name), width_(_width) {}

    int get_width() const {
      return width_;
    }

    void accept(ASTVisitor& visitor) override;

private:
    int width_; // 0 means 1-bit (default)
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

enum class FunctionType { FUNC_IN, FUNC_OUT, FUNC_SELF };

class FunctionDeclarationNode : public DeclarationNode {
public:
    FunctionDeclarationNode(FunctionType _type,
                              const std::string &_name,
                              const std::vector<std::string> &_params,
                              const SourceLocation &_loc)
        : DeclarationNode(ASTNodeType::AST_FUNCTION_DECLARATION, _loc, _name), type_(_type)
        , parameters_(_params), return_value_("") {}

    FunctionDeclarationNode(FunctionType _type,
                              const std::string &_name,
                              const std::vector<std::string> &_params,
                              const std::string &_return_value,
                              const SourceLocation &_loc)
        : DeclarationNode(ASTNodeType::AST_FUNCTION_DECLARATION, _loc, _name), type_(_type)
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
    struct Member {
        std::string name;
        int width; // 0 means 1-bit (default)
        SourceLocation location;
    };

    StructDeclarationNode(const std::string& _name, const SourceLocation& _loc)
        : DeclarationNode(ASTNodeType::AST_STRUCT_DECLARATION, _loc, _name) {}

    void add_member(const std::string& _name, int _width, const SourceLocation& _loc) {
        members_.push_back({ _name, _width, _loc });
    }

    const std::vector<Member>& get_members() const {
        return members_;
    }

    void accept(class ASTVisitor& visitor) override;

private:
    std::vector<Member> members_;
};

class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;

    virtual void visit_nsl(NSLNode* _node) = 0;
    virtual void visit_module_declaration(ModuleDeclarationNode* _node) = 0;
    virtual void visit_declaration(DeclarationNode* _node) = 0;
    virtual void visit_wire_declaration(WireDeclarationNode* _node) = 0;
    virtual void visit_port_declaration(PortDeclarationNode* _node) = 0;
    virtual void visit_function_declaration(FunctionDeclarationNode* _node) = 0;
    virtual void visit_module_implementation(ModuleImplementationNode* _node) = 0;
    virtual void visit_struct_declaration(StructDeclarationNode* _node) = 0;
};
