#include "parser.hpp"

#include <utility>

Parser::Parser(const std::vector<Token> &tokens, const std::string &source)
    : tokens_(tokens), source_(source)
{
    if (tokens_.empty() || tokens_.back().type != TokenType::Eof)
    {
        tokens_.emplace_back(TokenType::Eof, source_.size(), source_.size());
    }
}

Program Parser::parse()
{
    Program program;

    while (!isAtEnd())
    {
        program.declarations.push_back(parseDeclaration());
    }

    return program;
}

const Token &Parser::peek() const
{
    return tokens_[current_];
}

const Token &Parser::previous() const
{
    return tokens_[current_ - 1];
}

bool Parser::isAtEnd() const
{
    return peek().type == TokenType::Eof;
}

bool Parser::check(TokenType type) const
{
    if (isAtEnd())
    {
        return type == TokenType::Eof;
    }

    return peek().type == type;
}

bool Parser::match(TokenType type)
{
    if (!check(type))
    {
        return false;
    }

    advance();
    return true;
}

const Token &Parser::advance()
{
    if (!isAtEnd())
    {
        current_++;
    }

    return previous();
}

const Token &Parser::consume(TokenType type, const std::string &message)
{
    if (check(type))
    {
        return advance();
    }

    fail(peek(), message);
}

std::string Parser::lexeme(const Token &token) const
{
    if (token.span.begin > token.span.end || token.span.end > source_.size())
    {
        fail(token, "Token span is outside the source text.");
    }

    return source_.substr(
        token.span.begin,
        token.span.end - token.span.begin);
}

[[noreturn]] void Parser::fail(
    const Token &token,
    const std::string &message) const
{
    throw ParseError(message, token.span);
}

std::unique_ptr<Declaration> Parser::parseDeclaration()
{
    if (match(TokenType::KeywordFunction))
    {
        return parseFunctionDeclaration(false);
    }

    if (match(TokenType::KeywordRuleset))
    {
        return parseFunctionDeclaration(true);
    }

    fail(peek(), "Expected a function or ruleset declaration.");
}

std::unique_ptr<Declaration> Parser::parseFunctionDeclaration(bool isRuleset)
{
    const Token &nameToken =
        consume(TokenType::Name, "Expected declaration name.");

    std::string name = lexeme(nameToken);

    consume(
        TokenType::LeftBracket,
        "Expected '(' after declaration name.");

    std::vector<Parameter> parameters = parseParameters();

    consume(
        TokenType::RightBracket,
        "Expected ')' after parameters.");

    consume(
        TokenType::Arrow,
        "Expected '->' before return type.");

    Type returnType = parseType();

    std::unique_ptr<BlockStatement> body = parseBlock();

    if (isRuleset)
    {
        auto declaration = std::make_unique<RulesetDecl>();

        declaration->name = std::move(name);
        declaration->parameters = std::move(parameters);
        declaration->returnType = std::move(returnType);
        declaration->body = std::move(body);

        return declaration;
    }

    auto declaration = std::make_unique<FunctionDecl>();

    declaration->name = std::move(name);
    declaration->parameters = std::move(parameters);
    declaration->returnType = std::move(returnType);
    declaration->body = std::move(body);

    return declaration;
}

std::vector<Parameter> Parser::parseParameters()
{
    std::vector<Parameter> parameters;

    if (check(TokenType::RightBracket))
    {
        return parameters;
    }

    do
    {
        parameters.push_back(parseParameter());
    } while (match(TokenType::Comma));

    return parameters;
}

Parameter Parser::parseParameter()
{
    const Token &nameToken =
        consume(TokenType::Name, "Expected parameter name.");

    std::string name = lexeme(nameToken);

    consume(
        TokenType::Colon,
        "Expected ':' after parameter name.");

    Parameter parameter;

    parameter.name = std::move(name);
    parameter.type = parseType();

    return parameter;
}

Type Parser::parseType()
{
    const Token &typeToken =
        consume(TokenType::Name, "Expected type name.");

    std::string name = lexeme(typeToken);

    if (name == "i32" || name == "integer")
    {
        return Type{TypeKind::Int, nullptr};
    }

    if (name == "f32" || name == "float")
    {
        return Type{TypeKind::Float, nullptr};
    }

    if (name == "bool")
    {
        return Type{TypeKind::Bool, nullptr};
    }

    if (name == "void")
    {
        return Type{TypeKind::Void, nullptr};
    }

    if (name == "field")
    {
        consume(
            TokenType::LessThan,
            "Expected '<' after 'field'.");

        Type elementType = parseType();

        consume(
            TokenType::GreaterThan,
            "Expected '>' after field element type.");

        Type fieldType;

        fieldType.kind = TypeKind::Field;
        fieldType.elementType =
            std::make_shared<Type>(std::move(elementType));

        return fieldType;
    }

    fail(typeToken, "Unknown type '" + name + "'.");
}

std::unique_ptr<BlockStatement> Parser::parseBlock()
{
    consume(
        TokenType::LeftBrace,
        "Expected '{' to start block.");

    auto block = std::make_unique<BlockStatement>();

    // Statement parsing will be added in the next parser milestone.
    // Empty blocks establish the parser and AST interface first.
    if (!check(TokenType::RightBrace))
    {
        fail(
            peek(),
            "Statement parsing is not implemented yet.");
    }

    consume(
        TokenType::RightBrace,
        "Expected '}' after block.");

    return block;
}