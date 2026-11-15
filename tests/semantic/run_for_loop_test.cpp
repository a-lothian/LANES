#include "run_for_loop_test.h"
#include "lexer.hpp"
#include "parser.hpp"
#include "semantic.hpp"

#include <iostream>
#include <string>
#include <vector>

// for (i in start, end): integer bounds, i is a read-only integer scoped to the loop

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

// exactly one error is reported, and it contains the given text
static bool rejects(const std::string &name, const std::string &source, const std::string &expectedError)
{
    std::vector<std::string> errors = analyseSource(source);
    bool passed = errors.size() == 1 && errors[0].find(expectedError) != std::string::npos;

    std::cout << (passed ? "PASSED: " : "FAILED: ") << name << (passed ? "" : "<------|") << "\n";
    if (!passed)
    {
        std::cout << "    expected one error containing: " << expectedError << "\n";
        for (const std::string &error : errors)
        {
            std::cout << "    got: " << error << "\n";
        }
    }
    return passed;
}

static std::string function(const std::string &statements)
{
    return "function f(n: integer, x: f32) -> integer { let total = 0; " + statements + " return total; }";
}

bool runForLoopTests()
{
    bool passing = true;

    passing &= accepts("loop variable is an integer", function("for (i in 0, n) { total = total + i; }"));
    passing &= accepts("expression bounds", function("for (i in n - 1, n * 2) { total = total + i; }"));
    passing &= accepts("nested loops", function("for (i in 0, n) { for (j in 0, i) { total = total + j; } }"));
    passing &= accepts("loops in sequence reuse the name",
                       function("for (i in 0, 2) { total = total + i; } for (i in 0, 3) { total = total + i; }"));
    passing &= accepts("bounds use an outer variable of the same name",
                       function("let i = 3; for (i in 0, i) { total = total + i; }"));
    passing &= accepts("the body may shadow the loop variable", function("for (i in 0, n) { let i = 5; i = 6; }"));
    passing &= accepts("return inside a loop", function("for (i in 0, n) { if i == 3 { return i; } }"));

    passing &= rejects("float bound", function("for (i in 0, x) { }"), "Range bounds of 'for' must be integers");
    passing &= rejects("bool bound", function("for (i in true, n) { }"), "Range bounds of 'for' must be integers");
    passing &= rejects("assigning the loop variable", function("for (i in 0, n) { i = i + 1; }"),
                       "Cannot assign to loop variable: 'i'");
    passing &= rejects("loop variable is not a float", function("for (i in 0, n) { x = i; }"),
                       "Type mismatch in assignment to: 'x'");
    passing &= rejects("loop variable is gone after the loop", function("for (i in 0, n) { } total = i;"),
                       "Undefined variable: 'i'");

    // a loop is not guaranteed to run, so it does not count as returning
    passing &= rejects("return only inside a loop",
                       "function f(n: integer) -> integer { for (i in 0, n) { return i; } }",
                       "Missing return statement");

    // rulesets are straight-line
    passing &= rejects("for loop in a ruleset",
                       "ruleset r(x: field<f32, 4>) -> field<f32, 4> { for (i in 0, 3) { } return x; }",
                       "Ruleset 'r' cannot contain for loops");

    std::cout << "For Loop Semantic Test Result: " << (passing ? "PASSED" : "FAILED") << "\n";
    return passing;
}
