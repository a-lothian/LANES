#include <iostream>
#include <string>
#include <vector>
#include "token.hpp"


class Lexer {
    public:
    Lexer(std::string& src) : src_(src) {}
    struct Token next() {}

    private:
    std::string src_;
    uint pos = 0;
};


int main(int k, char** args) {

    std::string program = "let()!=<floatint{}";
    std::vector<Token> toks;
    Lexer l(program);

    toks.emplace_back(l.next());

    return 0;
}