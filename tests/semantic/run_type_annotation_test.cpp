#include "run_type_annotation_test.h"
#include "lexer.hpp"
#include "parser.hpp"
#include "semantic.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <utility>

static std::unique_ptr<Expression> makeLiteral(TypeKind kind, const std::string &value)
{
    auto literal = std::make_unique<LiteralExpr>();
    literal->value = value;
    literal->type = Type{kind, nullptr};
    return literal;
}

static std::unique_ptr<Expression> makeVariable(const std::string &name)
{
    auto variable = std::make_unique<VariableExpr>();
    variable->name = name;
    return variable;
}

static std::unique_ptr<Expression> makeBinary(BinaryOperator op,
                                              std::unique_ptr<Expression> left,
                                              std::unique_ptr<Expression> right)
{
    auto binary = std::make_unique<BinaryExpr>();
    binary->op = op;
    binary->left = std::move(left);
    binary->right = std::move(right);
    return binary;
}

// "let name = initializer;" with no declared type, so the type must be inferred
static std::unique_ptr<LetStatement> makeInferredLet(const std::string &name,
                                                     std::unique_ptr<Expression> initializer)
{
    auto let = std::make_unique<LetStatement>();
    let->name = name;
    let->type = Type{TypeKind::Void, nullptr};
    let->initializer = std::move(initializer);
    return let;
}

static std::unique_ptr<FunctionDecl> makeFunction(const std::string &name, TypeKind returnKind)
{
    auto function = std::make_unique<FunctionDecl>();
    function->name = name;
    function->returnType = Type{returnKind, nullptr};
    function->body = std::make_unique<BlockStatement>();
    return function;
}

static bool expectType(const std::string &what, const Type &actual, TypeKind expected)
{
    if (actual.kind == expected)
    {
        return true;
    }
    std::cout << "    " << what << ": expected TypeKind " << static_cast<int>(expected)
              << " but got " << static_cast<int>(actual.kind) << "\n";
    return false;
}

static bool report(const std::string &name, bool passed)
{
    std::cout << (passed ? "PASSED: " : "FAILED: ") << name << (passed ? "" : "<------|") << "\n";
    return passed;
}

// every node in "let x = 1 + 2;" gets Int, including the inferred variable
static bool testInferredLetAndArithmetic()
{
    auto function = makeFunction("test", TypeKind::Void);
    auto let = makeInferredLet("x", makeBinary(BinaryOperator::Add,
                                               makeLiteral(TypeKind::Int, "1"),
                                               makeLiteral(TypeKind::Int, "2")));
    const LetStatement *letPtr = let.get();
    const BinaryExpr *binary = static_cast<const BinaryExpr *>(letPtr->initializer.get());
    function->body->statements.push_back(std::move(let));

    Program program;
    program.declarations.push_back(std::move(function));
    SemanticAnalyser analyser;
    bool valid = analyser.analyse(program);

    bool ok = valid;
    ok &= expectType("let x", letPtr->resolvedType, TypeKind::Int);
    ok &= expectType("1 + 2", binary->resolvedType, TypeKind::Int);
    ok &= expectType("left literal", binary->left->resolvedType, TypeKind::Int);
    ok &= expectType("right literal", binary->right->resolvedType, TypeKind::Int);
    return ok;
}

// comparisons are Bool even though their operands are Float
static bool testComparisonIsBool()
{
    auto function = makeFunction("test", TypeKind::Void);
    auto let = makeInferredLet("b", makeBinary(BinaryOperator::LessThan,
                                               makeLiteral(TypeKind::Float, "1.5"),
                                               makeLiteral(TypeKind::Float, "2.5")));
    const LetStatement *letPtr = let.get();
    const BinaryExpr *binary = static_cast<const BinaryExpr *>(letPtr->initializer.get());
    function->body->statements.push_back(std::move(let));

    Program program;
    program.declarations.push_back(std::move(function));
    SemanticAnalyser analyser;
    bool ok = analyser.analyse(program);

    ok &= expectType("let b", letPtr->resolvedType, TypeKind::Bool);
    ok &= expectType("1.5 < 2.5", binary->resolvedType, TypeKind::Bool);
    ok &= expectType("operand", binary->left->resolvedType, TypeKind::Float);
    return ok;
}

// a variable reference and a call both get their type from the symbol table
static bool testVariableAndCall()
{
    auto callee = makeFunction("answer", TypeKind::Float);
    auto returnStatement = std::make_unique<ReturnStatement>();
    returnStatement->value = makeLiteral(TypeKind::Float, "4.5");
    callee->body->statements.push_back(std::move(returnStatement));

    auto function = makeFunction("test", TypeKind::Void);
    auto first = makeInferredLet("a", makeLiteral(TypeKind::Int, "7"));
    auto second = makeInferredLet("b", makeVariable("a"));
    auto call = std::make_unique<CallExpr>();
    call->name = "answer";
    auto third = makeInferredLet("c", std::move(call));

    const LetStatement *secondPtr = second.get();
    const LetStatement *thirdPtr = third.get();
    function->body->statements.push_back(std::move(first));
    function->body->statements.push_back(std::move(second));
    function->body->statements.push_back(std::move(third));

    Program program;
    program.declarations.push_back(std::move(callee));
    program.declarations.push_back(std::move(function));
    SemanticAnalyser analyser;
    bool ok = analyser.analyse(program);

    ok &= expectType("reference to a", secondPtr->initializer->resolvedType, TypeKind::Int);
    ok &= expectType("let b", secondPtr->resolvedType, TypeKind::Int);
    ok &= expectType("answer()", thirdPtr->initializer->resolvedType, TypeKind::Float);
    ok &= expectType("let c", thirdPtr->resolvedType, TypeKind::Float);
    return ok;
}

// an invalid expression is rejected and marked Error rather than left unset
static bool testErrorIsRecorded()
{
    auto function = makeFunction("test", TypeKind::Void);
    auto let = makeInferredLet("x", makeBinary(BinaryOperator::Add,
                                               makeLiteral(TypeKind::Int, "1"),
                                               makeLiteral(TypeKind::Float, "2.0")));
    const LetStatement *letPtr = let.get();
    function->body->statements.push_back(std::move(let));

    Program program;
    program.declarations.push_back(std::move(function));
    SemanticAnalyser analyser;
    bool rejected = !analyser.analyse(program);

    bool ok = rejected;
    ok &= expectType("1 + 2.0", letPtr->initializer->resolvedType, TypeKind::Error);
    ok &= expectType("let x", letPtr->resolvedType, TypeKind::Error);
    return ok;
}

// a field's length is part of its type: field<f32, 4> and field<f32, 5> do not match
static bool testFieldLengthIsPartOfType()
{
    auto analyse = [](const std::string &source) {
        Lexer lexer(source);
        lexer.tokenize();
        Parser parser(lexer.get_tokens(), source);
        Program program = parser.parse();
        SemanticAnalyser analyser;
        return analyser.analyse(program);
    };

    std::string callee = "function g(x: field<f32, 4>) -> integer { return 0; } ";

    bool sameLength = analyse(callee + "function f(y: field<f32, 4>) -> integer { return g(y); }");
    bool otherLength = analyse(callee + "function f(y: field<f32, 5>) -> integer { return g(y); }");
    bool otherElement = analyse(callee + "function f(y: field<i32, 4>) -> integer { return g(y); }");

    return sameLength && !otherLength && !otherElement;
}

bool runTypeAnnotationTests()
{
    bool passing = true;
    passing &= report("field length is part of the type", testFieldLengthIsPartOfType());
    passing &= report("inferred let and arithmetic", testInferredLetAndArithmetic());
    passing &= report("comparison is bool", testComparisonIsBool());
    passing &= report("variable reference and call", testVariableAndCall());
    passing &= report("error is recorded", testErrorIsRecorded());
    std::cout << "Type Annotation Test Result: " << (passing ? "PASSED" : "FAILED") << "\n";
    return passing;
}
