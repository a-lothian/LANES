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

inline std::string tokenSymbol(TokenType t) {
    switch (t) {
        case TokenType::Name: return "<name>";
        case TokenType::LiteralInt: return "<int>";
        case TokenType::LiteralFloat: return "<float>";
        case TokenType::KeywordFunction: return "function";
        case TokenType::KeywordRuleset: return "ruleset";
        case TokenType::KeywordLet: return "let";
        case TokenType::KeywordReturn: return "return";
        case TokenType::LeftBracket: return "(";
        case TokenType::RightBracket: return ")";
        case TokenType::LeftBrace: return "{";
        case TokenType::RightBrace: return "}";
        case TokenType::Comma: return ",";
        case TokenType::Colon: return ":";
        case TokenType::SemiColon: return ";";
        case TokenType::Arrow: return "->";
        case TokenType::Plus: return "+";
        case TokenType::Minus: return "-";
        case TokenType::Star: return "*";
        case TokenType::Slash: return "/";
        case TokenType::Equal: return "=";
        case TokenType::EqualTo: return "==";
        case TokenType::NotEqualTo: return "!=";
        case TokenType::LessThan: return "<";
        case TokenType::GreaterThan: return ">";
        case TokenType::LessThanEqualTo: return "<=";
        case TokenType::GreaterThanEqualTo: return ">=";
        case TokenType::Eof: return "<eof>";
        case TokenType::ERROR: return "<error>";
    }
    return "<UNKNOWN TOKEN>";
}

std::string tokenName(TokenType t) {
    switch (t) {
        case TokenType::Name: return "Name";
        case TokenType::LiteralInt: return "Literal int";
        case TokenType::LiteralFloat: return "Literal float";
        case TokenType::KeywordFunction: return "Keyword function";
        case TokenType::KeywordRuleset: return "Keyword ruleset";
        case TokenType::KeywordLet: return "Keyword let";
        case TokenType::KeywordReturn: return "Keyword return";
        case TokenType::LeftBracket: return "Left bracket";
        case TokenType::RightBracket: return "Right bracket";
        case TokenType::LeftBrace: return "Left brace";
        case TokenType::RightBrace: return "Right brace";
        case TokenType::Comma: return "Comma";
        case TokenType::Colon: return "Colon";
        case TokenType::SemiColon: return "Semicolon";
        case TokenType::Arrow: return "Arrow";
        case TokenType::Plus: return "Plus";
        case TokenType::Minus: return "Minus";
        case TokenType::Star: return "Star";
        case TokenType::Slash: return "Slash";
        case TokenType::Equal: return "Equal";
        case TokenType::EqualTo: return "Equal to";
        case TokenType::NotEqualTo: return "Not equal to";
        case TokenType::LessThan: return "Less than";
        case TokenType::GreaterThan: return "Greater than";
        case TokenType::LessThanEqualTo: return "Less than equal to";
        case TokenType::GreaterThanEqualTo: return "Greater than equal to";
        case TokenType::Eof: return "Eof";
        case TokenType::ERROR: return "Error";
    }
    return "UNKNOWN TOKEN";
}

struct Token {
    TokenType type;
    Span span;

    Token(TokenType t, uint b, uint e) : type(t), span(Span(b,e)) {}
    void print() { std::cout << tokenSymbol(type) << '\n'; }
    void printName() { std::cout << tokenName(type) << '\n'; }
};