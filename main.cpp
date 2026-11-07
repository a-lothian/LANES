#include <iostream>
#include <string>
#include <vector>
#include "lexer.h"
#include "file_reader.hpp"

int main() {
    // read file from disk, tokenize, and print result
    FileReader reader("tests/tokenizer/input/alltokens.txt");
    if (!reader.dumpFile()) {
        return 1;
    }
    Lexer l(reader.document);
    std::vector<Token> toks = l.tokenize();

    for (size_t k = 0; k < toks.size(); k++) {
        toks[k].print();
    }
    std::cout << std::endl;
    return 0;
}