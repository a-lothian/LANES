#include <iostream>
#include <string>
#include <vector>
#include "lexer.hpp"

int main() {
    // take input from terminal, tokenize, and print result
    std::string input;
    std::cin >> input;

    Lexer l(input);
    l.tokenize();
    
    std::vector<Token> toks = l.get_tokens();

    for (size_t k = 0; k < toks.size(); k++) {
        toks[k].print();
    }
    std::cout << std::endl;
    return 0;
}