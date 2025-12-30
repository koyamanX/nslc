#pragma once
#include <vector>
#include <string>
#include <memory>
#include "token.h"
#include "ast.h"

class Parser {

public:
    Parser(std::vector<Token> &_tokens, std::string &_filename)
        : tokens_(_tokens), filename_(_filename)
        , position_(0), has_error_(false), errors_() {};
    std::unique_ptr<NSLNode> parse();

    bool has_errors() const { return has_error_; }
    const std::vector<std::string> &get_errors() const { return errors_; }

private:
    std::vector<Token> tokens_;
    std::string filename_;
    size_t position_;
    bool has_error_;
    std::vector<std::string> errors_;

    const Token &peek(int _offset = 0) const;
    const Token &advance();
    bool match(TokenKind _kind);
    bool check(TokenKind _kind) const;
    bool is_eof() const;

    void error(const std::string &_msg);
    void error(const std::string &_msg, const Token &_token);
    Token expect(TokenKind _kind, const std::string &_msg);

    void synchronize();

    NSLNodePtr parse_nsl();
    ModuleDeclarationNodePtr parse_declaration();
    ModuleImplementationNodePtr parse_module_implementation();
};
