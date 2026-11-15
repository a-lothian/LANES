#include "run_math_builtin_test.h"
#include "lexer.hpp"
#include "parser.hpp"
#include "semantic.hpp"

#include <iostream>
#include <string>
#include <vector>

// math built-ins take uniforms anywhere, or fields in rulesets (applied to each element,
// with uniform arguments broadcast)

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

// statements in a function with uniform parameters
static std::string function(const std::string &statements)
{
    return "function f(n: integer, x: f32, b: bool, a: field<f32, 4>) -> integer { " +
           statements + " return 0; }";
}

// statements in a ruleset with field and uniform parameters
static std::string ruleset(const std::string &statements)
{
    return "ruleset r(a: field<f32, 4>, i: field<i32, 4>, m: field<bool, 4>, c: field<f32, 3>, "
           "x: f32, n: integer, b: bool) -> integer { " +
           statements + " return sum(i); }";
}

bool runMathBuiltinTests()
{
    bool passing = true;

    // uniforms
    passing &= accepts("integer abs, min, max, clamp",
                       function("let y: integer = abs(n) + min(n, 2) + max(n, 2) + clamp(n, 0, 10);"));
    passing &= accepts("float abs, min, max, clamp",
                       function("let y: f32 = abs(x) + min(x, 2.0) + max(x, 2.0) + clamp(x, 0.0, 1.0);"));
    passing &= accepts("float rounding and sqrt",
                       function("let y: f32 = sqrt(x) + floor(x) + ceil(x) + round(x);"));
    passing &= accepts("conversions", function("let y: f32 = to_f32(n); let z: integer = to_i32(x);"));
    passing &= accepts("uniform select", function("let y: integer = select(b, 1, 2);"));

    // fields, with uniforms broadcast
    passing &= accepts("field abs and sqrt", ruleset("let y: field<f32, 4> = sqrt(abs(a));"));
    passing &= accepts("field min with a uniform", ruleset("let y: field<f32, 4> = min(a, 0.0);"));
    passing &= accepts("field clamp with mixed bounds", ruleset("let y: field<i32, 4> = clamp(i, i, 5);"));
    passing &= accepts("field conversions",
                       ruleset("let y: field<f32, 4> = to_f32(i); let z: field<i32, 4> = to_i32(a);"));
    passing &= accepts("select with a field mask", ruleset("let y: field<f32, 4> = select(m, a, 0.0);"));
    passing &= accepts("select with a uniform mask", ruleset("let y: field<f32, 4> = select(b, a, a);"));
    passing &= accepts("select with uniform values and a field mask",
                       ruleset("let y: field<i32, 4> = select(a > x, 1, 0);"));
    passing &= accepts("index has the argument's length", ruleset("let y: field<i32, 3> = index(c);"));

    // fields are rulesets only
    passing &= rejects("field abs in a function", function("abs(a);"),
                       "Field operations are only allowed in rulesets");
    passing &= rejects("index in a function", function("index(a);"),
                       "Field operations are only allowed in rulesets");

    // element types
    passing &= rejects("sqrt of an integer", function("sqrt(n);"), "'sqrt' requires floats");
    passing &= rejects("round of an integer field", ruleset("round(i);"), "'round' requires floats");
    passing &= rejects("to_f32 of a float", function("to_f32(x);"), "'to_f32' requires integers");
    passing &= rejects("to_i32 of an integer", function("to_i32(n);"), "'to_i32' requires floats");
    passing &= rejects("abs of a bool", function("abs(b);"), "'abs' requires integers or floats");
    passing &= rejects("min of bool fields", ruleset("min(m, m);"), "'min' requires integers or floats");

    // argument agreement
    passing &= rejects("min of integer and float", function("min(n, x);"),
                       "Arguments of 'min' have mismatched types");
    passing &= rejects("max of different lengths", ruleset("max(a, c);"),
                       "Arguments of 'max' have mismatched types");
    passing &= rejects("clamp with a mismatched bound", ruleset("clamp(a, 0, 1.0);"),
                       "Arguments of 'clamp' have mismatched types");
    passing &= rejects("select with a non-bool mask", function("select(n, 1, 2);"),
                       "'select' requires a bool mask as its first argument");
    passing &= rejects("select of mismatched values", function("select(b, 1, 2.0);"),
                       "Arguments of 'select' have mismatched types");
    passing &= rejects("select mask of a different length", ruleset("select(m, c, c);"),
                       "Arguments of 'select' have mismatched types");
    passing &= rejects("index of a uniform", ruleset("index(n);"), "'index' requires a field argument");

    passing &= rejects("clamp with two arguments", function("clamp(n, 0);"),
                       "Incorrect number of arguments for function: 'clamp'");
    passing &= rejects("index with no arguments", ruleset("index();"),
                       "Incorrect number of arguments for function: 'index'");
    passing &= rejects("function named abs", "function abs(n: integer) -> integer { return n; }",
                       "Cannot redeclare built-in: 'abs'");

    std::cout << "Math Built-in Semantic Test Result: " << (passing ? "PASSED" : "FAILED") << "\n";
    return passing;
}
