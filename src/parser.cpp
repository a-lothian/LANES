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

    // a lexer ERROR token cannot form part of valid LANES syntax, so we're rejecting it before constructing a partial ast (malformed input)
    // as a result it will produce a clearer indication rather than just a misleading (syntax) error
    for (const Token &token : tokens_)
    {
        if (token.type == TokenType::ERROR)
        {
            fail(token, "Invalid token.");
        }
    }

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
    std::size_t offset = token.span.begin;

    if (offset > source_.size())
    {
        offset = source_.size();
    }

    std::size_t line = 1;
    std::size_t column = 1;

    for (std::size_t i = 0; i < offset; i++)
    {
        if (source_[i] == '\n')
        {
            line++;
            column = 1;
        }
        else
        {
            column++;
        }
    }

    std::string found;

    if (token.type == TokenType::Eof)
    {
        found = "end of file";
    }
    else if (token.span.begin <= token.span.end &&
             token.span.end <= source_.size())
    {
        found = "'" + source_.substr(token.span.begin, token.span.end - token.span.begin) +
                "'";
    }
    else
    {
        found = tokenSymbol(token.type);
    }

    std::string diagnostic =
        message +
        " Found " + found +
        " at line " + std::to_string(line) +
        ", column " + std::to_string(column) + ".";

    throw ParseError(diagnostic, token.span);
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
    consume(TokenType::LeftBrace, "Expected '{' to start block.");

    auto block = std::make_unique<BlockStatement>();

    while (!check(TokenType::RightBrace) && !isAtEnd())
    {
        block->statements.push_back(parseStatement());
    }

    consume(TokenType::RightBrace, "Expected '}' after block.");
    return block;
}

std::unique_ptr<Statement> Parser::parseStatement()
{
    if (match(TokenType::KeywordLet))
    {
        return parseLetStatement();
    }

    if (match(TokenType::KeywordReturn))
    {
        return parseReturnStatement();
    }

    if (check(TokenType::LeftBrace))
    {
        return parseBlock();
    }

    return parseExpressionStatement();
}

std::unique_ptr<Statement> Parser::parseLetStatement()
{
    const Token &nameToken =
        consume(
            TokenType::Name,
            "Expected variable name after 'let'.");

    auto statement = std::make_unique<LetStatement>();
    statement->name = lexeme(nameToken);

    // the lang (LANES) currently accepts both: ( let x: i32 = 5; ) and ( let x = 5; )
    //
    // current ast does not have a separate Unknown/Inferred type
    // TypeKind::Void used temporarily for an omitted type (because of this^)
    // actual type can be inferred / validated later during semantic analysis
    statement->type = Type{TypeKind::Void, nullptr};

    if (match(TokenType::Colon))
    {
        statement->type = parseType();
    }

    consume(
        TokenType::Equal,
        "Expected '=' in variable declaration.");

    statement->initializer = parseExpression();

    consume(
        TokenType::SemiColon,
        "Expected ';' after variable declaration.");

    return statement;
}

std::unique_ptr<Statement> Parser::parseReturnStatement()
{
    auto statement = std::make_unique<ReturnStatement>();

    if (!check(TokenType::SemiColon))
    {
        statement->value = parseExpression();
    }

    consume(
        TokenType::SemiColon,
        "Expected ';' after return statement.");

    return statement;
}

std::unique_ptr<Statement> Parser::parseExpressionStatement()
{
    auto statement = std::make_unique<ExpressionStatement>();

    statement->expression = parseExpression();

    consume(
        TokenType::SemiColon,
        "Expected ';' after expression.");

    return statement;
}

std::unique_ptr<Expression> Parser::parseExpression()
{
    return parseEquality();
}

std::unique_ptr<Expression> Parser::parseEquality()
{
    std::unique_ptr<Expression> expression = parseComparison();

    while (
        check(TokenType::EqualTo) ||
        check(TokenType::NotEqualTo))
    {
        TokenType operatorType = advance().type;

        std::unique_ptr<Expression> right =
            parseComparison();

        auto binary = std::make_unique<BinaryExpr>();

        binary->op =
            operatorType == TokenType::EqualTo
                ? BinaryOperator::Equal
                : BinaryOperator::NotEqual;

        binary->left = std::move(expression);
        binary->right = std::move(right);

        expression = std::move(binary);
    }

    return expression;
}

std::unique_ptr<Expression> Parser::parseComparison()
{
    std::unique_ptr<Expression> expression = parseTerm();

    while (
        check(TokenType::LessThan) ||
        check(TokenType::GreaterThan) ||
        check(TokenType::LessThanEqualTo) ||
        check(TokenType::GreaterThanEqualTo))
    {
        TokenType operatorType = advance().type;

        std::unique_ptr<Expression> right =
            parseTerm();

        auto binary = std::make_unique<BinaryExpr>();

        switch (operatorType)
        {
        case TokenType::LessThan:
            binary->op =
                BinaryOperator::LessThan;
            break;

        case TokenType::GreaterThan:
            binary->op =
                BinaryOperator::GreaterThan;
            break;

        case TokenType::LessThanEqualTo:
            binary->op =
                BinaryOperator::LessThanEqual;
            break;

        case TokenType::GreaterThanEqualTo:
            binary->op =
                BinaryOperator::GreaterThanEqual;
            break;

        default:
            fail(
                previous(),
                "Invalid comparison operator.");
        }

        binary->left = std::move(expression);
        binary->right = std::move(right);

        expression = std::move(binary);
    }

    return expression;
}

std::unique_ptr<Expression> Parser::parseTerm()
{
    std::unique_ptr<Expression> expression =
        parseFactor();

    while (
        check(TokenType::Plus) ||
        check(TokenType::Minus))
    {
        TokenType operatorType = advance().type;

        std::unique_ptr<Expression> right =
            parseFactor();

        auto binary = std::make_unique<BinaryExpr>();

        binary->op =
            operatorType == TokenType::Plus
                ? BinaryOperator::Add
                : BinaryOperator::Subtract;

        binary->left = std::move(expression);
        binary->right = std::move(right);

        expression = std::move(binary);
    }

    return expression;
}

std::unique_ptr<Expression> Parser::parseFactor()
{
    std::unique_ptr<Expression> expression =
        parseUnary();

    while (
        check(TokenType::Star) ||
        check(TokenType::Slash))
    {
        TokenType operatorType = advance().type;

        std::unique_ptr<Expression> right =
            parseUnary();

        auto binary = std::make_unique<BinaryExpr>();

        binary->op =
            operatorType == TokenType::Star
                ? BinaryOperator::Multiply
                : BinaryOperator::Divide;

        binary->left = std::move(expression);
        binary->right = std::move(right);

        expression = std::move(binary);
    }

    return expression;
}

std::unique_ptr<Expression> Parser::parseUnary()
{
    if (match(TokenType::Minus))
    {
        auto expression =
            std::make_unique<UnaryExpr>();

        expression->op = UnaryOperator::Negate;
        expression->operand = parseUnary();

        return expression;
    }

    return parsePrimary();
}

std::unique_ptr<Expression> Parser::parsePrimary()
{
    if (match(TokenType::LiteralInt))
    {
        auto expression =
            std::make_unique<LiteralExpr>();

        expression->value = lexeme(previous());
        expression->type =
            Type{TypeKind::Int, nullptr};

        return expression;
    }

    if (match(TokenType::LiteralFloat))
    {
        auto expression =
            std::make_unique<LiteralExpr>();

        expression->value = lexeme(previous());
        expression->type =
            Type{TypeKind::Float, nullptr};

        return expression;
    }

    if (match(TokenType::Name))
    {
        const Token &nameToken = previous();

        std::string name = lexeme(nameToken);

        // true and false currently tokenised as names by the lexer -> so the parser recognises them here
        if (name == "true" || name == "false")
        {
            auto expression =
                std::make_unique<LiteralExpr>();

            expression->value = name;
            expression->type =
                Type{TypeKind::Bool, nullptr};

            return expression;
        }

        // name immediately followed by '(' for function calling
        if (match(TokenType::LeftBracket))
        {
            auto expression =
                std::make_unique<CallExpr>();

            expression->name = std::move(name);

            if (!check(TokenType::RightBracket))
            {
                do
                {
                    expression->arguments.push_back(
                        parseExpression());
                } while (match(TokenType::Comma));
            }

            consume(
                TokenType::RightBracket,
                "Expected ')' after function arguments.");

            return expression;
        }

        auto expression =
            std::make_unique<VariableExpr>();

        expression->name = std::move(name);

        return expression;
    }

    if (match(TokenType::LeftBracket))
    {
        std::unique_ptr<Expression> expression =
            parseExpression();

        consume(
            TokenType::RightBracket,
            "Expected ')' after expression.");

        return expression;
    }

    fail(peek(), "Expected expression.");
}