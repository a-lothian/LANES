#ifndef LANES_LEXER_H
#define LANES_LEXER_H

#include <string>
#include <vector>
#include "token.hpp"

class Lexer {
public:
    Lexer(const std::string& src) : initialized(true), src_(src){}
    Lexer() {}

    // tokenize entire string
    bool tokenize();
    bool tokenize(const std::string& src);

    // get tokenized result
    const std::vector<Token> get_tokens();

    // returns next token from source
    Token next();

private:
    bool initialized;
    std::string src_;
    uint pos_ = 0;
    std::vector<Token> tokens_;

    char peek(int steps) const;
    bool peekMatch(int steps, char c) const;
    Token oneOrTwoCharToken(uint start, char peekCheck, TokenType peekSuccess, TokenType peekFail); // greedy handling e.g '-' vs '->'
    Token tokenizeNumber(uint start);
    Token tokenizeKeyword(uint start);


};

#endif // LANES_LEXER_H