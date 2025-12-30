#include <iostream>
#include <string>
#include "token.h"
#include "lexer.h"
#include "parser.h"

int main(int argc, char **argv)
{
    Lexer lexer("declare hello simulation {input a; output b; input x, y, z[32], zz;inout c; xxx; input x[32]; xxx; func_in f(a,b) : c; func_out b; func_in c : a;} module a {}", "sample.nsl");
    auto tokens = lexer.tokenize();
    for (auto it = tokens.begin(); it != tokens.end(); it++)
        std::cout << it->to_string() << std::endl;

    std::string filename = "sample.nsl";
    Parser parser(tokens, filename);
    auto ast = parser.parse();

    for (const auto &err : parser.get_errors()) {
        std::cerr << err << std::endl;
    }
    // Lexer
    // Parse
    // Semantics
    // Generator

    return 0;
}
