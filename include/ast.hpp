#ifndef LANES_AST_H
#define LANES_AST_H

#include <string>
#include <vector>
#include <memory>

enum class TypeKind
{
    Int,
    Float,
    Bool,
    Void,
    Field
};

// a data type
struct Type
{
    TypeKind kind;
    std::shared_ptr<Type> elementType;
};

// base structure for all expressions
struct Expression
{
    virtual ~Expression() = default;
};

// base structure for all statements
struct Statement
{
    virtual ~Statement() = default;
};

// operators used in expressions
enum class BinaryOperator
{
    Add,
    Subtract,
    Multiply,
    Divide,
    Equal,
    NotEqual,
    LessThan,
    GreaterThan,
    LessThanEqual,
    GreaterThanEqual
};

// unary operators
enum class UnaryOperator
{
    Negate
};

// represents a binary expression, e.g. a + b
struct BinaryExpr : Expression
{
    BinaryOperator op;
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
};

// represents a unary expression, e.g. -a
struct UnaryExpr : Expression
{
    UnaryOperator op;
    std::unique_ptr<Expression> operand;
};

// represents a literal value, e.g. 10 or 5.5
struct LiteralExpr : Expression
{
    std::string value;
    Type type;
};

// represents a variable reference, e.g. result
struct VariableExpr : Expression
{
    std::string name;
};

// represents a function call, e.g. add(a, b)
struct CallExpr : Expression
{
    std::string name;
    std::vector<std::unique_ptr<Expression>> arguments;
};

// represents a function parameter
struct Parameter
{
    std::string name;
    Type type;
};

// represents a variable declaration
struct LetStatement : Statement
{
    std::string name;
    Type type;
    std::unique_ptr<Expression> initializer;
};

// represents a return statement
struct ReturnStatement : Statement
{
    std::unique_ptr<Expression> value;
};

// represents a standalone expression statement
struct ExpressionStatement : Statement
{
    std::unique_ptr<Expression> expression;
};

// represents a block of statements
struct BlockStatement : Statement
{
    std::vector<std::unique_ptr<Statement>> statements;
};

// base structure for declarations
struct Declaration
{
    virtual ~Declaration() = default;
};

// represents a normal function declaration
struct FunctionDecl : Declaration
{
    std::string name;
    std::vector<Parameter> parameters;
    Type returnType;
    std::unique_ptr<BlockStatement> body;
};

// represents a Ruleset declaration
struct RulesetDecl : Declaration
{
    std::string name;
    std::vector<Parameter> parameters;
    Type returnType;
    std::unique_ptr<BlockStatement> body;
};

// represents the entire LANES program
struct Program
{
    std::vector<std::unique_ptr<Declaration>> declarations;
};

#endif // LANES_AST_H