#include "run_ruleset_call_test.h"
#include "lexer.hpp"
#include "parser.hpp"
#include "semantic.hpp"

#include <iostream>
#include <string>
#include <vector>

// rulesets are called like functions: argument count and types are checked,
// and rulesets join the call graph used by the recursion check

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

// a ruleset taking a field and a uniform, used by the tests below
static const std::string SCALE =
    "ruleset scale(x: field<f32, 4>, k: f32) -> field<f32, 4> { return x * k; } ";

bool runRulesetCallTests()
{
    bool passing = true;

    passing &= accepts("function calls a ruleset",
                       SCALE + "function main() -> integer { "
                               "  let a = scale([1.0, 2.0, 3.0, 4.0], 2.0); print(a); return 0; }");
    passing &= accepts("ruleset calls a ruleset",
                       SCALE + "ruleset twice(x: field<f32, 4>) -> field<f32, 4> { "
                               "  return scale(scale(x, 2.0), 2.0); }");
    passing &= accepts("ruleset result has the declared type",
                       SCALE + "function f(a: field<f32, 4>) -> integer { "
                               "  let b: field<f32, 4> = scale(a, 1.0); return 0; }");
    passing &= accepts("ruleset declared after its caller",
                       "function main() -> f32 { return half(1.0); } "
                       "ruleset half(s: f32) -> f32 { return s / 2.0; }");

    passing &= rejects("too few arguments to a ruleset",
                       SCALE + "function f(a: field<f32, 4>) -> integer { scale(a); return 0; }",
                       "Incorrect number of arguments for function: 'scale'");
    passing &= rejects("field length mismatch in ruleset argument",
                       SCALE + "function f(a: field<f32, 3>) -> integer { scale(a, 1.0); return 0; }",
                       "Argument type mismatch in function: 'scale'");
    passing &= rejects("uniform passed where a field is expected",
                       SCALE + "function f() -> integer { scale(1.0, 1.0); return 0; }",
                       "Argument type mismatch in function: 'scale'");
    passing &= rejects("ruleset without a return",
                       "ruleset r(x: field<f32, 4>) -> field<f32, 4> { let y = x; }",
                       "Missing return statement in ruleset: 'r'");

    // rulesets are part of the recursion check
    passing &= rejects("recursion through a ruleset",
                       "function f(n: integer) -> integer { "
                       "  if n == 0 { return 0; } "
                       "  return g(n - 1); "
                       "} "
                       "ruleset g(n: integer) -> integer { let a = [n]; return f(n); }",
                       "Recursive function cannot use fields: 'g'");

    std::cout << "Ruleset Call Semantic Test Result: " << (passing ? "PASSED" : "FAILED") << "\n";
    return passing;
}
