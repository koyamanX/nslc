#include "lexer.h"
#include "token.h"
#include <cstring>


void Lexer::init_keywords()
{
    keywords_ = {
        {"alt", TokenKind::TK_ALT},                  // alt
        {"any", TokenKind::TK_ANY},                  // any
        {"declare", TokenKind::TK_DECLARE},          // declare
        {"else", TokenKind::TK_ELSE},                // else
        {"finish", TokenKind::TK_FINISH},            // finish
        {"for", TokenKind::TK_FOR},                  // for
        {"func", TokenKind::TK_FUNC},                // func
        {"func_in", TokenKind::TK_FUNC_IN},          // func_in
        {"func_out", TokenKind::TK_FUNC_OUT},        // func_out
        {"func_self", TokenKind::TK_FUNC_SELF},      // func_self
        {"generate", TokenKind::TK_GENERATE},        // generate
        {"goto", TokenKind::TK_GOTO},                // goto
        {"if", TokenKind::TK_IF},                    // if
        {"inout", TokenKind::TK_INOUT},              // inout
        {"input", TokenKind::TK_INPUT},              // input
        {"integer", TokenKind::TK_INTEGER},          // integer
        {"interface", TokenKind::TK_INTERFACE},      // interface
        {"simulation", TokenKind::TK_SIMULATION},    // simulation
        {"label", TokenKind::TK_LABEL},              // label
        {"label_name", TokenKind::TK_LABEL_NAME},    // label_name
        {"m_clock", TokenKind::TK_M_CLOCK},          // m_clock
        {"mem", TokenKind::TK_MEM},                  // mem
        {"module", TokenKind::TK_MODULE},            // module
        {"output", TokenKind::TK_OUTPUT},            // output
        {"p_reset", TokenKind::TK_P_RESET},          // p_reset
        {"proc", TokenKind::TK_PROC},                // proc
        {"proc_name", TokenKind::TK_PROC_NAME},      // proc_name
        {"reg", TokenKind::TK_REG},                  // reg
        {"return", TokenKind::TK_RETURN},            // return
        {"seq", TokenKind::TK_SEQ},                  // seq
        {"state", TokenKind::TK_STATE},              // state
        {"state_name", TokenKind::TK_STATE_NAME},    // state_name
        {"variable", TokenKind::TK_VARIABLE},        // variable
        {"while", TokenKind::TK_WHILE},              // while
        {"wire", TokenKind::TK_WIRE},                // wire
    };
}

void Lexer::init_operators()
{
    ops1_ = {
        {"+", TokenKind::TK_OP_PLUS},
        {"-", TokenKind::TK_OP_MINUS},
        {"*", TokenKind::TK_OP_MULTIPLY},
        {"&", TokenKind::TK_OP_AND},
        {"^", TokenKind::TK_OP_XOR},
        {"~", TokenKind::TK_OP_NOT},
        {"<", TokenKind::TK_OP_LT},
        {">", TokenKind::TK_OP_GT},
        {"!", TokenKind::TK_OP_LNOT},
        {"=", TokenKind::TK_OP_ASSIGN},
        {"(", TokenKind::TK_LPAREN},
        {")", TokenKind::TK_RPAREN},
        {"{", TokenKind::TK_LBRACE},
        {"}", TokenKind::TK_RBRACE},
        {"[", TokenKind::TK_LBRACKET},
        {"]", TokenKind::TK_RBRACKET},
        {";", TokenKind::TK_SEMICOLON},
        {":", TokenKind::TK_COLON},
        {",", TokenKind::TK_COMMA},
        {".", TokenKind::TK_DOT},
        {"#", TokenKind::TK_HASH},
        {"'", TokenKind::TK_APOSTROPHE},
    };

    ops2_ = {
        {"++", TokenKind::TK_OP_INC},
        {"--", TokenKind::TK_OP_DEC},
        {"|", TokenKind::TK_OP_OR},
        {"<<", TokenKind::TK_OP_LSHIFT},
        {">>", TokenKind::TK_OP_RSHIFT},
        {"==", TokenKind::TK_OP_EQ},
        {"!=", TokenKind::TK_OP_NEQ},
        {"<=", TokenKind::TK_OP_LE},
        {">=", TokenKind::TK_OP_GE},
        {"&&", TokenKind::TK_OP_LAND},
        {"||", TokenKind::TK_OP_LOR},
        {":=", TokenKind::TK_OP_ASSIGN_REG},
    };
}

std::vector<Token> Lexer::tokenize()
{
    std::vector<Token> tokens;

    while(!is_eof())
    {
        skip_whitespace();

        if(is_eof())
            break;

        Token tk = next_token();

        if(tk.kind == TokenKind::TK_EOF)
            break;

        tokens.push_back(tk);
    }

    tokens.push_back(Token(TokenKind::TK_EOF, "", current_location()));

    return tokens;
}

Token Lexer::next_token()
{
    SourceLocation loc = current_location();

    if (is_eof()) {
        return Token(TokenKind::TK_EOF, "", current_location());
    }

    char c = peek();

    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
        // Identifier or keyword
        std::string str;
        while (!is_eof() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) {
            str += advance();
        }

        auto it = keywords_.find(str);
        if (it != keywords_.end()) {
            return Token(it->second, str, loc);
        }

        return Token(TokenKind::TK_IDENTIFIER, str, loc);
    }

    if (std::isxdigit(static_cast<unsigned char>(c))) {
        return tokenize_integer_literal();
    }

    if (c == '"') {
        return tokenize_string_literal();
    }

    std::string str;

    str += advance();
    if (!is_eof()) {
        str += peek();

        if (str == "//" || str == "/*") {
            skip_comment();
            skip_whitespace();
            return next_token();
        }

        auto it = ops2_.find(str);
        if (it != ops2_.end()) {
            advance();
            return Token(it->second, str, loc);
        }
        str.pop_back();
    }

    auto it = ops1_.find(str);
    if (it != ops1_.end()) {
        return Token(it->second, str, loc);
    }

    std::cout << "Error: Unknown token '" << str << "' at " << loc.to_string() << std::endl;
    return Token();
}

void Lexer::skip_comment()
{
    char c = advance(); // consume '*' or '/'
    if (c == '/') {
        // single-line comment
        while (!is_eof() && peek() != '\n') {
            advance();
        }
    } else if (c == '*') {
        const char *found = strstr(&source_[position_], "*/");
        if (found) {
            size_t end_pos = found - source_.c_str() + 2;
            while (position_ < end_pos && !is_eof()) {
                advance();
            }
        } else {
            // Unterminated comment, consume until EOF
            std::cout << "Warning: Unterminated comment at " << current_location().to_string() << std::endl;
            while (!is_eof()) {
                advance();
            }
        }
    }
}

bool Lexer::is_eof() const
{
    return position_ >= source_.length();
}

char Lexer::peek() const
{
    if(is_eof())
        return '\0';
    return source_[position_];
}

char Lexer::peek_next() const {
  if (position_ + 1 >= source_.length()) {
    return '\0';
  }
  return source_[position_ + 1];
}

char Lexer::advance()
{
  if (is_eof()) {
    return '\0';
  }

  char c = source_[position_];
  position_++;

  if (c == '\n') {
    line_++;
    column_ = 1;
  } else if (c == '\r') {
    // Handle Windows-style \r\n as a single line ending
    if (peek() == '\n') {
      position_++;
    }
    line_++;
    column_ = 1;
  } else {
    column_++;
  }

  return c;
}

void Lexer::skip_whitespace()
{
    while(!is_eof())
    {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            advance();
        else
            break;
    }
}

/*
<integer_literal> ::= <decimal_literal>
                    | <hexadecimal_literal>
                    | <octal_literal>
                    | <binary_literal>
                    | <sized_literal>

<decimal_literal> ::= <digit> { <digit> }

<hexadecimal_literal> ::= "0" ( "x" | "X" ) <hex_digit> { <hex_digit> }

<octal_literal> ::= "0" ( "o" | "O" ) <octal_digit> { <octal_digit> }

<binary_literal> ::= "0" ( "b" | "B" ) <binary_digit> { <binary_digit> }

<sized_literal> ::= <decimal_literal> "'" ( "b" | "o" | "d" | "h" ) <value_part>

<value_part> ::= <hex_digit> { <hex_digit> }
               | <decimal_literal>
               | <octal_digit> { <octal_digit> }
               | <binary_digit> { <binary_digit> }
*/


Token Lexer::tokenize_integer_literal()
{
    SourceLocation loc = current_location();
    std::string str;
    char c = peek();

    if (c == '0' && (peek_next() == 'x' || peek_next() == 'X')) {
        str += advance(); // 0
        str += std::tolower(advance()); // x or X
        while (!is_eof() && (std::isxdigit(static_cast<unsigned char>(peek())) || peek() == '_')) {
            str += advance();
        }
        return Token(TokenKind::TK_HEX, str, loc);
    } else if (c == '0' && (peek_next() == 'o' || peek_next() == 'O')) {
        str += advance(); // 0
        str += std::tolower(advance()); // o or O
        while (!is_eof() && ((peek() >= '0' && peek() <= '7') || peek() == '_')) {
            str += advance();
        }
        return Token(TokenKind::TK_OCTAL, str, loc);
    } else if (c == '0' && (peek_next() == 'b' || peek_next() == 'B')) {
        str += advance(); // 0
        str += std::tolower(advance()); // b or B
        while (!is_eof() && (peek() == '0' || peek() == '1' || peek() == '_')) {
            str += advance();
        }
        return Token(TokenKind::TK_BINARY, str, loc);
    } else {
        while (!is_eof() && (std::isdigit(static_cast<unsigned char>(peek())) || peek() == '_')) {
            str += advance();
        }

        if (peek() == '\'' && std::isxdigit(static_cast<unsigned char>(peek_next()))) {
            // sized literal, e.g., 8'hFF
            str += advance(); // consume '
            str += std::tolower(advance()); // consume base character (b, o, d, h)
            while (!is_eof() && (std::isxdigit(static_cast<unsigned char>(peek())) || peek() == '_')) {
                str += advance();
            }
            return Token(TokenKind::TK_SIZED_LITERAL, str, loc);
        } else {
            return Token(TokenKind::TK_INT, str, loc);
        }
    }
}

Token Lexer::tokenize_string_literal()
{
    SourceLocation loc = current_location();
    std::string str;
    advance(); // consume opening "

    const char *found = strchr(&source_[position_], '"');
    if (found) {
        size_t end_pos = found - source_.c_str();
        while (position_ < end_pos && !is_eof()) {
            str += advance();
        }
        advance(); // consume closing "
        return Token(TokenKind::TK_STRING, str, loc);
    } else {
        // Unterminated string literal
        std::cout << "Error: Unterminated string literal at " << loc.to_string() << std::endl;
        return Token();
    }
}
