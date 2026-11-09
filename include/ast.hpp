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

#endif // LANES_AST_H