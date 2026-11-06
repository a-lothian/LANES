#include <iostream>
#include <string>
#include <vector>
#include "lexer.h"

int main() {

    std::string program = "4321->(;*==/+){:+1.21-><<=+}**!=<{-+-}}";
    Lexer l(program);

    std::vector<Token> toks = l.tokenize();

    for (size_t k = 0; k < toks.size(); k++) {
        toks[k].print();
    }

    std::cout << std::endl;
    return 0;
}