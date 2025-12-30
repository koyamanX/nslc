#include "parser.h"


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

NSLNodePtr Parser::parse_nsl() {
    auto top = std::make_unique<NSLNode>(peek().loc);

    while (!is_eof()) {
        try {
            if (check(TokenKind::TK_DECLARE)) {
                top->add_declaration(parse_declaration());
            } else if (check(TokenKind::TK_MODULE)) {
                top->add_module(parse_module_implementation());
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

                auto func_node = std::make_unique<FunctionDeclarationNode>(
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

    expect(TokenKind::TK_LBRACE, "Expected '{' to start module implementation");

    /* TODO: Parse module implementation contents
    while (!check(TokenKind::TK_RBRACE) && !is_eof()) {
        advance(); // For now, just skip the contents
    }
    */
    
    expect(TokenKind::TK_RBRACE, "Expected '}' to end module implementation");

    auto module_impl_node = std::make_unique<ModuleImplementationNode>(name_token.text, name_token.loc);

    return module_impl_node;
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
            // TODO
                return;
            default:
                break;
        }

        advance();
    }
}


