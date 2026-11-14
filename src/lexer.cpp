#include "lexer.hpp"
#include <unordered_map>

bool isNameStart(char c) { return isalpha((unsigned char)c) || c == '_'; }
bool isNameChar(char c)  { return isalnum((unsigned char)c) || c == '_'; }

const std::vector<Token> Lexer::get_tokens() {
    if (!initialized) {
        std::cout << "ERROR: Lexer not initialized" << std::endl;
        return std::vector<Token>();
    }
    return tokens_;
}

bool Lexer::tokenize(const std::string& src) {
    src_ = src;
    initialized = true;
    return tokenize();
}

bool Lexer::tokenize() {
    if (!initialized) {
        std::cout << "ERROR: Lexer not initialized" << std::endl;
        return 0;
    }
    tokens_.clear();
    pos_ = 0;

    bool running = true;
    while (running) {
        tokens_.emplace_back(next());
        running = tokens_.back().type == TokenType::Eof ? false : true;  // stop when EOF file added
    }

    return 1;
}

Token Lexer::next() {
    skipCommentsWhitespace();
    uint start = pos_++;
    char c = src_[start];

    // check for end of file
    if (pos_ > src_.length()) {return Token(TokenType::Eof, start, start);}

    if (isdigit(c)) { return tokenizeNumber(start);} // parse numbers
    if (isNameStart(c)) {return tokenizeKeyword(start);} // parse keywords and names

    switch (c) {
        // single character tokens
        case '(': return Token(TokenType::LeftBracket, start, pos_);
        case ')': return Token(TokenType::RightBracket, start, pos_);
        case '{': return Token(TokenType::LeftBrace, start, pos_);
        case '}': return Token(TokenType::RightBrace, start, pos_);
        case '[': return Token(TokenType::LeftSquareBracket, start, pos_);
        case ']': return Token(TokenType::RightSquareBracket, start, pos_);
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

void Lexer::skipCommentsWhitespace() {
    while (pos_ < src_.length()) {
        char c = peek(0);
        if (isspace(c)) { // skip whitespace
            pos_++;
        } else if (peekMatch(0, '/') && peekMatch(1, '/')) {  // skip comments; between `//` and `\n`
            while (pos_ < src_.length() && !(peekMatch(0, '\n'))) {pos_++;}
        } else {
            break;
        }
    }
    
}

char Lexer::peek(int steps) const {
    if (pos_+steps < src_.length()) {
        return src_[pos_+steps];
    }
    // TODO: error logging if out of bounds
    return '@';
}

bool Lexer::peekMatch(int steps, char c) const {
    return peek(steps) == c;
}

Token Lexer::oneOrTwoCharToken(uint start, char peekCheck, TokenType peekSuccess, TokenType peekFail) {
    if (peekMatch(0, peekCheck)) {
        return Token(peekSuccess, start, ++pos_);
    } else {
        return Token(peekFail, start, pos_);
    }
}

// keyword lookups
const std::unordered_map<std::string_view, TokenType> keywords = {
    {"function", TokenType::KeywordFunction},
    {"ruleset", TokenType::KeywordRuleset},
    {"let", TokenType::KeywordLet},
    {"return", TokenType::KeywordReturn},
    {"if", TokenType::KeywordIf},
    {"else", TokenType::KeywordElse},
    {"while", TokenType::KeywordWhile},
    {"true", TokenType::KeywordTrue},
    {"false", TokenType::KeywordFalse},
};

Token Lexer::tokenizeKeyword(uint start) {
    while (isNameChar(peek(0))) {pos_++;} // advance to end of keyword
    std::string_view word(src_.data() + start, pos_ - start); 
    auto itr = keywords.find(word); // lookup substring
    return Token(itr != keywords.end() ? itr->second : TokenType::Name, start, pos_); // assume name if not found
}

Token Lexer::tokenizeNumber(uint start) {
    while (isdigit(peek(0))) {pos_++;} // advance to end of number

    if (peekMatch(0, '.') && isdigit(peek(1))) {
        pos_++; // skip over `.`
        while (isdigit(peek(0))) {pos_++;} // advance to end of number
        return Token(TokenType::LiteralFloat, start, pos_);
    }
    return Token(TokenType::LiteralInt, start, pos_);
}