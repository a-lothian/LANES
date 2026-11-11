#ifndef LANES_PARSER_H
#define LANES_PARSER_H

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "ast.hpp"
#include "token.hpp"

class ParseError : public std::runtime_error
{
public:
    ParseError(const std::string &message, Span span)
        : std::runtime_error(message), span(span) {}

    Span span;
};

class Parser
{
public:
    Parser(const std::vector<Token> &tokens, const std::string &source);

    Program parse();

private:
    std::vector<Token> tokens_;
    std::string source_;
    std::size_t current_ = 0;

    const Token &peek() const;
    const Token &previous() const;
    bool isAtEnd() const;
    bool check(TokenType type) const;
    bool match(TokenType type);
    const Token &advance();
    const Token &consume(TokenType type, const std::string &message);

    std::string lexeme(const Token &token) const;
    [[noreturn]] void fail(const Token &token, const std::string &message) const;

    std::unique_ptr<Declaration> parseDeclaration();
    std::unique_ptr<Declaration> parseFunctionDeclaration(bool isRuleset);
    std::vector<Parameter> parseParameters();
    Parameter parseParameter();
    Type parseType();

    std::unique_ptr<BlockStatement> parseBlock();
    std::unique_ptr<Statement> parseStatement();
    std::unique_ptr<Statement> parseLetStatement();
    std::unique_ptr<Statement> parseReturnStatement();
    std::unique_ptr<Statement> parseExpressionStatement();

    std::unique_ptr<Expression> parseExpression();
    std::unique_ptr<Expression> parsePrimary();
};

#endif