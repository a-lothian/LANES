#include "run_reduction_test.h"
#include "lexer.hpp"
#include "parser.hpp"
#include "semantic.hpp"

#include <iostream>
#include <string>
#include <vector>

// reductions combine a field into one uniform and are only allowed in rulesets;
// length is a compile-time constant and allowed anywhere

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

// statements inside a ruleset with float, integer and bool field parameters
static std::string ruleset(const std::string &statements)
{
    return "ruleset r(a: field<f32, 4>, i: field<i32, 4>, m: field<bool, 4>, s: f32) -> integer { " +
           statements + " return sum(i); }";
}

static std::string function(const std::string &statements)
{
    return "function f(a: field<f32, 4>, m: field<bool, 4>) -> integer { " + statements + " return 0; }";
}

bool runReductionTests()
{
    bool passing = true;

    // result types follow the elements
    passing &= accepts("sum of floats is a float", ruleset("let t: f32 = sum(a);"));
    passing &= accepts("sum of integers is an integer", ruleset("let t: integer = sum(i);"));
    passing &= accepts("min_of and max_of", ruleset("let lo: f32 = min_of(a); let hi: integer = max_of(i);"));
    passing &= accepts("any and all of bools", ruleset("let x: bool = any(m); let y: bool = all(a < s);"));
    passing &= accepts("reduction of a field expression", ruleset("let t: f32 = sum(a * s + 1.0);"));
    passing &= accepts("reduction results are uniforms", ruleset("let t: f32 = sum(a) / max_of(a) + s;"));

    // length is a constant, allowed in functions too
    passing &= accepts("length in a ruleset", ruleset("let n: integer = length(a);"));
    passing &= accepts("length in a function", function("let n: integer = length(a) + length([1, 2]);"));

    passing &= rejects("sum in a function", function("sum(a);"),
                       "Reductions are only allowed in rulesets: 'sum'");
    passing &= rejects("any in a function", function("any(m);"),
                       "Reductions are only allowed in rulesets: 'any'");
    passing &= rejects("sum of bools", ruleset("sum(m);"),
                       "'sum' requires a field of integers or floats");
    passing &= rejects("max_of of bools", ruleset("max_of(m);"),
                       "'max_of' requires a field of integers or floats");
    passing &= rejects("any of floats", ruleset("any(a);"),
                       "'any' requires a field of bools");
    passing &= rejects("all of integers", ruleset("all(i);"),
                       "'all' requires a field of bools");
    passing &= rejects("sum of a uniform", ruleset("sum(s);"),
                       "'sum' requires a field argument");
    passing &= rejects("length of a uniform", function("length(1);"),
                       "'length' requires a field argument");
    passing &= rejects("sum with two arguments", ruleset("sum(a, a);"),
                       "Incorrect number of arguments for function: 'sum'");
    passing &= rejects("length with no arguments", function("length();"),
                       "Incorrect number of arguments for function: 'length'");
    passing &= rejects("function named sum",
                       "function sum(n: integer) -> integer { return n; }",
                       "Cannot redeclare built-in: 'sum'");
    passing &= rejects("ruleset named length",
                       "ruleset length(n: integer) -> integer { return n; }",
                       "Cannot redeclare built-in: 'length'");

    std::cout << "Reduction Semantic Test Result: " << (passing ? "PASSED" : "FAILED") << "\n";
    return passing;
}
