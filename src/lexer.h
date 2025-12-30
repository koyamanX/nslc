#pragma once

#include <string>
#include <vector>
#include "token.h"
#include <iostream>

class Lexer
{
public:
    Lexer(const std::string &_source, const std::string &_filename)
        : source_(_source), filename_(_filename)
        , position_(0), line_(1), column_(1)
        {
            init_keywords();
            init_operators();
        }
    std::vector<Token> tokenize();

private:
    bool is_eof() const;
    char peek() const;
    char peek_next() const;
    char advance();
    void skip_whitespace();
    void skip_comment();
    Token next_token();
    Token tokenize_integer_literal();
    Token tokenize_string_literal();

    SourceLocation current_location() { return SourceLocation(filename_, line_, column_); }

    void init_keywords();
    void init_operators();

    std::map<std::string, TokenKind> keywords_;
    std::map<std::string, TokenKind> ops1_;
    std::map<std::string, TokenKind> ops2_;
    std::string source_;
    std::string filename_;
    size_t position_;
    size_t line_;
    size_t column_;
};
