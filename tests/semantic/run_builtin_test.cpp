#include "run_builtin_test.h"
#include "lexer.hpp"
#include "parser.hpp"
#include "semantic.hpp"

#include <iostream>
#include <string>
#include <vector>

static std::vector<std::string> analyseSource(const std::string &source)
{
    Lexer lexer(source);
    lexer.tokenize();
    Parser parser(lexer.get_tokens(), source);
    Program program = parser.parse();

    SemanticAnalyser analyser;
    analyser.analyse(program);
    return analyser.getErrors();
}

static bool accepts(const std::string &name, const std::string &source)
{
    std::vector<std::string> errors = analyseSource(source);
    bool passed = errors.empty();

    std::cout << (passed ? "PASSED: " : "FAILED: ") << name << (passed ? "" : "<------|") << "\n";
    for (const std::string &error : errors)
    {
        std::cout << "    unexpected error: " << error << "\n";
    }
    return passed;
}

// the analyser must report an error containing the given text
static bool rejects(const std::string &name, const std::string &source, const std::string &expectedError)
{
    std::vector<std::string> errors = analyseSource(source);
    bool passed = false;
    for (const std::string &error : errors)
    {
        if (error.find(expectedError) != std::string::npos)
        {
            passed = true;
        }
    }

    std::cout << (passed ? "PASSED: " : "FAILED: ") << name << (passed ? "" : "<------|") << "\n";
    if (!passed)
    {
        std::cout << "    expected error containing: " << expectedError << "\n";
        for (const std::string &error : errors)
        {
            std::cout << "    got: " << error << "\n";
        }
    }
    return passed;
}

// wraps statements in a function body
static std::string body(const std::string &statements)
{
    return "function f(n: integer, x: f32) -> integer { " + statements + " return 0; }";
}

bool runBuiltinTests()
{
    bool passing = true;

    // print
    passing &= accepts("print scalars", body("print(n); print(x * 2.0); print(n < 3); print(true);"));
    passing &= accepts("print fields", body("print([1, 2]); let a = [x, x]; print(a); print([true]);"));
    passing &= accepts("print a call result",
                       "function g() -> integer { return 1; } " + body("print(g());"));
    passing &= rejects("print with no arguments", body("print();"),
                       "Incorrect number of arguments for function: 'print'");
    passing &= rejects("print with two arguments", body("print(1, 2);"),
                       "Incorrect number of arguments for function: 'print'");
    passing &= rejects("print a void call",
                       "function g() -> void { } " + body("print(g());"),
                       "print requires an integer, float, bool or field argument");
    passing &= rejects("print argument with an error", body("print(missing);"),
                       "Undefined variable: 'missing'");
    passing &= rejects("print has no value", body("let y = print(1);"),
                       "Cannot infer type for variable: 'y'");

    // reserved names
    passing &= rejects("function named print",
                       "function print(n: integer) -> integer { return n; }",
                       "Cannot redeclare built-in: 'print'");
    passing &= rejects("ruleset named print",
                       "ruleset print(n: integer) -> integer { return n; }",
                       "Cannot redeclare built-in: 'print'");
    passing &= rejects("function named with __",
                       "function __print_i32(n: integer) -> integer { return n; }",
                       "Reserved name: '__print_i32'");
    passing &= accepts("single underscore is allowed",
                       "function _helper(n: integer) -> integer { return n; }");

    std::cout << "Built-in Semantic Test Result: " << (passing ? "PASSED" : "FAILED") << "\n";
    return passing;
}
