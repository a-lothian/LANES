#ifndef LANES_LEXER_H
#define LANES_LEXER_H

#include <string>
#include <vector>
#include "token.hpp"

class Lexer {
public:
    Lexer(const std::string& src) : src_(src) {}

    std::vector<Token> tokenize();
    // returns next token from source
    Token next();

private:
    std::string src_;
    uint pos_ = 0;

    char peek(int steps) const;
    bool peekMatch(int steps, char c) const;
    Token oneOrTwoCharToken(uint start, char peekCheck, TokenType peekSuccess, TokenType peekFail);
    Token tokenizeNumber(uint start);
};

#endif // LANES_LEXER_H