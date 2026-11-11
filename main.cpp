#include <iostream>
#include <string>
#include <vector>
#include <file_reader.hpp>
#include "lexer.hpp"

int main() {
    // take input from input.txt, tokenize, and print result
    FileReader reader;
    reader.dumpFile("input.txt");

    Lexer l(reader.document);
    l.tokenize();

    std::vector<Token> toks = l.get_tokens();

    for (size_t k = 0; k < toks.size(); k++) {
        toks[k].print();
    }
    std::cout << std::endl;

    return 0;
}