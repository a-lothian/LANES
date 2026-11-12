#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "semantic.hpp"

// creates an integer literal, such as 10
std::unique_ptr<Expression> makeInt(int value)
{
    std::unique_ptr<LiteralExpr> literal =
        std::make_unique<LiteralExpr>();

    literal->value = std::to_string(value);
    literal->type = Type{TypeKind::Int, nullptr};

    return literal;
}

// creates a boolean literal, such as true
std::unique_ptr<Expression> makeBool(bool value)
{
    std::unique_ptr<LiteralExpr> literal =
        std::make_unique<LiteralExpr>();

    literal->value = value ? "true" : "false";
    literal->type = Type{TypeKind::Bool, nullptr};

    return literal;
}

// creates a reference to an existing variable
std::unique_ptr<Expression> makeVariable(const std::string &name)
{
    std::unique_ptr<VariableExpr> variable =
        std::make_unique<VariableExpr>();

    variable->name = name;

    return variable;
}

// creates a variable declaration
std::unique_ptr<Statement> makeLet(
    const std::string &name,
    TypeKind type,
    std::unique_ptr<Expression> initializer)
{
    std::unique_ptr<LetStatement> statement =
        std::make_unique<LetStatement>();

    statement->name = name;
    statement->type = Type{type, nullptr};
    statement->initializer = std::move(initializer);

    return statement;
}

// creates a function containing the supplied statements
std::unique_ptr<FunctionDecl> makeFunction(
    const std::string &name)
{
    std::unique_ptr<FunctionDecl> function =
        std::make_unique<FunctionDecl>();

    function->name = name;
    function->returnType = Type{TypeKind::Void, nullptr};
    function->body = std::make_unique<BlockStatement>();

    return function;
}

// creates a binary expression from two smaller expressions
std::unique_ptr<Expression> makeBinary(
    BinaryOperator op,
    std::unique_ptr<Expression> left,
    std::unique_ptr<Expression> right)
{
    std::unique_ptr<BinaryExpr> binary =
        std::make_unique<BinaryExpr>();

    binary->op = op;
    binary->left = std::move(left);
    binary->right = std::move(right);

    return binary;
}

// creates a unary expression, such as -10
std::unique_ptr<Expression> makeUnary(
    UnaryOperator op,
    std::unique_ptr<Expression> operand)
{
    std::unique_ptr<UnaryExpr> unary =
        std::make_unique<UnaryExpr>();

    unary->op = op;
    unary->operand = std::move(operand);

    return unary;
}

// creates a function call
std::unique_ptr<CallExpr> makeCall(const std::string &name)
{
    std::unique_ptr<CallExpr> call =
        std::make_unique<CallExpr>();

    call->name = name;

    return call;
}

// creates a return statement
std::unique_ptr<Statement> makeReturn(
    std::unique_ptr<Expression> value)
{
    std::unique_ptr<ReturnStatement> statement =
        std::make_unique<ReturnStatement>();

    statement->value = std::move(value);

    return statement;
}

// runs the semantic analyser and checks the expected result
void runTest(
    const std::string &name,
    const Program &program,
    bool expectedValid)
{
    SemanticAnalyser analyser;

    bool actualValid = analyser.analyse(program);

    std::cout << "\nTest: " << name << "\n";

    if (actualValid == expectedValid)
    {
        std::cout << "PASS\n";
    }
    else
    {
        std::cout << "FAIL\n";
    }

    // print any semantic errors
    for (const std::string &error : analyser.getErrors())
    {
        std::cout << "  " << error << "\n";
    }
}

int main()
{
    // TEST 1: Valid integer declaration
    {
        Program program;

        std::unique_ptr<FunctionDecl> function =
            makeFunction("test");

        function->body->statements.push_back(
            makeLet("x", TypeKind::Int, makeInt(10)));

        program.declarations.push_back(std::move(function));

        runTest("Valid integer", program, true);
    }

    // TEST 2: Wrong variable type
    {
        Program program;

        std::unique_ptr<FunctionDecl> function =
            makeFunction("test");

        function->body->statements.push_back(
            makeLet("x", TypeKind::Int, makeBool(true)));

        program.declarations.push_back(std::move(function));

        runTest("Type mismatch", program, false);
    }

    // TEST 3: Duplicate variable declaration
    {
        Program program;

        std::unique_ptr<FunctionDecl> function =
            makeFunction("test");

        function->body->statements.push_back(
            makeLet("x", TypeKind::Int, makeInt(10)));

        function->body->statements.push_back(
            makeLet("x", TypeKind::Int, makeInt(20)));

        program.declarations.push_back(std::move(function));

        runTest("Duplicate variable", program, false);
    }

    // TEST 4: Undefined variable reference
    {
        Program program;

        std::unique_ptr<FunctionDecl> function =
            makeFunction("test");

        function->body->statements.push_back(
            makeLet("x", TypeKind::Int, makeVariable("y")));

        program.declarations.push_back(std::move(function));

        runTest("Undefined variable", program, false);
    }

    // TEST 5: Valid reference to an earlier variable
    {
        Program program;

        std::unique_ptr<FunctionDecl> function =
            makeFunction("test");

        function->body->statements.push_back(
            makeLet("x", TypeKind::Int, makeInt(10)));

        function->body->statements.push_back(
            makeLet("y", TypeKind::Int, makeVariable("x")));

        program.declarations.push_back(std::move(function));

        runTest("Valid variable reference", program, true);
    }

    // TEST 6: Duplicate function declaration
    {
        Program program;

        program.declarations.push_back(makeFunction("test"));
        program.declarations.push_back(makeFunction("test"));

        runTest("Duplicate function", program, false);
    }

    // TEST 7: Valid integer addition
    {
        Program program;

        std::unique_ptr<FunctionDecl> function =
            makeFunction("test");

        function->body->statements.push_back(
            makeLet(
                "x",
                TypeKind::Int,
                makeBinary(
                    BinaryOperator::Add,
                    makeInt(10),
                    makeInt(5))));

        program.declarations.push_back(std::move(function));

        runTest("Valid addition", program, true);
    }

    // TEST 8: Invalid addition between int and bool
    {
        Program program;

        std::unique_ptr<FunctionDecl> function =
            makeFunction("test");

        function->body->statements.push_back(
            makeLet(
                "x",
                TypeKind::Int,
                makeBinary(
                    BinaryOperator::Add,
                    makeInt(10),
                    makeBool(true))));

        program.declarations.push_back(std::move(function));

        runTest("Invalid addition", program, false);
    }

    // TEST 9: Valid comparison
    {
        Program program;

        std::unique_ptr<FunctionDecl> function =
            makeFunction("test");

        function->body->statements.push_back(
            makeLet(
                "x",
                TypeKind::Bool,
                makeBinary(
                    BinaryOperator::GreaterThan,
                    makeInt(10),
                    makeInt(5))));

        program.declarations.push_back(std::move(function));

        runTest("Valid comparison", program, true);
    }

    // TEST 10: Valid unary negation
    {
        Program program;

        std::unique_ptr<FunctionDecl> function =
            makeFunction("test");

        function->body->statements.push_back(
            makeLet(
                "x",
                TypeKind::Int,
                makeUnary(UnaryOperator::Negate, makeInt(10))));

        program.declarations.push_back(std::move(function));

        runTest("Valid unary negation", program, true);
    }

    // TEST 11: Invalid unary negation
    {
        Program program;

        std::unique_ptr<FunctionDecl> function =
            makeFunction("test");

        function->body->statements.push_back(
            makeLet(
                "x",
                TypeKind::Bool,
                makeUnary(UnaryOperator::Negate, makeBool(true))));

        program.declarations.push_back(std::move(function));

        runTest("Invalid unary negation", program, false);
    }

    // TEST 12: Valid function call
    {
        Program program;

        // create a function called add with two integer parameters
        std::unique_ptr<FunctionDecl> add =
            makeFunction("add");

        add->returnType = Type{TypeKind::Int, nullptr};

        add->parameters.push_back(
            Parameter{"a", Type{TypeKind::Int, nullptr}});

        add->parameters.push_back(
            Parameter{"b", Type{TypeKind::Int, nullptr}});

        program.declarations.push_back(std::move(add));

        // create another function that calls add
        std::unique_ptr<FunctionDecl> function =
            makeFunction("test");

        std::unique_ptr<CallExpr> call = makeCall("add");

        call->arguments.push_back(makeInt(5));
        call->arguments.push_back(makeInt(10));

        function->body->statements.push_back(
            makeLet("x", TypeKind::Int, std::move(call)));

        program.declarations.push_back(std::move(function));

        runTest("Valid function call", program, true);
    }

    // TEST 13: Incorrect number of arguments
    {
        Program program;

        std::unique_ptr<FunctionDecl> add =
            makeFunction("add");

        add->returnType = Type{TypeKind::Int, nullptr};

        add->parameters.push_back(
            Parameter{"a", Type{TypeKind::Int, nullptr}});

        add->parameters.push_back(
            Parameter{"b", Type{TypeKind::Int, nullptr}});

        program.declarations.push_back(std::move(add));

        std::unique_ptr<FunctionDecl> function =
            makeFunction("test");

        std::unique_ptr<CallExpr> call = makeCall("add");

        // only one argument, but add expects two
        call->arguments.push_back(makeInt(5));

        function->body->statements.push_back(
            makeLet("x", TypeKind::Int, std::move(call)));

        program.declarations.push_back(std::move(function));

        runTest("Incorrect argument count", program, false);
    }

    // TEST 14: Incorrect argument type
    {
        Program program;

        std::unique_ptr<FunctionDecl> add =
            makeFunction("add");

        add->returnType = Type{TypeKind::Int, nullptr};

        add->parameters.push_back(
            Parameter{"a", Type{TypeKind::Int, nullptr}});

        program.declarations.push_back(std::move(add));

        std::unique_ptr<FunctionDecl> function =
            makeFunction("test");

        std::unique_ptr<CallExpr> call = makeCall("add");

        // function expects an integer, but receives a boolean
        call->arguments.push_back(makeBool(true));

        function->body->statements.push_back(
            makeLet("x", TypeKind::Int, std::move(call)));

        program.declarations.push_back(std::move(function));

        runTest("Incorrect argument type", program, false);
    }

    // TEST 15: Valid return type
    {
        Program program;

        std::unique_ptr<FunctionDecl> function =
            makeFunction("test");

        function->returnType = Type{TypeKind::Int, nullptr};

        function->body->statements.push_back(
            makeReturn(makeInt(10)));

        program.declarations.push_back(std::move(function));

        runTest("Valid return type", program, true);
    }

    // TEST 16: Incorrect return type
    {
        Program program;

        std::unique_ptr<FunctionDecl> function =
            makeFunction("test");

        function->returnType = Type{TypeKind::Int, nullptr};

        function->body->statements.push_back(
            makeReturn(makeBool(true)));

        program.declarations.push_back(std::move(function));

        runTest("Incorrect return type", program, false);
    }

    // TEST 17: Valid standalone function call
    {
        Program program;

        // create a function that accepts one integer
        std::unique_ptr<FunctionDecl> helper =
            makeFunction("helper");

        helper->parameters.push_back(
            Parameter{"x", Type{TypeKind::Int, nullptr}});

        program.declarations.push_back(std::move(helper));

        // create a function that calls helper
        std::unique_ptr<FunctionDecl> function =
            makeFunction("test");

        std::unique_ptr<CallExpr> call = makeCall("helper");
        call->arguments.push_back(makeInt(10));

        std::unique_ptr<ExpressionStatement> statement =
            std::make_unique<ExpressionStatement>();

        statement->expression = std::move(call);

        function->body->statements.push_back(std::move(statement));

        program.declarations.push_back(std::move(function));

        runTest("Valid expression statement", program, true);
    }

    // TEST 18: Undefined standalone function call
    {
        Program program;

        std::unique_ptr<FunctionDecl> function =
            makeFunction("test");

        std::unique_ptr<ExpressionStatement> statement =
            std::make_unique<ExpressionStatement>();

        statement->expression = makeCall("unknown");

        function->body->statements.push_back(std::move(statement));

        program.declarations.push_back(std::move(function));

        runTest("Invalid expression statement", program, false);
    }

    return 0;
}
