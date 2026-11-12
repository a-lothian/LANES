#ifndef TOKEN_HPP
#define TOKEN_HPP

#include <string>
#include <iostream>
#include <string_view>

struct Span {
    uint begin;
    uint end;
    Span(uint b, uint e) : begin(b), end(e) {}
};

enum class TokenType {
    // literals and names (values)
    Name,
    LiteralInt,
    LiteralFloat,

    // keywords
    KeywordFunction,
    KeywordRuleset,
    KeywordLet,
    KeywordReturn,
    KeywordIf,
    KeywordElse,
    KeywordWhile,
    KeywordTrue,
    KeywordFalse,

    // punctuation
    LeftBracket,
    RightBracket,
    LeftBrace,
    RightBrace,
    Comma,
    Colon,
    SemiColon,
    Arrow,

    // operators
    Plus,
    Minus,
    Star,
    Slash,
    Equal,
    EqualTo,
    NotEqualTo,
    LessThan,
    GreaterThan,
    LessThanEqualTo,
    GreaterThanEqualTo,
    Eof,
    ERROR
};

std::string tokenSymbol(TokenType t);
std::string tokenName(TokenType t);

struct Token {
    TokenType type;
    Span span;

    Token(TokenType t, uint b, uint e) : type(t), span(Span(b,e)) {}

    void print() { std::cout << tokenSymbol(type); }
    void printName() { std::cout << tokenName(type); }
};

#endif // TOKEN_HPP