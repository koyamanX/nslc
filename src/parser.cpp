#include "parser.h"
#include "ast.h"

const Token &Parser::peek(int _offset) const {
    size_t pos = position_ + _offset;
    if (pos >= tokens_.size()) {
      return tokens_.back(); // Return EOF token
    }
    return tokens_[pos];
}

const Token &Parser::advance() {
    if (!is_eof()) {
        position_++;
    }
    return peek(-1);
}

bool Parser::match(TokenKind _kind) {
    if (check(_kind)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::check(TokenKind _kind) const {
    if (is_eof()) {
        return false;
    }
    return peek().kind == _kind;
}

bool Parser::is_eof() const {
    return peek().kind == TokenKind::TK_EOF;
}

void Parser::error(const std::string &_msg) {
    error(_msg, peek());
}

void Parser::error(const std::string &_msg, const Token &_token) {
    has_error_ = true;
    std::string error_msg = _token.to_string() + ": error: " + _msg;
    errors_.push_back(error_msg);
}

Token Parser::expect(TokenKind _kind, const std::string &_msg) {
    if (check(_kind)) {
        return advance();
    }
    error(_msg);

    Token dummy = peek();

    return Token(TokenKind::TK_EOF, "", dummy.loc);
}

NSLNodePtr Parser::parse() {
    return parse_nsl();
}

/*
<nsl> ::= <struct_declaration>
                   | <declare_block>
                   | <module_block>
                   | #line <decimal_literal> <identifier>
*/
NSLNodePtr Parser::parse_nsl() {
    auto top = std::make_unique<NSLNode>(peek().loc);

    while (!is_eof()) {
        try {
            if (check(TokenKind::TK_DECLARE)) {
                top->add_declaration(parse_declaration());
            } else if (check(TokenKind::TK_MODULE)) {
                top->add_module(parse_module_implementation());
            } else if (check(TokenKind::TK_STRUCT)) {
                top->add_struct(parse_struct_declaration());
            } else {
                error("declare/module/struct are expected", peek());
                synchronize(TokenKind::TK_RBRACE);
            }
        } catch (const std::exception &e) {
            error(std::string("Parse error: ") + e.what());
            synchronize();
        }
    }

    return top;
}

ModuleDeclarationNodePtr Parser::parse_declaration() {
    expect(TokenKind::TK_DECLARE, "Expected 'declare' keyword");

    Token name_token = expect(TokenKind::TK_IDENTIFIER, "Expected module name");
    bool is_interface = false;
    bool is_simulation = false;

    if (match(TokenKind::TK_INTERFACE)) {
        is_interface = true;
    }
    if (match(TokenKind::TK_SIMULATION)) {
        is_simulation = true;
    }
    auto module_node = std::make_unique<ModuleDeclarationNode>(name_token.text, name_token.loc, is_interface, is_simulation);

    expect(TokenKind::TK_LBRACE, "Expected '{' to start module declaration");

    while (!check(TokenKind::TK_RBRACE) && !is_eof()) {
        switch (peek().kind) {
            case TokenKind::TK_INPUT:
            case TokenKind::TK_OUTPUT:
            case TokenKind::TK_INOUT: {
                Token dir_token = advance();
                while (!check(TokenKind::TK_SEMICOLON) && !is_eof()) {
                    // Parse each port declaration
                    Token port_name_token = expect(TokenKind::TK_IDENTIFIER, "Expected port name");

                    int width = 0; // Default 1-bit
                    if (match(TokenKind::TK_LBRACKET)) {
                        Token width_token = expect(TokenKind::TK_INT, "Expected port width");
                        width = std::stoi(width_token.text);
                        expect(TokenKind::TK_RBRACKET, "Expected ']'");
                    }

                    PortDirection direction;
                    if (dir_token.kind == TokenKind::TK_INPUT) {
                        direction = PortDirection::INPUT;
                    } else if (dir_token.kind == TokenKind::TK_OUTPUT) {
                        direction = PortDirection::OUTPUT;
                    } else if (dir_token.kind == TokenKind::TK_INOUT) {
                        direction = PortDirection::INOUT;
                    }

                    auto port_node = std::make_unique<PortDeclarationNode>(
                        port_name_token.text, direction, width, port_name_token.loc);
                    module_node->add_port_declaration(std::move(port_node));

                    if (!match(TokenKind::TK_COMMA)) {
                        break;
                    }
                }
                expect(TokenKind::TK_SEMICOLON, "Expected ';' after port declaration");
                break;
            }
            case TokenKind::TK_FUNC_IN:
            case TokenKind::TK_FUNC_OUT:
            {
                Token func_token = advance();
                Token func_name_token = expect(TokenKind::TK_IDENTIFIER, "Expected function name");

                std::vector<std::string> parameters;

                if (match(TokenKind::TK_LPAREN)) {
                    while (!check(TokenKind::TK_RPAREN) && !is_eof()) {
                        Token param_token = expect(TokenKind::TK_IDENTIFIER, "Expected parameter name");
                        parameters.push_back(param_token.text);

                        if (!match(TokenKind::TK_COMMA)) {
                            break;
                        }
                    }
                    expect(TokenKind::TK_RPAREN, "Expected ')' after function parameters");
                }

                std::string return_type;
                if (match(TokenKind::TK_COLON)) {
                    Token return_type_token = expect(TokenKind::TK_IDENTIFIER, "Expected return type");
                    return_type = return_type_token.text;
                }

                expect(TokenKind::TK_SEMICOLON, "Expected ';' after function declaration");

                FunctionType func_type;
                if (func_token.kind == TokenKind::TK_FUNC_IN) {
                    func_type = FunctionType::FUNC_IN;
                } else if (func_token.kind == TokenKind::TK_FUNC_OUT) {
                    func_type = FunctionType::FUNC_OUT;
                }

                auto func_node = std::make_unique<FunctionPortDeclarationNode>(
                    func_type, func_name_token.text, parameters, return_type, func_name_token.loc);

                break;
            }
            default:
                error("input/output/inout/func_in/func_out are expected", peek());
                synchronize();
                break;
        }
    }

    expect(TokenKind::TK_RBRACE, "Expected '}' to end module declaration");

    return std::move(module_node);
}

ModuleImplementationNodePtr Parser::parse_module_implementation() {
    expect(TokenKind::TK_MODULE, "Expected 'module' keyword");

    Token name_token = expect(TokenKind::TK_IDENTIFIER, "Expected module name");

    auto module_impl_node = std::make_unique<ModuleImplementationNode>(name_token.text, name_token.loc);

    expect(TokenKind::TK_LBRACE, "Expected '{' to start module implementation");

    while (!check(TokenKind::TK_RBRACE) && !is_eof()) {
        // TODO: handle `type_t reg/wire ident`;
        module_impl_node->add_declarations(std::move(parse_declarations()));
    }
    
    expect(TokenKind::TK_RBRACE, "Expected '}' to end module implementation");

    return module_impl_node;
}

StructDeclarationNodePtr Parser::parse_struct_declaration() {
    expect(TokenKind::TK_STRUCT, "Expected 'struct' keyword");

    Token name_token = expect(TokenKind::TK_IDENTIFIER, "Expected struct name");
    auto struct_node = std::make_unique<StructDeclarationNode>(name_token.text, name_token.loc);

    expect(TokenKind::TK_LBRACE, "Expected '{' to start struct declaration");

    while (!check(TokenKind::TK_RBRACE) && !is_eof()) {
        Token field_name_token = expect(TokenKind::TK_IDENTIFIER, "Expected struct field name");

        int width = 0; // Default 1-bit
        if (match(TokenKind::TK_LBRACKET)) {
            Token width_token = expect(TokenKind::TK_INT, "Expected field width");
            width = std::stoi(width_token.text);
            expect(TokenKind::TK_RBRACKET, "Expected ']'");
        }

        struct_node->add_field(field_name_token.text, width, field_name_token.loc);

        expect(TokenKind::TK_SEMICOLON, "Expected ';' after struct field declaration");
    }

    expect(TokenKind::TK_RBRACE, "Expected '}' to end struct declaration");
    expect(TokenKind::TK_SEMICOLON, "Expected ';' after struct declaration");

    return struct_node;
}

ASTNodeList Parser::parse_declarations() {
    if (check(TokenKind::TK_WIRE)) {
        return parse_wire_declarations();
    } else if (check(TokenKind::TK_REG)) {
        return parse_reg_declarations();
    } else if (check(TokenKind::TK_MEM)) {
        return parse_mem_declarations();
    } else if (check(TokenKind::TK_VARIABLE)) {
        return parse_variable_declarations();
    } else if (check(TokenKind::TK_INTEGER)) {
        return parse_integer_declarations();
    } else {
        return ASTNodeList();
    }
    return ASTNodeList();
}

ASTNodeList Parser::parse_wire_declarations() {
    ASTNodeList wire_declarations;

    expect(TokenKind::TK_WIRE, "Expected 'wire' keyword");
    while (!check(TokenKind::TK_SEMICOLON) && !is_eof()) {
        Token name_token = expect(TokenKind::TK_IDENTIFIER, "Expected port name");

        int width = 0; // Default 1-bit
        if (match(TokenKind::TK_LBRACKET)) {
            Token width_token = expect(TokenKind::TK_INT, "Expected port width");
            width = std::stoi(width_token.text);
            expect(TokenKind::TK_RBRACKET, "Expected ']'");
        }

        auto port_node = std::make_unique<WireDeclarationNode>(
            name_token.text, width, name_token.loc);
        wire_declarations.push_back(std::move(port_node));

        if (!match(TokenKind::TK_COMMA)) {
            break;
        }
    }

    expect(TokenKind::TK_SEMICOLON, "Expected ';' after wire declarations");

    return wire_declarations;
}

ASTNodeList Parser::parse_reg_declarations() {
    ASTNodeList reg_declarations;

    expect(TokenKind::TK_REG, "Expected 'reg' keyword");
    while (!check(TokenKind::TK_SEMICOLON) && !is_eof()) {
        Token name_token = expect(TokenKind::TK_IDENTIFIER, "Expected reg name");

        int width = 0; // Default 1-bit
        if (match(TokenKind::TK_LBRACKET)) {
            Token width_token = expect(TokenKind::TK_INT, "Expected reg width");
            width = std::stoi(width_token.text);
            expect(TokenKind::TK_RBRACKET, "Expected ']'");
        }

        std::string init_value;
        if (match(TokenKind::TK_OP_ASSIGN)) {
            Token init_value_token = expect(TokenKind::TK_INT, "Expected reg initialization value");
            init_value = init_value_token.text;
        }

        auto port_node = std::make_unique<RegDeclarationNode>(
            name_token.text, width, init_value, name_token.loc);
        reg_declarations.push_back(std::move(port_node));

        if (!match(TokenKind::TK_COMMA)) {
            break;
        }
    }

    expect(TokenKind::TK_SEMICOLON, "Expected ';' after reg declarations");

    return reg_declarations;
}

ASTNodeList Parser::parse_mem_declarations() {
    ASTNodeList mem_declarations;

    expect(TokenKind::TK_MEM, "Expected 'mem' keyword");
    while (!check(TokenKind::TK_SEMICOLON) && !is_eof()) {
        Token name_token = expect(TokenKind::TK_IDENTIFIER, "Expected memory name");

        int depth = 0;
        if (!check(TokenKind::TK_LBRACKET)) {
            error("Expected '[' for memory depth");
            synchronize();
            // TODO: better error recovery?
            return mem_declarations;
        }
        expect(TokenKind::TK_LBRACKET, "Expected '[' for memory depth");
        Token depth_token = expect(TokenKind::TK_INT, "Expected memory depth");
        depth = std::stoi(depth_token.text);
        expect(TokenKind::TK_RBRACKET, "Expected ']' for memory depth");

        int width = 0; // Default 1-bit
        if (match(TokenKind::TK_LBRACKET)) {
            Token width_token = expect(TokenKind::TK_INT, "Expected memory width");
            width = std::stoi(width_token.text);
            expect(TokenKind::TK_RBRACKET, "Expected ']'");
        }

        std::vector<std::string> init_values;
        if (match(TokenKind::TK_OP_ASSIGN)) {
            expect(TokenKind::TK_LBRACE, "Expected '{' for memory initialization");

            while (!check(TokenKind::TK_RBRACE) && !is_eof()) {
                Token init_value_token = expect(TokenKind::TK_INT, "Expected memory initialization value");

                init_values.push_back(init_value_token.text);

                if (!match(TokenKind::TK_COMMA)) {
                    break;
                }
            }
            expect(TokenKind::TK_RBRACE, "Expected '}' for memory initialization");
        }

        auto port_node = std::make_unique<MemoryDeclarationNode>(
            name_token.text, width, depth, init_values, name_token.loc);
        mem_declarations.push_back(std::move(port_node));

        if (!match(TokenKind::TK_COMMA)) {
            break;
        }
    }

    expect(TokenKind::TK_SEMICOLON, "Expected ';' after memory declarations");

    return mem_declarations;
}

ASTNodeList Parser::parse_variable_declarations() {
    ASTNodeList variable_declarations;

    expect(TokenKind::TK_VARIABLE, "Expected 'variable' keyword");
    while (!check(TokenKind::TK_SEMICOLON) && !is_eof()) {
        Token name_token = expect(TokenKind::TK_IDENTIFIER, "Expected variable name");
        int width = 0; // Default 1-bit
        if (match(TokenKind::TK_LBRACKET)) {
            Token width_token = expect(TokenKind::TK_INT, "Expected variable width");
            width = std::stoi(width_token.text);
            expect(TokenKind::TK_RBRACKET, "Expected ']'");
        }
        auto variable_node = std::make_unique<VariableDeclarationNode>(
            name_token.text, width, name_token.loc);

        variable_declarations.push_back(std::move(variable_node));

        if (!match(TokenKind::TK_COMMA)) {
            break;
        }
    }

    expect(TokenKind::TK_SEMICOLON, "Expected ';' after variable declarations");

    return variable_declarations;
}

ASTNodeList Parser::parse_integer_declarations() {
    ASTNodeList integer_declarations;

    expect(TokenKind::TK_INTEGER, "Expected 'integer' keyword");
    while (!check(TokenKind::TK_SEMICOLON) && !is_eof()) {
        Token name_token = expect(TokenKind::TK_IDENTIFIER, "Expected integer name");

        auto integer_node = std::make_unique<IntegerDeclarationNode>(
            name_token.text, name_token.loc);

        integer_declarations.push_back(std::move(integer_node));

        if (!match(TokenKind::TK_COMMA)) {
            break;
        }
    }

    expect(TokenKind::TK_SEMICOLON, "Expected ';' after integer declarations");

    return integer_declarations;
}

void Parser::synchronize() {
    advance();

    while (!is_eof()) {
        if (peek(-1).kind == TokenKind::TK_SEMICOLON) {
            return;
        }

        switch (peek().kind) {
            case TokenKind::TK_DECLARE:
            case TokenKind::TK_INPUT:
            case TokenKind::TK_OUTPUT:
            case TokenKind::TK_INOUT:
            case TokenKind::TK_FUNC_IN:
            case TokenKind::TK_FUNC_OUT:
            case TokenKind::TK_MODULE:
            case TokenKind::TK_STRUCT:
            case TokenKind::TK_WIRE:
            case TokenKind::TK_REG:
            case TokenKind::TK_MEM:
            case TokenKind::TK_VARIABLE:
            case TokenKind::TK_INTEGER:
            // TODO
                return;
            default:
                break;
        }

        advance();
    }
}

void Parser::synchronize(TokenKind stop_kind) {
    advance();

    while (!is_eof()) {
        if (peek(-1).kind == stop_kind) {
            return;
        }
        advance();
    }
}


