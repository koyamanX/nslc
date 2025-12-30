#pragma once

#include <string>
#include <map>

struct SourceLocation
{
    std::string filename;
    size_t line;
    size_t column;

    SourceLocation() : filename(""), line(0), column(0) {}
    SourceLocation(std::string fname, size_t ln, size_t col)
        : filename(fname), line(ln), column(col) {}

    std::string to_string() const
    {
        return filename + ":" + std::to_string(line) + ":" + std::to_string(column);
    }

    bool operator==(const SourceLocation &other)
    {
        return filename == other.filename
            && line == other.line && column == other.column;
    }

    bool operator!=(const SourceLocation &other)
    {
        return !(*this == other);
    }
};

enum class TokenKind
{
    TK_KEYWORD_START,
    TK_ALT,              // alt
    TK_ANY,              // any
    TK_DECLARE,          // declare
    TK_ELSE,             // else
    TK_FINISH,           // finish
    TK_FOR,              // for
    TK_FUNC,             // func
    TK_FUNC_IN,          // func_in
    TK_FUNC_OUT,         // func_out
    TK_FUNC_SELF,        // func_self
    TK_GENERATE,         // generate
    TK_GOTO,             // goto
    TK_IF,               // if
    TK_INOUT,            // inout
    TK_INPUT,            // input
    TK_INTEGER,          // integer
    TK_INTERFACE,        // interface
    TK_SIMULATION,       // simulation
    TK_LABEL,            // label
    TK_LABEL_NAME,       // label_name
    TK_M_CLOCK,          // m_clock
    TK_MEM,              // mem
    TK_MODULE,           // module
    TK_OUTPUT,           // output
    TK_P_RESET,          // p_reset
    TK_PROC,             // proc
    TK_PROC_NAME,        // proc_name
    TK_REG,              // reg
    TK_RETURN,           // return
    TK_SEQ,              // seq
    TK_STATE,            // state
    TK_STATE_NAME,       // state_name
    TK_VARIABLE,         // variable
    TK_WHILE,            // while
    TK_WIRE,             // wire
    TK_STRUCT,           // struct
                            
    TK_KEYWORD_END,

    TK_IDENTIFIER,
    TK_INT,
    TK_BINARY,
    TK_HEX,
    TK_OCTAL,
    TK_SIZED_LITERAL,
    TK_STRING,

    TK_OP_PLUS,      // +
    TK_OP_MINUS,     // -
    TK_OP_INC,       // ++
    TK_OP_DEC,       // --
    TK_OP_MULTIPLY,  // *
    TK_OP_AND,       // &
    TK_OP_OR,        // |
    TK_OP_XOR,       // ^
    TK_OP_NOT,       // ~
    TK_OP_LSHIFT,    // <<
    TK_OP_RSHIFT,    // >>
    TK_OP_EQ,        // ==
    TK_OP_NEQ,       // !=
    TK_OP_LT,        // <
    TK_OP_LE,        // <=
    TK_OP_GT,        // >
    TK_OP_GE,        // >=
    TK_OP_LAND,      // &&
    TK_OP_LOR,       // ||
    TK_OP_LNOT,      // !
    TK_OP_ASSIGN,    // =
    TK_OP_ASSIGN_REG,// :=

    TK_LPAREN,     // (
    TK_RPAREN,     // )
    TK_LBRACE,     // {
    TK_RBRACE,     // }
    TK_LBRACKET,   // [
    TK_RBRACKET,   // ]
    TK_SEMICOLON,  // ;
    TK_COLON,      // :
    TK_COMMA,      // ,
    TK_DOT,        // .
    TK_HASH,       // #
    TK_APOSTROPHE, // '

    TK_EOF,
    TK_END,
};

struct Token
{
    TokenKind kind;
    std::string text;
    SourceLocation loc;

    Token() : kind(TokenKind::TK_EOF), text(""), loc() {}
    Token(TokenKind tk, const std::string &txt,
            const SourceLocation &l) :
        kind(tk), text(txt), loc(l) {}

    bool is_keyword() const
    {
        return TokenKind::TK_KEYWORD_START < kind && kind < TokenKind::TK_KEYWORD_END;
    }

    std::string to_string() const;
};
