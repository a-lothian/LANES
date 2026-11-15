#include "run_logical_test.h"
#include "lexer.hpp"
#include "parser.hpp"
#include "semantic.hpp"

#include <iostream>
#include <string>
#include <vector>

// and / or / not take bools, or bool fields in rulesets (with bool uniforms broadcast)

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
    return "function f(b: bool, n: integer, m: field<bool, 4>) -> integer { " + statements + " return 0; }";
}

static std::string ruleset(const std::string &statements)
{
    return "ruleset r(m: field<bool, 4>, k: field<bool, 3>, a: field<f32, 4>, i: field<i32, 4>, "
           "b: bool, x: f32) -> integer { " +
           statements + " return sum(i); }";
}

bool runLogicalTests()
{
    bool passing = true;

    passing &= accepts("uniform and / or / not", function("let c: bool = b and not b or b;"));
    passing &= accepts("comparisons combined", function("let c: bool = n > 0 and n < 10 or not (n == 5);"));
    passing &= accepts("logical in a condition", function("if b and n > 0 { n = 1; } while not b { b = true; }"));

    passing &= accepts("field and / or / not", ruleset("let y: field<bool, 4> = m and not m or m;"));
    passing &= accepts("field comparisons combined", ruleset("let y: field<bool, 4> = a > x and i < 3;"));
    passing &= accepts("bool uniform broadcast", ruleset("let y: field<bool, 4> = m or b;"));
    passing &= accepts("not of a reduction", ruleset("let y: bool = not any(m);"));

    passing &= rejects("and of integers", function("n and n;"), "Logical operators require bool operands");
    passing &= rejects("or of a bool and an integer", function("b or n;"),
                       "Logical operators require bool operands");
    passing &= rejects("not of an integer", function("not n;"), "'not' requires a bool operand");
    passing &= rejects("and of integer fields", ruleset("i and i;"), "Logical operators require bool operands");
    passing &= rejects("not of a float field", ruleset("not a;"), "'not' requires a bool operand");
    passing &= rejects("bool fields of different lengths", ruleset("m and k;"),
                       "Binary expression has mismatched types");

    // field operations are rulesets only
    passing &= rejects("field and in a function", function("m and m;"),
                       "Field operations are only allowed in rulesets");
    passing &= rejects("field not in a function", function("not m;"),
                       "Field operations are only allowed in rulesets");

    std::cout << "Logical Semantic Test Result: " << (passing ? "PASSED" : "FAILED") << "\n";
    return passing;
}
