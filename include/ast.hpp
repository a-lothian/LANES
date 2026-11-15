#ifndef LANES_AST_H
#define LANES_AST_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

enum class TypeKind
{
    Int,
    Float,
    Bool,
    Void,
    Field,
    Error
};

// a data type
struct Type
{
    TypeKind kind;
    std::shared_ptr<Type> elementType;

    // only for field types
    std::uint32_t length = 0;
};

// base structure for all expressions
struct Expression
{
    virtual ~Expression() = default;
    mutable Type resolvedType{TypeKind::Error, nullptr};  // mutable allows this to altered by the semantic analyser even when passed in const
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
    GreaterThanEqual,
    And,
    Or
};

// unary operators
enum class UnaryOperator
{
    Negate,
    Not
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

// represents a field literal, e.g. [1.0, 2.0, x * 2.0]
struct FieldLiteralExpr : Expression
{
    std::vector<std::unique_ptr<Expression>> elements;
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
    Type type; // as written in the source; Void means "infer it"
    std::unique_ptr<Expression> initializer;

    // the variable's final type
    mutable Type resolvedType{TypeKind::Error, nullptr};
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

// represents assignment to an existing variable
struct AssignmentStatement : Statement
{
    std::string name;
    std::unique_ptr<Expression> value;
};

// represents if statement
struct IfStatement : Statement
{
    std::unique_ptr<Expression> condition;
    std::unique_ptr<BlockStatement> thenBlock;

    // optional for else or else if statments
    std::unique_ptr<Statement> elseBranch;
};

// represents a loop
struct WhileStatement : Statement
{
    std::unique_ptr<Expression> condition;
    std::unique_ptr<BlockStatement> body;
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