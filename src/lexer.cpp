#include <iostream>
#include <string>
#include <vector>
#include "include/token.hpp"


class Lexer {
    public:
    Lexer(std::string& src) : src_(src) {}
    struct Token next() {
        int start = pos_++;
        char c = src_[start];

        switch (c) {
            // single character tokens
            case '(': return Token(TokenType::LeftBracket, start, pos_);
            case ')': return Token(TokenType::RightBracket, start, pos_);
            case '{': return Token(TokenType::LeftBrace, start, pos_);
            case '}': return Token(TokenType::RightBrace, start, pos_);
            case ',': return Token(TokenType::Comma, start, pos_);
            case ':': return Token(TokenType::Colon, start, pos_);
            case ';': return Token(TokenType::SemiColon, start, pos_);
            case '+': return Token(TokenType::Plus, start, pos_);
            case '*': return Token(TokenType::Star, start, pos_);
            case '/': return Token(TokenType::Slash, start, pos_);
            // two character tokens
            case '-': return oneOrTwoCharToken(start, '>', TokenType::Arrow, TokenType::Minus);
            case '=': return oneOrTwoCharToken(start, '=', TokenType::EqualTo, TokenType::Equal);
            case '<': return oneOrTwoCharToken(start, '=', TokenType::LessThanEqualTo, TokenType::LessThan);
            case '>': return oneOrTwoCharToken(start, '=', TokenType::GreaterThanEqualTo, TokenType::GreaterThan);
            case '!': return oneOrTwoCharToken(start, '=', TokenType::NotEqualTo, TokenType::ERROR);

        }
        return Token(TokenType::ERROR, start, pos_);
    }

    private:
    std::string src_;
    uint pos_ = 0;

    bool peekMatch(char c) {
        return src_[pos_] == c;
    }

    Token oneOrTwoCharToken(uint start, char peekCheck, TokenType peekSuccess, TokenType peekFail) {
        if (peekMatch(peekCheck)) {
            return Token(peekSuccess, start, ++pos_);
        } else {
            return Token(peekFail, start, pos_);
        }
    }
};


int main(int k, char** args) {

    std::string program = "->(;*==/+){:+-><<=+}";
    std::vector<Token> toks;
    Lexer l(program);

    for (int i = 0; i < program.length(); i++) {
        toks.emplace_back(l.next());
    }

    for (int k = 0; k < program.length(); k++) {
        toks[k].print();
    }

    return 0;
}