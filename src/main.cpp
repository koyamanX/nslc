#include <iostream>
#include <string>
#include "token.h"
#include "lexer.h"
#include "parser.h"

int main(int argc, char **argv)
{
    Lexer lexer("\
    declare test_inout {\
	 input a ;\
	 output b[4] ;\
	 inout c[12] ; \
	 func_in d ;\
	 func_in e(a) ; \
	 func_out f(b) ; \
	 input reti[8]; \
	 output reto[8]; \
	 func_in g : reto;\
	 func_out h(b) : reti;\
   }\
   module a {\
       wire x ;\
       wire y[4] ;\
       reg z ;\
       reg xy[8] ;\
       reg a, b, c; \
       reg x = 10 ; \
       reg z = 1'b0 ; \
       mem m1[16] ; \
       mem m2[32] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15} ; \
   }\
   "
, "sample.nsl");


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
